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
 * @file	IpcClient.cpp
 * @brief	This is the implementation file for the IpcClient class.
 *
 */

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>
#include <pthread.h>
#include <fcntl.h>

#include <iostream>
#include <queue>
#include <map>

#include "message-port.h"
#include "message-port-log.h"

#include "IpcClient.h"
#include "IIpcClientEventListener.h"

using namespace IPC;
using namespace std;


IpcClient::IpcClient(void)
	: __pReverseSource(NULL)
	, __pMutex(NULL)
	, __pListener(NULL)
{
	__messageBuffer[0] = '\0';
}

IpcClient::~IpcClient(void)
{
	int fd = 0;

	if (__pReverseSource != NULL)
	{
		g_source_destroy(__pReverseSource);
		g_source_unref(__pReverseSource);
		__pReverseSource = NULL;
	}

	while (__fds.size() > 0)
	{
		fd = __fds.back();
		__fds.pop_back();

		close(fd);
	}

	pthread_mutex_destroy(__pMutex);
}

int
IpcClient::Construct(const string& serverName, const IIpcClientEventListener* pListener)
{
	__name = serverName;
	__pListener = const_cast <IIpcClientEventListener*>(pListener);

	pthread_mutex_t* pMutex = (pthread_mutex_t*) malloc(sizeof(pthread_mutex_t));
	if (pMutex == NULL)
	{
		return MESSAGEPORT_ERROR_OUT_OF_MEMORY;
	}

	pthread_mutex_init(pMutex, NULL);

	__pMutex = pMutex;

	int ret = MakeConnection();
	if (ret != 0)
	{
		return ret;
	}

	if (__pListener)
	{
		ret = MakeConnection(true);
		if (ret != 0)
		{
			return ret;
		}
	}

	return MESSAGEPORT_ERROR_NONE;

}

string
IpcClient::GetName(void) const
{
	return __name;
}

struct HelloMessage
{
	int reverse;
};

int
IpcClient::MakeConnection(bool forReverse)
{
	int ret = 0;
	int retry = 0;

	size_t socketNameLength = 0;
	string socketName;

	socketName.append("/var/run/osp/");
	socketName.append(__name);

	socketNameLength = socketName.size() + 1;

	HelloMessage helloMessage = {0};

	if (forReverse)
	{
		helloMessage.reverse = 1;
	}
	else
	{
		helloMessage.reverse = 0;
	}

	struct sockaddr_un server;

	bzero(&server, sizeof(server));
	server.sun_family = AF_UNIX;
	strncpy(server.sun_path, socketName.c_str(), socketNameLength);
	socklen_t serverLen = sizeof(server);

	int client = socket(AF_UNIX, SOCK_STREAM, 0);
	if (client < 0)
	{
		_LOGE("Failed to create a socket : %s.", strerror(errno));
		return MESSAGEPORT_ERROR_IO_ERROR;
	}

	int flags = fcntl(client, F_GETFL, 0);
	ret = fcntl(client, F_SETFL, flags | O_NONBLOCK);
	if (ret != 0)
	{
		_LOGE("Failed to set file status flags (%d, %s).", errno, strerror(errno));
		goto CATCH;
	}

	// Retry if the server is not ready
	retry = 5;
	while (retry > 0)
	{
		ret = connect(client, (struct sockaddr*) &server, serverLen);
		if (ret < 0 && errno == ENOENT)
		{
			_LOGI("The server is not ready. %d", retry);

			usleep(1000 * 1000);

			--retry;
		}
		else
		{
			break;
		}
	}

	if (ret < 0)
	{
		if (errno != EINPROGRESS)
		{
			_LOGE("Failed to connect to server(%s) : %d, %s", socketName.c_str(), errno, strerror(errno));
			goto CATCH;
		}

		fd_set rset;
		fd_set wset;
		struct timeval timeout;
		int length = 0;
		int error = 0;
		socklen_t socketLength = 0;

		FD_ZERO(&rset);
		FD_SET(client, &rset);
		wset = rset;
		timeout.tv_sec = 10;
		timeout.tv_usec = 0;

		while (true)
		{
			ret = select(client+1, &rset, &wset, NULL, &timeout);
			if (ret < 0)
			{
				_LOGE("Failed to connect due to system error : %s.", strerror(errno));
				if (errno != EINTR)
				{
					goto CATCH;
				}

				continue;
			}
			else if (ret == 0)
			{
				_LOGE("Failed to connect due to timeout.");
				goto CATCH;
			}
			else
			{
				break;
			}
		}

		if (FD_ISSET(client, &rset) || FD_ISSET(client, &wset))
		{
			length = sizeof(error);
			ret = getsockopt(client, SOL_SOCKET, SO_ERROR, &error, &socketLength);
			if (ret < 0)
			{
				_LOGE("Failed to connect to server(%s) : %s", socketName.c_str(), strerror(errno));
				goto CATCH;
			}
		}
		else
		{
			_LOGE("Failed to connect due to system error.");
			goto CATCH;
		}
	}

	ret = fcntl(client, F_SETFL, flags);
	if (ret < 0)
	{
		_LOGE("Failed to set file status flags (%d, %s).", errno, strerror(errno));
		goto CATCH;
	}

	ret = write(client, &helloMessage, sizeof(helloMessage));
	if (ret < 0)
	{
		_LOGE("Failed to send a hello message: %d, %s", errno, strerror(errno));
		goto CATCH;
	}

	if (forReverse)
	{
		GError* pGError = NULL;
		GSource* pGSource = NULL;

		GIOChannel* pChannel = g_io_channel_unix_new(client);
		GMainContext* pGMainContext = g_main_context_default();

		g_io_channel_set_encoding(pChannel, NULL, &pGError);
		g_io_channel_set_flags(pChannel, G_IO_FLAG_NONBLOCK, &pGError);
		g_io_channel_set_close_on_unref(pChannel, TRUE);

		pGSource = g_io_create_watch(pChannel, (GIOCondition) (G_IO_IN | G_IO_ERR | G_IO_NVAL | G_IO_HUP));
		g_source_set_callback(pGSource, (GSourceFunc) OnReadMessage, this, NULL);
		g_source_attach(pGSource, pGMainContext);

		g_io_channel_unref(pChannel);
		__pReverseSource = pGSource;
	}
	else
	{
		ReleaseFd(client);
	}

	return MESSAGEPORT_ERROR_NONE;

CATCH:
	if (client != -1)
	{
		close(client);
	}

	return MESSAGEPORT_ERROR_IO_ERROR;

}

gboolean
IpcClient::OnReadMessage(GIOChannel* source, GIOCondition condition, gpointer data)
{
	IpcClient* pIpcClient = (IpcClient*) data;

	return pIpcClient->HandleReceivedMessage(source, condition);
}

gboolean
IpcClient::HandleReceivedMessage(GIOChannel* source, GIOCondition condition)
{
	GError* pGError = NULL;
	GIOStatus status;
	IPC::Message* pMessage = NULL;

	if (condition & G_IO_HUP)
	{
		_LOGI("G_IO_HUP, the connection is closed.");

		g_source_destroy(__pReverseSource);
		g_source_unref(__pReverseSource);
		__pReverseSource = NULL;

		if (__pListener)
		{
			__pListener->OnIpcServerDisconnected(*this);
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
			if (status == G_IO_STATUS_EOF || status == G_IO_STATUS_ERROR)
			{
				if (status == G_IO_STATUS_EOF)
				{
					_LOGI("G_IO_STATUS_EOF, the connection is closed.");
				}
				else
				{
					_LOGI("G_IO_STATUS_ERROR, the connection is closed.");
				}

				pGError = NULL;

				g_io_channel_shutdown(source, FALSE, &pGError);

				g_source_destroy(__pReverseSource);
				g_source_unref(__pReverseSource);
				__pReverseSource = NULL;

				if (__pListener)
				{
					__pListener->OnIpcServerDisconnected(*this);
				}

				return FALSE;
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
					_LOGE("The memory is insufficient");
					return MESSAGEPORT_ERROR_OUT_OF_MEMORY;
				}

				if (__pListener)
				{
					__pListener->OnIpcResponseReceived(*this, *pMessage);
				}

				delete pMessage;

				pStart = pEndOfMessage;
			}
		}
	}
	else
	{
		// empty statement.
	}

	return TRUE;
}

int
IpcClient::AcquireFd(void)
{
	int ret = 0;
	int fd = -1;

	while (fd == -1)
	{
		pthread_mutex_lock(__pMutex);
		if (__fds.size() == 0)
		{
			pthread_mutex_unlock(__pMutex);
			ret = MakeConnection(false);
			if (ret < 0)
			{
				_LOGE("Failed to connect to the server.");
				return MESSAGEPORT_ERROR_IO_ERROR;
			}

			continue;
		}

		fd = __fds.back();
		__fds.pop_back();

		pthread_mutex_unlock(__pMutex);
	}

	return fd;
}

void
IpcClient::ReleaseFd(int fd)
{
	pthread_mutex_lock(__pMutex);

	__fds.push_back(fd);

	pthread_mutex_unlock(__pMutex);
}

int
IpcClient::SendAsync(IPC::Message* pMessage)
{
	char* pData = (char*) pMessage->data();
	int remain = pMessage->size();
	int fd = AcquireFd();
	if (fd == -1)
	{
		_LOGE("Failed to get fd.");
		return MESSAGEPORT_ERROR_IO_ERROR;
	}

	int written = 0;
	while (remain > 0)
	{
		written = write(fd, (char*) pData, remain);
		if (written < 0)
		{
			_LOGE("Failed to send a request: %d, %s", errno, strerror(errno));

			ReleaseFd(fd);
			return MESSAGEPORT_ERROR_IO_ERROR;
		}

		remain -= written;
		pData += written;
	}

	ReleaseFd(fd);

	return MESSAGEPORT_ERROR_NONE;
}

int
IpcClient::SendSync(IPC::Message* pMessage)
{
	int error = MESSAGEPORT_ERROR_NONE;
	int ret = 0;

	int readSize = 0;
	char buffer[1024];
	char* pEndOfMessage = NULL;

	std::string message;

	IPC::Message* pReply = NULL;
	IPC::SyncMessage* pSyncMessage = dynamic_cast <IPC::SyncMessage*>(pMessage);
	if (pSyncMessage == NULL)
	{
		_LOGE("pMessage is not a sync message.");
		return MESSAGEPORT_ERROR_IO_ERROR;
	}

	MessageReplyDeserializer* pReplyDeserializer = pSyncMessage->GetReplyDeserializer();
	int messageId = SyncMessage::GetMessageId(*pSyncMessage);

	int fd = AcquireFd();
	if (fd < 0)
	{
		_LOGE("Failed to get fd.");

		delete pReplyDeserializer;
		return MESSAGEPORT_ERROR_IO_ERROR;
	}

	char* pData = (char*) pSyncMessage->data();
	int remain = pSyncMessage->size();
	int written = 0;

	while (remain > 0)
	{
		written = write(fd, (char*) pData, remain);
		if (written < 0)
		{
			_LOGE("Failed to send a request: %d, %s", errno, strerror(errno));

			error = MESSAGEPORT_ERROR_IO_ERROR;
			goto CATCH;
		}

		remain -= written;
		pData += written;
	}

	// Wait reply
	struct pollfd pfd;

	pfd.fd = fd;
	pfd.events = POLLIN | POLLRDHUP;
	pfd.revents = 0;

	while (true)
	{
		ret = poll(&pfd, 1, -1);
		if (ret < 0)
		{
			if (errno == EINTR)
			{
				continue;
			}

			_LOGE("Failed to poll (%d, %s).", errno, strerror(errno));

			error = MESSAGEPORT_ERROR_IO_ERROR;
			goto CATCH;
		}

		if (pfd.revents & POLLRDHUP)
		{
			_LOGE("POLLRDHUP");

			error = MESSAGEPORT_ERROR_IO_ERROR;
			goto CATCH;
		}

		if (pfd.revents & POLLIN)
		{
			readSize = read(fd, buffer, 1024);
		}

		if (readSize > 0)
		{
			message.append(buffer, readSize);
		}

		pEndOfMessage = (char*) IPC::Message::FindNext(message.data(), message.data() + message.size());
		if (pEndOfMessage)
		{
			pReply = new (std::nothrow) IPC::Message(message.data(), pEndOfMessage - message.data());
			if (pReply == NULL)
			{
				_LOGE("The memory is insufficient.");

				error = MESSAGEPORT_ERROR_OUT_OF_MEMORY;
				goto CATCH;
			}

			break;
		}
	}

	pReplyDeserializer->SerializeOutputParameters(*pReply);
	delete pReply;

CATCH:
	delete pReplyDeserializer;

	ReleaseFd(fd);

	return error;
}

int
IpcClient::Send(IPC::Message* pMessage)
{
	int ret = 0;

	if (pMessage->is_sync())
	{
		ret = SendSync(pMessage);
	}
	else
	{
		ret = SendAsync(pMessage);
	}

	return ret;
}

int
IpcClient::SendRequest(IPC::Message* pMessage)
{
	return Send(pMessage);
}

int
IpcClient::SendRequest(const IPC::Message& message)
{
	int ret = 0;

	if (message.is_sync())
	{
		ret = SendSync(const_cast<IPC::Message*>(&message));
	}
	else
	{
		ret = SendAsync(const_cast<IPC::Message*>(&message));
	}

	return ret;
}

