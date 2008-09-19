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


#if !defined(_CONDOR_JIC_SHADOW_H)
#define _CONDOR_JIC_SHADOW_H

#include "job_info_communicator.h"
#include "condor_ver_info.h"
#include "file_transfer.h"
#include "io_proxy.h"

#include "condor_daemon_client.h"

/** The base class of JobInfoCommunicator that knows how to talk to a
	remote condor_shadow.  this is where we deal with sending any
	shadow RSCs, FileTransfer, etc.
*/

class JICShadow : public JobInfoCommunicator
{
public:
		/// Constructor
	JICShadow( const char* shadow_name );

		/// Destructor
	~JICShadow();

		// // // // // // // // // // // //
		// Initialization
		// // // // // // // // // // // //

		/// Initialize ourselves
	bool init( void );

		/// Read anything relevent from the config file
	void config( void );

		/// If needed, transfer files.
	void setupJobEnvironment( void );

	bool streamInput();
	bool streamOutput();
	bool streamError();

		// // // // // // // // // // // //
		// Information about the job 
		// // // // // // // // // // // //

		/// Total bytes sent by this job 
	float bytesSent( void );

		/// Total bytes received by this job 
	float bytesReceived( void );

		/** Since the logic for getting the std filenames out of the
			job ad and munging them are identical for all 3, just use
			a helper to avoid duplicating code.  If we're not
			transfering files, we may need to use an alternate
			attribute (see comment for initStdFiles() for more
			details). 
			@param attr_name The ClassAd attribute name to lookup
			@param alt_name The alternate attribute name we might need
			@return a strdup() allocated string for the filename we 
			        care about, or NULL if it's not in the job ad.
		*/
	char* getJobStdFile( const char* attr_name,
						 const char* alt_name = NULL );

		// // // // // // // // // // // //
		// Job Actions
		// // // // // // // // // // // //

		/** 
		 * JICShadow does not need to implement these 
		 * at this time
		 **/ 
	virtual bool holdJob( const char*, int /*hold_reason_code*/, int /*hold_reason_subcode*/ ) { return ( false ); }
	virtual bool removeJob( const char* ) { return ( false ); }
	virtual bool terminateJob( const char* ) { return ( false ); }
	virtual bool requeueJob( const char* ) { return ( false ); }

		// // // // // // // // // // // //
		// Job execution and state changes
		// // // // // // // // // // // //

		/** The starter has been asked to suspend.  Suspend file
			transfer activity and notify the shadow.
		*/
	void Suspend( void );

		/** The starter has been asked to continue.  Resume file
			transfer activity and notify the shadow.
		*/
	void Continue( void );

		/** Once all the jobs are done, and after the optional
			HOOK_JOB_EXIT has returned, initiate the file transfer.
		*/
	bool transferOutput( void );

		/** The last job this starter is controlling has been
   			completely cleaned up.  We don't care, since we just wait
			for the shadow to tell the startd to tell us to go away. 
		*/
	void allJobsGone( void ) {};

		/** The starter has been asked to shutdown fast.  Disable file
			transfer, since we don't want that on fast shutdowns.
			Also, set a flag so we know we were asked to vacate. 
		 */
	void gotShutdownFast( void );

		/** Someone is attempting to reconnect to this job.
		 */
	int reconnect( ReliSock* s, ClassAd* ad );


		// // // // // // // // // // // //
		// Notfication to the shadow
		// // // // // // // // // // // //

		/** Send RSC to the shadow to tell it the job is about to
			spawn.
		 */
	void notifyJobPreSpawn( void );

		/** Notify the shadow that the job exited
			@param exit_status The exit status from wait()
			@param reason The Condor-defined exit reason
			@param user_proc The UserProc that was running the job
		*/
	bool notifyJobExit( int exit_status, int reason,
						UserProc* user_proc );

		/** Tell the shadow to log an error event in the user log.
			@param err_msg A desription of the error
			@param critical True if the shadow should exit
			@param hold_reason_code If non-zero, put job on hold with this code
			@param hold_reason_subcode The sub-code to record
		 */
	bool notifyStarterError( const char* err_msg, bool critical,
	                         int hold_reason_code, int hold_reason_subcode);


		/**
		   Send a periodic update ClassAd to the shadow.

		   @param update_ad Update ad to use if you've already got the info
		   @param insure_update Should we insure the update gets there?
		   @return true if success, false if failure
		*/
	virtual bool periodicJobUpdate(ClassAd* update_ad = NULL,
								   bool insure_update = false);


		// // // // // // // // // // // //
		// Misc utilities
		// // // // // // // // // // // //

		/** Check to see if we're configured to use file transfer or
			not.  If so, see if transfer_output_files is set.  If so,
			append the given filename to the list, so that we try to
			transfer it back, too.  If we're not using file transfer,
			the only way a remote shadow would get this file back is
			if we're on a shared filesystem, in which case, we don't
			have to do anything special, anyway.
			@param filename File to add to the job's output list 
		*/
	void addToOutputFiles( const char* filename );

		/** Send modified files in a job working direcotry 
		    to shadow by using file transfer
		*/
	bool uploadWorkingFiles(void);
	
		/* Update Job ClassAd with checkpoint info and log it */
	void updateCkptInfo(void);

private:

		// // // // // // // // // // // //
		// Private helper methods
		// // // // // // // // // // // //

		/** Publish information into the given classad for updates to
			the shadow
			@param ad ClassAd pointer to publish into
			@return true if success, false if failure
		*/ 
	bool publishUpdateAd( ClassAd* ad );

		/** Send an update ClassAd to the shadow.  The "insure_update"
			just means do we make sure the update gets there.  It has
			nothing to do with the "insure" memory analysis tool.
			@param update_ad Update ad to use if you've already got the info 
			@param insure_update Should we insure the update gets there?
			@return true if success, false if failure
		*/
	bool updateShadow( ClassAd* update_ad, bool insure_update = false );

		/** Send an update ClassAd to the startd.
			@param ad Update ad
		 */
	void updateStartd( ClassAd *ad, bool final_update );

		/** Read all the relevent attributes out of the job ad and
			decide if we need to transfer files.  If so, instantiate a
			FileTransfer object, start the transfer, and return true.
			If we don't have to transfer anything, return false.
			@return true if transfer was begun, false if not
		*/
	bool beginFileTransfer( void );

		/// Callback for when the FileTransfer object is done
	int transferCompleted(FileTransfer *);

		/// Do the RSC to get the job classad from the shadow
	bool getJobAdFromShadow( void );

		/** Initialize information about the shadow's version and
			sinful string from the given ClassAd.  At startup, we just
			pass the job ad, since that should have everything in it.
			But on reconnect, we call this with the request ad.  Also,
			try to initialize our ShadowVersion object.  If there's no
			shadow version, we leave our ShadowVersion NULL.  If we
			know the version, we instantiate a CondorVersionInfo
			object so we can perform checks on the version in the
			various places in the starter where we need to know this
			for compatibility.
		*/
	void initShadowInfo( ClassAd* ad );

		/** Register some important information about ourself that the
			shadow might need.
			@return true on success, false on failure
		*/
	virtual	bool registerStarterInfo( void );
	
		/** All the attributes the shadow cares about that we send via
			a ClassAd is handled in this method, so that we can share
			the code between registerStarterInfo() and when we're
			replying to accept an attempted reconnect.
		*/ 
	void publishStarterInfo( ClassAd* ad );

		/** Initialize the priv_state code with the appropriate user
			for this job.  This function deals with all the logic for
			checking UID_DOMAIN compatibility, SOFT_UID_DOMAIN
			support, and so on.
			@return true on success, false on failure
		*/
	bool initUserPriv( void );

		/** Initialize our version of important information for this
			job which the starter will want to know.  This should
			init the following: orig_job_name, job_input_name, 
			job_output_name, job_error_name, and job_iwd.
			@return true on success, false on failure */
	bool initJobInfo( void );

		/** Initialize the file-transfer-related properties of this
			job from the ClassAd.  This procedure is somewhat
			complicated, and involves some backwards compatibility
			cruft, too.  All of the code paths share some common code,
			too.  So, everything is split up into a few helper
			functions to maximize clarity, keep the length of
			individual functions reasonable, and to avoid code
			duplication.  

			initFileTransfer() is responsible for looking up
			ATTR_SHOULD_TRANSFER_FILES, the better way to specify the
			file transfer behavior.  If that's there, it decide what
			to do based on what it says, and call the appropriate
			helper method, either initNoFileTransfer() or
			initWithFileTransfer().  If it's defined to "IF_NEEDED",
			we compare the FileSystemDomain of the job with our local
			value, using the sameFSDomain() helper, and if there's a
			match, we don't setup file transfer.  If the new attribute
			isn't defined, we call the initWithFileTransfer() helper,
			since that knows what to do with the old
			ATTR_TRANSFER_FILES attribute.

			@return true on success, false if there are fatal errors
		*/
	bool initFileTransfer( void );

		/** If we know for sure we do NOT want file transfer, we call
			this method.  It sets the appropriate flags to turn off
			file transfer code in the starter, and uses the original
			value of ATTR_JOB_IWD to initialize the job's IWD and the
			standard files (STDIN, STDOUT, and STDERR), using the
			initStdFiles() method.
			@return true on success, false if there are fatal errors
		*/
	bool initNoFileTransfer( void );

		/** If we think we want file transfer, we call this method.
			It tries to find the new ATTR_WHEN_TO_TRANSFER_OUTPUT
			attribute and uses that to figure out if we should send
			back output files when we're evicted, or only if the job
			exits on its own.  If that attribute is not defined, we
			search for the deprecated ATTR_TRANSFER_FILES attribute
			and use that to figure out the same thing (or, if
			TransferFiles is set to "NEVER", we just call
			initNoFileTransfer(), instead).  Once we know that we're
			doing a file transfer, we initialize the job's IWD to the
			starter's temporary execute directory, and can then call
			the initStdFiles() method to initalize STDIN, STDOUT, and
			STDERR.
			@return true on success, false if there are fatal errors
		*/			
	bool initWithFileTransfer( void );

		/** This method is used whether or not we're doing a file
			transfer to initialize the valid full paths to use for
			STDIN, STDOUT, and STDERR.  The "job_iwd" data member of
			this object must be filled in before this can be called.  
			For the output files, if they contain full pathnames,
			condor_submit now stores the original values in alternate
			attribute names and puts a temporary value in the real
			attributes so that things work for file transfer.
			However, if we're not transfering files, we need to now
			use these alternate names, since that's where the user
			really wants the output, and we want to access them
			directly.  So, for STDOUT, and STDERR, we also pass in the
			alternate attribute names to the underlying helper method,
			getJobStdFile(). 
			@return at this time, this method always returns true
		*/
	bool initStdFiles( void );

		/// If the job ad says so, initialize our IO proxy
	bool initIOProxy( void );

		// If we are supposed to specially create a security session
		// for file transfer and reconnect, do it.
	void initMatchSecuritySession();

		/** Compare our own UIDDomain vs. where the job came from.  We
			check in the job ClassAd for ATTR_UID_DOMAIN and compare
			it to info we have about the shadow and the local machine.
			This is used to help initialize the priv-state code.
			@return true if they match, false if not
		*/
	bool sameUidDomain( void );

		/** Compare our own FileSystemDomain vs. where the job came from.
			We check in the job ClassAd for ATTR_FILESYSTEM_DOMAIN, and
			compare that to our locally defined value.  This is used
			to help figure out if we should do file-transfer or not.
			@return true if they match, false if not
		*/
	bool sameFSDomain( void );

		// Are we using file transfer.  Overridden from parent classes version.
	bool usingFileTransfer( void );

		// The shadow is feeding us a new proxy. Override from parent
	bool updateX509Proxy(int cmd, ReliSock * s);

		// // // // // // // //
		// Private Data Members
		// // // // // // // //

	DCShadow* shadow;

		/** The version of the shadow if known; otherwise NULL */
	CondorVersionInfo* shadow_version;

		/// hostname (or whatever the startd gave us) of our shadow 
	char* m_shadow_name;

	IOProxy io_proxy;

	FileTransfer *filetrans;

		// specially made security sessions if we are doing
		// SEC_ENABLE_MATCH_PASSWORD_AUTHENTICATION
	char *m_filetrans_sec_session;
	char *m_reconnect_sec_session;

	/// if true, transfer files at vacate time (in addtion to job exit)
	bool transfer_at_vacate;

	bool wants_file_transfer;

	char* uid_domain;
	char* fs_domain;
	bool trust_uid_domain;

		/** A flag to keep track of the case where we were trying to
			cleanup our job but we discovered that we were
			disconnected from the shadow.  This way, if there's a
			successful reconnect, we know we can try to clean up the
			job again...
		*/
	bool job_cleanup_disconnected;

	Stream *m_job_startd_update_sock;

		/** A list of output files that have been dynamically added
		    (e.g. a core file dumped by the job)
		*/
	StringList m_added_output_files;
};


#endif /* _CONDOR_JIC_SHADOW_H */
