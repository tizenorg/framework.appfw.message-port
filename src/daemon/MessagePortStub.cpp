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
 * @file	MessagePortStub.cpp
 * @brief	This is the implementation file for the  _MessagePortStub class.
 *
 */

#include <message-port-messages.h>
#include <assert.h>
#include <bundle.h>

#include "message_port_error.h"
#include "message-port-log.h"

#include "MessagePortStub.h"
#include "MessagePortService.h"

_MessagePortStub::_MessagePortStub(void)
	: __pIpcServer(NULL)
	, __pService(NULL)
{

}

_MessagePortStub::~_MessagePortStub(void)
{
	if (__pIpcServer != NULL)
	{
		__pIpcServer->Stop();
		delete __pIpcServer;
	}
}

int
_MessagePortStub::Construct(void)
{

	_LOGI("MessagePort Stub constructed.");
	int r = MESSAGEPORT_ERROR_NONE;

	assert(__pIpcServer == NULL);

	_MessagePortIpcServer* pIpcServer = new (std::nothrow) _MessagePortIpcServer;
	if (pIpcServer == NULL)
	{
		_LOGE("Out of memory");
		return MESSAGEPORT_ERROR_OUT_OF_MEMORY;
	}
	int ret = pIpcServer->Construct("message-port-server", *this, false);
	if (ret != 0)
	{
		_LOGE("Failed to create ipc server: %d.", ret);
		r = MESSAGEPORT_ERROR_IO_ERROR;
		goto CATCH;
	}
	__pIpcServer = pIpcServer;
	return MESSAGEPORT_ERROR_NONE;


CATCH:
	delete pIpcServer;
	return r;

}

void
_MessagePortStub::SetMessagePortService(_MessagePortService& service)
{
	__pService = &service;
}

bool
_MessagePortStub::OnRegisterMessagePort(const BundleBuffer& buffer, int* pResult)
{
	_LOGI("_MessagePortStub::OnRegisterMessagePort.");
	assert(__pIpcServer);

	int clientId = __pIpcServer->GetClientId();
	*pResult = __pService->RegisterMessagePort(clientId, buffer);
	bundle_free(buffer.b);
	return true;
}

bool
_MessagePortStub::OnCheckRemotePort(const BundleBuffer& buffer, int* pResult)
{
	_LOGI("_MessagePortStub::OnCheckRemotePort.");
	assert(__pIpcServer);

	*pResult = __pService->CheckRemotePort(buffer);
	bundle_free(buffer.b);
	return true;
}

bool
_MessagePortStub::OnSendMessage(const BundleBuffer& metadata, const BundleBuffer& buffer, int* pResult)
{
	_LOGI("MessagePort OnSendMessage");
	assert(__pIpcServer);

	*pResult = __pService->SendMessage(metadata, buffer);
	bundle_free(metadata.b);
	bundle_free(buffer.b);
	return true;
}

int
_MessagePortStub::SendMessage(int clientId, const BundleBuffer& metadata, const BundleBuffer& buffer)
{
	_LOGI("MessagePort SendMessage");
	assert(__pIpcServer);

	int ret = __pIpcServer->SendResponse(clientId, new MessagePort_sendMessageAsync(metadata, buffer));

	if (ret < 0)
	{
		_LOGE("Failed to send a response: %d.", ret);
		return MESSAGEPORT_ERROR_IO_ERROR;
	}
	return MESSAGEPORT_ERROR_NONE;
}

void
_MessagePortStub::OnIpcRequestReceived(_MessagePortIpcServer& server, const IPC::Message& message)
{
	_LOGI("MessagePort message received");

	IPC_BEGIN_MESSAGE_MAP(_MessagePortStub, message)
		IPC_MESSAGE_HANDLER_EX(MessagePort_registerPort, &server, OnRegisterMessagePort)
		IPC_MESSAGE_HANDLER_EX(MessagePort_checkRemotePort, &server, OnCheckRemotePort)
		IPC_MESSAGE_HANDLER_EX(MessagePort_sendMessage, &server, OnSendMessage)
	IPC_END_MESSAGE_MAP()
}

void
_MessagePortStub::OnIpcServerStarted(const _MessagePortIpcServer& server)
{

}

void
_MessagePortStub::OnIpcServerStopped(const _MessagePortIpcServer& server)
{

}

void
_MessagePortStub::OnIpcClientConnected(const _MessagePortIpcServer& server, int clientId)
{
	_LOGI("MessagePort Ipc connected");
}

void
_MessagePortStub::OnIpcClientDisconnected(const _MessagePortIpcServer& server, int clientId)
{
	_LOGI("MessagePort Ipc disconnected");
	assert(__pIpcServer);

	_LOGE("Unregister - client =  %d", clientId);

	int ret = __pService->UnregisterMessagePort(clientId);
	if (ret != 0)
	{
		_LOGE("Failed to send a request: %d.", ret);
	}
}

