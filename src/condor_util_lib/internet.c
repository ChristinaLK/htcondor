/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
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

/*
** These are functions for generating internet addresses 
** and internet names
**
**             Author : Dhrubajyoti Borthakur
**               28 July, 1994
*/

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_uid.h"
#include "internet.h"
#include "my_hostname.h"
#include "condor_config.h"
#include "get_port_range.h"
#include "condor_socket_types.h"
#include "condor_netdb.h"
#if defined ( WIN32 )
#include <iphlpapi.h>
#endif

int bindWithin(const int fd, const int low_port, const int high_port);


/* Convert a string of the form "<xx.xx.xx.xx:pppp>" to a sockaddr_in  TCP */
/* (Also allow strings of the form "<hostname>:pppp>")  */
/* This function has a unit test. */
int
string_to_sin( const char *addr, struct sockaddr_in *sa_in )
{
	char    *cur_byte;
	char    *end_string;
	int 	temp=0;
	char*	addrCpy;
	char*	string;
	char*   colon = 0;
	struct  hostent *hostptr;

	if( ! addr ) {
		return 0;
	}
	addrCpy = strdup(addr);
	string = addrCpy;
	string++;					/* skip the leading '<' */

	/* allow strings of the form "<hostname:pppp>" */
	if( !(colon = strchr(string, ':')) ) {
			// Not a valid sinful string, return failure
		free(addrCpy);
		return 0;
	}
	*colon = '\0';
	if ( is_ipaddr(string,NULL) != TRUE &&	// only call gethostbyname if not numbers
		((hostptr=condor_gethostbyname(string)) != NULL && hostptr->h_addrtype==AF_INET) )
	{
			sa_in->sin_addr = *(struct in_addr *)(hostptr->h_addr_list[0]);
			string = colon + 1;
	}
	else
	{	
		/* parse the string in the traditional <xxx.yyy.zzz.aaa> form ... */	
		*colon = ':';
		cur_byte = (char *) &(sa_in->sin_addr);
		for(end_string = string; end_string != 0; ) {
			end_string = strchr(string, '.');
			if (end_string == 0) {
				end_string = strchr(string, ':');
				if (end_string) colon = end_string;
			}
			if (end_string) {
				*end_string = '\0';
				*cur_byte = atoi(string);
				cur_byte++;
				string = end_string + 1;
				*end_string = '.';
			}
		}
	}
	
	string[strlen(string) - 1] = '\0'; /* Chop off the trailing '>' */
	sa_in->sin_port = htons((short)atoi(string));
	sa_in->sin_family = AF_INET;
	string[temp-1] = '>';
	string[temp] = '\0';
	*colon = ':';
	free(addrCpy);
	return 1;
}


/* This function has a unit test. */
char *
sin_to_string(const struct sockaddr_in *sa_in)
{
	static  char    buf[SINFUL_STRING_BUF_SIZE];

	buf[0] = '\0';
	if (!sa_in) return buf;
	buf[0] = '<';
	buf[1] = '\0';
    if (sa_in->sin_addr.s_addr == INADDR_ANY) {
        strcat(buf, my_ip_string());
    } else {
        strcat(buf, inet_ntoa(sa_in->sin_addr));
    }
    sprintf(&buf[strlen(buf)], ":%d>", ntohs(sa_in->sin_port));
    return buf;
}


char *
sock_to_string(SOCKET sockd)
{
	struct sockaddr_in	addr;
	SOCKET_LENGTH_TYPE	addr_len;
	static char *mynull = "\0";

	addr_len = sizeof(addr);

	if (getsockname(sockd, (struct sockaddr *)&addr, &addr_len) < 0) 
		return mynull;

	return ( sin_to_string( &addr ) );
}

char const *
sock_peer_to_string( SOCKET fd, char *buf, size_t buflen, char const *unknown )
{
	/* This value is never used.
	char const *sinful = sock_to_string( fd );
	*/

	struct sockaddr_in who;
	SOCKET_LENGTH_TYPE addr_len;

	addr_len = sizeof(who);
	memset(buf,0,buflen);
	if( getpeername(fd, (struct sockaddr *)&who, &addr_len) == 0) {
		if( who.sin_family == AF_INET ) {
			char const *sinful = sin_to_string( &who );
			if( sinful ) {
				strncpy(buf, sinful, buflen );
				if(buflen) {
					buf[buflen-1] = '\0'; /* ensure null termination */
				}
				return buf;
			}
		}
	}
	return unknown;
}

char *
sin_to_hostname( const struct sockaddr_in *from, char ***aliases)
{
    struct hostent  *hp;
    struct sockaddr_in caddr;

	if( !from ) {
		// make certain from is not NULL before derefencing it
		return NULL;
	}
    memcpy(&caddr, from, sizeof(caddr));

    // If 'from' is INADDR_ANY, then we use the canonical IP address
    // instead of "0.0.0.0" as the input to name query
    if (caddr.sin_addr.s_addr == INADDR_ANY) {
        caddr.sin_addr.s_addr = my_ip_addr();
    }

    if( (hp=condor_gethostbyaddr((char *)&caddr.sin_addr,
                sizeof(struct in_addr), AF_INET)) == NULL ) {
		// could not find a name for this address
        return NULL;
    } else {
		// CAREFULL: we are returning a staic buffer from gethostbyaddr.
		// The caller had better use the result immediately or copy it.
		// Also note this is not thread safe.  (as are lots of things in internet.c).
		if( aliases ) {
			*aliases = hp->h_aliases;
		}
		return hp->h_name;
    }
}


void
display_from( from )
struct sockaddr_in  *from;
{
    struct hostent  *hp;
    struct sockaddr_in caddr;

	if( !from ) {
		dprintf( D_ALWAYS, "from NULL source\n" );
		return;
	}
    memcpy(&caddr, from, sizeof(caddr));

    // If 'from' is INADDR_ANY, then we use the canonical IP address
    // instead of "0.0.0.0"
    if (caddr.sin_addr.s_addr == INADDR_ANY) {
        caddr.sin_addr.s_addr = my_ip_addr();
    }

    if( (hp=condor_gethostbyaddr((char *)&caddr.sin_addr,
                sizeof(struct in_addr), AF_INET)) == NULL ) {
        dprintf( D_ALWAYS, "from (%s), port %d\n",
            inet_ntoa(caddr.sin_addr), ntohs(caddr.sin_port) );
    } else {
        dprintf( D_ALWAYS, "from %s, port %d\n",
                                        hp->h_name, ntohs(caddr.sin_port) );
    }
}

char *
calc_subnet_name(void)
{
	char			subnetname[MAXHOSTNAMELEN];
	char			*subnet_ptr;
	char			*host_addr_string;
	int				subnet_length;
	struct			in_addr	in;
	unsigned int	host_ordered_addr;
	unsigned int		net_ordered_addr;

	if ( !(host_ordered_addr = my_ip_addr()) ) {
		return strdup("");
	}

	net_ordered_addr = htonl(host_ordered_addr);
	memcpy((char *) &in,(char *)&net_ordered_addr, sizeof(host_ordered_addr));
	host_addr_string = inet_ntoa( in );
	if( host_addr_string ) {
		subnet_ptr = (char *) strrchr(host_addr_string, '.');
		if(subnet_ptr == NULL) {
			return strdup("");
		}
		subnet_length = subnet_ptr - host_addr_string;
		strncpy(subnetname, host_addr_string, subnet_length);
		subnetname[subnet_length] = '\0';
		return (strdup(subnetname));
	}
	return strdup("");
}

int
same_host(const char *h1, const char *h2)
{
	struct hostent *he1, *he2;
	char cn1[MAXHOSTNAMELEN];

	if( h1 == NULL || h2 == NULL) {
		dprintf( D_ALWAYS, "Warning: attempting to compare null hostnames in same_host.\n");
		return FALSE;
	}

	if (strcmp(h1, h2) == MATCH) {
		return TRUE;
	}
	
	if ((he1 = condor_gethostbyname(h1)) == NULL) {
		return -1;
	}

	// stash h_name before our next call to gethostbyname
	strncpy(cn1, he1->h_name, MAXHOSTNAMELEN);
	cn1[MAXHOSTNAMELEN-1] = '\0';

	if ((he2 = condor_gethostbyname(h2)) == NULL) {
		return -1;
	}

	return (strcmp(cn1, he2->h_name) == MATCH);
}


/*
  Return TRUE if the given domain contains the given hostname, FALSE
  if not.  Origionally taken from condor_starter.V5/starter_common.C. 
  Moved here on 1/18/02 by Derek Wright.
*/
int
host_in_domain( const char *host, const char *domain )
{
    int skip;

    skip = strlen(host) - strlen(domain);
    if( skip < 0 ) {
        return FALSE;
    }

    if( stricmp(host+skip,domain) == MATCH ) {
        if(skip==0 || host[skip-1] == '.' || domain[0] == '.' ) {
            return TRUE;
        }
    }
    return FALSE;
}



static int
is_ipaddr_implementation(const char *inbuf, struct in_addr *sin_addr, struct in_addr *mask_addr,int allow_wildcard)
{
	int len;
	char buf[17];
	int part = 0;
	int i,j,x;
	char save_char;
	unsigned char *cur_byte = NULL;
	unsigned char *cur_mask_byte = NULL;
	if( sin_addr ) {
		cur_byte = (unsigned char *) sin_addr;
	}
	if( mask_addr ) {
		cur_mask_byte = (unsigned char *) mask_addr;
	}

	len = strlen(inbuf);
	if ( len < 1 || len > 15 ) 
		return FALSE;
		// shortest possible IP addr is "*" - 1 char
		// longest possible IP addr is "123.123.123.123" - 15 chars
	
	// copy to our local buf
	strncpy( buf, inbuf, 16 );

	// strip off any trailing wild card or '.'
	if ( buf[len-1] == '*' || buf[len-1] == '.' ) {
		if ( buf[len-2] == '.' )
			buf[len-2] = '\0';
		else
			buf[len-1] = '\0';
	}

	// Make certain we have a valid IP address, and count the parts,
	// and fill in sin_addr
	i = 0;
	for(;buf[i];) {
		
		j = i;
		while (buf[i] >= '0' && buf[i] <= '9') i++;
		// make certain a number was here
		if ( i == j )
			return FALSE;	
		// now that we know there was a number, check it is between 0 & 255
		save_char = buf[i];
		buf[i] = '\0';
		x = atoi( &(buf[j]) );
		if( x < 0 || x > 255 ) {
			return FALSE;
		}
		if( cur_byte ) {
			*cur_byte = x;	/* save into sin_addr */
			cur_byte++;
		}
		if( cur_mask_byte ) {
			*(cur_mask_byte++) = 255;
		}
		buf[i] = save_char;

		part++;
		
		if ( buf[i] == '\0' ) 
			break;
		
		if ( buf[i] == '.' )
			i++;
		else
			return FALSE;

		if ( part >= 4 )
			return FALSE;
	}

	if( !allow_wildcard && part != 4 ) {
		return FALSE;
	}

	if( cur_byte ) {
		for (i=0; i < 4 - part; i++) {
			*cur_byte = (unsigned char) 255;
			cur_byte++;
		}
	}
	if( cur_mask_byte ) {
		for (i=0; i < 4 - part; i++) {
			*(cur_mask_byte++) = 0;
		}
	}
	return TRUE;
}


/*
  is_ipaddr() returns TRUE if buf is an ascii IP address (like
  "144.11.11.11") and false if not (like "cs.wisc.edu").  Allow
  wildcard "*".  If we return TRUE, and we were passed in a non-NULL 
  sin_addr, it's filled in with the integer version of the ip address. 
NOTE: it looks like sin_addr may be modified even if the return
  value is FALSE.  -zmiller
*/
/* XXX:  Known problems:  This function succeeds even if something with less
 * than four octets is passed in without a wildcard.  Also, strings with
 * multiple wildcards (such as 192.168.*.*) are not allowed, perhaps those
 * should be considered the same as 192.168.*  ~tristan 8/16/07
 */
/* This function has a unit test. */
int
is_ipaddr(const char *inbuf, struct in_addr *sin_addr)
{
		// In keeping with the historical definition of this function,
		// we allow wildcards, even though the caller did not explicitly
		// ask for them.  This usage needs to be reviewed.
	return is_ipaddr_implementation(inbuf,sin_addr,NULL,1);
}

int
is_ipaddr_no_wildcard(const char *inbuf, struct in_addr *sin_addr)
{
	return is_ipaddr_implementation(inbuf,sin_addr,NULL,0);
}

int
is_ipaddr_wildcard(const char *inbuf, struct in_addr *sin_addr, struct in_addr *mask_addr)
{
	return is_ipaddr_implementation(inbuf,sin_addr,mask_addr,1);
}


// checks to see if 'network' is a valid ip/netmask.  if given pointers to
// ip_addr structs, they will be filled in.
/* XXX:  Known Problems: The netmask doesn't have to be a valid
   netmask, as long as it looks something like an IP address.  ~tristan 8/16/07
 */
/* This function has a unit test. */
int
is_valid_network( const char *network, struct in_addr *ip, struct in_addr *mask)
{
	// copy the string, only 32 is necessary since the lonest
	// legitimate one is 123.567.901.345/789.123.567.901
	//                            1         2         3
	// 31 characters.
	//
	// we make a copy because we then find the slash and
	// overwrite it with a null to create two separate strings.
	// those are then validated and parsed into the structures
	// that were optionally passed in.
	char nmcopy[32];
	char *tmp;
	int  numbits;
	strncpy( nmcopy, network, 31 );
	nmcopy[31] = '\0';

	// find a slash and make sure both sides are valid
	tmp = strchr(nmcopy, '/');
	if( !tmp ) {
		if( is_ipaddr_wildcard(nmcopy,ip,mask) ) {
				// this is just a plain IP or IP.*
			return TRUE;
		}
	}
	else {
		// separate by overwriting the slash with a null, and moving tmp
		// to point to the begining of the second string.
		*tmp++ = 0;

		// now validate
		if (is_ipaddr_no_wildcard(nmcopy, ip)) {
			// first part is a valid ip, now validate the netmask.  two
			// different formats are valid, we check for both.
			if (is_ipaddr_no_wildcard(tmp, mask)) {
				// format is a.b.c.d/m.a.s.k
				// is_ipaddr fills in the value for both ip and mask,
				// so we are done!
				return TRUE;
			} else {
				// try format a.b.c.d/num
				char *end = NULL;
				numbits = strtol(tmp,&end,10);
				if (end && *end == '\0') {
					if (mask) {
						// fill in the structure
					    mask->s_addr = 0;
					    mask->s_addr = htonl(~(~(mask->s_addr) >> numbits));
					}
					return TRUE;
				} else {
					dprintf (D_SECURITY, "ISVALIDNETWORK: malformed netmask: %s\n", network);
				}
			}
		}
	}

	return FALSE;
}

/*
	XXX:  known problems:  This function allows anything after the :, it
	doesn't have to be a port number.  ~tristan 8/20/07
*/
/* This function has a unit test. */
int
is_valid_sinful( const char *sinful )
{
	char* tmp;
	char* copy;
	if( !sinful ) return FALSE;
	if( !(sinful[0] == '<') ) return FALSE;
	if( !(tmp = strrchr(sinful, '>')) ) return FALSE;
	copy = strdup( sinful );

	if( !(tmp = strchr(copy, ':')) ) {
		free( copy );
		return FALSE;
	}
	*tmp = '\0';
	if( ! copy[1] ) {
		free( copy );
		return FALSE;
	}
	if( ! is_ipaddr(&copy[1], NULL) ) {
		free( copy );
		return FALSE;
	}
	free( copy );
	return TRUE;
}

/*
	XXX:  known problems:  is_valid_sinful() doesn't do error checking on the
	port number, this function doesn't, and neither does atoi().  Possible fix
	would be to migrate all uses of this function to getPortFromAddr(), which
	does at least basic error checking.
		  ~tristan 8/20/07
*/
/* This function has a unit test. */
int
string_to_port( const char* addr )
{
	char *sinful, *tmp;
	int port = 0;

	if( ! (addr && is_valid_sinful(addr)) ) {
		return 0;
	}

	sinful = strdup( addr );
	if( (tmp = strrchr(sinful, '>')) ) {
		*tmp = '\0';
		tmp = strchr( sinful, ':' );
		if( tmp && tmp[1] ) {
			port = atoi( &tmp[1] );
		} 
	}
	free( sinful );
	return port;
}

/* XXX:  known problems:  string_to_ip() uses is_ipaddr(), and so has the same
 * flaws.  Mainly, input like 66.199 is assumed to be followed by ".*" even
 * though that's a badly formed IP address.  ~tristan 8/22/07
 */
/* This function has a unit test. */
unsigned int
string_to_ip( const char* addr )
{
	char *sinful, *tmp;
	unsigned int ip = 0;
	struct in_addr sin_addr;

	if( ! (addr && is_valid_sinful(addr)) ) {
		return 0;
	}

	sinful = strdup( addr );
	if( (tmp = strchr(sinful, ':')) ) {
		*tmp = '\0';
		if( is_ipaddr(&sinful[1], &sin_addr) ) {
			ip = sin_addr.s_addr;
		}
	} else {
		EXCEPT( "is_valid_sinful(\"%s\") is true, but can't find ':'", addr );
	}
	free( sinful );
	return ip;
}


char*
string_to_ipstr( const char* addr ) 
{
	char *tmp;
	static char result[MAXHOSTNAMELEN];
	char sinful[MAXHOSTNAMELEN];

	if( ! (addr && is_valid_sinful(addr)) ) {
		return NULL;
	}

	strncpy( sinful, addr, MAXHOSTNAMELEN-1 );
	tmp = strchr( sinful, ':' );
	if( tmp ) {
		*tmp = '\0';
	} else {
		return NULL;
	}
	if( is_ipaddr(&sinful[1], NULL) ) {
		strncpy( result, &sinful[1], MAXHOSTNAMELEN-1 );
		return result;
	}
	return NULL;
}

/* This function has a unit test. */
char*
string_to_hostname( const char* addr ) 
{
	char *tmp;
	static char result[MAXHOSTNAMELEN];
    struct sockaddr_in sa_in;

	if( ! (addr && is_valid_sinful(addr)) ) {
		return NULL;
	}

    string_to_sin( addr, &sa_in );
	if( (tmp = sin_to_hostname(&sa_in, NULL)) ) {
		strncpy( result, tmp, MAXHOSTNAMELEN );
		result[MAXHOSTNAMELEN-1] = '\0';
	} else {
		return NULL;
	}
	return result;
}

/* Bind the given fd to the correct local interface. */
int
_condor_local_bind( int is_outgoing, int fd )
{
	/* Note: this function is completely WinNT screwed.  However,
	 * only non-Cedar components call this function (ckpt-server,
	 * old shadow) --- and these components are not destined for NT
	 * anyhow.  So on NT, just pass back success so log file is not
	 * full of scary looking error messages.
	 *
	 * This function should go away when everything uses CEDAR.
	 */
#ifndef WIN32
	int lowPort, highPort;
	if ( get_port_range(is_outgoing, &lowPort, &highPort) == TRUE ) {
		if ( bindWithin(fd, lowPort, highPort) == TRUE )
            return TRUE;
        else
			return FALSE;
	} else {
		struct sockaddr_in sa_in;

		memset( (char *)&sa_in, 0, sizeof(sa_in) );
		sa_in.sin_family = AF_INET;
		sa_in.sin_port = 0;
			/*
			  we don't want to honor BIND_ALL_INTERFACES == true in
			  here.  this code is only used in the ckpt server (which
			  isn't daemoncore, so the hostallow localhost stuff
			  doesn't matter) and for bind()ing outbound connections
			  inside do_connect().  so, there's no harm in always
			  binding to all interfaces in here...
			  Derek <wright@cs.wisc.edu> 2005-09-20
			*/
		sa_in.sin_addr.s_addr = htonl(INADDR_ANY);

		if( bind(fd, (struct sockaddr*)&sa_in, sizeof(sa_in)) < 0 ) {
			dprintf( D_ALWAYS, "ERROR: bind(%s:%d) failed, errno: %d\n",
					 inet_ntoa(sa_in.sin_addr), sa_in.sin_port, errno );
			return FALSE;
		}
	}
#endif  /* of ifndef WIN32 */
	return TRUE;
}


int bindWithin(const int fd, const int low_port, const int high_port)
{
	int start_trial, this_trial;
	int pid, range;

	// Use hash function with pid to get the starting point
    pid = (int) getpid();
    range = high_port - low_port + 1;
    // this line must be changed to use the hash function of condor
    start_trial = low_port + (pid * 173/*some prime number*/ % range);

    this_trial = start_trial;
	do {
		struct sockaddr_in sa_in;
		priv_state old_priv;
		int bind_return_value;

		memset(&sa_in, 0, sizeof(sa_in));
		sa_in.sin_family = AF_INET;
		sa_in.sin_addr.s_addr = htonl(INADDR_ANY);
		sa_in.sin_port = htons((u_short)this_trial++);

// windows doesn't have privileged ports.
#ifndef WIN32
		if (this_trial <= 1024) {
			// use root priv for the call to bind to allow privileged ports
			old_priv = PRIV_UNKNOWN;
			old_priv = set_root_priv();
		}
#endif
		bind_return_value = bind(fd, (struct sockaddr *)&sa_in, sizeof(sa_in));
#ifndef WIN32
		if (this_trial <= 1024) {
			set_priv (old_priv);
		}
#endif
		if (bind_return_value == 0) { // success
			dprintf(D_NETWORK, "_condor_local_bind - bound to %d...\n", this_trial-1);
			return TRUE;
		} else {
            dprintf(D_NETWORK, "_condor_local_bind - failed to bind: %s\n", strerror(errno));
        }
		if ( this_trial > high_port )
			this_trial = low_port;
    } while(this_trial != start_trial);

	dprintf(D_ALWAYS, "_condor_local_bind::bindWithin - failed to bind any port within (%d ~ %d)\n",
	        low_port, high_port);

	return FALSE;
}

/* Check if the ip is in private ip address space */
/* ip: in host byte order */
/* This function has a unit test. */
int
is_priv_net(uint32_t ip)
{
    return ((ip & 0xFF000000) == 0x0A000000 ||      // 10/8
            (ip & 0xFFF00000) == 0xAC100000 ||      // 172.16/12
            (ip & 0xFFFF0000) == 0xC0A80000);       // 192.168/16
}

/* Check if two ip addresses, given in network byte, are in the same network */
int
in_same_net(uint32_t ipA, uint32_t ipB)
{
    unsigned char *byteA, *fA, *byteB;
    int i, index_to;

    fA = byteA = (char *)&ipA;
    byteB = (char *)&ipB;

    if (*fA < 128) { // A class
        index_to = 1;
    } else if(*fA < 192) { // B class
        index_to = 2;
    } else {    // C class
        index_to = 3;
    }

    for (i = 0; i < index_to; i++) {
        if (*byteA != *byteB) {
            return 0;
        }
        byteA++;
        byteB++;
    }

    return 1;
}

// ip: network-byte order
// port: network-byte order
char * ipport_to_string(const unsigned int ip, const unsigned short port)
{
    static  char    buf[24];
    struct in_addr inaddr;

    buf[0] = '<';
    buf[1] = '\0';
    if (ip == INADDR_ANY) {
        strcat(buf, my_ip_string());
    } else {
        inaddr.s_addr = ip;
        strcat(buf, inet_ntoa(inaddr));
    }
    sprintf(&buf[strlen(buf)], ":%d>", ntohs(port));
    return buf;
}

char *
prt_fds(int maxfd, fd_set *fds)
{
    static char buf[50];
    int i, size;

    sprintf(buf, "<");
    for(i=0; i<maxfd; i++) {
        if (fds && FD_ISSET(i, fds)) {
            if ((size = strlen(buf)) > 40) {
                strcat(buf, "...>");
                return buf;
            }
        sprintf(&buf[strlen(buf)], "%d ", i);
        }
    }
    strcat(buf, ">");
    return buf;
}

/* This function has a unit test. */
int
getPortFromAddr( const char* addr )
{
	char *tmp; 
	char *end; 
	long port = -1;

	if( ! addr ) {
		return -1;
	}

	tmp = strchr( addr, ':' );
	if( !tmp || !tmp[1] ) {
			/* address didn't specify a port section */
		return -1;
	}

		/* clear out our errno so we know if it's set after the
		   strtol(), that it was set from there */
	errno = 0;
	port = strtol( &tmp[1], &end, 10 );
	if( errno == ERANGE ) {
			/* port number was too big */
		return -1;
	}
	if( end == &tmp[1] ) {
			/* port section of the address wasn't a number */
		return -1;
	}
	if( port < 0 ) {
			/* port numbers can't be negative */
		return -1;
	}
	if( port > INT_MAX ) {
			/* port number was too big to fit in an int */
		return -1;
	}
	return (int)port;
}


/* This function has a unit test. */
char*
getHostFromAddr( const char* addr )
{
	char *copy, *host = NULL, *tmp; 

	if( ! (addr && addr[0]) ) {
		return 0;
	}
	
	copy = strdup( addr );

		// if there's a colon, we want to end the host part there
	if( (tmp = strchr(copy, ':')) ) {
		*tmp = '\0';
	}

		// if it still ends with '>', we want to chop that off from
		// the string
	if( (tmp = strrchr(copy, '>')) ) {
		*tmp = '\0';
	}

		// if there's an '@' sign, we just want everything after that.
	if( (tmp = strchr(copy, '@')) ) {
		if( tmp[1] ) {
			host = strdup( &tmp[1] );
		}
		free( copy );
		return host;
	}

		// if there was no '@', but it begins with '<', we want to
		// skip that and just use what follows it...
	if( copy[0] == '<' ) {
		if( copy[1] ) { 
			host = strdup( &copy[1] );
		}
	} else if( copy[0] ) {
		host = strdup( copy );
	}
	free( copy );
	return host;
}


char*
getAddrFromClaimId( const char* id )
{
	char* tmp;
	char* addr;
	char* copy = strdup( id );
	tmp = strchr( copy, '#' );
	if( tmp ) {
		*tmp = '\0';
		if( is_valid_sinful(copy) ) { 
			addr = strdup( copy );
			free( copy );
			return addr;
		}
	}
	free( copy );
	return NULL;
}


struct sockaddr_in *
getSockAddr(int sockfd)
{
    // do getsockname
    static struct sockaddr_in sa_in;
    SOCKET_LENGTH_TYPE namelen = sizeof(sa_in);
    if (getsockname(sockfd, (struct sockaddr *)&sa_in, &namelen) < 0) {
        dprintf(D_ALWAYS, "failed getsockname(%d): %s\n", sockfd, strerror(errno));
        return NULL;
    }
    // if getsockname returns INADDR_ANY, we rely upon my_ip_addr() since returning
    // 0.0.0.0 is not a good idea.
    if (sa_in.sin_addr.s_addr == ntohl(INADDR_ANY)) {
        sa_in.sin_addr.s_addr = htonl(my_ip_addr());
    }
    return &sa_in;
}

int
condor_inet_aton(const char *ipstr, struct in_addr *result)
{
	if( !is_ipaddr(ipstr,result) ) {
		return 0;
	}
	return 1;
}

char*
string_to_hardware_address(const char *sinful) {

    char                *hardware_address   = NULL;

#if defined ( WIN32 )

    PIP_ADAPTER_INFO    adapter_info,
                        adapter             = NULL;  
    PCHAR               ip_address          = NULL,
                        other_ip_address    = NULL,
                        tmp                 = NULL;
    PBYTE               address             = NULL;
    ULONG               adapter_info_length;
    UINT                hardware_address_length, 
                        address_length, 
                        last_address,
                        i;
    CHAR                component[4];
    DWORD               ok = 0;

    __try {

        /* Convert the sinful string to an IP string.  We use this
        to match against the adapters on this host */
        if (NULL == (tmp = string_to_ipstr(sinful))) {
            dprintf(D_ALWAYS, "Error parsing address: '%s'\n", sinful);
            __leave;
        }
        ip_address = strdup(tmp);
        
        /* Attempt to retrieve the information for all the hardware 
        adapters on this host */
        adapter_info_length = sizeof(IP_ADAPTER_INFO);
        adapter_info = (IP_ADAPTER_INFO*)malloc(adapter_info_length);        
        ok = GetAdaptersInfo(
            adapter_info, 
            &adapter_info_length);
        if (ERROR_BUFFER_OVERFLOW == ok) {
            free(adapter_info);
            adapter_info = (IP_ADAPTER_INFO*)malloc(adapter_info_length); 
            if (NULL == adapter_info) {
                dprintf(D_ALWAYS, "Error allocating memory needed to "
                    "call GetAdaptersinfo\n");
                __leave;
            }
            ok = GetAdaptersInfo(
                adapter_info, 
                &adapter_info_length);
            if (NO_ERROR != ok) {
                dprintf(D_ALWAYS, "GetAdaptersInfo failed with "
                    "error: %d\n", ok);
                __leave;
            }
        }

        /* Iterate through the list of adapters and find the one that
        matches the IP address we are concerned with */
        adapter = adapter_info;
        while (NULL != adapter) {            
            other_ip_address = adapter->IpAddressList.IpAddress.String;
            if (MATCH == strcmp(ip_address, other_ip_address)) {
                /* Allocate a buffer to hold the hardware address. It
                should be able to hold MAX_ADAPTER_ADDRESS_LENGTH*2 
                characters, plus the character separators between every 
                two character component (and NULL, of course). */
                hardware_address_length = 
                    MAX_ADAPTER_ADDRESS_LENGTH*3;
                hardware_address = (char*)malloc(
                    hardware_address_length);
                if (NULL == hardware_address) {
                    dprintf(D_ALWAYS, "Error allocating memory "
                        "needed to store hardware address of: %s\n",
                        sinful);
                    __leave;
                }
                *hardware_address   = '\0';
                address             = adapter->Address;
                address_length      = adapter->AddressLength;
                last_address        = address_length - 1;
                for (i = 0; i < address_length; ++i) {
                    sprintf(component, "%.2X%s", 
                        (int) address[i],
                        i == last_address ? "" : ":");
                    strcat(hardware_address, component);
                }
                /* If we get here it means we found the adapter we were 
                looking for, so we can bail out early */
                __leave;
            } 
            adapter = adapter_info->Next;
        }

    }
    __finally {

        if (NULL != ip_address) {
            free(ip_address);
        }
        if (NULL != adapter_info) {
            free(adapter_info);
        }

    }

#endif

    return hardware_address;
}
