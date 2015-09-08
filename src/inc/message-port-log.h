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


#ifndef __APPFW_MESSAGE_PORT_LOG_H__
#define __APPFW_MESSAGE_PORT_LOG_H__

#include <dlog.h>
#include <bundle.h>

#ifdef __cplusplus
extern "C" {
#endif

#undef LOG_TAG
#define LOG_TAG "MESSAGE_PORT"

#define _LOGE(fmt, arg...) LOGE(fmt, ##arg)
#define _LOGI(fmt, arg...) LOGI(fmt, ##arg)

#define _SECURE_LOGE(fmt, arg...) SECURE_LOGE(fmt, ##arg)
#define _SECURE_LOGI(fmt, arg...) SECURE_LOGI(fmt, ##arg)

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* __APPFW_MESSAGE_PORT_LOG_H__ */
