//
// Copyright (c) 2014 Samsung Electronics Co., Ltd.
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

#include <unistd.h>
#include <assert.h>
#include <glib.h>
#include <glib-object.h>
#include <message-port-log.h>

#include <MessagePortStub.h>
#include <MessagePortService.h>

GMainLoop *mainloop;

static _MessagePortStub *__pMessagePortStub = NULL;
static _MessagePortService *__pMessagePortService = NULL;

static int
__initialize(void)
{
	g_type_init();

	return 0;
}

static int
__before_loop(void)
{
	// Message Port
	__pMessagePortStub = new (std::nothrow) _MessagePortStub();
	if (__pMessagePortStub == NULL)
	{
		return -1;
	}

	int res = __pMessagePortStub->Construct();
	if (res != 0)
	{
		delete __pMessagePortStub;
		__pMessagePortStub = NULL;

		return -1;
	}

	__pMessagePortService = new (std::nothrow) _MessagePortService();
	if (__pMessagePortService == NULL)
	{
		delete __pMessagePortStub;
		__pMessagePortStub = NULL;

		return -1;
	}

	res = __pMessagePortService->Construct(*__pMessagePortStub);
	if (res != 0)
	{
		delete __pMessagePortService;
		__pMessagePortService = NULL;

		delete __pMessagePortStub;
		__pMessagePortStub = NULL;

		return -1;
	}

	__pMessagePortStub->SetMessagePortService(*__pMessagePortService);

	return 0;
}

static void
__after_loop(void)
{
	delete __pMessagePortStub;
	delete __pMessagePortService;
}

int main(int argc, char *argv[])
{
	_LOGI("messageportd is started!");

	if (__initialize() < 0)
	{
		_LOGE("messageportd initialization failed!");
		assert(0);
	}

	mainloop = g_main_loop_new(NULL, FALSE);

	if (__before_loop() < 0)
	{
		_LOGE("messageportd failed!");
		assert(0);
	}

	g_main_loop_run(mainloop);

	g_main_loop_unref(mainloop);

	_LOGE("messageportd is closed unexpectly!");

	__after_loop();

	return 0;
}
