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
 * @file	MessagePortProxy.h
 * @brief	This is the header file for the MessagePortProxy class.
 *
 * This file contains the declarations of MessagePortProxy.
 */


#ifndef _MESSAGE_PORT_PROXY_H_
#define _MESSAGE_PORT_PROXY_H_

#include <string>
#include <map>
#include <pthread.h>

#include <bundle.h>

#include "message-port.h"
#include "message-port-data-types.h"

#include "IIpcClientEventListener.h"

namespace IPC { class Message; }

class IpcClient;

class MessagePortProxy
	: public IIpcClientEventListener
{
public:
	virtual ~MessagePortProxy(void);

	int Construct(void);

	virtual void OnIpcResponseReceived(IpcClient& client, const IPC::Message& message);


	int RegisterMessagePort(const std::string& localPort,
							bool isTrusted,
							messageport_message_cb callback);

	int CheckRemotePort(const std::string& remoteAppId,
						const std::string& remotePort,
						bool isTrusted,
						bool *exist);

	int SendMessage(const std::string& remoteAppId,
					const std::string& remotePort,
					bool trustedMessage,
					bundle* data);

	int SendMessage(const std::string& localPort,
					bool trustedPort,
					const std::string& remoteAppId,
					const std::string& remotePort,
					bool trustedMessage,
					bundle* data);

	char* GetLocalPortNameN(int id);

	int CheckTrustedLocalPort(int id, bool* trusted);

	static MessagePortProxy* GetProxy(void);

private:
	MessagePortProxy(void);

	int SendMessageInternal(const BundleBuffer& metadata, const BundleBuffer& buffer);

	bool OnSendMessageInternal(const BundleBuffer& metadata, const BundleBuffer& buffer);

	int GetNextId(void);

	bool IsLocalPortRegisted(const std::string& localPort, bool trusted, int &id);

private:
	IpcClient* __pIpcClient;
	pthread_mutex_t* __pMutex;

	std::string __appId;

	std::map<std::string, messageport_message_cb> __listeners;
	std::map<std::string, int> __idFromString;
	std::map<int, std::string> __ids;

	std::map<std::string, messageport_message_cb> __trustedListeners;
	std::map<std::string, int> __trustedIdFromString;
	std::map<int, std::string> __trustedIds;

}; // MessagePortProxy

#endif // _MESSAGE_PORT_PROXY_H_
