/****************************************************************************************
 **
 ** Copyright (C) 2021 Jolla Ltd.
 ** All rights reserved.
 **
 ** This file is part of Wayland enablement for libhybris
 **
 ** You may use this file under the terms of the GNU Lesser General
 ** Public License version 2.1 as published by the Free Software Foundation
 ** and appearing in the file license.lgpl included in the packaging
 ** of this file.
 **
 ** This library is free software; you can redistribute it and/or
 ** modify it under the terms of the GNU Lesser General Public
 ** License version 2.1 as published by the Free Software Foundation
 ** and appearing in the file license.lgpl included in the packaging
 ** of this file.
 **
 ** This library is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 ** Lesser General Public License for more details.
 **
 ****************************************************************************************/

#include <android-config.h>
#include <ws.h>
#include <assert.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>

#include <vulkanplatformcommon.h>
#include <vulkanhybris.h>

#include <vulkan/vulkan.h>

#include <hybris/common/binding.h>
#include "logging.h"

static VkResult (*_vkEnumerateInstanceExtensionProperties)(const char *pLayerName, uint32_t *pPropertyCount, VkExtensionProperties *pProperties) = NULL;
static VkResult (*_vkCreateInstance)(const VkInstanceCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkInstance *pInstance) = NULL;
static PFN_vkVoidFunction (*_vkGetInstanceProcAddr)(VkInstance instance, const char *pName) = NULL;

static void _init_vulkan_funcs()
{
	if (_vkEnumerateInstanceExtensionProperties != NULL
		&& _vkCreateInstance != NULL
		&& _vkGetInstanceProcAddr != NULL)
		return;

	_vkEnumerateInstanceExtensionProperties = (VkResult (*)(const char*, uint32_t*, VkExtensionProperties*))
			hybris_android_vulkan_dlsym("vkEnumerateInstanceExtensionProperties");
	assert(_vkEnumerateInstanceExtensionProperties);

	_vkCreateInstance = (VkResult (*)(const VkInstanceCreateInfo *, const VkAllocationCallbacks *, VkInstance *))
			hybris_android_vulkan_dlsym("vkCreateInstance");
	assert(_vkCreateInstance);

	_vkGetInstanceProcAddr = (PFN_vkVoidFunction (*)(VkInstance, const char *))
			hybris_android_vulkan_dlsym("vkGetInstanceProcAddr");
	assert(_vkGetInstanceProcAddr);
}

static void nullws_init_module(struct ws_vulkan_interface *vulkan_iface)
{
	vulkanplatformcommon_init(vulkan_iface);
	_init_vulkan_funcs();
}

static VkResult nullws_vkEnumerateInstanceExtensionProperties(const char* pLayerName, uint32_t* pPropertyCount, VkExtensionProperties* pProperties)
{
	return (*_vkEnumerateInstanceExtensionProperties)(pLayerName, pPropertyCount, pProperties);
}

VkResult nullws_vkCreateInstance(const VkInstanceCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkInstance *pInstance)
{
	return (*_vkCreateInstance)(pCreateInfo, pAllocator, pInstance);
}

static void nullws_vkDestroySurfaceKHR(VkInstance instance, VkSurfaceKHR surface, const VkAllocationCallbacks* pAllocator)
{
}

static PFN_vkVoidFunction nullws_vkGetDeviceProcAddr(VkDevice device, const char *procname)
{
	return NULL;
}

static PFN_vkVoidFunction nullws_vkGetInstanceProcAddr(VkInstance instance, const char *procname)
{
	return NULL;
}

static void nullws_vkSetInstanceProcAddrFunc(PFN_vkVoidFunction addr)
{
	(void)addr;
}

struct ws_module ws_module_info = {
	nullws_init_module,

	nullws_vkEnumerateInstanceExtensionProperties,
	nullws_vkCreateInstance,
	nullws_vkDestroySurfaceKHR,
	nullws_vkGetDeviceProcAddr,
	nullws_vkGetInstanceProcAddr,
	nullws_vkSetInstanceProcAddrFunc,
};
