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
 * @file	message-port.cpp
 * @brief	This is the implementation file for the MessagePort.
 */

#include <stdlib.h>

#include "message-port.h"
#include "message-port-log.h"

#include "MessagePortProxy.h"

int
messageport_register_local_port(const char* local_port, messageport_message_cb callback)
{
	MessagePortProxy* pProxy = MessagePortProxy::GetProxy();
	if (pProxy != NULL)
	{
		if (callback)
			return pProxy->RegisterMessagePort(local_port, false, callback);
		else
			return pProxy->UnRegisterMessagePort(local_port, false);
	}

	return MESSAGEPORT_ERROR_IO_ERROR;
}

int
messageport_register_trusted_local_port(const char* local_port, messageport_message_cb callback)
{
	MessagePortProxy* pProxy = MessagePortProxy::GetProxy();
	if (pProxy != NULL)
	{
		if (callback)
			return pProxy->RegisterMessagePort(local_port, true, callback);
		else
			return pProxy->UnRegisterMessagePort(local_port, true);

	}

	return MESSAGEPORT_ERROR_IO_ERROR;
}

int
messageport_check_remote_port(const char* remote_app_id, const char *remote_port, bool* exist)
{
	MessagePortProxy* pProxy = MessagePortProxy::GetProxy();
	if (pProxy != NULL)
	{
		return pProxy->CheckRemotePort(remote_app_id, remote_port, false, exist);
	}

	return MESSAGEPORT_ERROR_IO_ERROR;
}

int
messageport_check_trusted_remote_port(const char* remote_app_id, const char *remote_port, bool* exist)
{
	MessagePortProxy* pProxy = MessagePortProxy::GetProxy();
	if (pProxy != NULL)
	{
		return pProxy->CheckRemotePort(remote_app_id, remote_port, true, exist);
	}

	return MESSAGEPORT_ERROR_IO_ERROR;
}

int
messageport_send_message(const char* remote_app_id, const char* remote_port, bundle* message)
{
	MessagePortProxy* pProxy = MessagePortProxy::GetProxy();
	if (pProxy != NULL)
	{
		return pProxy->SendMessage(remote_app_id, remote_port, false, message);
	}

	return MESSAGEPORT_ERROR_IO_ERROR;
}

int
messageport_send_trusted_message(const char* remote_app_id, const char* remote_port, bundle* message)
{
	MessagePortProxy* pProxy = MessagePortProxy::GetProxy();
	if (pProxy != NULL)
	{
		return pProxy->SendMessage(remote_app_id, remote_port, true, message);
	}

	return MESSAGEPORT_ERROR_IO_ERROR;
}

int
messageport_send_bidirectional_message(int id, const char* remote_app_id, const char* remote_port, bundle* message)
{
	int ret = 0;
	MessagePortProxy* pProxy = MessagePortProxy::GetProxy();
	if (pProxy != NULL)
	{
		char* pName = pProxy->GetLocalPortNameN(id);
		bool trusted = false;
		if (pName == NULL) {
			return MESSAGEPORT_ERROR_INVALID_PARAMETER;
		}

		ret = pProxy->CheckTrustedLocalPort(id, &trusted);
		if (ret < 0)
		{
			free(pName);
			return MESSAGEPORT_ERROR_INVALID_PARAMETER;
		}

		ret = pProxy->SendMessage(pName, trusted, remote_app_id, remote_port, false, message);

		free(pName);

		return ret;
	}

	return MESSAGEPORT_ERROR_IO_ERROR;
}

int
messageport_send_bidirectional_trusted_message(int id, const char* remote_app_id, const char* remote_port, bundle* message)
{
	MessagePortProxy* pProxy = MessagePortProxy::GetProxy();
	if (pProxy != NULL)
	{
		bool trusted = false;
		int ret = pProxy->CheckTrustedLocalPort(id, &trusted);
		if (ret < 0)
		{
			return ret;
		}

		char* pName = pProxy->GetLocalPortNameN(id);
		if (pName == NULL)
		{
			return MESSAGEPORT_ERROR_INVALID_PARAMETER;
		}

		ret = pProxy->SendMessage(pName, trusted, remote_app_id, remote_port, true, message);

		free(pName);

		return ret;
	}

	return MESSAGEPORT_ERROR_IO_ERROR;
}

int
messageport_get_local_port_name(int id, char **name)
{
	MessagePortProxy* pProxy = MessagePortProxy::GetProxy();
	if (pProxy != NULL)
	{
		char* pName = pProxy->GetLocalPortNameN(id);
		if (pName == NULL)
		{
			return MESSAGEPORT_ERROR_INVALID_PARAMETER;
		}
		else
		{
			*name = pName;
			return 0;
		}
	}

	return MESSAGEPORT_ERROR_IO_ERROR;
}

int
messageport_check_trusted_local_port(int id, bool *trusted)
{
	MessagePortProxy* pProxy = MessagePortProxy::GetProxy();
	if (pProxy != NULL)
	{
		return pProxy->CheckTrustedLocalPort(id, trusted);
	}

	return MESSAGEPORT_ERROR_IO_ERROR;
}

