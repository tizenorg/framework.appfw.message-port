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
 * @file		message-port-param-traits.h
 * @brief		This is the header file for message port param traits for IPC.
 */

#ifndef _APPFW_MESSAGE_PORT_PARAM_TRAITS_H_
#define _APPFW_MESSAGE_PORT_PARAM_TRAITS_H_
#pragma once

#include <base/tuple.h>
#include <ipc/ipc_param_traits.h>

#include <bundle.h>

#include "message-port-data-types.h"

namespace IPC
{

template<>
struct ParamTraits <BundleBuffer>
{
	typedef BundleBuffer param_type;

	static void Write(Message* m, const param_type& p)
	{
		int len = 0;
		bundle_raw* raw = NULL;
		bundle_encode_raw(p.b, &raw, &len);


		m->WriteInt(len);
		m->WriteBytes((const void*) raw, len);

		m->WriteInt(len);

		bundle_free_encoded_rawdata(&raw);
	}

	static bool Read(const Message* m, void** iter, param_type* r)
	{
		int len = 0;
		const char* pBuffer = NULL;

		if (!m->ReadLength(iter, &len))
		{
			return false;
		}

		if (!m->ReadBytes(iter, &pBuffer, len))
		{
			return false;
		}

		if (!m->ReadLength(iter, &len))
		{
			return false;
		}

		if (pBuffer != NULL)
		{
			// Truncated
			((char*)pBuffer)[len] = '\0';
		}
		else
		{
			return false;
		}

		r->b = bundle_decode_raw((const bundle_raw*)pBuffer, len);

		return true;
	}

	static void Log(const param_type& p, std::string* l)
	{
	}
};

}

#endif //_APPFW_MESSAGE_PORT_PARAM_TRAITS_H_
