// // Open Service Platform
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
 * @file	MessagePortService.cpp
 * @brief	This is the implementation file for the _MessagePortService class.
 *
 */

#include <bundle.h>
#include <message-port.h>
#include <pkgmgr-info.h>

#include "message-port-log.h"
#include "MessagePortStub.h"
#include "MessagePortService.h"

using namespace std;

static const char LOCAL_APPID[] = "LOCAL_APPID";
static const char LOCAL_PORT[] = "LOCAL_PORT";
static const char TRUSTED_LOCAL[] = "TRUSTED_LOCAL";

static const char REMOTE_APPID[] = "REMOTE_APPID";
static const char REMOTE_PORT[] = "REMOTE_PORT";
static const char TRUSTED_REMOTE[] = "TRUSTED_REMOTE";
static const char TRUSTED_MESSAGE[] = "TRUSTED_MESSAGE";

_MessagePortService::_MessagePortService(void)
	: __pStub(NULL)
{

}

_MessagePortService::~_MessagePortService(void)
{

}

int
_MessagePortService::Construct(_MessagePortStub& stub)
{
	_LOGI("_MessagePortService::Construct");

	__pPorts = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	if (__pPorts == NULL)
	{
		_LOGE("Out of memory");
		return MESSAGEPORT_ERROR_OUT_OF_MEMORY;
	}
	__pTrustedPorts = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	if (__pTrustedPorts == NULL)
	{
		_LOGE("Out of memory");
		return MESSAGEPORT_ERROR_OUT_OF_MEMORY;
	}
	__pStub = &stub;

	return MESSAGEPORT_ERROR_NONE;
}

int
_MessagePortService::RegisterMessagePort(int clientId, const BundleBuffer& buffer)
{
	_LOGI("_MessagePortService::RegisterMessagePort");

	bundle* b = buffer.b;
	char *key = GetKey(buffer);
	if (key == NULL)
	{
		_LOGE("Out of memory");
		return MESSAGEPORT_ERROR_OUT_OF_MEMORY;
	}

	_LOGI("Register a message port: [%s], client = %d", key, clientId);

	const char *trusted(bundle_get_val(b, TRUSTED_LOCAL));
	int *id = (int *) g_malloc(sizeof(int));
	if (id == NULL)
	{
		_LOGE("Out of memory");
		return MESSAGEPORT_ERROR_OUT_OF_MEMORY;
	}
	*id = clientId;
	int *value = NULL;

	if (strncmp(trusted, "TRUE", 4) == 0)
	{
		value = (int*) g_hash_table_lookup(__pTrustedPorts, key);
		if (value)
		{
			if (*value == clientId)
			{
				_SECURE_LOGE("The local message port (%s) has already registered", key);
				return MESSAGEPORT_ERROR_IO_ERROR;
			}
			_SECURE_LOGI("Remove garbage values : %s", key);
			g_hash_table_remove(__pTrustedPorts, key);
		}
		g_hash_table_insert(__pTrustedPorts, key, id);
	}
	else
	{
		value = (int*) g_hash_table_lookup(__pPorts, key);
		if (value)
		{
			if (*value == clientId)
			{
				_SECURE_LOGE("The local message port (%s) has already registered", key);
				return MESSAGEPORT_ERROR_IO_ERROR;
			}
			_SECURE_LOGI("Remove garbage values : %s", key);
			g_hash_table_remove(__pPorts, key);
		}
		g_hash_table_insert(__pPorts, key, id);
	}

	return MESSAGEPORT_ERROR_NONE;
}

int
_MessagePortService::CheckRemotePort(const BundleBuffer& buffer)
{
	_LOGI("_MessagePortService::CheckRemotePort");

	bundle* b = buffer.b;

	char *key = GetKey(buffer, false);
	if (key == NULL)
	{
		_LOGE("Out of memory");
		return MESSAGEPORT_ERROR_OUT_OF_MEMORY;
	}

	const char *trusted(bundle_get_val(b, TRUSTED_REMOTE));

	_LOGI("Check a remote message port: [%s]", key);

	bool out = false;

	gpointer orig_key;
	gpointer value;

	if (strncmp(trusted, "TRUE", 4) == 0)
	{
		out = g_hash_table_lookup_extended(__pTrustedPorts, key, &orig_key, &value);
	}
	else
	{
		out = g_hash_table_lookup_extended(__pPorts, key, &orig_key, &value);
	}

	if (out == false)
	{
		_LOGE("_MessagePortService::CheckRemotePort() Failed: MESSAGEPORT_ERROR_MESSAGEPORT_NOT_FOUND");
		return MESSAGEPORT_ERROR_MESSAGEPORT_NOT_FOUND;
	}

	if (strncmp(trusted, "TRUE", 4) == 0)
	{
		const char* localAppId = bundle_get_val(b, LOCAL_APPID);
		const char* remoteAppId = bundle_get_val(b, REMOTE_APPID);

		// Check the preloaded
		if (!IsPreloaded(localAppId, remoteAppId))
		{
			// Check the certificate
			return CheckCertificate(localAppId, remoteAppId);
		}
	}

	return MESSAGEPORT_ERROR_NONE;
}

static gboolean unregistermessageport(gpointer key, gpointer value, gpointer user_data)
{
	_LOGI("unregistermessageport");

	char *clientId = (char *)key;
	char *removeclient = (char *)user_data;

	if (strcmp(clientId, removeclient) == 0)
	{
		return true;
	}

	return false;
}


int
_MessagePortService::UnregisterMessagePort(int clientId)
{
	_LOGI("_MessagePortService::UnregisterMessagePort");

	g_hash_table_foreach_remove(__pPorts, unregistermessageport, &clientId);
	g_hash_table_foreach_remove(__pTrustedPorts, unregistermessageport, &clientId);

	return MESSAGEPORT_ERROR_NONE;
}

int
_MessagePortService::SendMessage(const BundleBuffer& metadata, const BundleBuffer& buffer)
{
	_LOGI("_MessagePortService::SendMessage");

	int *clientId = NULL;

	bundle* b = metadata.b;

	char *key = GetKey(metadata, false);
	_LOGI("Sends a message to a remote message port [%s]", key);

	const char *trustedMessage(bundle_get_val(b, TRUSTED_MESSAGE));
	if (strncmp(trustedMessage, "TRUE", 4) == 0)
	{
		clientId = (int*) g_hash_table_lookup(__pTrustedPorts, key);
	}
	else
	{
		clientId = (int*) g_hash_table_lookup(__pPorts, key);
	}

	if (clientId)
	{
		if (strncmp(trustedMessage, "TRUE", 4) == 0)
		{
			const char *localAppId = bundle_get_val(b, LOCAL_APPID);
			const char *remoteAppId = bundle_get_val(b, REMOTE_APPID);

			// Check the preloaded
			if (!IsPreloaded(localAppId, remoteAppId))
			{
				// Check the certificate
				int ret = CheckCertificate(localAppId, remoteAppId);
				if (ret < 0)
				{
					return ret;;
				}
			}
		}
		int ret  = __pStub->SendMessage(*clientId, metadata, buffer);
		if (ret < 0)
		{
			_LOGE("_MessagePortService::SendMessage: Failed");
			return MESSAGEPORT_ERROR_IO_ERROR;
		}
	}
	else
	{
		_LOGE("_MessagePortService::SendMessage: Failed :MESSAGEPORT_ERROR_MESSAGEPORT_NOT_FOUND");
		return MESSAGEPORT_ERROR_MESSAGEPORT_NOT_FOUND;
	}

	return MESSAGEPORT_ERROR_NONE;
}


char*
_MessagePortService::GetKey(const BundleBuffer& buffer, bool local) const
{
	_LOGI("_MessagePortService::GetKey");

	const char* pAppId = NULL;
	const char* pPortName = NULL;
	char* key = NULL;

	bundle* b = buffer.b;

	if (local)
	{
		pAppId = bundle_get_val(b, LOCAL_APPID);
		pPortName = bundle_get_val(b, LOCAL_PORT);
	}
	else
	{
		pAppId = bundle_get_val(b, REMOTE_APPID);
		pPortName = bundle_get_val(b, REMOTE_PORT);
	}

	key = (char *) g_malloc0((strlen(pAppId) + strlen(pPortName) + strlen(":") + 1) * sizeof(char));
	if (key == NULL)
	{
		_LOGE("Out of memory");
		return NULL;
	}

	strcat(key, pAppId);
	strcat(key, ":");
	strcat(key, pPortName);

	_SECURE_LOGI("_MessagePortService::GetKey Key:[%s]", key);
	return key;
}

bool
_MessagePortService::IsPreloaded(const char *localAppId, const char *remoteAppId) const
{
	_LOGI("_MessagePortService::IsPreloaded");

	bool preload_local = false;
	bool preload_remote = false;

	pkgmgrinfo_appinfo_h handle = NULL;
	int ret = pkgmgrinfo_appinfo_get_appinfo(localAppId, &handle);
	if (ret != PMINFO_R_OK)
	{
		_LOGE("Failed to get the appinfo. %d", ret);
		pkgmgrinfo_appinfo_destroy_appinfo(handle);
		return false;
	}
	ret = pkgmgrinfo_appinfo_is_preload(handle, &preload_local);
	if (ret != PMINFO_R_OK)
	{
		_LOGE("Failed to check the preloaded application. %d", ret);
		pkgmgrinfo_appinfo_destroy_appinfo(handle);
		return false;
	}

	ret = pkgmgrinfo_appinfo_get_appinfo(remoteAppId, &handle);
	if (ret != PMINFO_R_OK)
	{
		_LOGE("Failed to get the appinfo. %d", ret);
		pkgmgrinfo_appinfo_destroy_appinfo(handle);
		return false;
	}
	ret = pkgmgrinfo_appinfo_is_preload(handle, &preload_remote);
	if (ret != PMINFO_R_OK)
	{
		_LOGE("Failed to check the preloaded application. %d", ret);
		pkgmgrinfo_appinfo_destroy_appinfo(handle);
		return false;
	}

	if (preload_local && preload_remote)
	{
		pkgmgrinfo_appinfo_destroy_appinfo(handle);
		return true;
	}
	pkgmgrinfo_appinfo_destroy_appinfo(handle);
	return false;
}

int
_MessagePortService::CheckCertificate(const char *localAppId, const char *remoteAppId) const
{
	_LOGI("_MessagePortService::CheckCertificate");

	pkgmgrinfo_cert_compare_result_type_e res;
	int ret = pkgmgrinfo_pkginfo_compare_app_cert_info(localAppId, remoteAppId, &res);
	if (ret < 0)
	{
		_LOGE("_MessagePortService::CheckCertificate() Failed");
		return MESSAGEPORT_ERROR_IO_ERROR;
	}
	if (res != PMINFO_CERT_COMPARE_MATCH)
	{
		_LOGE("_MessagePortService::CheckCertificate() Failed : MESSAGEPORT_ERROR_CERTIFICATE_NOT_MATCH");
		return MESSAGEPORT_ERROR_CERTIFICATE_NOT_MATCH;
	}

	return MESSAGEPORT_ERROR_NONE;
}

