/***************************************************************
 *
 * Copyright (C) 1990-2011, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/



#include "condor_common.h"
#include "condor_io.h"
#include "condor_debug.h"
#include "daemon.h"
#include "condor_uid.h"
#include "lsa_mgr.h"
#include "store_cred.h"
#include "condor_config.h"
#include "ipv6_hostname.h"

static int code_store_cred(Stream *socket, char* &user, char* &pw, int &mode);

#ifndef WIN32
	// **** UNIX CODE *****

void SecureZeroMemory(void *p, size_t n)
{
	// TODO: make this Secure
	memset(p, 0, n);
}

// trivial scramble on password file to prevent accidental viewing
// NOTE: its up to the caller to ensure scrambled has enough room
void simple_scramble(char* scrambled,  const char* orig, int len)
{
	const unsigned char deadbeef[] = {0xDE, 0xAD, 0xBE, 0xEF};

	for (int i = 0; i < len; i++) {
		scrambled[i] = orig[i] ^ deadbeef[i % sizeof(deadbeef)];
	}
}

// writes a pool password file using the given password
// returns true(success) or false(failure)
//
int write_password_file(const char* path, const char* password)
{
	size_t password_len = strlen(password);
	char scrambled_password[MAX_PASSWORD_LENGTH + 1];
	memset(scrambled_password, 0, MAX_PASSWORD_LENGTH + 1);
	simple_scramble(scrambled_password, password, password_len);
	return write_secure_file(path, scrambled_password, MAX_PASSWORD_LENGTH + 1, true);
}


// writes data of length len securely to file specified in path
// returns true(success) or false (failure)
//
bool write_secure_file(const char* path, const char* data, size_t len, bool as_root)
{
	int fd = 0;
	int save_errno = 0;

	if(as_root) {
		// create file with root priv but drop it asap
		priv_state priv = set_root_priv();
		fd = safe_open_wrapper_follow(path,
				   O_WRONLY | O_CREAT | O_TRUNC,
				   0600);
		save_errno = errno;
		set_priv(priv);
	} else {
		// create file as euid
		fd = safe_open_wrapper_follow(path,
				   O_WRONLY | O_CREAT | O_TRUNC,
				   0600);
		save_errno = errno;
	}

	if (fd == -1) {
		dprintf(D_ALWAYS,
			"ERROR: write_secure_file(%s): open() failed: %s (%d)\n",
			path,
			strerror(errno),
				errno);
		return false;
	}
	FILE *fp = fdopen(fd, "w");
	if (fp == NULL) {
		dprintf(D_ALWAYS,
			"ERROR: write_secure_file(%s): fdopen() failed: %s (%d)\n",
			path,
			strerror(errno),
			errno);
		return false;
	}

	size_t sz = fwrite(data, 1, len, fp);
	save_errno = errno;
	fclose(fp);

	if (sz != len) {
		dprintf(D_ALWAYS,
			"ERROR: write_secure_file(%s): error writing to file: %s (%d)\n",
			path,
			strerror(save_errno),
			save_errno);
		return false;
	}

	return true;
}

int ZKM_UNIX_STORE_CRED(const char *user, const char *pw, const int len, int mode) {
	dprintf(D_ALWAYS, "ZKM: store cred user %s len %i mode %i contents: {%s}\n", user, len, mode, pw);

	char* cred_dir = param("SEC_CREDENTIAL_DIRECTORY");
	if(!cred_dir) {
		dprintf(D_ALWAYS, "ERROR: got STORE_CRED but SEC_CREDENTIAL_DIRECTORY not defined!\n");
		return FAILURE;
	}

	// get username
	char username[256];
	const char *at = strchr(user, '@');
	strncpy(username, user, (at-user));
	username[at-user] = 0;

	// check to see if .cc already exists
	char ccfilename[PATH_MAX];
	sprintf(ccfilename, "%s%c%s.cc", cred_dir, DIR_DELIM_CHAR, username);
	struct stat junk_buf;
	int rc = stat(ccfilename, &junk_buf);
	if (rc==0) {
		// if the credential cache already exists, we don't even need
		// to write the file.  just return success as quickly as
		// possible.
		return SUCCESS;
	}

	// create filenames
	char tmpfilename[PATH_MAX];
	char filename[PATH_MAX];
	sprintf(tmpfilename, "%s%c%s.cred.tmp", cred_dir, DIR_DELIM_CHAR, username);
	sprintf(filename, "%s%c%s.cred", cred_dir, DIR_DELIM_CHAR, username);
	dprintf(D_ALWAYS, "ZKM: writing data to %s\n", tmpfilename);

	// ultimately, decode base64 encode pw
	int pwlen = strlen(pw);

	// write temp file
	rc = write_secure_file(tmpfilename, pw, pwlen, true);

	if (rc != SUCCESS) {
		dprintf(D_ALWAYS, "ZKM: failed to write secure temp file %s\n", tmpfilename);
		return FAILURE;
	}

	// now move into place
	dprintf(D_ALWAYS, "ZKM: renaming %s to %s\n", tmpfilename, filename);

	priv_state priv = set_root_priv();
	rc = rename(tmpfilename, filename);
	set_priv(priv);

	if (rc == -1) {
		dprintf(D_ALWAYS, "ZKM: failed to rename %s to %s\n", tmpfilename, filename);

		// should we rm tmpfilename ?
		return FAILURE;
	}

	// now signal the credmon
	pid_t credmon_pid = get_credmon_pid();
	if (credmon_pid == -1) {
		dprintf(D_ALWAYS, "ZKM: failed to get pid of credmon.\n");
		return FAILURE;
	}

	dprintf(D_ALWAYS, "ZKM: sending SIGHUP to credmon pid %i\n", credmon_pid);
	rc = kill(credmon_pid, SIGHUP);
	if (rc == -1) {
		dprintf(D_ALWAYS, "ZKM: failed to signal credmon: %i\n", errno);
		return FAILURE;
	}

	// now poll for existence of .cc file
	int retries = 20;
	while (retries > 0) {
		rc = stat(ccfilename, &junk_buf);
		if (rc==-1) {
			dprintf(D_ALWAYS, "ZKM: errno %i, waiting for %s to appear (%i seconds left)\n", errno, ccfilename, retries);
			sleep(1);
			retries--;
		} else {
			break;
		}
	}
	if (retries == 0) {
		dprintf(D_ALWAYS, "ZKM: FAILURE: credmon never created %s after 20 seconds!\n", ccfilename);
		return FAILURE;
	}

	dprintf(D_ALWAYS, "ZKM: SUCCESS: file %s found after %i seconds\n", ccfilename, 20-retries);
	return SUCCESS;
}

// read a "secure" file.
//
// this means we do a few extra checks:
// 1) we use the safe_open wrapper
// 2) we verify the file is owned by us
// 3) we verify the permissions are set such that no one else could read or write the file
// 4) we do our best to ensure the file did not change while we were reading it.
//
// *buf and *len will not be modified unless the call is succesful.  on
// success, they receive a pointer to a newly-malloc()ed buffer and the length.
//
bool
read_secure_file(const char *fname, char **buf, size_t *len, bool as_root)
{
	FILE* fp = 0;
	int save_errno = 0;

	if(as_root) {
		// open the file with root priv but drop it asap
		priv_state priv = set_root_priv();
		fp = safe_fopen_wrapper_follow(fname, "r");
		save_errno = errno;
		set_priv(priv);
	} else {
		fp = safe_fopen_wrapper_follow(fname, "r");
		save_errno = errno;
	}

	if (fp == NULL) {
		dprintf(D_FULLDEBUG,
		        "ERROR: read_secure_file(%s): open() failed: %s (errno: %d)\n",
		        fname,
		        strerror(save_errno),
		        save_errno);
		return false;
	}

	struct stat st;
	if (fstat(fileno(fp), &st) == -1) {
		dprintf(D_ALWAYS,
		        "ERROR: read_secure_file(%s): fstat() failed, %s (errno: %d)\n",
		        fname,
		        strerror(errno),
		        errno);
		fclose(fp);
		return false;
	}

	// make sure the file owner matches expected owner
	uid_t fowner;
	if(as_root) {
		fowner = getuid();
	} else {
		fowner = geteuid();
	}
	if (st.st_uid != fowner) {
		dprintf(D_ALWAYS,
			"ERROR: read_secure_file(%s): file must be owned "
			    "by uid %i, was uid %i\n", fname, fowner, st.st_uid);
		fclose(fp);
		return false;
	}

	// make sure no one else can read the file
	if (st.st_mode & 077) {
		dprintf(D_ALWAYS,
			"ERROR: read_secure_file(%s): file must not be readable "
			    "by others, had perms %o\n", fname, st.st_mode);
		fclose(fp);
		return false;
	}

	// now read the entire file.
	size_t fsize = st.st_size;
	char *fbuf = (char*)malloc(fsize);
	if(fbuf == NULL) {
		dprintf(D_ALWAYS,
			"ERROR: read_secure_file(%s): malloc(%lu) failed!\n", fname, fsize);
		fclose(fp);
		return false;
	}

	// actually read the data
	size_t readsize = fread(fbuf, 1, fsize, fp);

	// this if block is debatable, as far as "security" goes.  it's
	// perfectly legal to have a short read.  if the file was indeed
	// modified then that will be reflected in the second fstat below.
	//
	// however, since i haven't implemented a retry loop here, i'm going to
	// report failure on a short read for now.
	//
	if(readsize != fsize) {
		dprintf(D_ALWAYS,
			"ERROR: read_secure_file(%s): failed due to short read: %lu != %lu!\n",
			fname, readsize, fsize);
		fclose(fp);
		free(fbuf);
		return false;
	}

	// now, let's try to ensure the file or its attributes did not change
	// during this function.  stat it again and fail if anything was
	// modified.
	struct stat st2;
	if (fstat(fileno(fp), &st2) == -1) {
		dprintf(D_ALWAYS,
		        "ERROR: read_secure_file(%s): second fstat() failed, %s (errno: %d)\n",
		        fname,
		        strerror(errno),
		        errno);
		fclose(fp);
		free(fbuf);
		return false;
	}

	if ( (st.st_mtime != st2.st_mtime) || (st.st_ctime != st2.st_ctime) ) {
		dprintf(D_ALWAYS,
		        "ERROR: read_secure_file(%s): %lu!=%lu  OR  %lu!=%lu\n",
			 fname, st.st_mtime,st2.st_mtime, st.st_ctime,st2.st_ctime);
		fclose(fp);
		free(fbuf);
		return false;
	}

	if(fclose(fp) != 0) {
		dprintf(D_ALWAYS,
		        "ERROR: read_secure_file(%s): fclose() failed: %s (errno: %d)\n",
			fname, strerror(errno), errno);
		free(fbuf);
		return false;
	}

	// give malloc()ed buffer and length to caller.  do not free.
	*buf = fbuf;
	*len = fsize;

	dprintf(D_ALWAYS, "CERN: STORE CRED SUCCEEDED!\n");
	return true;
}


char*
ZKM_UNIX_GET_CRED(const char *user, const char *domain)
{
	dprintf(D_ALWAYS, "ZKM: get cred user %s domain %s\n", user, domain);

	char* cred_dir = param("SEC_CREDENTIAL_DIRECTORY");
	if(!cred_dir) {
		dprintf(D_ALWAYS, "ERROR: got GET_CRED but SEC_CREDENTIAL_DIRECTORY not defined!\n");
		return NULL;
	}

	// create filenames
	MyString filename;
	filename.formatstr("%s%c%s.cred", cred_dir, DIR_DELIM_CHAR, user);
	dprintf(D_ALWAYS, "CERN: reading data from %s\n", filename.c_str());

	// read the file (fourth argument "true" means as_root)
	char  *buf = 0;
	size_t len = 0;
	bool rc = read_secure_file(filename.c_str(), &buf, &len, true);

	// HACK: asserting for now that the contents are TEXT, and contain NO
	// NULL so that we can return a null-terminated string.  it's possible
	// we'll convert to base64 right here and return that, so it might
	// actually be true.
	if(rc) {
		char* textpw = (char*)malloc(len+1);
		strncpy(textpw, buf, len);
		textpw[len] = 0;
		free(buf);
		return textpw;
	}

	return NULL;
}


char* getStoredCredential(const char *username, const char *domain)
{
	// TODO: add support for multiple domains

	if ( !username || !domain ) {
		return NULL;
	}

	if (strcmp(username, POOL_PASSWORD_USERNAME) != 0) {
		dprintf(D_ALWAYS, "ZKM: GOT UNIX GET CRED\n");
		return ZKM_UNIX_GET_CRED(username, domain);
	} 

	// EVERYTHING BELOW HERE IS FOR POOL PASSWORD ONLY

	char *filename = param("SEC_PASSWORD_FILE");
	if (filename == NULL) {
		dprintf(D_ALWAYS,
		        "error fetching pool password; "
		            "SEC_PASSWORD_FILE not defined\n");
		return NULL;
	}

	// open the pool password file with root priv
	priv_state priv = set_root_priv();
	FILE* fp = safe_fopen_wrapper_follow(filename, "r");
	int save_errno = errno;
	set_priv(priv);
	if (fp == NULL) {
		dprintf(D_FULLDEBUG,
		        "error opening SEC_PASSWORD_FILE (%s), %s (errno: %d)\n",
		        filename,
		        strerror(save_errno),
		        save_errno);
		free(filename);
		return NULL;
	}

	// make sure the file owner matches our real uid
	struct stat st;
	if (fstat(fileno(fp), &st) == -1) {
		dprintf(D_ALWAYS,
		        "fstat failed on SEC_PASSWORD_FILE (%s), %s (errno: %d)\n",
		        filename,
		        strerror(errno),
		        errno);
		fclose(fp);
		free(filename);
		return NULL;
	}
	free(filename);
	if (st.st_uid != get_my_uid()) {
		dprintf(D_ALWAYS,
		        "error: SEC_PASSWORD_FILE must be owned "
		            "by Condor's real uid\n");
		fclose(fp);
		return NULL;
	}

	char scrambled_pw[MAX_PASSWORD_LENGTH + 1];
	size_t sz = fread(scrambled_pw, 1, MAX_PASSWORD_LENGTH, fp);
	fclose(fp);

	if (sz == 0) {
		dprintf(D_ALWAYS, "error reading pool password (file may be empty)\n");
		return NULL;
	}
	scrambled_pw[sz] = '\0';  // ensure the last char is nil

	// undo the trivial scramble
	int len = strlen(scrambled_pw);
	char *pw = (char *)malloc(len + 1);
	simple_scramble(pw, scrambled_pw, len);
	pw[len] = '\0';

	return pw;
}

int store_cred_service(const char *user, const char *cred, const size_t credlen, int mode)
{
	const char *at = strchr(user, '@');
	if ((at == NULL) || (at == user)) {
		dprintf(D_ALWAYS, "store_cred: malformed user name\n");
		return FAILURE;
	}
	if (( (size_t)(at - user) != strlen(POOL_PASSWORD_USERNAME)) ||
	    (memcmp(user, POOL_PASSWORD_USERNAME, at - user) != 0))
	{
		dprintf(D_ALWAYS, "ZKM: GOT UNIX STORE CRED\n");
		return ZKM_UNIX_STORE_CRED(user, cred, credlen, mode);
	}

	//
	// THIS CODE BELOW ALL DEALS EXCLUSIVELY WITH POOL PASSWORD
	//

	char *filename;
	if (mode != QUERY_MODE) {
		filename = param("SEC_PASSWORD_FILE");
		if (filename == NULL) {
			dprintf(D_ALWAYS, "store_cred: SEC_PASSWORD_FILE not defined\n");
			return FAILURE;
		}
	}

	int answer;
	switch (mode) {
	case ADD_MODE: {
		answer = FAILURE;
		size_t cred_sz = strlen(cred);
		if (!cred_sz) {
			dprintf(D_ALWAYS,
			        "store_cred_service: empty password not allowed\n");
			break;
		}
		if (cred_sz > MAX_PASSWORD_LENGTH) {
			dprintf(D_ALWAYS, "store_cred_service: password too large\n");
			break;
		}
		priv_state priv = set_root_priv();
		answer = write_password_file(filename, cred);
		set_priv(priv);
		break;
	}
	case DELETE_MODE: {
		priv_state priv = set_root_priv();
		int err = unlink(filename);
		set_priv(priv);
		if (!err) {
			answer = SUCCESS;
		}
		else {
			answer = FAILURE_NOT_FOUND;
		}
		break;
	}
	case QUERY_MODE: {
		char *password = getStoredCredential(POOL_PASSWORD_USERNAME, NULL);
		if (password) {
			answer = SUCCESS;
			SecureZeroMemory(password, MAX_PASSWORD_LENGTH);
			free(password);
		}
		else {
			answer = FAILURE_NOT_FOUND;
		}
		break;
	}
	default:
		dprintf(D_ALWAYS, "store_cred_service: unknown mode: %d\n", mode);
		answer = FAILURE;
	}

	// clean up after ourselves
	if (mode != QUERY_MODE) {
		free(filename);
	}

	return answer;
}


#else
	// **** WIN32 CODE ****

#include <conio.h>

//extern "C" FILE *DebugFP;
//extern "C" int DebugFlags;

char* getStoredCredential(const char *username, const char *domain)
{
	lsa_mgr lsaMan;
	char pw[255];
	wchar_t w_fullname[512];
	wchar_t *w_pw;

	if ( !username || !domain ) {
		return NULL;
	}

	if ( _snwprintf(w_fullname, 254, L"%S@%S", username, domain) < 0 ) {
		return NULL;
	}

	// make sure we're SYSTEM when we do this
	priv_state priv = set_root_priv();
	w_pw = lsaMan.query(w_fullname);
	set_priv(priv);

	if ( ! w_pw ) {
		dprintf(D_ALWAYS, 
			"getStoredCredential(): Could not locate credential for user "
			"'%s@%s'\n", username, domain);
		return NULL;
	}

	if ( _snprintf(pw, sizeof(pw), "%S", w_pw) < 0 ) {
		return NULL;
	}

	// we don't need the wide char pw anymore, so clean it up
	SecureZeroMemory(w_pw, wcslen(w_pw)*sizeof(wchar_t));
	delete[](w_pw);

	dprintf(D_FULLDEBUG, "Found credential for user '%s@%s'\n",
		username, domain );
	return strdup(pw);
}

int store_cred_service(const char *user, const char *pw, int mode) 
{

	wchar_t pwbuf[MAX_PASSWORD_LENGTH];
	wchar_t userbuf[MAX_PASSWORD_LENGTH];
	priv_state priv;
	int answer = FAILURE;
	lsa_mgr lsa_man;
	wchar_t *pw_wc;
	
	// we'll need a wide-char version of the user name later
	if ( user ) {
		swprintf_s(userbuf, COUNTOF(userbuf), L"%S", user);
	}

	if (!can_switch_ids()) {
		answer = FAILURE_NOT_SUPPORTED;
	} else {
		priv = set_root_priv();
		
		switch(mode) {
		case ADD_MODE:
			bool retval;

			dprintf( D_FULLDEBUG, "Adding %S to credential storage.\n", 
				userbuf );

			retval = isValidCredential(user, pw);

			if ( ! retval ) {
				dprintf(D_FULLDEBUG, "store_cred: tried to add invalid credential\n");
				answer=FAILURE_BAD_PASSWORD; 
				break; // bail out 
			}

			if (pw) {
				swprintf_s(pwbuf, COUNTOF(pwbuf), L"%S", pw); // make a wide-char copy first
			}

			// call lsa_mgr api
			// answer = return code
			if (!lsa_man.add(userbuf, pwbuf)){
				answer = FAILURE;
			} else {
				answer = SUCCESS;
			}
			SecureZeroMemory(pwbuf, MAX_PASSWORD_LENGTH*sizeof(wchar_t)); 
			break;
		case DELETE_MODE:
			dprintf( D_FULLDEBUG, "Deleting %S from credential storage.\n", 
				userbuf );

			pw_wc = lsa_man.query(userbuf);
			if ( !pw_wc ) {
				answer = FAILURE_NOT_FOUND;
				break;
			}
			else {
				SecureZeroMemory(pw_wc, wcslen(pw_wc));
				delete[] pw_wc;
			}

			if (!isValidCredential(user, pw)) {
				dprintf(D_FULLDEBUG, "store_cred: invalid credential given for delete\n");
				answer = FAILURE_BAD_PASSWORD;
				break;
			}

			// call lsa_mgr api
			// answer = return code
			if (!lsa_man.remove(userbuf)) {
				answer = FAILURE;
			} else {
				answer = SUCCESS;
			}
			break;
		case QUERY_MODE:
			{
				dprintf( D_FULLDEBUG, "Checking for %S in credential storage.\n", 
					 userbuf );
				
				char passw[MAX_PASSWORD_LENGTH];
				pw_wc = lsa_man.query(userbuf);
				
				if ( !pw_wc ) {
					answer = FAILURE_NOT_FOUND;
				} else {
					sprintf(passw, "%S", pw_wc);
					SecureZeroMemory(pw_wc, wcslen(pw_wc));
					delete[] pw_wc;
					
					if ( isValidCredential(user, passw) ) {
						answer = SUCCESS;
					} else {
						answer = FAILURE_BAD_PASSWORD;
					}
					
					SecureZeroMemory(passw, MAX_PASSWORD_LENGTH);
				}
				break;
			}
		default:
				dprintf( D_ALWAYS, "store_cred: Unknown access mode (%d).\n", mode );
				answer=0;
				break;
		}
			
		dprintf(D_FULLDEBUG, "Switching back to old priv state.\n");
		set_priv(priv);
	}
	
	return answer;
}	


// takes user@domain format for user argument
bool
isValidCredential( const char *input_user, const char* input_pw ) {
	// see if we can get a user token from this password
	HANDLE usrHnd = NULL;
	char* dom;
	DWORD LogonUserError;
	BOOL retval;

	retval = 0;
	usrHnd = NULL;

	char * user = strdup(input_user);
	
	// split the domain and the user name for LogonUser
	dom = strchr(user, '@');

	if ( dom ) {
		*dom = '\0';
		dom++;
	}

	// the POOL_PASSWORD_USERNAME account is not a real account
	if (strcmp(user, POOL_PASSWORD_USERNAME) == 0) {
		free(user);
		return true;
	}

	char * pw = strdup(input_pw);
	bool wantSkipNetworkLogon = param_boolean("SKIP_WINDOWS_LOGON_NETWORK", false);
	if (!wantSkipNetworkLogon) {
	  retval = LogonUser(
		user,						// user name
		dom,						// domain or server - local for now
		pw,							// password
		LOGON32_LOGON_NETWORK,		// NETWORK is fastest. 
		LOGON32_PROVIDER_DEFAULT,	// logon provider
		&usrHnd						// receive tokens handle
	  );
	  LogonUserError = GetLastError();
	}
	if ( 0 == retval ) {
		if (!wantSkipNetworkLogon) {
		  dprintf(D_FULLDEBUG, "NETWORK logon failed. Attempting INTERACTIVE\n");
		} else {
		  dprintf(D_FULLDEBUG, "NETWORK logon disabled. Trying INTERACTIVE only!\n");
		}
		retval = LogonUser(
			user,						// user name
			dom,						// domain or server - local for now
			pw,							// password
			LOGON32_LOGON_INTERACTIVE,	// INTERACTIVE should be held by everyone.
			LOGON32_PROVIDER_DEFAULT,	// logon provider
			&usrHnd						// receive tokens handle
		);
		LogonUserError = GetLastError();
	}

	if (user) free(user);
	if (pw) {
		SecureZeroMemory(pw,strlen(pw));
		free(pw);
	}

	if ( retval == 0 ) {
		dprintf(D_ALWAYS, "Failed to log in %s with err=%d\n", 
				input_user, LogonUserError);
		return false;
	} else {
		dprintf(D_FULLDEBUG, "Succeeded to log in %s\n", input_user);
		CloseHandle(usrHnd);
		return true;
	}
}

#endif // WIN32


/* NOW WORKS ON BOTH WINDOWS AND UNIX */
void store_cred_handler(void *, int /*i*/, Stream *s)
{
	char *user = NULL;
	char *pw = NULL;
	int mode;
	int result;
	int answer = FAILURE;

	s->decode();

	result = code_store_cred(s, user, pw, mode);

	if( result == FALSE ) {
		dprintf(D_ALWAYS, "store_cred: code_store_cred failed.\n");
		return;
	}

	if ( user ) {
			// ensure that the username has an '@' delimteter
		char const *tmp = strchr(user, '@');
		if ((tmp == NULL) || (tmp == user)) {
			dprintf(D_ALWAYS, "store_cred_handler: user not in user@domain format\n");
			answer = FAILURE;
		}
		else {
				// we don't allow updates to the pool password through this interface
			if ((mode != QUERY_MODE) &&
			    (tmp - user == strlen(POOL_PASSWORD_USERNAME)) &&
			    (memcmp(user, POOL_PASSWORD_USERNAME, tmp - user) == 0))
			{
				dprintf(D_ALWAYS, "ERROR: attempt to set pool password via STORE_CRED! (must use STORE_POOL_CRED)\n");
				answer = FAILURE;
			} else {
				answer = store_cred_service(user,pw,strlen(pw)+1,mode);
			}
		}
	}

	if (pw) {
		SecureZeroMemory(pw, strlen(pw));
		free(pw);
	}
	if (user) {
		free(user);
	}

	s->encode();
	if( ! s->code(answer) ) {
		dprintf( D_ALWAYS,
			"store_cred: Failed to send result.\n" );
		return;
	}

	if( ! s->end_of_message() ) {
		dprintf( D_ALWAYS,
			"store_cred: Failed to send end of message.\n");
	}

	return;
}

static int code_store_cred(Stream *socket, char* &user, char* &pw, int &mode) {
	
	int result;
	
	result = socket->code(user);
	if( !result ) {
		dprintf(D_ALWAYS, "store_cred: Failed to send/recv user.\n");
		return FALSE;
	}
	
	result = socket->code(pw);
	if( !result ) {
		dprintf(D_ALWAYS, "store_cred: Failed to send/recv pw.\n");
		return FALSE;
	}
	
	result = socket->code(mode);
	if( !result ) {
		dprintf(D_ALWAYS, "store_cred: Failed to send/recv mode.\n");
		return FALSE;
	}
	
	result = socket->end_of_message();
	if( !result ) {
		dprintf(D_ALWAYS, "store_cred: Failed to send/recv eom.\n");
		return FALSE;
	}
	
	return TRUE;
	
}

void store_pool_cred_handler(void *, int  /*i*/, Stream *s)
{
	int result;
	char *pw = NULL;
	char *domain = NULL;
	MyString username = POOL_PASSWORD_USERNAME "@";

	if (s->type() != Stream::reli_sock) {
		dprintf(D_ALWAYS, "ERROR: pool password set attempt via UDP\n");
		return;
	}

	// if we're the CREDD_HOST, make sure any password setting is done locally
	// (since knowing what the pool password is on the CREDD_HOST means being
	//  able to fetch users' passwords)
	char *credd_host = param("CREDD_HOST");
	if (credd_host) {

		MyString my_fqdn_str = get_local_fqdn();
		MyString my_hostname_str = get_local_hostname();
		// TODO: Arbitrarily picking IPv4
		MyString my_ip_str = get_local_ipaddr(CP_IPV4).to_ip_string();

		// figure out if we're on the CREDD_HOST
		bool on_credd_host = (strcasecmp(my_fqdn_str.Value(), credd_host) == MATCH);
		on_credd_host = on_credd_host || (strcasecmp(my_hostname_str.Value(), credd_host) == MATCH);
		on_credd_host = on_credd_host || (strcmp(my_ip_str.Value(), credd_host) == MATCH);

		if (on_credd_host) {
				// we're the CREDD_HOST; make sure the source address matches ours
			const char *addr = ((ReliSock*)s)->peer_ip_str();
			if (!addr || strcmp(my_ip_str.Value(), addr)) {
				dprintf(D_ALWAYS, "ERROR: attempt to set pool password remotely\n");
				free(credd_host);
				return;
			}
		}
		free(credd_host);
	}

	s->decode();
	if (!s->code(domain) || !s->code(pw) || !s->end_of_message()) {
		dprintf(D_ALWAYS, "store_pool_cred: failed to receive all parameters\n");
		goto spch_cleanup;
	}
	if (domain == NULL) {
		dprintf(D_ALWAYS, "store_pool_cred_handler: domain is NULL\n");
		goto spch_cleanup;
	}

	// construct the full pool username
	username += domain;	

	// do the real work
	if (pw) {
		result = store_cred_service(username.Value(), pw, strlen(pw)+1, ADD_MODE);
		SecureZeroMemory(pw, strlen(pw));
	}
	else {
		result = store_cred_service(username.Value(), NULL, 0, DELETE_MODE);
	}

	s->encode();
	if (!s->code(result)) {
		dprintf(D_ALWAYS, "store_pool_cred: Failed to send result.\n");
		goto spch_cleanup;
	}
	if (!s->end_of_message()) {
		dprintf(D_ALWAYS, "store_pool_cred: Failed to send end of message.\n");
	}

spch_cleanup:
	if (pw) free(pw);
	if (domain) free(domain);
}

int 
store_cred(const char* user, const char* pw, int mode, Daemon* d, bool force) {
	
	int result;
	int return_val;
	Sock* sock = NULL;

		// to help future debugging, print out the mode we are in
	static const int mode_offset = 100;
	static const char *mode_name[] = {
		ADD_CREDENTIAL,
		DELETE_CREDENTIAL,
		QUERY_CREDENTIAL
#ifdef WIN32
		, CONFIG_CREDENTIAL
#endif
	};	
	dprintf ( D_ALWAYS, 
		"STORE_CRED: In mode '%s'\n", 
		mode_name[mode - mode_offset] );
	
		// If we are root / SYSTEM and we want a local daemon, 
		// then do the work directly to the local registry.
		// If not, then send the request over the wire to a remote credd or schedd.

	if ( is_root() && d == NULL ) {
			// do the work directly onto the local registry
		return_val = store_cred_service(user,pw,strlen(pw)+1,mode);
	} else {
			// send out the request remotely.

			// first see if we're operating on the pool password
		int cmd = STORE_CRED;
		char const *tmp = strchr(user, '@');
		if (tmp == NULL || tmp == user || *(tmp + 1) == '\0') {
			dprintf(D_ALWAYS, "store_cred: user not in user@domain format\n");
			return FAILURE;
		}
		if (((mode == ADD_MODE) || (mode == DELETE_MODE)) &&
		    ( (size_t)(tmp - user) == strlen(POOL_PASSWORD_USERNAME)) &&
		    (memcmp(POOL_PASSWORD_USERNAME, user, tmp - user) == 0))
		{
			cmd = STORE_POOL_CRED;
			user = tmp + 1;	// we only need to send the domain name for STORE_POOL_CRED
		}

		if (d == NULL) {
			if (cmd == STORE_POOL_CRED) {
				// need to go to the master for setting the pool password
				dprintf(D_FULLDEBUG, "Storing credential to local master\n");
				Daemon my_master(DT_MASTER);
				sock = my_master.startCommand(cmd, Stream::reli_sock, 0);
			}
			else {
				dprintf(D_FULLDEBUG, "Storing credential to local schedd\n");
				Daemon my_schedd(DT_SCHEDD);
				sock = my_schedd.startCommand(cmd, Stream::reli_sock, 0);
			}
		} else {
			dprintf(D_FULLDEBUG, "Starting a command on a REMOTE schedd\n");
			sock = d->startCommand(cmd, Stream::reli_sock, 0);
		}
		
		if( !sock ) {
			dprintf(D_ALWAYS, 
				"STORE_CRED: Failed to start command.\n");
			dprintf(D_ALWAYS, 
				"STORE_CRED: Unable to contact the REMOTE schedd.\n");
			return FAILURE;
		}

		// for remote updates (which send the password), verify we have a secure channel,
		// unless "force" is specified
		if (((mode == ADD_MODE) || (mode == DELETE_MODE)) && !force && (d != NULL) &&
			((sock->type() != Stream::reli_sock) || !((ReliSock*)sock)->triedAuthentication() || !sock->get_encryption())) {
			dprintf(D_ALWAYS, "STORE_CRED: blocking attempt to update over insecure channel\n");
			delete sock;
			return FAILURE_NOT_SECURE;
		}
		
		if (cmd == STORE_CRED) {
			result = code_store_cred(sock, const_cast<char*&>(user),
				const_cast<char*&>(pw), mode);
			if( result == FALSE ) {
				dprintf(D_ALWAYS, "store_cred: code_store_cred failed.\n");
				delete sock;
				return FAILURE;
			}
		}
		else {
				// only need to send the domain and password for STORE_POOL_CRED
			if (!sock->code(const_cast<char*&>(user)) ||
				!sock->code(const_cast<char*&>(pw)) ||
				!sock->end_of_message()) {
				dprintf(D_ALWAYS, "store_cred: failed to send STORE_POOL_CRED message\n");
				delete sock;
				return FAILURE;
			}
		}
		
		sock->decode();
		
		result = sock->code(return_val);
		if( !result ) {
			dprintf(D_ALWAYS, "store_cred: failed to recv answer.\n");
			delete sock;
			return FAILURE;
		}
		
		result = sock->end_of_message();
		if( !result ) {
			dprintf(D_ALWAYS, "store_cred: failed to recv eom.\n");
			delete sock;
			return FAILURE;
		}
	}	// end of case where we send out the request remotely
	
	
	switch(mode)
	{
	case ADD_MODE:
		if( return_val == SUCCESS ) {
			dprintf(D_FULLDEBUG, "Addition succeeded!\n");					
		} else {
			dprintf(D_FULLDEBUG, "Addition failed!\n");
		}
		break;
	case DELETE_MODE:
		if( return_val == SUCCESS ) {
			dprintf(D_FULLDEBUG, "Delete succeeded!\n");
		} else {
			dprintf(D_FULLDEBUG, "Delete failed!\n");
		}
		break;
	case QUERY_MODE:
		if( return_val == SUCCESS ) {
			dprintf(D_FULLDEBUG, "We have a credential stored!\n");
		} else {
			dprintf(D_FULLDEBUG, "Query failed!\n");
		}
		break;
	}

	if ( sock ) delete sock;

	return return_val;
}	

int deleteCredential( const char* user, const char* pw, Daemon *d ) {
	return store_cred(user, pw, DELETE_MODE, d);	
}

int addCredential( const char* user, const char* pw, Daemon *d ) {
	return store_cred(user, pw, ADD_MODE, d);	
}

int queryCredential( const char* user, Daemon *d ) {
	return store_cred(user, NULL, QUERY_MODE, d);	
}

#if !defined(WIN32)

// helper routines for UNIX keyboard input
#include <termios.h>

static struct termios stored_settings;

static void echo_off(void)
{
	struct termios new_settings;
	tcgetattr(0, &stored_settings);
	memcpy(&new_settings, &stored_settings, sizeof(struct termios));
	new_settings.c_lflag &= (~ECHO);
	tcsetattr(0, TCSANOW, &new_settings);
	return;
}

static void echo_on(void)
{
    tcsetattr(0,TCSANOW,&stored_settings);
    return;
}

#endif

// reads at most maxlength chars without echoing to the terminal into buf
bool
read_from_keyboard(char* buf, int maxlength, bool echo) {
#if !defined(WIN32)
	int ch, ch_count;

	ch = ch_count = 0;
	fflush(stdout);

	const char end_char = '\n';
	if (!echo) echo_off();
			
	while ( ch_count < maxlength-1 ) {
		ch = getchar();
		if ( ch == end_char ) {
			break;
		} else if ( ch == '\b') { // backspace
			if ( ch_count > 0 ) { ch_count--; }
			continue;
		} else if ( ch == '\003' ) { // CTRL-C
			return FALSE;
		}
		buf[ch_count++] = (char) ch;
	}
	buf[ch_count] = '\0';

	if (!echo) echo_on();
#else
	/*
	The Windows method for getting keyboard input is very different due to
	issues encountered by British users trying to use the pound character in their
	passwords.  _getch did not accept any input from using the alt+#### method of
	inputting characters nor the pound symbol when using a British keyboard layout.
	The solution was to explicitly use ReadConsoleW, the unicode version of ReadConsole,
	to take in the password and down convert it into ascii.
	See Ticket #1639
	*/
	HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);

	/*
	There is a difference between the code page the console is reporting and
	the code page that needs to be used for the ascii conversion.  Below code
	acts to provide additional debug information should the need arise.
	*/
	//UINT cPage = GetConsoleCP();
	//printf("Console CP: %d\n", cPage);

	//Preserve the previous console mode and switch back once input is complete.
	DWORD oldMode;
	GetConsoleMode(hStdin, &oldMode);
	//Default entry method is to not echo back what is entered.
	DWORD newMode = ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT;
	if(echo)
	{
		newMode |= ENABLE_ECHO_INPUT;
	}

	SetConsoleMode(hStdin, newMode);

	int cch;
	//maxlength is passed in even though it is a fairly constant value, so need to dynamically allocate.
	wchar_t *wbuffer = new wchar_t[maxlength];
	if(!wbuffer)
	{
		return FALSE;
	}
	ReadConsoleW(hStdin, wbuffer, maxlength, (DWORD*)&cch, NULL);
	SetConsoleMode(hStdin, oldMode);
	//Zero terminate the input.
	cch = min(cch, maxlength-1);
	wbuffer[cch] = '\0';

	--cch;
	//Strip out the newline and return that ReadConsoleW appends and zero terminate again.
	while (cch >= 0)
	{
		if(wbuffer[cch] == '\r' || wbuffer[cch] == '\n')
			wbuffer[cch] = '\0';
		else
			break;
		--cch;
	}

	//Down convert the input into ASCII.
	int converted = WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK, wbuffer, -1, buf, maxlength, NULL, NULL);

	delete[] wbuffer;
#endif

	return TRUE;
}

char*
get_password() {
	char *buf;
	
	buf = new char[MAX_PASSWORD_LENGTH + 1];
	
	if (! buf) { fprintf(stderr, "Out of Memory!\n\n"); return NULL; }
	
		
	printf("Enter password: ");
	if ( ! read_from_keyboard(buf, MAX_PASSWORD_LENGTH + 1, false) ) {
		delete[] buf;
		return NULL;
	}
	
	return buf;
}

static int zkm_credmon_pid = -1;

int get_credmon_pid() {
	if(zkm_credmon_pid == -1) {
		// get pid of credmon
		MyString cred_dir;
		param(cred_dir, "SEC_CREDENTIAL_DIRECTORY");
		MyString pid_path;
		pid_path.formatstr("%s/pid", cred_dir.c_str());
		FILE* credmon_pidfile = fopen(pid_path.c_str(), "r");
		int num_items = fscanf(credmon_pidfile, "%i", &zkm_credmon_pid);
		fclose(credmon_pidfile);
		if (num_items != 1) {
			zkm_credmon_pid = -1;
		}
		dprintf(D_ALWAYS, "CERN: get_credmon_pid %s == %i\n", pid_path.c_str(), zkm_credmon_pid);
	}
	return zkm_credmon_pid;
}

