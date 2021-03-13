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
static VkResult (*_vkQueuePresentKHR)(VkQueue queue, const VkPresentInfoKHR *pPresentInfo) = NULL;
static PFN_vkVoidFunction (*_vkGetInstanceProcAddr)(VkInstance instance, const char *pName) = NULL;

static void _init_vulkan_funcs()
{
	if (_vkEnumerateInstanceExtensionProperties != NULL
		&& _vkCreateInstance != NULL
		&& _vkQueuePresentKHR != NULL
		&& _vkGetInstanceProcAddr != NULL)
		return;

	_vkEnumerateInstanceExtensionProperties = (VkResult (*)(const char*, uint32_t*, VkExtensionProperties*))
			hybris_android_vulkan_dlsym("vkEnumerateInstanceExtensionProperties");
	assert(_vkEnumerateInstanceExtensionProperties);

	_vkCreateInstance = (VkResult (*)(const VkInstanceCreateInfo *, const VkAllocationCallbacks *, VkInstance *))
			hybris_android_vulkan_dlsym("vkCreateInstance");
	assert(_vkCreateInstance);

	_vkQueuePresentKHR = (VkResult (*)(VkQueue, const VkPresentInfoKHR *))
			hybris_android_vulkan_dlsym("vkQueuePresentKHR");
	assert(_vkQueuePresentKHR);

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

#ifdef WANT_WAYLAND
static VkResult nullws_vkCreateWaylandSurfaceKHR(VkInstance instance,
		const VkWaylandSurfaceCreateInfoKHR* pCreateInfo,
		const VkAllocationCallbacks* pAllocator,
		VkSurfaceKHR* pSurface)
{
	return VK_ERROR_OUT_OF_HOST_MEMORY;
}

static VkBool32 nullws_vkGetPhysicalDeviceWaylandPresentationSupportKHR(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex, struct wl_display* display)
{
	return VK_FALSE;
}
#endif

static void nullws_vkDestroySurfaceKHR(VkInstance instance, VkSurfaceKHR surface, const VkAllocationCallbacks* pAllocator)
{
}

static VkResult nullws_vkWaitForFences(VkDevice device, uint32_t fenceCount, const VkFence* pFences, VkBool32 waitAll, uint64_t timeout)
{
	return VK_SUCCESS;
}

static VkResult nullws_vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo)
{
	return (*_vkQueuePresentKHR)(queue, pPresentInfo);
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
#ifdef WANT_WAYLAND
	nullws_vkCreateWaylandSurfaceKHR,
	nullws_vkGetPhysicalDeviceWaylandPresentationSupportKHR,
#endif
	nullws_vkDestroySurfaceKHR,
	nullws_vkWaitForFences,
	nullws_vkQueuePresentKHR,
	nullws_vkGetDeviceProcAddr,
	nullws_vkGetInstanceProcAddr,
	nullws_vkSetInstanceProcAddrFunc,
};
