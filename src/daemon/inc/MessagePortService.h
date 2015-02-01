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
 * @file	MessagePortService.h
 * @brief	This is the header file for the _MessagePortService class.
 *
 * This file contains the declarations of _MessagePortService.
 */

#ifndef _MESSAGE_PORT_SERVICE_H_
#define _MESSAGE_PORT_SERVICE_H_

#include <message-port-data-types.h>

//#include <FBaseResult.h>
//#include <FBaseString.h>
//#include <FBaseColHashMap.h>
//#include <FBaseColHashMapT.h>
//#include <FAppTypes.h>

namespace IPC
{
class Message;
}

class _MessagePortStub;

class _MessagePortService
{
public:
	_MessagePortService(void);

	virtual ~_MessagePortService(void);

	virtual int Construct(_MessagePortStub& stub);

	virtual int RegisterMessagePort(int clientId, const BundleBuffer& buffer);

	virtual int CheckRemotePort(const BundleBuffer& buffer);

	int UnregisterMessagePort(int clientId);

	virtual int SendMessage(const BundleBuffer& metadata, const BundleBuffer& buffer);

private:
	char* GetKey(const BundleBuffer& buffer, bool local = true) const;

	bool IsPreloaded(const char *localAppId, const char *remoteAppId) const;

	int CheckCertificate(const char *localAppId, const char *remoteAppId) const;

private:
	_MessagePortStub* __pStub;

	GHashTable*	__pPorts;
	GHashTable*	__pTrustedPorts;

}; // _MessagePortService

#endif // _MESSAGE_PORT_SERVICE_H_

