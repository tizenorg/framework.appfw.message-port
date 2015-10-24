
// Message Port
// Copyright (c) 2015 Samsung Electronics Co., Ltd.
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
#include <stdio.h>
#include <glib.h>
#include <gio/gio.h>
#include <aul/aul.h>
#include <openssl/md5.h>
#include <bundle.h>
#include <bundle_internal.h>
#include <pkgmgr-info.h>

#include "message-port.h"
#include "message-port-log.h"


#define MAX_PACKAGE_STR_SIZE 512
#define MESSAGEPORT_BUS_NAME_PREFIX "org.tizen.messageport._"
#define MESSAGEPORT_OBJECT_PATH "/org/tizen/messageport"
#define MESSAGEPORT_INTERFACE_PREFIX "org.tizen.messageport._"

#define DBUS_SERVICE_DBUS "org.freedesktop.DBus"
#define DBUS_PATH_DBUS "/org/freedesktop/DBus"
#define DBUS_INTERFACE_DBUS "org.freedesktop.DBus"

#define DBUS_RELEASE_NAME_REPLY_RELEASED        1 /* *< Service was released from the given name */
#define DBUS_RELEASE_NAME_REPLY_NON_EXISTENT    2 /* *< The given name does not exist on the bus */
#define DBUS_RELEASE_NAME_REPLY_NOT_OWNER       3 /* *< Service is not an owner of the given name */


#define retvm_if(expr, val, fmt, arg...) do { \
	if (expr) { \
		_LOGE(fmt, ##arg); \
		_LOGE("(%s) -> %s() return", #expr, __func__); \
		return val; \
	} \
} while (0)

#define retv_if(expr, val) do { \
	if (expr) { \
		_LOGE("(%s) -> %s() return", #expr, __func__); \
		return val; \
	} \
} while (0)

#define FREE_AND_NULL(ptr) do { \
	if (ptr) { \
		free((void *)ptr); \
		ptr = NULL; \
	} \
} while (0)

static bool _initialized = false;
static GDBusConnection *__gdbus_conn = NULL;
static char *__app_id;
static GHashTable *__local_port_info = NULL;
static GHashTable *__remote_port_info = NULL;;
static GHashTable *__sender_appid_hash = NULL;;
static GHashTable *__trusted_app_list_hash = NULL;
static const int MAX_MESSAGE_SIZE = 16 * 1024;



enum __certificate_info_type {
	UNKNOWN = 0,
	CERTIFICATE_MATCH,
	CERTIFICATE_NOT_MATCH,
};

typedef struct message_port_local_port_info {
	messageport_message_cb callback;
	bool is_trusted;
	char *port_name;
	int local_id;
} message_port_local_port_info_s;

typedef struct message_port_remote_port_info {
	char *sender_id;
	char *remote_app_id;
	int certificate_info;
	GList *port_list;
} message_port_remote_app_info_s;

typedef struct port_list_info {
	char *port_name;
	char *encoded_bus_name;
	bool is_trusted;
	int watcher_id;
	bool exist;
} port_list_info_s;

static char *__get_encoded_name(const char *remote_app_id, const char *port_name, bool is_trusted)
{

	int prefix_len = strlen(MESSAGEPORT_BUS_NAME_PREFIX);
	int postfix_len = 1;
	char *postfix = is_trusted ? "1" : "0";

	unsigned char c[MD5_DIGEST_LENGTH] = {0};
	char *md5_interface = NULL;
	char *temp;
	int index = 0;
	MD5_CTX mdContext;
	int encoded_bus_name_len = prefix_len + postfix_len + (MD5_DIGEST_LENGTH * 2) + 2;
	int bus_name_len = strlen(remote_app_id) + strlen(port_name) + 2;
	char *bus_name = (char *)calloc(bus_name_len, sizeof(char));
	if (bus_name == NULL) {
		_LOGE("bus_name calloc failed");
		return 0;
	}

	snprintf(bus_name, bus_name_len, "%s_%s", remote_app_id, port_name);

	MD5_Init(&mdContext);
	MD5_Update(&mdContext, bus_name, bus_name_len);
	MD5_Final(c, &mdContext);

	md5_interface = (char *)calloc(encoded_bus_name_len , sizeof(char));
	if (md5_interface == NULL) {
		if (bus_name)
			free(bus_name);

		_LOGE("md5_interface calloc failed!!");
		return 0;
	}

	snprintf(md5_interface, encoded_bus_name_len, "%s", MESSAGEPORT_BUS_NAME_PREFIX);
	temp = md5_interface;
	temp += prefix_len;

	for (index = 0; index < MD5_DIGEST_LENGTH; index++) {
		snprintf(temp, 3, "%02x", c[index]);
		temp += 2;
	}

	if (postfix && postfix_len > 0) {
		snprintf(temp, encoded_bus_name_len - (temp - md5_interface), "%s", postfix);
	}
	if (bus_name)
		free(bus_name);

	_LOGI("encoded_bus_name : %s ", md5_interface);

	return md5_interface;
}

static int __remote_port_compare_cb(gconstpointer a, gconstpointer b)
{
	port_list_info_s *key1 = (port_list_info_s *)a;
	port_list_info_s *key2 = (port_list_info_s *)b;

	if (key1->is_trusted == key2->is_trusted) {
		return strcmp(key1->port_name, key2->port_name);
	}

	return 1;
}


static bool __is_preloaded(const char *local_appid, const char *remote_appid)
{
	_LOGI("IsPreloaded");

	bool preload_local = false;
	bool preload_remote = false;

	pkgmgrinfo_appinfo_h handle = NULL;
	int ret = pkgmgrinfo_appinfo_get_appinfo(local_appid, &handle);
	if (ret != PMINFO_R_OK)	{
		_LOGE("Failed to get the appinfo. %d", ret);
		pkgmgrinfo_appinfo_destroy_appinfo(handle);
		return false;
	}
	ret = pkgmgrinfo_appinfo_is_preload(handle, &preload_local);
	if (ret != PMINFO_R_OK) {
		_LOGE("Failed to check the preloaded application. %d", ret);
		pkgmgrinfo_appinfo_destroy_appinfo(handle);
		return false;
	}
	ret = pkgmgrinfo_appinfo_get_appinfo(remote_appid, &handle);
	if (ret != PMINFO_R_OK) {
		_LOGE("Failed to get the appinfo. %d", ret);
		pkgmgrinfo_appinfo_destroy_appinfo(handle);
		return false;
	}
	ret = pkgmgrinfo_appinfo_is_preload(handle, &preload_remote);
	if (ret != PMINFO_R_OK) {
		_LOGE("Failed to check the preloaded application. %d", ret);
		pkgmgrinfo_appinfo_destroy_appinfo(handle);
		return false;
	}

	if (preload_local && preload_remote) {
		pkgmgrinfo_appinfo_destroy_appinfo(handle);
		return true;
	}
	pkgmgrinfo_appinfo_destroy_appinfo(handle);
	return false;
}

static int __check_certificate(const char *local_appid, const char *remote_appid)
{
	_LOGI("CheckCertificate");

	pkgmgrinfo_cert_compare_result_type_e res;
	int ret = pkgmgrinfo_pkginfo_compare_app_cert_info(local_appid, remote_appid, &res);
	if (ret < 0) {
		_LOGE(":CheckCertificate() Failed");
		return MESSAGEPORT_ERROR_IO_ERROR;
	}
	if (res != PMINFO_CERT_COMPARE_MATCH) {
		_LOGE("CheckCertificate() Failed : MESSAGEPORT_ERROR_CERTIFICATE_NOT_MATCH");
		return MESSAGEPORT_ERROR_CERTIFICATE_NOT_MATCH;
	}

	return MESSAGEPORT_ERROR_NONE;
}

static void on_name_appeared (GDBusConnection *connection,
		const gchar     *name,
		const gchar     *name_owner,
		gpointer         user_data)
{
	_LOGI("name appeared : %s %s", __app_id, name);
}

static void on_name_vanished (GDBusConnection *connection,
		const gchar     *name,
		gpointer         user_data)
{
	_LOGI("name vanished : %s", name);
	port_list_info_s *pli = (port_list_info_s *)user_data;
	g_bus_unwatch_name(pli->watcher_id);
	pli->exist = false;
}

static int __get_local_port_info(int id, message_port_local_port_info_s **info)
{
	message_port_local_port_info_s *mi = (message_port_local_port_info_s *)g_hash_table_lookup(__local_port_info, GINT_TO_POINTER(id));

	if (mi == NULL) {
		return MESSAGEPORT_ERROR_INVALID_PARAMETER;
	}
	*info = mi;

	return MESSAGEPORT_ERROR_NONE;
}

static port_list_info_s *__set_remote_port_info(const char *remote_app_id, const char *remote_port, bool is_trusted)
{
	int ret_val = MESSAGEPORT_ERROR_NONE;
	port_list_info_s *port_info = (port_list_info_s *)calloc(1, sizeof(port_list_info_s));

	if (!port_info) {
		ret_val = MESSAGEPORT_ERROR_OUT_OF_MEMORY;
		goto out;
	}
	port_info->port_name = strdup(remote_port);
	if (!port_info->port_name) {
		ret_val = MESSAGEPORT_ERROR_OUT_OF_MEMORY;
		goto out;
	}
	port_info->is_trusted = is_trusted;

	port_info->encoded_bus_name = __get_encoded_name(remote_app_id, remote_port, is_trusted);
	if (port_info->encoded_bus_name == NULL) {
		ret_val = MESSAGEPORT_ERROR_OUT_OF_MEMORY;
		goto out;
	}


	out:
	if (ret_val != MESSAGEPORT_ERROR_NONE) {
		if (port_info) {
			FREE_AND_NULL(port_info->port_name);
			FREE_AND_NULL(port_info->encoded_bus_name);
			free(port_info);
		}
		return NULL;
	}
	return port_info;
}

static message_port_remote_app_info_s *__set_remote_app_info(const char *remote_app_id, const char *remote_port, bool is_trusted)
{
	port_list_info_s *port_info = NULL;
	message_port_remote_app_info_s *remote_app_info = NULL;
	int ret_val = MESSAGEPORT_ERROR_NONE;

	remote_app_info = (message_port_remote_app_info_s *)calloc(1, sizeof(message_port_remote_app_info_s));
	if (!remote_app_info) {
		ret_val = MESSAGEPORT_ERROR_OUT_OF_MEMORY;
		goto out;
	}

	remote_app_info->remote_app_id = strdup(remote_app_id);
	if (remote_app_info->remote_app_id == NULL) {
		ret_val = MESSAGEPORT_ERROR_OUT_OF_MEMORY;;
		goto out;
	}

	port_info = __set_remote_port_info(remote_app_id, remote_port, is_trusted);
	if (port_info == NULL) {
		ret_val = MESSAGEPORT_ERROR_OUT_OF_MEMORY;
		goto out;
	}

	remote_app_info->port_list = g_list_append(remote_app_info->port_list, port_info);

	out:
	if (ret_val != MESSAGEPORT_ERROR_NONE) {
		if (remote_app_info) {
			FREE_AND_NULL(remote_app_info->remote_app_id);
			FREE_AND_NULL(remote_app_info);
		}
		return NULL;
	}
	return remote_app_info;
}

static int __get_remote_port_info(const char *remote_app_id, const char *remote_port, bool is_trusted,
		message_port_remote_app_info_s **mri, port_list_info_s **pli)
{
	message_port_remote_app_info_s *remote_app_info = NULL;
	port_list_info_s port_info;
	GList *cb_list = NULL;
	int ret_val = MESSAGEPORT_ERROR_NONE;
	char *bus_name = __get_encoded_name(remote_app_id, remote_port, is_trusted);
	if (bus_name == NULL) {
		ret_val = MESSAGEPORT_ERROR_OUT_OF_MEMORY;
		goto out;
	}

	remote_app_info = (message_port_remote_app_info_s *)g_hash_table_lookup(__remote_port_info, remote_app_id);

	if (remote_app_info == NULL) {
		remote_app_info = __set_remote_app_info(remote_app_id, remote_port, is_trusted);

		if (remote_app_info == NULL) {
			ret_val = MESSAGEPORT_ERROR_OUT_OF_MEMORY;
			goto out;
		}
		g_hash_table_insert(__remote_port_info, remote_app_info->remote_app_id, remote_app_info);

	}
	*mri = remote_app_info;

	port_info.port_name = strdup(remote_port);
	port_info.is_trusted = is_trusted;
	cb_list = g_list_find_custom(remote_app_info->port_list, &port_info,
					(GCompareFunc)__remote_port_compare_cb);
	if (port_info.port_name)
		free(port_info.port_name);
	if (cb_list == NULL) {
		port_list_info_s *tmp = __set_remote_port_info(remote_app_id, remote_port, is_trusted);

		if (tmp == NULL) {
			ret_val = MESSAGEPORT_ERROR_OUT_OF_MEMORY;
			goto out;
		}
		remote_app_info->port_list = g_list_append(remote_app_info->port_list, tmp);
		*pli = tmp;
		g_hash_table_insert(__remote_port_info, (*pli)->encoded_bus_name, *pli);
	} else {
		*pli = (port_list_info_s *)cb_list->data;
	}

out:
	if (bus_name)
		FREE_AND_NULL(bus_name);


	return ret_val;
}

static bool __is_local_port_registed(const char *local_port, bool trusted, int *local_id, message_port_local_port_info_s **lpi)
{
	GHashTableIter iter;
	gpointer key, value;

	g_hash_table_iter_init(&iter, __local_port_info);

	while (g_hash_table_iter_next(&iter, &key, &value)) {
		message_port_local_port_info_s *mi = (message_port_local_port_info_s *)value;

		if ((mi->is_trusted == trusted) && strcmp(mi->port_name, local_port) == 0) {
			*local_id = mi->local_id;
			if (lpi != NULL) {
				*lpi = mi;
			}
			return true;
		}
	}
	return false;
}

static int __get_sender_pid(GDBusConnection *conn, const char *sender_name)
{
	GDBusMessage *msg = NULL;
	GDBusMessage *reply = NULL;
	GError *err = NULL;
	GVariant *body;
	int pid = 0;

	msg = g_dbus_message_new_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus",
			"org.freedesktop.DBus", "GetConnectionUnixProcessID");
	if (!msg) {
		_LOGE("Can't allocate new method call");
		goto out;
	}

	g_dbus_message_set_body (msg, g_variant_new ("(s)", sender_name));
	reply = g_dbus_connection_send_message_with_reply_sync(conn, msg,
							G_DBUS_SEND_MESSAGE_FLAGS_NONE, -1, NULL, NULL, &err);

	if (!reply) {
		if (err != NULL) {
			_LOGE("Failed to get pid [%s]", err->message);
			g_error_free(err);
		}
		goto out;
	}

	body = g_dbus_message_get_body(reply);
	g_variant_get(body, "(u)", &pid);

out:
	if (msg)
		g_object_unref(msg);
	if (reply)
		g_object_unref(reply);

	return pid;
}


static bool send_message(GVariant *parameters)
{
	char *local_port = NULL;
	char *local_appid = NULL;
	char *remote_appid = NULL;
	char *remote_port = NULL;
	gboolean local_trusted = false;
	gboolean remote_trusted = false;
	gboolean bi_dir = false;
	int len = 0;

	bundle *data = NULL;
	bundle_raw *raw = NULL;
	message_port_local_port_info_s *mi;
	int local_reg_id = 0;

	g_variant_get(parameters, "(ssbbssbus)", &local_appid, &local_port, &local_trusted, &bi_dir,
			&remote_appid, &remote_port, &remote_trusted, &len, &raw);

	if (!remote_port) {
		_LOGE("Invalid argument : remote_port is NULL");
		goto out;
	}
	if (!remote_appid) {
		_LOGE("Invalid argument : remote_appid is NULL");
		goto out;
	}
	if (!__is_local_port_registed(remote_port, remote_trusted, &local_reg_id, &mi)) {
		_LOGE("Invalid argument : remote_port:(%s) trusted(%d)", remote_port, remote_trusted);
		goto out;
	}
	if (!local_appid) {
		_LOGE("Invalid argument : local_appid");
		goto out;
	}
	if (!local_port) {
		_LOGE("Invalid argument : local_port");
		goto out;
	}
	if (strcmp(remote_appid, __app_id) != 0) {
		_LOGE("Invalid argument : remote_appid (%s)", remote_appid);
		goto out;
	}
	if (strcmp(remote_port, mi->port_name) != 0) {
		_LOGE("Invalid argument : remote_port (%s)", remote_port);
		goto out;
	}
	if (!len) {
		_LOGE("Invalid argument : data_len");
		goto out;
	}
	if (remote_trusted) {
		if (g_hash_table_lookup(__trusted_app_list_hash, (gpointer)local_appid) == NULL) {
			if (!__is_preloaded(local_appid, remote_appid)) {
				// Check the certificate
				int ret = __check_certificate(local_appid, remote_appid);
				if (ret == MESSAGEPORT_ERROR_NONE) {
					g_hash_table_insert(__trusted_app_list_hash, local_appid, "TRUE");
				} else {
					_LOGE("The application (%s) is not signed with the same certificate",
							local_appid);
					goto out;
				}
			}
		}
	}

	data = bundle_decode(raw, len);
	bundle_free_encoded_rawdata(&raw);

	if (!data) {
		_LOGE("Invalid argument : message");
		goto out;
	}

	if (bi_dir) {
		mi->callback(mi->local_id, local_appid, local_port, local_trusted, data);
	} else {
		mi->callback(mi->local_id, local_appid, NULL, false, data);
	}
out:

	return true;
}

static int __check_remote_port(const char *remote_app_id, const char *remote_port, bool is_trusted, bool *exist)
{
	_LOGI("Check a remote port : [%s:%s]", remote_app_id, remote_port);

	GVariant *result = NULL;
	GError *err = NULL;
	int ret_val = MESSAGEPORT_ERROR_NONE;
	char *bus_name = NULL;
	message_port_remote_app_info_s *remote_app_info = NULL;
	port_list_info_s *port_info = NULL;
	int local_reg_id = 0;
	message_port_local_port_info_s *mi = NULL;
	gboolean name_exist = false;

	_LOGI("remote_app_id, app_id :[%s : %s] ", remote_app_id, __app_id);

	ret_val = __get_remote_port_info(remote_app_id, remote_port, is_trusted, &remote_app_info, &port_info);
	if (ret_val != MESSAGEPORT_ERROR_NONE) {
		return ret_val;
	}

	// self check
	if (strcmp(remote_app_id, __app_id) == 0) {

		_LOGI("__is_local_port_registed ");
		if (!__is_local_port_registed(remote_port, is_trusted, &local_reg_id, &mi)) {
			*exist = false;
		} else {
			*exist = true;
		}
		_LOGI("__is_local_port_registed : %d ", *exist);
		return MESSAGEPORT_ERROR_NONE;
	}

	port_info->exist = false;
	bus_name = port_info->encoded_bus_name;

	result = g_dbus_connection_call_sync(
			__gdbus_conn,
			DBUS_SERVICE_DBUS,
			DBUS_PATH_DBUS,
			DBUS_INTERFACE_DBUS,
			"NameHasOwner",
			g_variant_new("(s)", bus_name),
			G_VARIANT_TYPE("(b)"),
			G_DBUS_CALL_FLAGS_NONE,
			-1,
			NULL,
			&err);

	if (err || (result == NULL)) {
		if (err) {
			_LOGE("No reply. error = %s", err->message);
			g_error_free(err);
		}
		ret_val = MESSAGEPORT_ERROR_RESOURCE_UNAVAILABLE;
	} else {
		g_variant_get(result, "(b)", &name_exist);

		if (!name_exist) {
			LOGE("Name not exist %s", bus_name);
			*exist = false;
			ret_val = MESSAGEPORT_ERROR_NONE;
		}
		else {

			if (is_trusted) {
				if (remote_app_info->certificate_info != CERTIFICATE_MATCH) {
					if (!__is_preloaded(__app_id, remote_app_id)) {
						if (__check_certificate(__app_id, remote_app_id) != MESSAGEPORT_ERROR_NONE) {
							ret_val = MESSAGEPORT_ERROR_CERTIFICATE_NOT_MATCH;
							goto out;
						}
					}
					remote_app_info->certificate_info = CERTIFICATE_MATCH;
				}
			}

			port_info->watcher_id = g_bus_watch_name_on_connection(
					__gdbus_conn,
					port_info->encoded_bus_name,
					G_BUS_NAME_WATCHER_FLAGS_NONE,
					on_name_appeared,
					on_name_vanished,
					port_info,
					NULL);

			port_info->exist = true;
			*exist = true;
			ret_val = MESSAGEPORT_ERROR_NONE;
			_LOGI("Exist port: %s", bus_name);
		}

	}
out:
	if (result)
		g_variant_unref(result);

	return ret_val;
}

static bool __check_sender_validation(GVariant *parameters, const char *sender, GDBusConnection *conn)
{
	int ret = 0;
	char buffer[MAX_PACKAGE_STR_SIZE] = {0, };
	char *local_appid = NULL;
	int pid = __get_sender_pid(conn, sender);

	ret = aul_app_get_appid_bypid(pid, buffer, sizeof(buffer));
	retvm_if(ret != AUL_R_OK, false, "Failed to get the sender ID: (%s) (%d)", sender, pid);

	g_variant_get_child(parameters, 0, "s", &local_appid);
	retvm_if(!local_appid, false, "remote_appid is NULL (%s) (%d)", sender, pid);

	if (strncmp(buffer, local_appid, MAX_PACKAGE_STR_SIZE) == 0) {
		g_hash_table_insert(__sender_appid_hash, strdup(sender), GINT_TO_POINTER(pid));
		g_free(local_appid);
	} else {
		g_free(local_appid);
		return false;
	}
	return true;
}

static void __dbus_method_call_handler(GDBusConnection *conn,
				const gchar *sender, const gchar *object_path,
				const gchar *iface_name, const gchar *method_name,
				GVariant *parameters, GDBusMethodInvocation *invocation,
				gpointer user_data)
{
	_LOGI("method_name: %s", method_name);
	 gpointer sender_pid = g_hash_table_lookup(__sender_appid_hash, sender);
	if (sender_pid == NULL) {
		if (!__check_sender_validation(parameters, sender, conn))
			return;
	}

	if (g_strcmp0(method_name, "send_message") == 0) {
		send_message(parameters);
	}

}

static const GDBusInterfaceVTable interface_vtable = {
	__dbus_method_call_handler,
	NULL,
	NULL
};

static int __dbus_init(void)
{
	bool ret = false;
	GError *error = NULL;

	__gdbus_conn = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
	if (__gdbus_conn == NULL) {
		if (error != NULL) {
			_LOGE("Failed to get dbus [%s]", error->message);
			g_error_free(error);
		}
		goto out;
	}

	ret = true;

out:
	if (!__gdbus_conn)
		g_object_unref(__gdbus_conn);

	return ret;

}


int __register_dbus_interface(const char *port_name, bool is_trusted) {

	GDBusNodeInfo *introspection_data = NULL;
	int registration_id = 0;

	static gchar introspection_prefix[] =
		"<node>"
		"  <interface name='";

	static gchar introspection_postfix[] =
		"'>"
		"	 <method name='send_message'>"
		"	   <arg type='s' name='local_appid' direction='in'/>"
		"	   <arg type='s' name='local_port' direction='in'/>"
		"	   <arg type='b' name='local_trusted' direction='in'/>"
		"	   <arg type='b' name='bi_dir' direction='in'/>"
		"	   <arg type='s' name='remote_appid' direction='in'/>"
		"	   <arg type='s' name='remote_port' direction='in'/>"
		"	   <arg type='b' name='remote_trusted' direction='in'/>"
		"	   <arg type='u' name='data_len' direction='in'/>"
		"	   <arg type='s' name='data' direction='in'/>"
		"	 </method>"
		"  </interface>"
		"</node>";

	char *introspection_xml = NULL;
	int introspection_xml_len = 0;


	int owner_id = 0;
	GError *error = NULL;
	char *bus_name = NULL;
	char *interface_name = NULL;
	GVariant *result = NULL;

	bus_name = __get_encoded_name(__app_id, port_name, is_trusted);
	if (!bus_name) {
		_LOGE("Fail to get bus name");
		goto out;
	}
	interface_name = bus_name;

	introspection_xml_len = strlen(introspection_prefix) + strlen(interface_name) +
		strlen(introspection_postfix) + 1;

	introspection_xml = (char *)calloc(introspection_xml_len, sizeof(char));
	if (!introspection_xml) {
		_LOGE("out of memory");
		goto out;
	}


	result = g_dbus_connection_call_sync(
			__gdbus_conn,
			DBUS_SERVICE_DBUS,
			DBUS_PATH_DBUS,
			DBUS_INTERFACE_DBUS,
			"RequestName",
			g_variant_new("(su)", bus_name, G_BUS_NAME_OWNER_FLAGS_NONE),
			G_VARIANT_TYPE("(u)"),
			G_DBUS_CALL_FLAGS_NONE,
			-1,
			NULL,
			&error);
	if (error) {
		_LOGE("RequestName fail : %s", error->message);
		goto out;
	}
	if (result == NULL) {
		_LOGE("fail to get name NULL");
		goto out;
	}
	g_variant_get(result, "(u)", &owner_id);
	if (owner_id == 0) {
		_LOGE("Acquiring the own name is failed");
		goto out;
	}

	_LOGI("Acquiring the own name : %d", owner_id);

	snprintf(introspection_xml, introspection_xml_len, "%s%s%s", introspection_prefix, interface_name, introspection_postfix);

	introspection_data = g_dbus_node_info_new_for_xml(introspection_xml, NULL);
	if (!introspection_data) {
		_LOGE("g_dbus_node_info_new_for_xml() is failed.");
		goto out;
	}

	registration_id = g_dbus_connection_register_object(__gdbus_conn,
						MESSAGEPORT_OBJECT_PATH, introspection_data->interfaces[0],
						&interface_vtable, NULL, NULL, NULL);

	_LOGI("registration_id %d", registration_id);

	if (registration_id == 0) {
		_LOGE("Failed to g_dbus_connection_register_object");
		goto out;
	}

out:
	if (introspection_data)
		g_dbus_node_info_unref(introspection_data);
	if (introspection_xml)
		free(introspection_xml);
	if (bus_name)
		free(bus_name);
	if (result)
		g_variant_unref(result);


	return registration_id;
}


void __list_free_port_list(gpointer data)
{
	port_list_info_s *n = (port_list_info_s *)data;

	FREE_AND_NULL(n->encoded_bus_name);
	FREE_AND_NULL(n->port_name);
	FREE_AND_NULL(n);
}

static void __hash_destory_local_value(gpointer data)
{
	message_port_local_port_info_s *mli = (message_port_local_port_info_s *)data;
	if (mli->port_name)
		free(mli->port_name);
}
static void __hash_destory_remote_value(gpointer data)
{
	message_port_remote_app_info_s *mri = (message_port_remote_app_info_s *)data;

	if (mri) {
		FREE_AND_NULL(mri->sender_id);
		FREE_AND_NULL(mri->remote_app_id);
		if (mri->port_list) {
			g_list_free_full(mri->port_list, __list_free_port_list);
		}
	}
}

static bool __initialize(void)
{

#if !GLIB_CHECK_VERSION(2, 35, 0)
	g_type_init();
#endif

	int pid = getpid();
	int ret = 0;
	char buffer[MAX_PACKAGE_STR_SIZE] = {0, };

	_LOGI("initialize");
	ret = aul_app_get_appid_bypid(pid, buffer, sizeof(buffer));
	retvm_if(ret != AUL_R_OK, false, "Failed to get the application ID: %d", ret);

	__app_id = strdup(buffer);
	retvm_if(!__app_id, false, "Malloc failed");
	_LOGI("init : %s", __app_id);

	if (__local_port_info == NULL) {
		__local_port_info = g_hash_table_new_full(g_direct_hash,  g_direct_equal, NULL, __hash_destory_local_value);
		retvm_if(!__local_port_info, false, "fail to create __local_port_info");
	}

	if (__remote_port_info == NULL) {
		__remote_port_info = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, __hash_destory_remote_value);
		retvm_if(!__remote_port_info, false, "fail to create __remote_port_info");
	}

	if (__sender_appid_hash == NULL) {
		__sender_appid_hash = g_hash_table_new(g_str_hash, g_str_equal);
		retvm_if(!__sender_appid_hash, false, "fail to create __sender_appid_hash");
	}

	if (__trusted_app_list_hash == NULL)
		__trusted_app_list_hash = g_hash_table_new(g_str_hash, g_str_equal);

	if (!__dbus_init()) {
		return false;
	}
	_initialized = true;

	return true;
}


static bool __message_port_register_port(const int local_id, const char *local_port, bool is_trusted, messageport_message_cb callback)
{
	message_port_local_port_info_s *mi = (message_port_local_port_info_s *)calloc(1, sizeof(message_port_local_port_info_s));
	retvm_if(!mi, false, "Malloc failed");

	mi->callback = callback;
	mi->is_trusted = is_trusted;
	mi->port_name = strdup(local_port);
	if (mi->port_name == NULL) {
		_LOGE("Malloc failed (%s)", local_port);
		free(mi);
		return false;
	}
	mi->local_id = local_id;

	g_hash_table_insert(__local_port_info, GINT_TO_POINTER(mi->local_id), mi);
	return true;
}

static int __register_message_port(const char *local_port, bool is_trusted, messageport_message_cb callback)
{
	_SECURE_LOGI("Register a message port : [%s:%s]", __app_id, local_port);

	int local_id = 0;

	// Check the message port is already registed
	if (__is_local_port_registed(local_port, is_trusted, &local_id, NULL)) {
		return local_id;
	}

	local_id = __register_dbus_interface(local_port, is_trusted);
	if (local_id < 1) {
		_LOGE("register_dbus_interface fail !!");
		return MESSAGEPORT_ERROR_OUT_OF_MEMORY;
	}

	if (!__message_port_register_port(local_id, local_port, is_trusted, callback)) {
		return MESSAGEPORT_ERROR_OUT_OF_MEMORY;
	}

	return local_id;
}

static int __message_port_send_message(const char *remote_appid, const char *remote_port,
		const char *local_port, bool trusted_message, bool local_trusted, bool bi_dir, bundle *message)
{
	int ret = MESSAGEPORT_ERROR_NONE;
	int len = 0;
	bundle_raw *raw = NULL;
	char *bus_name = NULL;
	char *interface_name = NULL;

	message_port_remote_app_info_s *remote_app_info = NULL;
	port_list_info_s *port_info = NULL;
	GDBusMessage *msg = NULL;
	GError *err = NULL;
	GVariant *body = NULL;

	ret = __get_remote_port_info(remote_appid, remote_port, trusted_message, &remote_app_info, &port_info);
	if (ret != MESSAGEPORT_ERROR_NONE) {
		return ret;
	}

	if (port_info->exist == false) {
		bool exist = false;
		_LOGI("port exist check !!");
		ret =  __check_remote_port(remote_appid, remote_port, trusted_message, &exist);
		if (ret != MESSAGEPORT_ERROR_NONE) {
			goto out;
		} else if (!exist) {
			ret = MESSAGEPORT_ERROR_MESSAGEPORT_NOT_FOUND;
			goto out;
		}
	}

	bus_name = port_info->encoded_bus_name;
	interface_name = bus_name;

	if (bundle_encode(message, &raw, &len) != BUNDLE_ERROR_NONE) {
		ret = MESSAGEPORT_ERROR_INVALID_PARAMETER;
		goto out;
	}

	if (MAX_MESSAGE_SIZE < len) {
		_LOGE("The size of message (%d) has exceeded the maximum limit.", len);
		ret = MESSAGEPORT_ERROR_MAX_EXCEEDED;
	}

	body = g_variant_new("(ssbbssbus)", __app_id, (local_port) ? local_port : "", local_trusted, bi_dir,
			  remote_appid, remote_port, trusted_message, len, raw);

	msg = g_dbus_message_new_method_call(bus_name, MESSAGEPORT_OBJECT_PATH, interface_name, "send_message");
	if (!msg) {
		_LOGE("Can't allocate new method call");
		ret = MESSAGEPORT_ERROR_OUT_OF_MEMORY;
		goto out;
	}

	g_dbus_message_set_body(msg, body);
	g_dbus_message_set_flags(msg, G_DBUS_MESSAGE_FLAGS_NO_REPLY_EXPECTED);
	g_dbus_connection_send_message(__gdbus_conn, msg, G_DBUS_SEND_MESSAGE_FLAGS_NONE, NULL, &err);
	if (err != NULL) {
		_LOGE("No reply. error = %s", err->message);
		g_error_free(err);
		ret = MESSAGEPORT_ERROR_IO_ERROR;
		goto out;
	}

	out:
	if (msg)
		g_object_unref(msg);
	if (raw)
		bundle_free_encoded_rawdata(&raw);

	return ret;
}

int __message_send_bidirectional_message(int id, const char *remote_app_id, const char *remote_port,  bool trusted_message, bundle *message)
{
	message_port_local_port_info_s *local_info;
	int ret = __get_local_port_info(id, &local_info);
	if (ret != MESSAGEPORT_ERROR_NONE) {
		return ret;
	}

	_LOGI("bidirectional_message %s", local_info->port_name);
	return __message_port_send_message(remote_app_id, remote_port,
			local_info->port_name, trusted_message, local_info->is_trusted, true, message);
}

int messageport_unregister_local_port(int local_port_id, bool trusted_port)
{

	GVariant *result;
	char *bus_name = NULL;
	GError *err = NULL;
	int ret = 0;

	_LOGI("unregister : %d", local_port_id);

	message_port_local_port_info_s *mi =
		(message_port_local_port_info_s *)
		g_hash_table_lookup(__local_port_info, GINT_TO_POINTER(local_port_id));
	if (mi == NULL)
		return MESSAGEPORT_ERROR_MESSAGEPORT_NOT_FOUND;

	if (mi->is_trusted != trusted_port)
		return MESSAGEPORT_ERROR_INVALID_PARAMETER;

	bus_name = __get_encoded_name(__app_id, mi->port_name, mi->is_trusted);
	if (bus_name == NULL)
		return MESSAGEPORT_ERROR_OUT_OF_MEMORY;

	g_dbus_connection_unregister_object(__gdbus_conn, local_port_id);

	result = g_dbus_connection_call_sync(
			__gdbus_conn,
			DBUS_SERVICE_DBUS,
			DBUS_PATH_DBUS,
			DBUS_INTERFACE_DBUS,
			"ReleaseName",
			g_variant_new("(s)", bus_name),
			G_VARIANT_TYPE("(u)"),
			G_DBUS_CALL_FLAGS_NONE,
			-1,
			NULL,
			&err);

	if (bus_name)
		free(bus_name);

	if (err) {
		_LOGE("RequestName fail : %s", err->message);
		g_error_free(err);
		return MESSAGEPORT_ERROR_MESSAGEPORT_NOT_FOUND;
	}
	g_variant_get(result, "(u)", &ret);

	if (result)
		g_variant_unref(result);

	if (ret != DBUS_RELEASE_NAME_REPLY_RELEASED) {

		if (ret == DBUS_RELEASE_NAME_REPLY_NON_EXISTENT) {
			_LOGE("Port Not exist");
			return MESSAGEPORT_ERROR_MESSAGEPORT_NOT_FOUND;
		} else if (ret == DBUS_RELEASE_NAME_REPLY_NOT_OWNER) {
			_LOGE("Try to release not owned name. MESSAGEPORT_ERROR_INVALID_PARAMETER");
			return MESSAGEPORT_ERROR_INVALID_PARAMETER;
		}
	}


	g_hash_table_remove(__local_port_info, GINT_TO_POINTER(local_port_id));

	return MESSAGEPORT_ERROR_NONE;
}

int messageport_register_local_port(const char *local_port, messageport_message_cb callback)
{
	if (!_initialized) {
		if (!__initialize())
			return MESSAGEPORT_ERROR_IO_ERROR;
	}

	return __register_message_port(local_port, false, callback);
}

int messageport_register_trusted_local_port(const char *local_port, messageport_message_cb callback)
{
	if (!_initialized) {
		if (!__initialize())
			return MESSAGEPORT_ERROR_IO_ERROR;
	}

	return __register_message_port(local_port, true, callback);

}

int messageport_check_remote_port(const char *remote_app_id, const char *remote_port, bool *exist)
{
	if (!_initialized) {
		if (!__initialize())
			return MESSAGEPORT_ERROR_IO_ERROR;
	}

	int ret = __check_remote_port(remote_app_id, remote_port, false, exist);
	if (ret == MESSAGEPORT_ERROR_MESSAGEPORT_NOT_FOUND) {
		*exist = false;
		ret = MESSAGEPORT_ERROR_NONE;
	}

	return ret;
}

int messageport_check_trusted_remote_port(const char *remote_app_id, const char *remote_port, bool *exist)
{
	if (!_initialized) {
		if (!__initialize())
			return MESSAGEPORT_ERROR_IO_ERROR;
	}

	int ret = __check_remote_port(remote_app_id, remote_port, true, exist);
	if (ret == MESSAGEPORT_ERROR_MESSAGEPORT_NOT_FOUND) {
		*exist = false;
		ret = MESSAGEPORT_ERROR_NONE;
	}

	return ret;
}

int messageport_send_message(const char *remote_app_id, const char *remote_port, bundle *message)
{
	if (!_initialized) {
		if (!__initialize())
			return MESSAGEPORT_ERROR_IO_ERROR;
	}

	return __message_port_send_message(remote_app_id, remote_port, NULL, false, false, false, message);
}

int messageport_send_trusted_message(const char *remote_app_id, const char *remote_port, bundle *message)
{
	if (!_initialized) {
		if (!__initialize())
			return MESSAGEPORT_ERROR_IO_ERROR;
	}

	return __message_port_send_message(remote_app_id, remote_port, NULL, true, false, false, message);
}

int messageport_send_bidirectional_message(int id, const char *remote_app_id, const char *remote_port,
		bundle *message)
{
	if (!_initialized) {
		if (!__initialize())
			return MESSAGEPORT_ERROR_IO_ERROR;
	}

	return __message_send_bidirectional_message(id, remote_app_id, remote_port, false, message);
}

int messageport_send_bidirectional_trusted_message(int id, const char *remote_app_id, const char *remote_port,
		bundle *message)
{
	if (!_initialized) {
		if (!__initialize())
			return MESSAGEPORT_ERROR_IO_ERROR;
	}
	return __message_send_bidirectional_message(id, remote_app_id, remote_port, true, message);
}

int messageport_get_local_port_name(int id, char **name)
{
	message_port_local_port_info_s *local_info;
	int ret = __get_local_port_info(id, &local_info);

	if (ret != MESSAGEPORT_ERROR_NONE) {
		return ret;
	}

	*name = strdup(local_info->port_name);

	if (*name == NULL) {
		return MESSAGEPORT_ERROR_OUT_OF_MEMORY;
	}

	return MESSAGEPORT_ERROR_NONE;
}

int messageport_check_trusted_local_port(int id, bool *trusted)
{
	message_port_local_port_info_s *local_info;
	int ret = __get_local_port_info(id, &local_info);

	if (ret != MESSAGEPORT_ERROR_NONE) {
		return ret;
	}

	*trusted = local_info->is_trusted;

	return MESSAGEPORT_ERROR_NONE;;
}

