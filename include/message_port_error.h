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


#ifndef __APPFW_MESSAGE_PORT_ERROR_H__
#define __APPFW_MESSAGE_PORT_ERROR_H__

#include <errno.h>

/**
 * @brief Enumerations of error code for Application.
 */
typedef enum
{
	MESSAGEPORT_ERROR_NONE = 0,					/**< Successful */
	MESSAGEPORT_ERROR_IO_ERROR = -EIO,			/**< Internal I/O error */
	MESSAGEPORT_ERROR_OUT_OF_MEMORY = -ENOMEM,		/**< Out of memory */
	MESSAGEPORT_ERROR_INVALID_PARAMETER = -EINVAL,	/**< Invalid parameter */
	MESSAGEPORT_ERROR_MESSAGEPORT_NOT_FOUND = -ENOKEY,	/**< The message port of the remote application is not found */
	MESSAGEPORT_ERROR_CERTIFICATE_NOT_MATCH = -EACCES,	/**< The remote application is not signed with the same certificate */
	MESSAGEPORT_ERROR_MAX_EXCEEDED = -EMSGSIZE,			/**< The size of message has exceeded the maximum limit */
	MESSAGEPORT_ERROR_RESOURCE_UNAVAILABLE = -EBUSY,	/**< Resource temporarily unavailable */
} messageport_error_e;

#endif /* __APPFW_MESSAGE_PORT_ERROR_H__ */
