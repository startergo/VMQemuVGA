#ifndef QEMU_VIRGL_METAL_SCANOUT_BRIDGE_H
#define QEMU_VIRGL_METAL_SCANOUT_BRIDGE_H

#include "qemu/osdep.h"

#if defined(__has_include)
#  if __has_include("virglrenderer-metal/vrend_metal.h")
#    include "virglrenderer-metal/vrend_metal.h"
#    define VREND_METAL_BRIDGE_HAS_NATIVE_HEADER 1
#  endif
#endif

#ifndef VREND_METAL_BRIDGE_HAS_NATIVE_HEADER
#include <stdint.h>
#include <stdbool.h>
#include <mach/mach_port.h>

struct virgl_box {
	uint32_t x, y, z;
	uint32_t w, h, d;
};

struct vrend_metal_scanout_surface_info {
	uint32_t scanout_id;
	uint32_t ctx_id;
	uint32_t width;
	uint32_t height;
	uint32_t bytes_per_row;
	uint32_t pixel_format_fourcc;
	uint32_t iosurface_id;
	uint32_t metal_pixel_format;
	uint32_t reserved;
	uint64_t frame_id;
};

struct vrend_metal_shared_event_info {
	uint32_t ctx_id;
	mach_port_t mach_port;
	uint64_t shared_event_handle;
	uint64_t signal_value;
};

typedef void (*vrend_metal_scanout_callback)(
	uint32_t scanout_id,
	const struct virgl_box *region,
	const void *pixels,
	uint32_t bytes_per_row,
	uint32_t width,
	uint32_t height,
	uint64_t frame,
	void *userdata);

typedef void (*vrend_metal_shared_event_listener)(
	const struct vrend_metal_shared_event_info *info,
	void *userdata);

bool vrend_metal_scanout_supports_iosurface(void);
int vrend_metal_get_scanout_iosurface(
	uint32_t scanout_id,
	struct vrend_metal_scanout_surface_info *info);
void vrend_metal_register_scanout_callback(
	vrend_metal_scanout_callback callback,
	void *userdata);
void vrend_metal_register_shared_event_listener(
	vrend_metal_shared_event_listener listener,
	void *userdata);
void vrend_metal_set_scanout_throttle(uint32_t scanout_id, double max_fps);

#endif /* !VREND_METAL_BRIDGE_HAS_NATIVE_HEADER */

#ifdef __cplusplus
extern "C" {
#endif

struct VirtIOGPU;

void virtio_gpu_metal_scanout_init(struct VirtIOGPU *g);
void virtio_gpu_metal_scanout_shutdown(void);
typedef void (*VirtioMetalIOSurfaceConsumer)(const struct vrend_metal_scanout_surface_info *info, void *opaque);
void virtio_gpu_metal_register_iosurface_consumer(VirtioMetalIOSurfaceConsumer cb, void *opaque);
typedef void (*VirtioMetalSharedEventConsumer)(const struct vrend_metal_shared_event_info *info, void *opaque);
void virtio_gpu_metal_register_shared_event_consumer(VirtioMetalSharedEventConsumer cb, void *opaque);
void virtio_gpu_metal_set_scanout_throttle(uint32_t scanout_id, double max_fps);

#ifdef __cplusplus
}
#endif

#endif /* QEMU_VIRGL_METAL_SCANOUT_BRIDGE_H */
