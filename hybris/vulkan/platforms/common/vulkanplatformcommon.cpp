/*
 * Copyright (c) 2021 Jolla Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <android-config.h>
#include <ws.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "config.h"
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "logging.h"

#define VK_USE_PLATFORM_ANDROID_KHR 1
#include <vulkan/vulkan.h>

#include "windowbuffer.h"

#include <hybris/gralloc/gralloc.h>

static struct ws_vulkan_interface *my_vulkan_interface;

extern "C" void vulkanplatformcommon_init(struct ws_vulkan_interface *vulkan_iface)
{
	my_vulkan_interface = vulkan_iface;
}

extern "C" void *hybris_android_vulkan_dlsym(const char *symbol)
{
	return (*my_vulkan_interface->android_vulkan_dlsym)(symbol);
}

extern "C" int hybris_vulkan_has_mapping(VkSurfaceKHR *surface)
{
	return (*my_vulkan_interface->has_mapping)(surface);
}

ANativeWindow *hybris_vulkan_get_mapping(VkSurfaceKHR *surface)
{
	return (*my_vulkan_interface->get_mapping)(surface);
}
