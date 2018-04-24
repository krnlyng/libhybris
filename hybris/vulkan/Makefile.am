
lib_LTLIBRARIES = \
	libvulkan.la

libvulkan_la_SOURCES = \
	vulkan.c \
	helper.cpp \
	ws.c

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = vulkan.pc

libvulkan_la_CFLAGS = -I$(top_srcdir)/include $(ANDROID_HEADERS_CFLAGS) -DPKGLIBDIR="\"$(pkglibdir)/\"" -I$(top_srcdir)/common
if WANT_MESA
libvulkan_la_CFLAGS += -DLIBHYBRIS_WANTS_MESA_X11_HEADERS
endif
if WANT_TRACE
libvulkan_la_CFLAGS += -DDEBUG
endif
if WANT_DEBUG
libvulkan_la_CFLAGS += -ggdb -O0
endif

libvulkan_la_CXXFLAGS = -I$(top_srcdir)/include $(ANDROID_HEADERS_CFLAGS) -I$(top_srcdir)/common -DPKGLIBDIR="\"$(pkglibdir)/\""
if WANT_MESA
libvulkan_la_CXXFLAGS += -DLIBHYBRIS_WANTS_MESA_X11_HEADERS
endif
if WANT_TRACE
libvulkan_la_CXXFLAGS += -DDEBUG
endif
if WANT_DEBUG
libvulkan_la_CXXFLAGS += -ggdb -O0
endif
libvulkan_la_LDFLAGS = \
	-ldl \
	$(top_builddir)/common/libhybris-common.la \
	$(top_builddir)/hardware/libhardware.la \
	-version-info "1":"0":"0"

vulkanincludedir = $(includedir)/vulkan
vulkaninclude_HEADERS = \
	vulkanhybris.h

wsincludedir = $(includedir)/hybris/vulkanplatformcommon
wsinclude_HEADERS = \
	ws.h
