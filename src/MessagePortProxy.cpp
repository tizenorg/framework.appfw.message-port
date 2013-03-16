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

#include <package_manager.h>

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

static const int MAX_MESSAGE_SIZE = 8 * 1024;

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
		return MESSAGEPORT_ERROR_OUT_OF_MEMORY;
	}

	int ret = pIpcClient->Construct("message-port-server", this);
	if (ret != 0)
	{
		delete pIpcClient;

		_LOGE("Failed to create ipc client: %d.", ret);

		return MESSAGEPORT_ERROR_IO_ERROR;
	}

	pthread_mutex_t* pMutex = (pthread_mutex_t*) malloc(sizeof(pthread_mutex_t));
	if (pMutex == NULL)
	{
		return MESSAGEPORT_ERROR_OUT_OF_MEMORY;
	}

	pthread_mutex_init(pMutex, NULL);

	__pMutex = pMutex;
	__pIpcClient = pIpcClient;
	__appId = pIpcClient->GetAppId();

	return 0;
}

void
MessagePortProxy::OnIpcResponseReceived(IpcClient& client, const IPC::Message& message)
{
	_LOGD("Message received, type %d", message.type());

	IPC_BEGIN_MESSAGE_MAP(MessagePortProxy, message)
	IPC_MESSAGE_HANDLER_EX(MessagePort_sendMessageAsync, &client, OnSendMessageInternal)
	IPC_END_MESSAGE_MAP_EX()
}

int
MessagePortProxy::RegisterMessagePort(const string& localPort, bool isTrusted,  messageport_message_cb callback)
{
	_LOGD("Register a message port : [%s:%s]", __appId.c_str(), localPort.c_str());

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

	bundle *b = bundle_create();

	if (!isTrusted)
	{
		bundle_add(b, TRUSTED_LOCAL, "FALSE");
	}
	else
	{
		bundle_add(b, TRUSTED_LOCAL, "TRUE");
	}

	bundle_add(b, LOCAL_APPID, __appId.c_str());
	bundle_add(b, LOCAL_PORT, localPort.c_str());


	// Create Bundle Buffer from bundle
	BundleBuffer buffer;
	buffer.b = b;

	int ret = 0;
	int return_value = 0;

	IPC::Message* pMsg = new MessagePort_registerPort(buffer, &return_value);
	if (pMsg == NULL)
	{
		bundle_free(b);
		return  MESSAGEPORT_ERROR_OUT_OF_MEMORY;
	}

	ret = __pIpcClient->SendRequest(pMsg);

	delete pMsg;

	bundle_free(b);

	if (ret != 0)
	{
		_LOGE("Failed to send a request: %d.", ret);

		return MESSAGEPORT_ERROR_IO_ERROR;
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
	_LOGD("Check a remote port : [%s:%s]", remoteAppId.c_str(), remotePort.c_str());

	int ret = 0;

	// Check the certificate
	if (isTrusted)
	{
		// Check the preloaded
		if (!IsPreloaded(remoteAppId))
		{
			ret = CheckCertificate(remoteAppId);
			if (ret < 0)
			{
				return ret;
			}
		}
	}

	bundle *b = bundle_create();
	bundle_add(b, REMOTE_APPID, remoteAppId.c_str());
	bundle_add(b, REMOTE_PORT, remotePort.c_str());

	if (!isTrusted)
	{
		bundle_add(b, TRUSTED_REMOTE, "FALSE");
	}
	else
	{
		bundle_add(b, TRUSTED_REMOTE, "TRUE");
	}

	// To Bundle Buffer
	BundleBuffer buffer;
	buffer.b = b;

	int return_value = 0;
	IPC::Message* pMsg = new MessagePort_checkRemotePort(buffer, &return_value);
	if (pMsg == NULL)
	{
		bundle_free(b);

		return MESSAGEPORT_ERROR_OUT_OF_MEMORY;
	}

	ret = __pIpcClient->SendRequest(pMsg);

	delete pMsg;

	bundle_free(b);

	if (ret < 0)
	{
		_LOGE("Failed to send a request: %d.", ret);

		return MESSAGEPORT_ERROR_IO_ERROR;
	}

	if (return_value < 0)
	{
		if (return_value == MESSAGEPORT_ERROR_MESSAGEPORT_NOT_FOUND)
		{
			*exist = false;
			return 0;
		}
		else
		{
			_LOGE("Failed to check the remote messge port: %d.", return_value);
			return MESSAGEPORT_ERROR_IO_ERROR;
		}
	}

	*exist = true;
	return 0;
}

int
MessagePortProxy::SendMessage(const string& remoteAppId, const string& remotePort, bool trustedMessage, bundle* data)
{
	_LOGD("Send a message to : [%s:%s]", remoteAppId.c_str(), remotePort.c_str());

	int ret = 0;

	// Check the certificate
	if (trustedMessage)
	{
		// Check the preloaded
		if (!IsPreloaded(remoteAppId))
		{
			ret = CheckCertificate(remoteAppId);
			if (ret < 0)
			{
				return ret;
			}
		}
	}

	bundle_add(data, MESSAGE_TYPE, "UNI-DIR");

	bundle_add(data, REMOTE_APPID, remoteAppId.c_str());
	bundle_add(data, REMOTE_PORT, remotePort.c_str());

	if (!trustedMessage)
	{
		bundle_add(data, TRUSTED_MESSAGE, "FALSE");
	}
	else
	{
		bundle_add(data, TRUSTED_MESSAGE, "TRUE");
	}

	BundleBuffer buffer;
	buffer.b = data;

	ret = SendMessageInternal(buffer);

	return ret;
}

int
MessagePortProxy::SendMessage(const string& localPort, bool trustedPort, const string& remoteAppId, const string& remotePort, bool trustedMessage, bundle* data)
{
	_LOGD("Send a bidirectional message from [%s:%s] to [%s:%s]", __appId.c_str(), localPort.c_str(), remoteAppId.c_str(), remotePort.c_str());

	int ret = 0;

	// Check the certificate
	if (trustedMessage)
	{
		// Check the preloaded
		if (!IsPreloaded(remoteAppId))
		{
			ret = CheckCertificate(remoteAppId);
			if (ret < 0)
			{
				return ret;
			}
		}
	}

	bundle_add(data, MESSAGE_TYPE, "BI-DIR");

	bundle_add(data, LOCAL_APPID, __appId.c_str());
	bundle_add(data, LOCAL_PORT, localPort.c_str());

	if (!trustedPort)
	{
		bundle_add(data, TRUSTED_LOCAL, "FALSE");
	}
	else
	{
		bundle_add(data, TRUSTED_LOCAL, "TRUE");
	}

	bundle_add(data, REMOTE_APPID, remoteAppId.c_str());
	bundle_add(data, REMOTE_PORT, remotePort.c_str());

	if (!trustedMessage)
	{
		bundle_add(data, TRUSTED_MESSAGE, "FALSE");
	}
	else
	{
		bundle_add(data, TRUSTED_MESSAGE, "TRUE");
	}

	BundleBuffer buffer;
	buffer.b = data;

	ret = SendMessageInternal(buffer);

	return ret;
}

int
MessagePortProxy::SendMessageInternal(const BundleBuffer& buffer)
{
	int ret = 0;

	IPC::Message* pMsg = new MessagePort_sendMessage(buffer, &ret);
	if (pMsg == NULL)
	{
		return MESSAGEPORT_ERROR_OUT_OF_MEMORY;
	}

	// Check the message size
	int len = 0;
	bundle_raw* raw = NULL;
	bundle_encode(buffer.b, &raw, &len);

	bundle_free_encoded_rawdata(&raw);

	if (len > MAX_MESSAGE_SIZE)
	{
		_LOGE("The size of message (%d) has exceeded the maximum limit.", len);
		return MESSAGEPORT_ERROR_MAX_EXCEEDED;
	}

	ret = __pIpcClient->SendRequest(pMsg);
	delete pMsg;

	if (ret != 0)
	{
		_LOGE("Failed to send a request: %d.", ret);
		return MESSAGEPORT_ERROR_IO_ERROR;
	}

	return 0;
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
			return MESSAGEPORT_ERROR_INVALID_PARAMETER;
		}
		else
		{
			*trusted = true;
			return 0;
		}
	}
	else
	{
		*trusted = false;
		return 0;
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
			for (map<int, string>::iterator it = __ids.begin(); it != __trustedIds.end(); ++it)
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

int
MessagePortProxy::CheckCertificate(const std::string& remoteAppId)
{
	package_manager_compare_result_type_e res;
	int ret = package_manager_compare_app_cert_info(__appId.c_str(), remoteAppId.c_str(), &res);

	if (ret == 0)
	{
		if (res != PACAKGE_MANAGER_COMPARE_MATCH)
		{
			_LOGE("The remote application (%s) is not signed with the same certificate", remoteAppId.c_str());
			return MESSAGEPORT_ERROR_CERTIFICATE_NOT_MATCH;
		}
	}
	else
	{
		_LOGE("Failed to check the certificate: %d.", ret);
		return MESSAGEPORT_ERROR_IO_ERROR;
	}

	return 0;
}

bool
MessagePortProxy::IsPreloaded(const std::string& remoteAppId)
{
	bool preload_local = false;
	bool preload_remote = false;

	if (package_manager_is_preload_package_by_app_id(__appId.c_str(), &preload_local) == 0)
	{
		if (package_manager_is_preload_package_by_app_id(remoteAppId.c_str(), &preload_remote) == 0)
		{
			if (preload_local && preload_remote)
			{
				return true;
			}
		}
		else
		{
			_LOGE("Failed to check the preloaded application.");
		}
	}
	else
	{
		_LOGE("Failed to check the preloaded application.");
	}

	return false;
}

bool
MessagePortProxy::OnSendMessageInternal(const BundleBuffer& buffer)
{
	bundle* b = buffer.b;

	const char* pRemoteAppId = bundle_get_val(b, REMOTE_APPID);
	const char* pRemotePort = bundle_get_val(b, REMOTE_PORT);
	string trustedMessage = bundle_get_val(b, TRUSTED_MESSAGE);

	string messageType = bundle_get_val(b, MESSAGE_TYPE);

	_LOGD("Message received to AppId: %s, Port: %s, Trusted: %s", pRemoteAppId, pRemotePort, trustedMessage.c_str());

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
		// remove system data
		bundle_del(b, REMOTE_APPID);
		bundle_del(b, REMOTE_PORT);
		bundle_del(b, TRUSTED_MESSAGE);
		bundle_del(b, MESSAGE_TYPE);

		if (messageType.compare("UNI-DIR") == 0)
		{
			callback(id, NULL, NULL, false, b);
		}
		else
		{
			string localAppId = bundle_get_val(b, LOCAL_APPID);
			string localPort = bundle_get_val(b, LOCAL_PORT);
			string trustedLocal = bundle_get_val(b, TRUSTED_LOCAL);

			_LOGD("From AppId: %s, Port: %s, TrustedLocal: %s", localAppId.c_str(), localPort.c_str(), trustedLocal.c_str());

			bool trustedPort = (trustedLocal.compare("TRUE") == 0);

			// remove system data
			bundle_del(b, LOCAL_APPID);
			bundle_del(b, LOCAL_PORT);
			bundle_del(b, TRUSTED_LOCAL);

			callback(id, localAppId.c_str(), localPort.c_str(), trustedPort, b);
		}
	}
	else
	{
		_LOGD("No callback");
	}

	return true;
}

