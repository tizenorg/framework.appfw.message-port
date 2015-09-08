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
 * @file  FIo_MessagePortIpcServer.cpp
 * @brief This is the implementation file for the _MessagePortIpcServer class.
 *
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <iostream>
#include <new>

#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <fcntl.h>

#include <sys/smack.h>

#include <aul/aul.h>

#include <ipc/ipc_message.h>

#include "message_port_error.h"
#include "message-port-log.h"

#include "IMessagePortIpcServerEventListener.h"
#include "MessagePortIpcServer.h"

using namespace std;
using namespace IPC;

static const char _SOCKET_DIR[] = "/run/messageportd/";

_MessagePortIpcServer::_ChannelInfo::_ChannelInfo(void)
	: pClientInfo(NULL)
	, pGIOChannel(NULL)
	, pGSource(NULL)
	, destroySource(true)
{

}

_MessagePortIpcServer::_ChannelInfo::~_ChannelInfo(void)
{
	if (pGIOChannel != NULL)
	{
		g_io_channel_unref(pGIOChannel);
	}

	if (pGSource != NULL)
	{
		if (destroySource)
		{
			g_source_destroy(pGSource);
		}

		g_source_unref(pGSource);
	}
}

_MessagePortIpcServer::_ClientInfo::_ClientInfo(void)
	: clientId(-1)
	, pIpcServer(NULL)
	, pReverseChannel(NULL)
{

}

_MessagePortIpcServer::_ClientInfo::~_ClientInfo(void)
{
	if (pReverseChannel != NULL)
	{
		g_io_channel_unref(pReverseChannel);
	}

	channels.clear();
}

_MessagePortIpcServer::_MessagePortIpcServer(void)
	: __runOnCallerThread(false)
	//, __pEventDispatcher(NULL)
	, __pListener(NULL)
	, __handlerThread(0)
	, __pHandlerGMainContext(NULL)
	, __pHandlerGMainLoop(NULL)
	, __pConnectGSource(NULL)
	, __pCurrentChannel(NULL)
	, __pCurrentClientInfo(NULL)
{
	__messageBuffer[0] = '\0';
}


_MessagePortIpcServer::~_MessagePortIpcServer(void)
{
	if (__name)
	{
		delete[] __name;
	}

	if (__pConnectGSource != NULL)
	{
		g_source_destroy(__pConnectGSource);
		g_source_unref(__pConnectGSource);
		__pConnectGSource = NULL;
	}

	if (!__runOnCallerThread)
	{
		/*if (__pEventDispatcher)
		{
			delete __pEventDispatcher;
			__pEventDispatcher = NULL;
		}*/

		if (__pHandlerGMainLoop)
		{
			g_main_loop_unref(__pHandlerGMainLoop);
			__pHandlerGMainLoop = NULL;
		}

		if (__pHandlerGMainContext)
		{
			g_main_context_unref(__pHandlerGMainContext);
			__pHandlerGMainContext = NULL;
		}
	}

	// clean-up clients
	__clients.clear();
}

static int __set_smack_label_as_floor(const char *path)
{
	if (smack_lsetlabel(path, "_", SMACK_LABEL_ACCESS) != 0)
	{
		if (errno != EOPNOTSUPP)
		{
			_LOGE("SMACK labeling failed.");
			return MESSAGEPORT_ERROR_IO_ERROR;
		}
	}

	return MESSAGEPORT_ERROR_NONE;
}

static int __set_fsmack_label_as_star(int fd)
{
	if (smack_fsetlabel(fd, "@", SMACK_LABEL_IPOUT) != 0)
	{
		if (errno != EOPNOTSUPP)
		{
			_LOGE("SMACK labeling failed.");
			return MESSAGEPORT_ERROR_IO_ERROR;
		}
	}

	if (smack_fsetlabel(fd, "*", SMACK_LABEL_IPIN) != 0)
	{
		if (errno != EOPNOTSUPP)
		{
			_LOGE("SMACK labeling failed.");
			return MESSAGEPORT_ERROR_IO_ERROR;
		}
	}

	return MESSAGEPORT_ERROR_NONE;
}

int
_MessagePortIpcServer::Construct(const string& name, const _IMessagePortIpcServerEventListener& listener, bool runOnCallerThread)
{
	_LOGI("_MessagePortIpcServer::Construct");

	int ret = MESSAGEPORT_ERROR_NONE;

	GIOChannel* pGIOChannel = NULL;
	GSource* pGSource = NULL;

	struct sockaddr_un serverAddress;
	int serverSocket = -1;
	int serverLen = 0;
	std::string socketName;
	size_t socketNameLength = 0;

	__name = new char[name.length() + 1]();
	if(__name == NULL)
	{
		return MESSAGEPORT_ERROR_OUT_OF_MEMORY;
	}
	strcpy(__name, name.c_str());

	__pListener = const_cast <_IMessagePortIpcServerEventListener*>(&listener);
	__runOnCallerThread = runOnCallerThread;

	socketName.append(_SOCKET_DIR);
	socketName.append(__name);

	socketNameLength = socketName.size() + 1;
	if (socketNameLength >= 108)
	{
		_LOGE("Server name is too long");
		return MESSAGEPORT_ERROR_INVALID_PARAMETER;
	}

	if (!__runOnCallerThread)
	{
		__pHandlerGMainContext = g_main_context_new();
		__pHandlerGMainLoop = g_main_loop_new(__pHandlerGMainContext, FALSE);

	}
	else
	{
		__pHandlerGMainContext = g_main_context_get_thread_default();
		if (__pHandlerGMainContext == NULL) // is global?
		{
			__pHandlerGMainContext = g_main_context_default();
			if (__pHandlerGMainContext == NULL)
			{
				return MESSAGEPORT_ERROR_IO_ERROR;
			}
		}
	}

	ret = mkdir(_SOCKET_DIR, 0755);
	if (ret < 0)
	{
		if (errno == EEXIST)
		{
			_LOGI("%s is already exist", _SOCKET_DIR);
			ret = unlink(socketName.c_str());
			if (ret < 0)
			{
				if (errno == ENOENT)
				{
					_LOGI("%s is not exist", socketName.c_str());
				}
				else
				{
					_LOGE("Failed to unlink %s. %s(%d)", socketName.c_str(), strerror(errno), errno);
					return MESSAGEPORT_ERROR_IO_ERROR;
				}
			}
		}
		else
		{
			_LOGE("Failed to make directory %s. %s(%d)", _SOCKET_DIR, strerror(errno), errno);
			return MESSAGEPORT_ERROR_IO_ERROR;
		}
	}

	// SMACK (Add a _ label to socket dir)
	ret = __set_smack_label_as_floor(_SOCKET_DIR);
	if (ret != MESSAGEPORT_ERROR_NONE)
	{
		return MESSAGEPORT_ERROR_IO_ERROR;
	}

	serverSocket = socket(AF_UNIX, SOCK_STREAM, 0);
	if (serverSocket == -1)
	{
		_LOGE("Failed to create a socket.");
		return MESSAGEPORT_ERROR_IO_ERROR;
	}

	// SMACK (Add a * label to socket)
	ret = __set_fsmack_label_as_star(serverSocket);
	if (ret != MESSAGEPORT_ERROR_NONE)
	{
		goto CATCH;
	}

	bzero(&serverAddress, sizeof(serverAddress));
	serverAddress.sun_family = AF_UNIX;
	strncpy(serverAddress.sun_path, socketName.c_str(), socketNameLength);
	serverLen = sizeof(serverAddress);

	ret = bind(serverSocket, (const struct sockaddr*) &serverAddress, serverLen);
	if (ret == -1)
	{
		_LOGE(" Failed to bind a socket(%d, %s): %s", serverSocket,
			   socketName.c_str(), strerror(errno));
		ret = MESSAGEPORT_ERROR_IO_ERROR;
		goto CATCH;
	}

	ret = chmod(socketName.c_str(), 0666);
	if (ret != 0)
	{
		_LOGE(" Failed to change permission of a socket(%d, %s): %s", serverSocket,
			   socketName.c_str(), strerror(errno));
		ret = MESSAGEPORT_ERROR_IO_ERROR;
		goto CATCH;
	}

	ret = listen(serverSocket, 128);
	if (ret != 0)
	{
		_LOGE(" Failed to listen a socket(%d, %s): %s", serverSocket, socketName.c_str(), strerror(errno));
		ret = MESSAGEPORT_ERROR_IO_ERROR;
		goto CATCH;
	}

	pGIOChannel = g_io_channel_unix_new(serverSocket);
	if (pGIOChannel == NULL)
	{
		_LOGE(" Not enough memory");
		ret = MESSAGEPORT_ERROR_IO_ERROR;
		goto CATCH;
	}

	// socket will be closed when pGIOChannel is deleted.
	g_io_channel_set_close_on_unref(pGIOChannel, TRUE);
	serverSocket = -1;

	pGSource = g_io_create_watch(pGIOChannel, (GIOCondition) (G_IO_IN | G_IO_ERR | G_IO_NVAL | G_IO_HUP));
	if (pGSource == NULL)
	{
		_LOGE(" Not enough memory : Failed to create watch for socket.");
		ret = MESSAGEPORT_ERROR_IO_ERROR;
		goto CATCH;
	}

	// channel will be delete when pGSource is deleted.
	g_io_channel_unref(pGIOChannel);
	pGIOChannel = NULL;

	g_source_set_callback(pGSource, (GSourceFunc) OnConnectionRequest, this, NULL);
	g_source_attach(pGSource, __pHandlerGMainContext);

	if (__runOnCallerThread)
	{
		__pListener->OnIpcServerStarted(*this);
	}
	else
	{
		ret = pthread_create(&__handlerThread, NULL, &ThreadProc, (void*) this);
		if (ret != 0)
		{
			_LOGE(" Failed to an IPC thread.");
			ret =  MESSAGEPORT_ERROR_IO_ERROR;
			goto CATCH;
		}
	}

	__pConnectGSource = pGSource;

	return ret;

CATCH:
	if (pGIOChannel != NULL)
	{
		g_io_channel_unref(pGIOChannel);
	}

	if (serverSocket != -1)
	{
		close(serverSocket);
	}

	if (runOnCallerThread && __pHandlerGMainContext)
	{
		g_main_context_unref(__pHandlerGMainContext);
		__pHandlerGMainContext = NULL;
	}

	return ret;
}

struct HelloMessage
{
	int reverse;  // if the connection is for reverse message
};

gboolean
_MessagePortIpcServer::OnConnectionRequest(GIOChannel* source, GIOCondition condition, gpointer data)
{
	_LOGI("_MessagePortIpcServer::OnConnectionRequest");

	_MessagePortIpcServer* pIpcServer = (_MessagePortIpcServer*) data;
	GError* pGError = NULL;
	HelloMessage helloMessage;
	_ClientInfo* pClientInfo = NULL;
	_ChannelInfo* pChannelInfo = NULL;
	GSource* pGSource = NULL;
	GIOChannel* pChannel = NULL;
	ssize_t readBytes = 0;
	int ret = 0;

	int server = -1;
	int client = -1;
	struct sockaddr_un clientAddress;
	socklen_t clientLen = sizeof(clientAddress);

	struct ucred cr;
	socklen_t ucredLen = sizeof(struct ucred);

	assert(pIpcServer);
	assert(pIpcServer->__pListener);

	server = g_io_channel_unix_get_fd(source);

	client = accept(server, (struct sockaddr*) &clientAddress, &clientLen);
	if (client == -1)
	{
		_LOGE("Accept failed");
		ret = MESSAGEPORT_ERROR_IO_ERROR;
		goto CATCH;
	}

	readBytes = read(client, &helloMessage, sizeof(helloMessage));
	if (readBytes < 0)
	{
		_LOGE("Failed to receive hello message (%d, %s).", errno, strerror(errno));
		ret = MESSAGEPORT_ERROR_IO_ERROR;
		goto CATCH;
	}

	pChannel = g_io_channel_unix_new(client);
	if (pChannel == NULL)
	{
		_LOGE("Not enough memory.");
		ret = MESSAGEPORT_ERROR_IO_ERROR;
		goto CATCH;
	}

	g_io_channel_set_encoding(pChannel, NULL, &pGError);
	g_io_channel_set_flags(pChannel, G_IO_FLAG_NONBLOCK, &pGError);

	g_io_channel_set_close_on_unref(pChannel, TRUE);

	ret = getsockopt(client, SOL_SOCKET, SO_PEERCRED, &cr, &ucredLen);
	if (ret < 0)
	{
		_LOGE(" Failed to get peer cred information: %s", strerror(errno));
		ret = MESSAGEPORT_ERROR_IO_ERROR;
		goto CATCH;
	}

	client = -1;

	pClientInfo = pIpcServer->__clients[cr.pid];

	if (pClientInfo == NULL) // first connection request from this client
	{
		pClientInfo = new (std::nothrow) _ClientInfo;
		if (pClientInfo ==  NULL)
		{
			_LOGE(" Not Enough Memory");
			ret = MESSAGEPORT_ERROR_OUT_OF_MEMORY;
			goto CATCH;
		}

		pClientInfo->pIpcServer = pIpcServer;
		pClientInfo->clientId = cr.pid;

		char buffer[256] = {0, };
		ret = aul_app_get_appid_bypid(cr.pid, buffer, sizeof(buffer));
		if (ret != AUL_R_OK)
		{
			delete pClientInfo;
			_LOGE("Failed to get the application ID of pid: %d", ret);
			ret = MESSAGEPORT_ERROR_IO_ERROR;
			goto CATCH;
		}

		pClientInfo->appId = buffer;

		pClientInfo->pReverseChannel = NULL;

		pIpcServer->__clients[cr.pid] = pClientInfo;
		pIpcServer->__pCurrentClientInfo = pClientInfo;
		pIpcServer->__pListener->OnIpcClientConnected(*pIpcServer, cr.pid);
		pIpcServer->__pCurrentClientInfo = NULL;
	}

	if (helloMessage.reverse != 0)
	{
		pClientInfo->pReverseChannel = pChannel;
	}
	else
	{
		pChannelInfo = new (std::nothrow) _ChannelInfo;
		if (pChannelInfo == NULL)
		{
			_LOGE("Not Enough Memory");
			ret = MESSAGEPORT_ERROR_OUT_OF_MEMORY;
			goto CATCH;
		}

		pGSource = g_io_create_watch(pChannel, (GIOCondition) (G_IO_IN | G_IO_ERR | G_IO_NVAL | G_IO_HUP));
		g_source_set_callback(pGSource, (GSourceFunc) OnReadMessage, pChannelInfo, NULL);
		g_source_attach(pGSource, pIpcServer->__pHandlerGMainContext);

		pChannelInfo->pClientInfo = pClientInfo;
		pChannelInfo->pGIOChannel = pChannel;
		pChannelInfo->pGSource = pGSource;

		pClientInfo->channels.push_back(pChannelInfo);
	}

	return true;

CATCH:
	if (pChannel != NULL)
	{
		g_io_channel_unref(pChannel);
	}

	if (client != -1)
	{
		close(client);
	}

	return true;
}

int
_MessagePortIpcServer::GetClientId(void) const
{
	if (__pCurrentClientInfo)
	{
		return __pCurrentClientInfo->clientId;
	}

	return -1;
}

char*
_MessagePortIpcServer::GetClientApplicationId(void) const
{
	static char *nullString = NULL;

	if (__pCurrentClientInfo)
	{
		return __pCurrentClientInfo->appId;
	}

	return nullString;
}

gboolean
_MessagePortIpcServer::HandleReceivedMessage(GIOChannel* source, GIOCondition condition, gpointer data)
{
	_LOGI("_MessagePortIpcServer::HandleReceivedMessage");

	GError* pGError = NULL;
	GIOStatus status;
	IPC::Message* pMessage = NULL;
	_ChannelInfo* pChannelInfo = (_ChannelInfo*) data;
	_ClientInfo* pClientInfo = pChannelInfo->pClientInfo;

	if (condition & G_IO_HUP)
	{
		_LOGE("Connection closed");
		int clientId = pClientInfo->clientId;

		g_io_channel_shutdown(source, FALSE, &pGError);

		for (unsigned int i = 0; i < pClientInfo->channels.size(); i++)
		{
			if (pChannelInfo == pClientInfo->channels[i])
			{
				pClientInfo->channels.erase(pClientInfo->channels.begin() + i);

				// Do not destroy a source in a dispatch callback
				// because main loop will do it if the callback return FALSE.
				pChannelInfo->destroySource = false;
				delete pChannelInfo;

				break;
			}
		}

		if (pClientInfo->channels.size() == 0)
		{
			_LOGE("All connections of client(%d) are closed. delete client info", clientId);

			__pListener->OnIpcClientDisconnected(*this, clientId);

			__clients[clientId] = NULL;

			delete pClientInfo;
		}

		return FALSE;
	}
	else if (condition & G_IO_IN)
	{
		gsize readSize = 0;
		const char* pStart = NULL;
		const char* pEnd = NULL;
		const char* pEndOfMessage = NULL;

		while (true)
		{
			pGError = NULL;
			status = g_io_channel_read_chars(source, (char*) __messageBuffer, __MAX_MESSAGE_BUFFER_SIZE, &readSize, &pGError);
			if (status != G_IO_STATUS_NORMAL)
			{
				if (status == G_IO_STATUS_EOF || status == G_IO_STATUS_ERROR)
				{
					if (status == G_IO_STATUS_EOF)
					{
						_LOGE("G_IO_STATUS_EOF, the connection is closed.");
					}
					else
					{
						_LOGE("G_IO_STATUS_ERROR, the connection is closed. ");
					}

					pGError = NULL;
					g_io_channel_shutdown(source, FALSE, &pGError);

					int clientId = pClientInfo->clientId;

					for (unsigned int i = 0; i < pClientInfo->channels.size(); i++)
					{
						if (pChannelInfo == pClientInfo->channels[i])
						{
							pClientInfo->channels.erase(pClientInfo->channels.begin() + i);

							pChannelInfo->destroySource = false;
							delete pChannelInfo;
							break;
						}
					}

					if (pClientInfo->channels.size() == 0)
					{
						_LOGE("All connections of client(%d) are closed normally by the client.", clientId);

						if (__pListener)
						{
							__pListener->OnIpcClientDisconnected(*this, clientId);
						}

						__clients[clientId] = NULL;

						delete pClientInfo;
					}

					return FALSE;
				}
			}

			if (readSize == 0)
			{
				break;
			}

			if (__pending.empty())
			{
				pStart = __messageBuffer;
				pEnd = pStart + readSize;
			}
			else
			{
				__pending.append(__messageBuffer, readSize);
				pStart = __pending.data();
				pEnd = pStart + __pending.size();
			}

			while (true)
			{
				pEndOfMessage = IPC::Message::FindNext(pStart, pEnd);
				if (pEndOfMessage == NULL)
				{
					__pending.assign(pStart, pEnd - pStart);
					break;
				}

				pMessage = new (std::nothrow) IPC::Message(pStart, pEndOfMessage - pStart);
				if (pMessage == NULL)
				{
					_LOGE("Out of memory");
					return MESSAGEPORT_ERROR_OUT_OF_MEMORY;
				}

				__pCurrentChannel = source;

				if (__pListener)
				{
					__pListener->OnIpcRequestReceived(*this, *pMessage);
				}

				delete pMessage;

				__pCurrentChannel = NULL;

				pStart = pEndOfMessage;
			}
		}
	}
	else
	{
		// empty statement
	}

	return TRUE;
}

gboolean
_MessagePortIpcServer::OnReadMessage(GIOChannel* source, GIOCondition condition, gpointer data)
{
	_LOGI("_MessagePortIpcServer::OnReadMessage");

	gboolean ret = FALSE;
	_ChannelInfo* pChannelInfo = (_ChannelInfo*) data;
	_ClientInfo* pClientInfo = pChannelInfo->pClientInfo;
	_MessagePortIpcServer* pIpcServer = (_MessagePortIpcServer*) pClientInfo->pIpcServer;

	pIpcServer->__pCurrentClientInfo = pClientInfo;
	ret = pIpcServer->HandleReceivedMessage(source, condition, data);
	pIpcServer->__pCurrentClientInfo = NULL;

	return ret;
}

void*
_MessagePortIpcServer::ThreadProc(void* pParam)
{
	_MessagePortIpcServer* pIpcServer = (_MessagePortIpcServer*) pParam;
	if (pIpcServer != NULL)
	{
		pIpcServer->Run(NULL);
	}

	return NULL;
}

void
_MessagePortIpcServer::Run(void* pParam)
{
	//int r = 0;
	_LOGI("_MessagePortIpcServer::Run");

	if (__pListener == NULL)
	{
		return;
	}

	/*__pEventDispatcher = new (std::nothrow) _EventDispatcher;
	SysTryReturnVoidResult(NID_IO, __pEventDispatcher != NULL, E_OUT_OF_MEMORY, "[E_OUT_OF_MEMORY] The memory is insufficient.");

	r = __pEventDispatcher->Construct(__pHandlerGMainContext);
	if (IsFailed(r))
	{
		delete __pEventDispatcher;
		__pEventDispatcher = NULL;
	}
	*/ // Need to Check
	__pListener->OnIpcServerStarted(*this);

	g_main_loop_run(__pHandlerGMainLoop);

	__pListener->OnIpcServerStopped(*this);
}

int
_MessagePortIpcServer::Start(void)
{
	return MESSAGEPORT_ERROR_NONE;
}

char*
_MessagePortIpcServer::GetName(void) const
{
	return __name;
}

int
_MessagePortIpcServer::Stop(void)
{
	_LOGI("_MessagePortIpcServer::Stop");

	int r = MESSAGEPORT_ERROR_NONE;
	int ret = 0;

	if(__pListener == NULL)
	{
		return MESSAGEPORT_ERROR_IO_ERROR;
	}

	if (!__runOnCallerThread)
	{
		pthread_t self = pthread_self();

		if (__pHandlerGMainLoop)
		{
			g_main_loop_quit(__pHandlerGMainLoop);
		}

		if (__handlerThread != self)
		{
			ret = pthread_join(__handlerThread, NULL);
			if (ret != 0)
			{
				_LOGE("Join an IPC thread returns an error");
			}
		}
	}
	else
	{
		__pListener->OnIpcServerStopped(*this);
	}

	return r;
}

bool
_MessagePortIpcServer::Send(IPC::Message* msg)
{
	_LOGI("_MessagePortIpcServer::Stop");

	gsize remain = 0;
	gsize written = 0;
	char* pData = NULL;
	GError* pGError = NULL;


	pData = (char*) msg->data();
	remain = msg->size();

	if (msg->is_reply())
	{
		while (remain > 0)
		{
			pGError = NULL;
			g_io_channel_write_chars(__pCurrentChannel, (char*) pData, remain, &written, &pGError);

			remain -= written;
			pData += written;
		}

		g_io_channel_flush(__pCurrentChannel, &pGError);
	}
	else
	{
		// empty statement;
	}

	delete msg;

	return true;
}

int
_MessagePortIpcServer::SendResponse(int client, IPC::Message* pMessage)
{
	_LOGI("_MessagePortIpcServer::SendResponse");

	int r = MESSAGEPORT_ERROR_NONE;
	gsize remain = 0;
	gsize written = 0;
	char* pData = NULL;
	GError* pGError = NULL;
	_ClientInfo* pClientInfo = NULL;
	int ret = 0;

	if (client < 0 && pMessage == NULL)
	{
		_LOGE("pMessage(0x%x) is NULL or clinet(%d) < 0", pMessage, client);
		return MESSAGEPORT_ERROR_INVALID_PARAMETER;
	}

	if (pMessage->is_sync())
	{
		_LOGE(" Can't send sync. messagee.");
		return MESSAGEPORT_ERROR_INVALID_PARAMETER;
	}

	pClientInfo = __clients[client];
	if (pClientInfo == NULL)
	{
		_LOGE(" client(%d) has not been registered.", client);
		return MESSAGEPORT_ERROR_INVALID_PARAMETER;
	}

	pData = (char*) pMessage->data();
	remain = pMessage->size();

	while (remain > 0)
	{
		pGError = NULL;
		ret = g_io_channel_write_chars(pClientInfo->pReverseChannel, (char*) pData, remain, &written, &pGError);
		if (ret != G_IO_STATUS_NORMAL)
		{
			_LOGE("Failed to send a response: %d", ret);
			 if (ret == G_IO_STATUS_ERROR)
			 {
				 _LOGE("Error occurred during writing message to socket.");
				 r = MESSAGEPORT_ERROR_INVALID_PARAMETER;
				 goto CATCH;
			 }
		}

		remain -= written;
		pData += written;
	}

	g_io_channel_flush(pClientInfo->pReverseChannel, &pGError);

	delete pMessage;

	return r;

CATCH:
	delete pMessage;
	return r;
}

int
_MessagePortIpcServer::SendResponse(int client, const IPC::Message& message)
{
	_LOGI("_MessagePortIpcServer::SendResponse");

	int r = MESSAGEPORT_ERROR_NONE;
	gsize remain = 0;
	gsize written = 0;
	char* pData = NULL;
	GError* pGError = NULL;
	_ClientInfo* pClientInfo = NULL;
	int ret = 0;

	if (client < 0)
	{
		_LOGE("clinet(%d) < 0", client);
		return MESSAGEPORT_ERROR_INVALID_PARAMETER;
	}

	if (message.is_sync())
	{
		_LOGE(" Can't send sync. messagee.");
		return MESSAGEPORT_ERROR_INVALID_PARAMETER;
	}

	pClientInfo = __clients[client];
	if (pClientInfo == NULL)
	{
		_LOGE(" client(%d) has not been registered.", client);
		return MESSAGEPORT_ERROR_INVALID_PARAMETER;
	}

	pData = (char*) message.data();
	remain = message.size();

	while (remain > 0)
	{
		pGError = NULL;
		ret = g_io_channel_write_chars(pClientInfo->pReverseChannel, (char*) pData, remain, &written, &pGError);
		if (ret != G_IO_STATUS_NORMAL)
		{
			 if (ret == G_IO_STATUS_ERROR)
			 {
				 _LOGE("Error occurred during writing message to socket.");
				 r = MESSAGEPORT_ERROR_INVALID_PARAMETER;
				 goto CATCH;
			 }
		}

		remain -= written;
		pData += written;
	}

	g_io_channel_flush(pClientInfo->pReverseChannel, &pGError);

	return r;

CATCH:
	return r;
}
