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
 * @file	MessagePortProxy.cpp
 * @brief	This is the implementation file for the MessagePortProxy class.
 *
 */


#include <sys/types.h>
#include <unistd.h>
#include <sstream>

#include <aul/aul.h>

#include "message-port.h"
#include "message-port-messages.h"
#include "message-port-log.h"

#include "IpcClient.h"
#include "MessagePortProxy.h"

using namespace std;

static const char MESSAGE_TYPE[] = "MESSAGE_TYPE";

static const char LOCAL_APPID[] = "LOCAL_APPID";
static const char LOCAL_PORT[] = "LOCAL_PORT";
static const char TRUSTED_LOCAL[] = "TRUSTED_LOCAL";

static const char REMOTE_APPID[] = "REMOTE_APPID";
static const char REMOTE_PORT[] = "REMOTE_PORT";
static const char TRUSTED_REMOTE[] = "TRUSTED_REMOTE";
static const char TRUSTED_MESSAGE[] = "TRUSTED_MESSAGE";

static const int MAX_MESSAGE_SIZE = 16 * 1024;

namespace {
template <class Type>
void _Deleter(Type* pInstance)
{
	delete pInstance;
}

template <>
void _Deleter(pthread_mutex_t* pInstance)
{
	if (pInstance)
	{
		pthread_mutex_destroy(pInstance);
		delete pInstance;
	}
}

template <>
void _Deleter(bundle* pInstance)
{
	bundle_free(pInstance);
}

template <class Type>
class _ResourceGuard
{
public:
	_ResourceGuard(Type* pInstance)
		: __pInstance(pInstance)
	{
	}
	~_ResourceGuard(void)
	{
		_Deleter<Type>(__pInstance);
	}
	Type* Release(void)
	{
		Type* pInstance = __pInstance;
		__pInstance = NULL;
		return pInstance;
	}
private:
	Type* __pInstance;
};
}

MessagePortProxy::MessagePortProxy(void)
	: __pIpcClient(NULL)
	, __pMutex(NULL)
{
}

MessagePortProxy::~MessagePortProxy(void)
{
	pthread_mutex_destroy(__pMutex);
}

int
MessagePortProxy::Construct(void)
{
	IpcClient* pIpcClient = new (std::nothrow) IpcClient();
	if (pIpcClient == NULL)
	{
		_LOGE("Out of memory");
		return MESSAGEPORT_ERROR_OUT_OF_MEMORY;
	}
	_ResourceGuard<IpcClient> cIpcClient(pIpcClient);

	int ret = pIpcClient->Construct("message-port-server", this);
	if (ret != 0)
	{
		_LOGE("Failed to create ipc client: %d.", ret);
		return MESSAGEPORT_ERROR_IO_ERROR;
	}

	pthread_mutex_t* pMutex = new (std::nothrow) pthread_mutex_t;
	if (pMutex == NULL)
	{
		_LOGE("Out of memory");
		return MESSAGEPORT_ERROR_OUT_OF_MEMORY;
	}
	_ResourceGuard<pthread_mutex_t> cMutex(pMutex);

	int pid = getpid();
	char buffer[256] = {0, };

	ret = aul_app_get_appid_bypid(pid, buffer, sizeof(buffer));
	if (ret != AUL_R_OK)
	{
		_LOGE("Failed to get the application ID: %d", ret);

		return MESSAGEPORT_ERROR_IO_ERROR;
	}

	pthread_mutex_init(pMutex, NULL);

	__pMutex = cMutex.Release();
	__pIpcClient = cIpcClient.Release();
	__appId = buffer;

	return MESSAGEPORT_ERROR_NONE;
}

void
MessagePortProxy::OnIpcResponseReceived(IpcClient& client, const IPC::Message& message)
{
	IPC_BEGIN_MESSAGE_MAP(MessagePortProxy, message)
	IPC_MESSAGE_HANDLER_EX(MessagePort_sendMessageAsync, &client, OnSendMessageInternal)
	IPC_END_MESSAGE_MAP_EX()
}

int
MessagePortProxy::RegisterMessagePort(const string& localPort, bool isTrusted,  messageport_message_cb callback)
{
	_SECURE_LOGI("Register a message port : [%s:%s]", __appId.c_str(), localPort.c_str());

	int id = 0;

	// Check the message port is already registed
	if (IsLocalPortRegisted(localPort, isTrusted, id))
	{
		if (!isTrusted)
		{
			__listeners[localPort] = callback;
		}
		else
		{
			__trustedListeners[localPort] = callback;
		}

		return id;
	}

	bundle *pBundle = bundle_create();
	if (pBundle == NULL)
	{
		_LOGE("Out of memory");
		return MESSAGEPORT_ERROR_OUT_OF_MEMORY;
	}
	_ResourceGuard<bundle> cBundle(pBundle);

	if (!isTrusted)
	{
		bundle_add(pBundle, TRUSTED_LOCAL, "FALSE");
	}
	else
	{
		bundle_add(pBundle, TRUSTED_LOCAL, "TRUE");
	}

	bundle_add(pBundle, LOCAL_APPID, __appId.c_str());
	bundle_add(pBundle, LOCAL_PORT, localPort.c_str());


	// Create Bundle Buffer from bundle
	BundleBuffer buffer;
	buffer.b = pBundle;

	int ret = 0;
	int return_value = 0;

	IPC::Message* pMsg = new MessagePort_registerPort(buffer, &return_value);
	if (pMsg == NULL)
	{
		_LOGE("Out of memory");
		return  MESSAGEPORT_ERROR_OUT_OF_MEMORY;
	}
	_ResourceGuard<IPC::Message> cMsg(pMsg);

	ret = __pIpcClient->SendRequest(pMsg);
	if (ret != MESSAGEPORT_ERROR_NONE)
	{
		_LOGE("Failed to send a request: %d.", ret);
		return ret;
	}

	// Add a listener
	if (!isTrusted)
	{
		// Local port id
		id = GetNextId();

		__listeners[localPort] = callback;
		__idFromString[localPort] = id;
		__ids[id] = localPort;
	}
	else
	{
		id = GetNextId();

		__trustedListeners[localPort] = callback;
		__trustedIdFromString[localPort] = id;
		__trustedIds[id] = localPort;
	}

	return id;
}

int
MessagePortProxy::CheckRemotePort(const string& remoteAppId, const string& remotePort,	bool isTrusted, bool *exist)
{
	_SECURE_LOGI("Check a remote port : [%s:%s]", remoteAppId.c_str(), remotePort.c_str());

	bundle *pBundle = bundle_create();
	if (pBundle == NULL)
	{
		_LOGE("Out of memory");
		return MESSAGEPORT_ERROR_OUT_OF_MEMORY;
	}
	_ResourceGuard<bundle> cBundle(pBundle);

	bundle_add(pBundle, LOCAL_APPID, __appId.c_str());

	bundle_add(pBundle, REMOTE_APPID, remoteAppId.c_str());
	bundle_add(pBundle, REMOTE_PORT, remotePort.c_str());

	if (!isTrusted)
	{
		bundle_add(pBundle, TRUSTED_REMOTE, "FALSE");
	}
	else
	{
		bundle_add(pBundle, TRUSTED_REMOTE, "TRUE");
	}

	// To Bundle Buffer
	BundleBuffer buffer;
	buffer.b = pBundle;

	int return_value = 0;
	IPC::Message* pMsg = new MessagePort_checkRemotePort(buffer, &return_value);
	if (pMsg == NULL)
	{
		_LOGE("Out of memory");
		return MESSAGEPORT_ERROR_OUT_OF_MEMORY;
	}
	_ResourceGuard<IPC::Message> cMsg(pMsg);

	int ret = __pIpcClient->SendRequest(pMsg);
	if (ret != MESSAGEPORT_ERROR_NONE)
	{
		_LOGE("Failed to send a request: %d.", ret);
		return ret;
	}

	if (return_value < 0)
	{
		if (return_value == MESSAGEPORT_ERROR_MESSAGEPORT_NOT_FOUND)
		{
			_LOGE("The remote message port (%s) is not found.", remotePort.c_str());

			*exist = false;
			return MESSAGEPORT_ERROR_NONE;
		}
		else if (return_value == MESSAGEPORT_ERROR_CERTIFICATE_NOT_MATCH)
		{
			_SECURE_LOGI("The remote application (%s) is not signed with the same certificate", remoteAppId.c_str());
			return MESSAGEPORT_ERROR_CERTIFICATE_NOT_MATCH;
		}
		else
		{
			_LOGE("Failed to check the remote messge port: %d.", return_value);
			return MESSAGEPORT_ERROR_IO_ERROR;
		}
	}

	*exist = true;
	return MESSAGEPORT_ERROR_NONE;
}

int
MessagePortProxy::SendMessage(const string& remoteAppId, const string& remotePort, bool trustedMessage, bundle* data)
{
	_SECURE_LOGI("Send a message to : [%s:%s]", remoteAppId.c_str(), remotePort.c_str());

	int ret = 0;

	bundle *pBundle = bundle_create();
	if (pBundle == NULL)
	{
		_LOGE("Out of memory");
		return MESSAGEPORT_ERROR_OUT_OF_MEMORY;
	}
	_ResourceGuard<bundle> cBundle(pBundle);

	bundle_add(pBundle, MESSAGE_TYPE, "UNI-DIR");

	bundle_add(pBundle, LOCAL_APPID, __appId.c_str());

	bundle_add(pBundle, REMOTE_APPID, remoteAppId.c_str());
	bundle_add(pBundle, REMOTE_PORT, remotePort.c_str());

	if (!trustedMessage)
	{
		bundle_add(pBundle, TRUSTED_MESSAGE, "FALSE");
	}
	else
	{
		bundle_add(pBundle, TRUSTED_MESSAGE, "TRUE");
	}

	BundleBuffer metadata;
	metadata.b = pBundle;

	BundleBuffer buffer;
	buffer.b = data;

	ret = SendMessageInternal(metadata, buffer);

	return ret;
}

int
MessagePortProxy::SendMessage(const string& localPort, bool trustedPort, const string& remoteAppId, const string& remotePort, bool trustedMessage, bundle* data)
{
	_SECURE_LOGI("Send a bidirectional message from [%s:%s] to [%s:%s]", __appId.c_str(), localPort.c_str(), remoteAppId.c_str(), remotePort.c_str());

	int ret = 0;

	bundle *pBundle = bundle_create();
	if (pBundle == NULL)
	{
		_LOGE("Out of memory");
		return MESSAGEPORT_ERROR_OUT_OF_MEMORY;
	}
	_ResourceGuard<bundle> cBundle(pBundle);

	bundle_add(pBundle, MESSAGE_TYPE, "BI-DIR");

	bundle_add(pBundle, LOCAL_APPID, __appId.c_str());
	bundle_add(pBundle, LOCAL_PORT, localPort.c_str());

	if (!trustedPort)
	{
		bundle_add(pBundle, TRUSTED_LOCAL, "FALSE");
	}
	else
	{
		bundle_add(pBundle, TRUSTED_LOCAL, "TRUE");
	}

	bundle_add(pBundle, REMOTE_APPID, remoteAppId.c_str());
	bundle_add(pBundle, REMOTE_PORT, remotePort.c_str());

	if (!trustedMessage)
	{
		bundle_add(pBundle, TRUSTED_MESSAGE, "FALSE");
	}
	else
	{
		bundle_add(pBundle, TRUSTED_MESSAGE, "TRUE");
	}

	BundleBuffer metadata;
	metadata.b = pBundle;

	BundleBuffer buffer;
	buffer.b = data;

	ret = SendMessageInternal(metadata, buffer);

	return ret;
}

int
MessagePortProxy::SendMessageInternal(const BundleBuffer& metadata, const BundleBuffer& buffer)
{
	int return_value = 0;
	IPC::Message* pMsg = new MessagePort_sendMessage(metadata, buffer, &return_value);
	if (pMsg == NULL)
	{
		_LOGE("Out of memory");
		return MESSAGEPORT_ERROR_OUT_OF_MEMORY;
	}
	_ResourceGuard<IPC::Message> cMsg(pMsg);

	// Check the message size
	int len = 0;
	bundle_raw* raw = NULL;
	bundle_encode_raw(buffer.b, &raw, &len);

	bundle_free_encoded_rawdata(&raw);

	if (len > MAX_MESSAGE_SIZE)
	{
		_LOGE("The size of message (%d) has exceeded the maximum limit.", len);
		return MESSAGEPORT_ERROR_MAX_EXCEEDED;
	}

	int ret = __pIpcClient->SendRequest(pMsg);
	if (ret != MESSAGEPORT_ERROR_NONE)
	{
		_LOGE("Failed to send a request: %d.", ret);
		return ret;
	}

	if (return_value < 0)
	{
		if (return_value == MESSAGEPORT_ERROR_MESSAGEPORT_NOT_FOUND)
		{
			_LOGE("The remote message port is not found.");

			return MESSAGEPORT_ERROR_MESSAGEPORT_NOT_FOUND;
		}
		else if (return_value == MESSAGEPORT_ERROR_CERTIFICATE_NOT_MATCH)
		{
			_LOGE("The remote application is not signed with the same certificate.");

			return MESSAGEPORT_ERROR_CERTIFICATE_NOT_MATCH;
		}
		else if (return_value == MESSAGEPORT_ERROR_RESOURCE_UNAVAILABLE)
		{
			_LOGE("The socket receiver buffer of remote port is temporarily full.");

			return MESSAGEPORT_ERROR_RESOURCE_UNAVAILABLE;
		}
		else
		{
			_LOGE("Failed to check the remote messge port: %d.", return_value);

			return MESSAGEPORT_ERROR_IO_ERROR;
		}
	}

	return MESSAGEPORT_ERROR_NONE;
}

char*
MessagePortProxy::GetLocalPortNameN(int id)
{
	string value;

	map<int, std::string>::iterator it;

	it = __ids.find(id);
	if (it == __ids.end())
	{
		it = __trustedIds.find(id);
		if (it == __ids.end())
		{
			_LOGE("Invalid value %d", id);
			return NULL;
		}
		else
		{
			value = __trustedIds[id];
			return strdup(value.c_str());
		}
	}
	else
	{
		value = __ids[id];
		return strdup(value.c_str());
	}

	return NULL;
}

int
MessagePortProxy::CheckTrustedLocalPort(int id, bool* trusted)
{
	map<int, std::string>::iterator it;

	it = __ids.find(id);
	if (it == __ids.end())
	{
		it = __trustedIds.find(id);
		if (it == __ids.end())
		{
			_LOGE("Invalid value %d", id);
			return MESSAGEPORT_ERROR_INVALID_PARAMETER;
		}
		else
		{
			*trusted = true;
			return MESSAGEPORT_ERROR_NONE;
		}
	}
	else
	{
		*trusted = false;
		return MESSAGEPORT_ERROR_NONE;
	}

	return MESSAGEPORT_ERROR_INVALID_PARAMETER;
}

MessagePortProxy*
MessagePortProxy::GetProxy(void)
{
	static MessagePortProxy* pProxy = NULL;

	if (pProxy == NULL)
	{
		MessagePortProxy* p = new MessagePortProxy();
		if (p == NULL)
		{
			_LOGE("Out of memory");
			return NULL;
		}

		int ret = p->Construct();
		if (ret < 0)
		{
			return NULL;
		}

		pProxy = p;
	}

	return pProxy;
}

int
MessagePortProxy::GetNextId(void)
{
	static int count = 0;

	pthread_mutex_lock(__pMutex);
	++count;
	pthread_mutex_unlock(__pMutex);

	return count;
}

bool
MessagePortProxy::IsLocalPortRegisted(const string& localPort, bool trusted, int &id)
{
	if (!trusted)
	{
		map<string, messageport_message_cb>::iterator port_it = __listeners.find(localPort);
		if (port_it == __listeners.end())
		{
			return false;
		}
		else
		{
			_LOGI("MessagePort name is already registered.");
			for (map<int, string>::iterator it = __ids.begin(); it != __ids.end(); ++it)
			{
				if (localPort.compare(it->second) == 0)
				{
					id = it->first;
					return true;
				}
			}
		}
	}
	else
	{
		map<string, messageport_message_cb>::iterator port_it = __trustedListeners.find(localPort);
		if (port_it == __trustedListeners.end())
		{
			return false;
		}
		else
		{
			_LOGI("MessagePort name is already registered.");
			for (map<int, string>::iterator it = __trustedIds.begin(); it != __trustedIds.end(); ++it)
			{
				if (localPort.compare(it->second) == 0)
				{
					id = it->first;
					return true;
				}
			}
		}
	}

	return false;
}

bool
MessagePortProxy::OnSendMessageInternal(const BundleBuffer& metadata, const BundleBuffer& buffer)
{
	bundle* pBundle = metadata.b;
	_ResourceGuard<bundle> cBundle(pBundle);

	const char* pLocalAppId = bundle_get_val(pBundle, LOCAL_APPID);
	const char* pRemoteAppId = bundle_get_val(pBundle, REMOTE_APPID);
	const char* pRemotePort = bundle_get_val(pBundle, REMOTE_PORT);
	string trustedMessage = bundle_get_val(pBundle, TRUSTED_MESSAGE);

	string messageType = bundle_get_val(pBundle, MESSAGE_TYPE);

	_SECURE_LOGI("Message received! from App: %s, to App: %s, Port: %s, Trusted: %s", pLocalAppId, pRemoteAppId, pRemotePort, trustedMessage.c_str());

	int id = 0;
	messageport_message_cb callback;

	if (trustedMessage.compare("FALSE") == 0)
	{
		callback = __listeners[pRemotePort];
		id = __idFromString[pRemotePort];
	}
	else
	{
		callback = __trustedListeners[pRemotePort];
		id = __trustedIdFromString[pRemotePort];
	}


	if (callback)
	{
		if (messageType.compare("UNI-DIR") == 0)
		{
			callback(id, pLocalAppId, NULL, false, buffer.b);
		}
		else
		{
			string localPort = bundle_get_val(pBundle, LOCAL_PORT);
			string trustedLocal = bundle_get_val(pBundle, TRUSTED_LOCAL);

			_SECURE_LOGI("From App: %s, Port: %s, TrustedLocal: %s", pLocalAppId, localPort.c_str(), trustedLocal.c_str());

			bool trustedPort = (trustedLocal.compare("TRUE") == 0);

			callback(id, pLocalAppId, localPort.c_str(), trustedPort, buffer.b);
		}

	}
	else
	{
		_LOGI("No callback");
	}

	return true;
}

