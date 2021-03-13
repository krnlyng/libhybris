/****************************************************************************************
 **
 ** Copyright (C) 2013-2021 Jolla Ltd.
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
#include <malloc.h>
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <vector>

#define VK_USE_PLATFORM_ANDROID_KHR 1
#define VK_USE_PLATFORM_WAYLAND_KHR 1
extern "C" {
#include <vulkanplatformcommon.h>
};
#include <vulkanhybris.h>

extern "C" {
#include <wayland-client.h>
#include <wayland-egl.h>
}

#include <vulkan/vulkan.h>

#include <hybris/gralloc/gralloc.h>
#include <hybris/common/binding.h>
#include "wayland_window.h"
#include "logging.h"
#include "../../helper.h"
#include "server_wlegl_buffer.h"
#include "wayland-android-client-protocol.h"

struct WaylandDisplay {
	wl_display *wl_dpy;
	wl_event_queue *queue;
	wl_registry *registry;
	android_wlegl *wlegl;
};

#define _VULKAN_MAX_DISPLAYS 100

void *_displayMappings[_VULKAN_MAX_DISPLAYS];

WaylandNativeWindow *global_win;

void _addMapping(void *display_id)
{
	int i;
	for (i = 0; i < _VULKAN_MAX_DISPLAYS; i++) {
		if (_displayMappings[i] == NULL) {
			_displayMappings[i] = display_id;
			return;
		}
	}
}

void *hybris_egl_display_get_mapping(void *display)
{
	int i;
	for (i = 0; i < _VULKAN_MAX_DISPLAYS; i++) {
		if (_displayMappings[i]) {
			if (((struct WaylandDisplay *)_displayMappings[i])->wl_dpy == display) {
				return _displayMappings[i];
			}
		}
	}
	return NULL;
}

static VkResult (*_vkCreateAndroidSurfaceKHR)(VkInstance instance, const VkAndroidSurfaceCreateInfoKHR *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkSurfaceKHR *pSurface) = NULL;
static PFN_vkVoidFunction (*_vkDestroySurfaceKHR)(VkInstance instance, VkSurfaceKHR surface, const VkAllocationCallbacks* pAllocator) = NULL;
static VkResult (*_vkEnumerateInstanceExtensionProperties)(const char *pLayerName, uint32_t *pPropertyCount, VkExtensionProperties *pProperties) = NULL;
static VkResult (*_vkCreateInstance)(const VkInstanceCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkInstance *pInstance) = NULL;
static VkResult (*_vkQueuePresentKHR)(VkQueue queue, const VkPresentInfoKHR *pPresentInfo) = NULL;
static PFN_vkVoidFunction (*_vkGetInstanceProcAddr)(VkInstance instance, const char *pName) = NULL;

static void _init_vulkan_funcs()
{
	if (_vkCreateAndroidSurfaceKHR != NULL
		&& _vkDestroySurfaceKHR != NULL
		&& _vkEnumerateInstanceExtensionProperties != NULL
		&& _vkCreateInstance != NULL
		&& _vkGetInstanceProcAddr != NULL
		&& _vkQueuePresentKHR != NULL)
		return;

	_vkCreateAndroidSurfaceKHR = (VkResult (*)(VkInstance, const VkAndroidSurfaceCreateInfoKHR *, const VkAllocationCallbacks *, VkSurfaceKHR *))
			hybris_android_vulkan_dlsym("vkCreateAndroidSurfaceKHR");
	assert(_vkCreateAndroidSurfaceKHR);

	_vkDestroySurfaceKHR = (PFN_vkVoidFunction (*)(VkInstance, VkSurfaceKHR, const VkAllocationCallbacks *))
			hybris_android_vulkan_dlsym("vkDestroySurfaceKHR");
	assert(_vkDestroySurfaceKHR);

	_vkEnumerateInstanceExtensionProperties = (VkResult (*)(const char*, uint32_t*, VkExtensionProperties*))
			hybris_android_vulkan_dlsym("vkEnumerateInstanceExtensionProperties");
	assert(_vkEnumerateInstanceExtensionProperties);

	_vkCreateInstance = (VkResult (*)(const VkInstanceCreateInfo *, const VkAllocationCallbacks *, VkInstance *))
			hybris_android_vulkan_dlsym("vkCreateInstance");
	assert(_vkCreateInstance);

	_vkGetInstanceProcAddr = (PFN_vkVoidFunction (*)(VkInstance, const char *))
			hybris_android_vulkan_dlsym("vkGetInstanceProcAddr");
	assert(_vkGetInstanceProcAddr);

	_vkQueuePresentKHR = (VkResult (*)(VkQueue, const VkPresentInfoKHR *))
			hybris_android_vulkan_dlsym("vkQueuePresentKHR");
	assert(_vkQueuePresentKHR);
}

extern "C" void waylandws_init_module(struct ws_vulkan_interface *vulkan_iface)
{
	hybris_gralloc_initialize(0);
	vulkanplatformcommon_init(vulkan_iface);
	_init_vulkan_funcs();
}

static void registry_handle_global(void *data, wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
{
	WaylandDisplay *dpy = (WaylandDisplay *)data;

	if (strcmp(interface, "android_wlegl") == 0) {
		dpy->wlegl = static_cast<struct android_wlegl *>(wl_registry_bind(registry, name, &android_wlegl_interface, std::min(2u, version)));
	}
}

static const wl_registry_listener registry_listener = {
	registry_handle_global
};

static void callback_done(void *data, wl_callback *cb, uint32_t d)
{
	WaylandDisplay *dpy = (WaylandDisplay *)data;

	wl_callback_destroy(cb);
	if (!dpy->wlegl) {
		fprintf(stderr, "Fatal: the server doesn't advertise the android_wlegl global!");
		abort();
	}
}

static const wl_callback_listener callback_listener = {
	callback_done
};

static VkResult waylandws_vkCreateWaylandSurfaceKHR(VkInstance instance,
		const VkWaylandSurfaceCreateInfoKHR* pCreateInfo,
		const VkAllocationCallbacks* pAllocator,
		VkSurfaceKHR* pSurface)
{
    VkAndroidSurfaceCreateInfoKHR createInfo;

	HYBRIS_TRACE_BEGIN("hybris-vulkan", "vkCreateWaylandSurfaceKHR", "");

	WaylandDisplay *wdpy = new WaylandDisplay;
	wdpy->wl_dpy = pCreateInfo->display;
	wdpy->wlegl = NULL;
	wdpy->queue = wl_display_create_queue(wdpy->wl_dpy);
	wdpy->registry = wl_display_get_registry(wdpy->wl_dpy);
	wl_proxy_set_queue((wl_proxy *) wdpy->registry, wdpy->queue);
	wl_registry_add_listener(wdpy->registry, &registry_listener, wdpy);

	wl_callback *cb = wl_display_sync(wdpy->wl_dpy);
	wl_proxy_set_queue((wl_proxy *) cb, wdpy->queue);
	wl_callback_add_listener(cb, &callback_listener, wdpy);

	int ret = 0;
	while (ret == 0 && !wdpy->wlegl) {
		ret = wl_display_dispatch_queue(wdpy->wl_dpy, wdpy->queue);
	}
	assert(ret >= 0);

	struct wl_egl_window *window = wl_egl_window_create(pCreateInfo->surface, 1, 1);

	HYBRIS_TRACE_BEGIN("native-vulkan", "vkCreateWaylandSurfaceKHR", "");

	WaylandNativeWindow *win = new WaylandNativeWindow((struct wl_egl_window *)window, wdpy->wl_dpy, wdpy->wlegl);
	global_win = win;
	win->common.incRef(&win->common);
    createInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    createInfo.pNext = NULL;
    createInfo.flags = 0;
    createInfo.window = win;

	VkResult result = (*_vkCreateAndroidSurfaceKHR)(instance, &createInfo, pAllocator, pSurface);

	HYBRIS_TRACE_END("native-vulkan", "vkCreateWaylandSurfaceKHR", "");

	if (result == VK_SUCCESS) {
		vulkan_helper_push_mapping(pSurface, win);
	} else {
		HYBRIS_ERROR("vkCreateAndroidSurfaceKHR failed");
	}

	HYBRIS_TRACE_END("hybris-vulkan", "vkCreateWaylandSurfaceKHR", "");
	return result;
}

static VkBool32 waylandws_vkGetPhysicalDeviceWaylandPresentationSupportKHR(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex, struct wl_display* display)
{
	return VK_TRUE;
}

static void waylandws_vkDestroySurfaceKHR(VkInstance instance, VkSurfaceKHR surface, const VkAllocationCallbacks* pAllocator)
{
	if (vulkan_helper_has_mapping(&surface)) {
		WaylandNativeWindow *window = (WaylandNativeWindow *)vulkan_helper_pop_mapping(&surface);
		window->common.decRef(&window->common);
		delete window;
		_vkDestroySurfaceKHR(instance, surface, pAllocator);
	}
}

static VkResult waylandws_vkEnumerateInstanceExtensionProperties(const char* pLayerName, uint32_t* pPropertyCount, VkExtensionProperties* pProperties)
{
	VkResult res = (*_vkEnumerateInstanceExtensionProperties)(pLayerName, pPropertyCount, pProperties);
	if (res == VK_SUCCESS && *pPropertyCount > 0 && pProperties != NULL) {
		uint32_t i;
		for (i = 0; i < *pPropertyCount; i++) {
			if (strcmp(pProperties[i].extensionName, VK_KHR_ANDROID_SURFACE_EXTENSION_NAME) == 0) {
				strncpy(pProperties[i].extensionName, VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME, VK_MAX_EXTENSION_NAME_SIZE);
			}
		}
	}
	return res;
}

VkResult waylandws_vkCreateInstance(const VkInstanceCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkInstance *pInstance)
{
	VkInstanceCreateInfo createInfo = *pCreateInfo;
	// Temporary array to replace wayland extensions with android extensions
	char **enabledExtensions = (char **)malloc(VK_MAX_EXTENSION_NAME_SIZE * pCreateInfo->enabledExtensionCount * sizeof(char));
	enabledExtensions = (char **)malloc(pCreateInfo->enabledExtensionCount * sizeof(char *));
	uint32_t i;
	for (i = 0; i < pCreateInfo->enabledExtensionCount; i++) {
		enabledExtensions[i] = (char *)malloc(VK_MAX_EXTENSION_NAME_SIZE * sizeof(char));
		if (strcmp(pCreateInfo->ppEnabledExtensionNames[i], VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME) == 0) {
			strncpy(enabledExtensions[i], VK_KHR_ANDROID_SURFACE_EXTENSION_NAME, VK_MAX_EXTENSION_NAME_SIZE);
		} else {
			strncpy(enabledExtensions[i], pCreateInfo->ppEnabledExtensionNames[i], VK_MAX_EXTENSION_NAME_SIZE);
		}
	}
	createInfo.ppEnabledExtensionNames = enabledExtensions;

	VkResult result = (*_vkCreateInstance)(&createInfo, pAllocator, pInstance);

	// Free temporary array
	for (i = 0; i < pCreateInfo->enabledExtensionCount; i++) {
		free(enabledExtensions[i]);
	}
	free(enabledExtensions);

	return result;
}

static VkResult waylandws_vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo)
{
	VkResult res = (*_vkQueuePresentKHR)(queue, pPresentInfo);
	
	if (res == VK_SUCCESS) {
		global_win->presentQueue();
	} else {
		HYBRIS_ERROR("vkQueuePresentKHR failed");
	}

	return res;
}

VkResult waylandws_vkWaitForFences(VkDevice device, uint32_t fenceCount, const VkFence* pFences, VkBool32 waitAll, uint64_t timeout) {
	// FIXME
	return VK_SUCCESS;
}

extern "C" PFN_vkVoidFunction waylandws_vkGetDeviceProcAddr(VkDevice device, const char *pName)
{
	if (!strcmp(pName, "vkQueuePresentKHR")) {
		return (PFN_vkVoidFunction)waylandws_vkQueuePresentKHR;
	} else if (!strcmp(pName, "vkWaitForFences")) {
		return (PFN_vkVoidFunction)waylandws_vkWaitForFences;
	}

	return NULL;
}

extern "C" PFN_vkVoidFunction waylandws_vkGetInstanceProcAddr(VkInstance instance, const char *pName)
{
	if (!strcmp(pName, "vkCreateWaylandSurfaceKHR")) {
		return (PFN_vkVoidFunction)waylandws_vkCreateWaylandSurfaceKHR;
	}
	return NULL;
}

extern "C" void waylandws_vkSetInstanceProcAddrFunc(PFN_vkVoidFunction addr)
{
	if (_vkGetInstanceProcAddr == NULL)
		_vkGetInstanceProcAddr = (PFN_vkVoidFunction (*)(VkInstance, const char*))addr;
}

struct ws_module ws_module_info = {
	waylandws_init_module,

	waylandws_vkEnumerateInstanceExtensionProperties,
	waylandws_vkCreateInstance,
	waylandws_vkCreateWaylandSurfaceKHR,
	waylandws_vkGetPhysicalDeviceWaylandPresentationSupportKHR,
	waylandws_vkDestroySurfaceKHR,
	waylandws_vkWaitForFences,
	waylandws_vkQueuePresentKHR,
	waylandws_vkGetDeviceProcAddr,
	waylandws_vkGetInstanceProcAddr,
	waylandws_vkSetInstanceProcAddrFunc,
};
