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


#ifndef _CONDOR_CCB_CLIENT_H
#define _CONDOR_CCB_CLIENT_H

/*
 CCBClient: the Condor Connection Broker Client
 This is instantiated by CEDAR when a client wants to connect to a daemon
 that is not directly accessible (e.g. because the daemon is in a private
 network).  The client requests the daemon's CCB server to inform the
 daemon of the request.  If all goes well, the daemon connects to the client
 and then CEDAR continues on its way as though it had created the connection
 the normal way from the client to the server.
 */

#include "classy_counted_ptr.h"
#include "counted_ptr.h"
#include "reli_sock.h"
#include "dc_message.h"
#include "shared_port_endpoint.h"

class CCBClient: public Service, public ClassyCountedPtr {
 public:
	CCBClient( char const *ccb_contact, ReliSock *target_sock );
	~CCBClient();

	bool ReverseConnect( CondorError *error, bool non_blocking );
	void CancelReverseConnect();

 private:
	MyString m_ccb_contact;
	MyString m_cur_ccb_address;
	StringList m_ccb_contacts;
	ReliSock *m_target_sock; // socket to receive the reversed connection
	MyString m_target_peer_description; // who we are trying to connect to
	Sock *m_ccb_sock;        // socket to the CCB server
	MyString m_connect_id;
	DCMsgCallback *m_ccb_cb; // callback object for async CCB request
	int m_deadline_timer;

	bool ReverseConnect_blocking( CondorError *error );
	bool SplitCCBContact( char const *ccb_contact, MyString &ccb_address, MyString &ccbid, CondorError *error );

	bool AcceptReversedConnection(counted_ptr<ReliSock> listen_sock,counted_ptr<SharedPortEndpoint> shared_listener);
	bool HandleReversedConnectionRequestReply(CondorError *error);

	bool try_next_ccb();
	void CCBResultsCallback(DCMsgCallback *cb);
	void ReverseConnectCallback(Sock *sock);
	void RegisterReverseConnectCallback();
	void UnregisterReverseConnectCallback();
	static int ReverseConnectCommandHandler(Service *,int cmd,Stream *stream);
	MyString myName();
	void DeadlineExpired();


};

#endif
