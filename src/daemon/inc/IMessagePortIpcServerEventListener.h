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
 * @file	FIo_IMessagePortIpcServerEventListener.h
 * @brief	This is the header file for the _IMessagePortIpcServerEventListener class.
 *
 * This file contains the declarations of _IMessagePortIpcServerEventListener.
 */

#ifndef _MESSAGE_PORT_IPC_SERVER_EVENT_LISTENER_H_
#define _MESSAGE_PORT_IPC_SERVER_EVENT_LISTENER_H_

namespace IPC
{
class Message;
}


class _MessagePortIpcServer;
/**
 * @interface _IMessagePortIpcServerEventListener
 * @brief     This interface provides listener method for the request event from an IPC client.
 * since      3.0
 */
class _IMessagePortIpcServerEventListener
	//: virtual Tizen::Base::Runtime::IEventListener
{
public:
	/**
	 * This is the destructor for this class.
	 *
	 * @since 2.1
	 */
	virtual ~_IMessagePortIpcServerEventListener(void) {}

	/**
	 * Called when an IPC server started.
	 *
	 * @since 2.1
	 * @param[in] server The IPC server
	 */
	virtual void OnIpcServerStarted(const _MessagePortIpcServer& server) = 0;

	/**
	 * Called when an IPC server stopped.
	 *
	 * @since 2.1
	 * @param[in] server The IPC server
	 */
	virtual void OnIpcServerStopped(const _MessagePortIpcServer& server) = 0;

	/**
	 * Called when an IPC client connected.
	 *
	 * @since 2.1
	 * @param[in] server The IPC server
	 */
	virtual void OnIpcClientConnected(const _MessagePortIpcServer& server, int clientId) = 0;

	/**
	 * Called when an IPC client disconnected.
	 *
	 * @since 2.1
	 * @param[in] server The IPC server
	 * @param[in] clientId The id of the connected IPC client
	 */
	virtual void OnIpcClientDisconnected(const _MessagePortIpcServer& server, int clientId) = 0;

	/**
	 * Called when an IPC request message received.
	 *
	 * @since 2.1
	 * @code
	 *
	 * bool
	 * CalculatorStub::OnSumRequested(int a, int b, int* pC)
	 * {
	 *    *pC = a + b;
	 *    return true;
	 * }
	 *
	 * bool
	 * CalculatorStub::OnMultiplyRequested(int a, int b, int* pC)
	 * {
	 *    *pC = a * b;
	 *    return true;
	 * }
	 *
	 * bool
	 * CalculatorStub::OnIpcRequestReceived(_MessagePortIpcServer& server, const IPC::Message& message)
	 * {
	 *    IPC_BEGIN_MESSAGE_MAP(CalculatorStub, message)
	 *         IPC_MESSAGE_HANDLER(My_sum, OnSumRequested, &server)
	 *         IPC_MESSAGE_HANDLER(My_mul, OnMultiplyRequested, &server)
	 *    IPC_END_MESSAGE_MAP()
	 *
	 *    return true;
	 * }
	 *
	 * @endcode
	 * @param[in] server	The IPC server
	 * @param[in] message	The received message
	 */
	virtual void OnIpcRequestReceived(_MessagePortIpcServer& server, const IPC::Message& message) = 0;

}; // _IMessagePortIpcServerEventListener

#endif // _MESSAGE_PORT_IPC_SERVER_EVENT_LISTENER_H_

