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
 * @file	MessagePortStub.h
 * @brief	This is the header file for the _MessagePortStub class.
 *
 * This file contains the declarations of _MessagePortStub.
 */

#ifndef _MESSAGE_PORT_STUB_H_
#define _MESSAGE_PORT_STUB_H_

#include <glib.h>
#include <message-port-data-types.h>

//#include <FBaseResult.h>
//#include <FBaseObject.h>
//#include <FBaseColArrayList.h>
//#include <FBaseColHashMap.h>
//#include <FAppTypes.h>

#include "MessagePortIpcServer.h"
#include "IMessagePortIpcServerEventListener.h"

namespace IPC
{
class Message;
}

class _MessagePortService;

class _MessagePortStub
	: public _IMessagePortIpcServerEventListener
{
public:
	_MessagePortStub(void);
	virtual ~_MessagePortStub(void);

	virtual int Construct(void);

	int SendMessage(int clientId, const BundleBuffer& metadata, const BundleBuffer& buffer);

	void SetMessagePortService(_MessagePortService& service);

private:
	bool OnRegisterMessagePort(const BundleBuffer& buffer, int* pResult);

	bool OnCheckRemotePort(const BundleBuffer& buffer, int* pResult);

	bool OnSendMessage(const BundleBuffer& metadata, const BundleBuffer& buffer, int* pResult);

	virtual void OnIpcRequestReceived(_MessagePortIpcServer& server, const IPC::Message& message);

	virtual void OnIpcServerStarted(const _MessagePortIpcServer& server);

	virtual void OnIpcServerStopped(const _MessagePortIpcServer& server);

	virtual void OnIpcClientConnected(const _MessagePortIpcServer& server, int clientId);

	virtual void OnIpcClientDisconnected(const _MessagePortIpcServer& server, int clientId);

	_MessagePortStub(const _MessagePortStub& value);
	_MessagePortStub& operator = (const _MessagePortStub& value);

private:
	_MessagePortIpcServer* __pIpcServer;
	_MessagePortService* __pService;

}; // _MessagePortStub

#endif // _MESSAGE_PORT_STUB_H_


