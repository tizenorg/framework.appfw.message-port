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
 * @file        message-port-messages.h
 * @brief		This is the header file for message types for IPC.
 */

#ifndef _APPFW_MESSAGE_PORT_MESSAGES_H_
#define _APPFW_MESSAGE_PORT_MESSAGES_H_

#include "ipc/ipc_message_macros.h"
#include "message-port-data-types.h"
#include "message-port-param-traits.h"

#define MessagePortStart 0
#define IPC_MESSAGE_START MessagePortStart

IPC_SYNC_MESSAGE_CONTROL1_1(MessagePort_registerPort, BundleBuffer, int)
IPC_SYNC_MESSAGE_CONTROL1_1(MessagePort_checkRemotePort, BundleBuffer, int)
IPC_SYNC_MESSAGE_CONTROL2_1(MessagePort_sendMessage, BundleBuffer, BundleBuffer, int)
IPC_MESSAGE_CONTROL2(MessagePort_sendMessageAsync, BundleBuffer, BundleBuffer)

#endif //_APPFW_MESSAGE_PORT_MESSAGES_H_
