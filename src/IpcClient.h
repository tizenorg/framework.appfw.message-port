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
 * @file	IpcClient.h
 * @brief	This is the header file for the IpcClient class.
 *
 * This file contains the declarations of IpcClient.
 */


#ifndef _IPC_CLIENT_H_
#define _IPC_CLIENT_H_

#include <string.h>
#include <vector>
#include <stdio.h>
#include <pthread.h>
#include <glib.h>

#include <ipc/ipc_message_macros.h>
#include <ipc/ipc_message_utils.h>


class IIpcClientEventListener;

/**
 * @class IpcClient
 * @brief This class provides methods for sending a message to an IPC server.
 * @since 2.1
 *
 */
class IpcClient
{
public:
	/**
	 * This is the default constructor for this class.
	 */
	IpcClient(void);

	/**
	 * This is the destructor for this class.
	 */
	virtual ~IpcClient(void);

	/**
	 * Constructs the instance of this class.
	 *
	 * @return 0 on success, otherwise a negative error value.
	 * @param [in] serverName	The name of the server
	 * @param [in] pListener	Set if the client want to handle a message from the IPC server.
	 *                                 @c NULL, otherwise.
	 * @retval MESSAGEPORT_ERROR_OUT_OF_MEMORY	Insufficient memory.
	 * @retval MESSAGEPORT_ERROR_IO_ERROR	A system error occurred.
	 */
	int Construct(const std::string& serverName, const IIpcClientEventListener* pListener = NULL);

	/**
	 * Returns the name of the IPC server.
	 *
	 * @return The name of the IPC server.
	 */
	std::string GetName(void) const;

	/**
	 * Sends a request message to an IPC server.
	 *
	 * @return 0 on success, otherwise a negative error value.
	 * @param [in] message	The message to send
	 * @retval MESSAGEPORT_ERROR_OUT_OF_MEMORY	Insufficient memory.
	 * @retval MESSAGEPORT_ERROR_IO_ERROR	A system error occurred.
	 *
	 */
	int SendRequest(const IPC::Message& message);

	int SendRequest(IPC::Message* pMessage);

private:
	IpcClient(const IpcClient& value);

	IpcClient& operator =(const IpcClient& value);

	int Send(IPC::Message* pMessage);

	int SendAsync(IPC::Message* pMessage);

	int SendSync(IPC::Message* pMessage);

	int MakeConnection(bool forReverse = false);

	int AcquireFd(void);

	void ReleaseFd(int fd);

	static gboolean OnReadMessage(GIOChannel* source, GIOCondition condition, gpointer data);

	gboolean HandleReceivedMessage(GIOChannel* source, GIOCondition condition);

private:
	GSource* __pReverseSource;
	pthread_mutex_t* __pMutex;

	std::vector <int> __fds;
	std::string __name;
	std::string __appId;
	IIpcClientEventListener* __pListener;

	static const int __MAX_MESSAGE_BUFFER_SIZE = 1024;
	char __messageBuffer[__MAX_MESSAGE_BUFFER_SIZE];
	std::string __pending;
};

#endif // _IPC_CLIENT_H_
