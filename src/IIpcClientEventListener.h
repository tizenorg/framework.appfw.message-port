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
 * @file	IIpcClientEventListener.h
 * @brief	This is the header file for the IIpcClientEventListener class.
 *
 * This file contains the declarations of  IIpcClientEventListener.
 */

#ifndef _IIPC_CLIENT_EVENT_LISTENER_H_
#define _IIPC_CLIENT_EVENT_LISTENER_H_

namespace IPC { class Message; }

class IpcClient;

/**
 * @interface IIpcClientEventListener
 * @brief     This interface provides the listener method for the response from IPC server.
 */
class IIpcClientEventListener
{
public:

	/**
	 * This is the destructor for this class.
	 */
	virtual ~IIpcClientEventListener(void) {}

	/**
	 * Called when an IPC response message received.
	 *
	 * @param[in] client       The IPC client
	 * @param[in] message      The response message
	 */
	virtual void OnIpcResponseReceived(IpcClient& client, const IPC::Message& message) = 0;

	/**
	 * Called when an IPC server disconnected.
	 *
	 * @param[in] client		The IPC client
	 */
	virtual void OnIpcServerDisconnected(IpcClient& client) {}
}; // IIpcClientEventListener

#endif //_IIPC_CLIENT_EVENT_LISTENER_H_
