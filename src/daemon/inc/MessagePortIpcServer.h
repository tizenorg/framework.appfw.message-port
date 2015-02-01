//
// Open Service Platform
// Copyright (c) 2012 Samsung Electronics Co., Ltd.
//
// Licensed under the Apache License, Version 2.0 (the License);
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

/**
 * @file	FIo_MessagePortIpcServer.h
 * @brief	This is the header file for the _MessagePortIpcServer class.
 *
 * This file contains the declarations of _MessagePortIpcServer.
 */

#ifndef _MESSAGE_PORT_IPC_SERVER_H_
#define _MESSAGE_PORT_IPC_SERVER_H_

#include <string>
#include <pthread.h>
#include <map>
#include <glib.h>

#include <ipc/ipc_message_macros.h>
#include <ipc/ipc_message_utils.h>

//#include <FBaseResult.h>
//#include <FBaseObject.h>
//#include <FBaseString.h>
//#include <FAppTypes.h>

#include "IMessagePortIpcServerEventListener.h"

//namespace Tizen { namespace Base { namespace Runtime
//{
//class _EventDispatcher;
//}}}


/**
 * @class _MessagePortIpcServer
 * @brief This class provides methods to handle IPC request messages.
 *
 */
class _MessagePortIpcServer
{
public:
	_MessagePortIpcServer(void);

	virtual ~_MessagePortIpcServer(void);

	/**
	 * Constructs the instance of this class and starts the IPC server.
	 *
	 * @return An error code
	 * @param[in] name			The name of IPC server
	 * @param[in] listener		The listener for IPC server
	 * @param[in] runOnCallerThread	Set to @c true, if the server runs on the caller thread
	 *					@c false, if the server runs on its own thread.
	 * @exception E_SUCCESS		The method was successful.
	 * @exception E_OUT_OF_MEMORY	Insufficient memory.
	 * @exception E_SYSTEM		Occurs when runOnCallerThread is set to true where the caller thread is worker thread.
	 */
	int Construct(const std::string& name, const _IMessagePortIpcServerEventListener& listener, bool runOnCallerThread = true);

	/**
	 * Returns the name of the IPC server.
	 *
	 * @return The name of the IPC server.
	 */
	char* GetName(void) const;

	/**
	 * Returns the id the of the client which sent a request message.
	 *
	 * @return The id of the IPC client.
	 * @remark This can be called only in a message handler.
	 */
	int GetClientId(void) const;

	/**
	 * Returns the application id of the client which sent a request message.
	 *
	 * @return The application id of the IPC client.
	 * @remark This can be called only in a message handler.
	 */
	char* GetClientApplicationId(void) const;

	/**
	 * Stops the IPC server.
	 *
	 * @return An error code
	 * @exception E_SUCCESS		The method was successful.
	 * @exception E_INVALID_STATE	The IPC server has not been started.
	 */
	int Stop(void);

	/**
	 * Sends a message to an IPC client.
	 *
	 * @return An error code
	 * @param[in] clientId      The id of the IPC client
	 * @param[in] message	The message to send
	 * @exception E_SUCCESS	The method was successful.
	 * @exception E_INVALID_ARG		The message is synchronous.
	 * @exception E_INVALID_OPERATION	The client didn't set a listener.
	 * @exception E_OUT_OF_MEMORY	Insufficient memory.
	 * @exception E_SYSTEM		A system error occurred.
	 *
	 * @remark Only an asychronous message can be sent to an IPC client.
	 */
	int SendResponse(int clientId, const IPC::Message& message);

	int Start(void);

	int SendResponse(int clientId, IPC::Message* pMessage);

	bool Send(IPC::Message* msg);

private:
	_MessagePortIpcServer(const _MessagePortIpcServer& value);

	_MessagePortIpcServer& operator =(const _MessagePortIpcServer& value);

	static void* ThreadProc(void* pParam);

	void Run(void* pParam);

	static gboolean OnConnectionRequest(GIOChannel* source, GIOCondition condition, gpointer data);

	static gboolean OnReadMessage(GIOChannel* source, GIOCondition condition, gpointer data);

	gboolean HandleReceivedMessage(GIOChannel* source, GIOCondition condition, gpointer data);

	static const int __MAX_MESSAGE_BUFFER_SIZE = 1024;

	struct  _ClientInfo;

	/**
	 *	@struct	__ChannelInfo
	 *	@brief	This struct represent a channel.
	 */
	struct  _ChannelInfo
	{
		_ChannelInfo(void);
		~_ChannelInfo(void);

		struct _ClientInfo* pClientInfo;
		GIOChannel* pGIOChannel;
		GSource* pGSource;
		bool destroySource;
	};

	/**
	 *	@struct	__ClientInfo
	 *	@brief	This struct represent a client connected to this server.
	 */
	struct  _ClientInfo
	{
		_ClientInfo(void);
		~_ClientInfo(void);

		int clientId;                              /**< the client id */
		_MessagePortIpcServer* pIpcServer;         /**< the pointer to an _MessagePortIpcServer */
		GIOChannel* pReverseChannel;               /**< the channel for sending reverse message */
		std::vector <struct _ChannelInfo*> channels;   /**< the set of channels associated with a client */
		char *appId;
	};

	char *__name;
	bool __runOnCallerThread;
	//Tizen::Base::Runtime::_EventDispatcher* __pEventDispatcher;
	_IMessagePortIpcServerEventListener* __pListener;

	pthread_t __handlerThread;
	GMainContext* __pHandlerGMainContext;
	GMainLoop* __pHandlerGMainLoop;

	// handling connection
	GSource* __pConnectGSource;

	// handling received message
	char __messageBuffer[__MAX_MESSAGE_BUFFER_SIZE];
	std::string __pending;

	// current message handling context
	GIOChannel* __pCurrentChannel;
	_ClientInfo* __pCurrentClientInfo;

	std::map <int, _ClientInfo*> __clients;   // pid of client is used for key

}; // _MessagePortIpcServer

#endif // _MESSAGE_PORT_IPC_SERVER_H_

