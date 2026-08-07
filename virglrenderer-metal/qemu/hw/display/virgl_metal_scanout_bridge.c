/*
 * QEMU-side helper that wires the virgl Metal backend's scanout callback
 * into the existing Cocoa/SDL display surfaces. This file is not part of
 * upstream QEMU today; drop it into the QEMU tree (e.g. hw/display/) and
 * hook virtio-gpu initialization so virtio_gpu_metal_scanout_init() gets
 * invoked when the Metal capset is active.
 */

#include <string.h>
#include <stdio.h>

#include "virgl_metal_scanout_bridge.h"
#include "ui/console.h"

#ifndef VREND_METAL_BRIDGE_HAS_NATIVE_HEADER
bool vrend_metal_scanout_supports_iosurface(void)
{
    return false;
}

int vrend_metal_get_scanout_iosurface(uint32_t scanout_id,
                                      struct vrend_metal_scanout_surface_info *info)
{
    (void)scanout_id;
    if (info) {
        memset(info, 0, sizeof(*info));
    }
    return -1;
}

void vrend_metal_register_scanout_callback(vrend_metal_scanout_callback callback,
                                           void *userdata)
{
    (void)callback;
    (void)userdata;
}

void vrend_metal_register_shared_event_listener(
    vrend_metal_shared_event_listener listener,
    void *userdata)
{
    (void)listener;
    (void)userdata;
}

void vrend_metal_set_scanout_throttle(uint32_t scanout_id, double max_fps)
{
    (void)scanout_id;
    (void)max_fps;
}
#endif /* !VREND_METAL_BRIDGE_HAS_NATIVE_HEADER */

#if defined(__has_include)
#  if __has_include("hw/display/virtio-gpu.h")
#    include "hw/display/virtio-gpu.h"
#  elif __has_include("include/hw/display/virtio-gpu.h")
#    include "include/hw/display/virtio-gpu.h"
#  elif __has_include("hw/virtio/virtio-gpu.h")
#    include "hw/virtio/virtio-gpu.h"
#  elif __has_include("include/hw/virtio/virtio-gpu.h")
#    include "include/hw/virtio/virtio-gpu.h"
#  else
#    error "virtio-gpu.h header not found; copy this bridge into a QEMU source tree"
#  endif
#else
#  include "hw/display/virtio-gpu.h"
#endif

#define METAL_BRIDGE_MAX_SCANOUTS VIRTIO_GPU_MAX_SCANOUTS

struct MetalScanoutState {
    QemuConsole *con;
    DisplaySurface *surface;
    QemuMutex lock;
    bool lock_initialized;
    bool has_iosurface_info;
    struct vrend_metal_scanout_surface_info last_iosurface_info;
    uint32_t last_ctx_id;
};

static struct MetalScanoutState g_scanout_states[METAL_BRIDGE_MAX_SCANOUTS];
static uint32_t g_scanout_count;
static bool g_bridge_registered;
static bool g_iosurface_supported;
static VirtioMetalIOSurfaceConsumer g_iosurface_consumer;
static void *g_iosurface_consumer_opaque;
static VirtioMetalSharedEventConsumer g_shared_event_consumer;
static void *g_shared_event_consumer_opaque;
static bool g_logged_surface_info[METAL_BRIDGE_MAX_SCANOUTS];
static bool g_dumped_surface_ppm;

static inline uint32_t clamp_dimension(uint32_t value, uint32_t limit) {
    return (value > limit) ? limit : value;
}

static void metal_publish_iosurface(uint32_t scanout_id) {
    if (!g_iosurface_supported || scanout_id >= g_scanout_count) {
        return;
    }

    struct MetalScanoutState *state = &g_scanout_states[scanout_id];
    struct vrend_metal_scanout_surface_info info = {0};
    if (vrend_metal_get_scanout_iosurface(scanout_id, &info) != 0) {
        if (state->has_iosurface_info) {
            state->has_iosurface_info = false;
                state->last_ctx_id = 0;
            memset(&state->last_iosurface_info, 0, sizeof(state->last_iosurface_info));
            if (g_iosurface_consumer) {
                struct vrend_metal_scanout_surface_info cleared = {0};
                cleared.scanout_id = scanout_id;
                g_iosurface_consumer(&cleared, g_iosurface_consumer_opaque);
            }
        }
        return;
    }

    if (state->has_iosurface_info &&
        memcmp(&state->last_iosurface_info, &info, sizeof(info)) == 0) {
        return;
    }

    state->last_iosurface_info = info;
    state->has_iosurface_info = true;
    state->last_ctx_id = info.ctx_id;

    if (g_iosurface_consumer) {
        g_iosurface_consumer(&info, g_iosurface_consumer_opaque);
    }
}

static void metal_shared_event_listener(const struct vrend_metal_shared_event_info *info, void *userdata) {
    (void)userdata;
    if (g_shared_event_consumer) {
        g_shared_event_consumer(info, g_shared_event_consumer_opaque);
    }
}

static void metal_scanout_present(
    uint32_t scanout_id,
    const struct virgl_box *region,
    const void *pixels,
    uint32_t bytes_per_row,
    uint32_t width,
    uint32_t height,
    uint64_t frame,
    void *userdata) {

    struct MetalScanoutState *states = userdata;
    if (!states || !pixels || scanout_id >= g_scanout_count) {
        return;
    }

    struct MetalScanoutState *state = &states[scanout_id];
    if (!state->lock_initialized || !state->con) {
        return;
    }

    qemu_mutex_lock(&state->lock);

    DisplaySurface *surface = qemu_console_surface(state->con);
    if (surface != state->surface) {
        state->surface = surface;
    }
    if (!state->surface) {
        qemu_mutex_unlock(&state->lock);
        return;
    }

    uint32_t dest_x = region ? region->x : 0;
    uint32_t dest_y = region ? region->y : 0;
    uint32_t copy_width = region && region->w ? region->w : width;
    uint32_t copy_height = region && region->h ? region->h : height;
    if (!copy_width || !copy_height) {
        qemu_mutex_unlock(&state->lock);
        return;
    }

    uint32_t dst_max_w = surface_width(state->surface);
    uint32_t dst_max_h = surface_height(state->surface);
    if (dest_x >= dst_max_w || dest_y >= dst_max_h) {
        qemu_mutex_unlock(&state->lock);
        return;
    }

    uint32_t clamped_width = clamp_dimension(copy_width, dst_max_w - dest_x);
    uint32_t clamped_height = clamp_dimension(copy_height, dst_max_h - dest_y);
    if (!clamped_width || !clamped_height) {
        qemu_mutex_unlock(&state->lock);
        return;
    }
    uint32_t dst_stride = surface_stride(state->surface);
    uint8_t *dst_base = surface_data(state->surface);
    const uint8_t *src_base = pixels;

    if (!g_logged_surface_info[scanout_id]) {
        g_logged_surface_info[scanout_id] = true;
        fprintf(stderr,
            "[Bridge] Surface scanout %u surface=%p format=%d bpp=%d stride=%u dims=%ux%u\n",
            scanout_id,
            (void *)state->surface,
            surface_format(state->surface),
            surface_bits_per_pixel(state->surface),
            dst_stride,
            dst_max_w,
            dst_max_h);
        fprintf(stderr,
            "[Bridge] Surface scanout %u data=%p\n",
            scanout_id,
            (void *)dst_base);
        }

    static int copy_log_count = 0;
    bool log_now = (copy_log_count < 20) || (copy_log_count % 120 == 0);
    const uint32_t *src_pixels = (const uint32_t *)pixels;
    uint32_t *dst_pixels = (uint32_t *)dst_base;

    if (log_now) {
        fprintf(stderr,
                "[Bridge] Present scanout %u frame=%llu region=(%u,%u %ux%u) src_first=0x%08x dst_before=0x%08x src_stride=%u dst_stride=%u width=%u height=%u\n",
                scanout_id,
                (unsigned long long)frame,
                dest_x,
                dest_y,
                clamped_width,
                clamped_height,
                src_pixels ? src_pixels[0] : 0,
                dst_pixels ? dst_pixels[0] : 0,
                bytes_per_row,
                dst_stride,
                width,
                height);
    }

    for (uint32_t row = 0; row < clamped_height; row++) {
        uint8_t *dst_row = dst_base + (dest_y + row) * dst_stride + dest_x * 4;
        const uint8_t *src_row = src_base + row * bytes_per_row;
        memcpy(dst_row, src_row, clamped_width * 4);
    }

    if (log_now && dst_pixels) {
        uint32_t sample0 = dst_pixels[0];
        uint32_t sample_a = dst_base && dst_stride >= 4 ? *(uint32_t *)(dst_base + dest_y * dst_stride + dest_x * 4) : 0;
        uint32_t sample_b = 0;
        uint32_t sample_c = 0;
        uint32_t max_x = dst_max_w ? dst_max_w - 1 : 0;
        uint32_t max_y = dst_max_h ? dst_max_h - 1 : 0;
        if (dst_base && dst_stride) {
            uint32_t px_bx = max_x / 2;
            uint32_t px_by = max_y / 2;
            uint32_t px_cx = max_x;
            uint32_t px_cy = max_y;
            sample_b = *(uint32_t *)(dst_base + px_by * dst_stride + px_bx * 4);
            sample_c = *(uint32_t *)(dst_base + px_cy * dst_stride + px_cx * 4);
        }
        fprintf(stderr,
                "[Bridge] Present scanout %u done dst_after=0x%08x console=%p samples tl=0x%08x mid=0x%08x br=0x%08x\n",
                scanout_id,
                sample0,
                (void *)state->con,
                sample_a,
                sample_b,
                sample_c);

        if (!g_dumped_surface_ppm) {
            bool full_frame = (clamped_width == dst_max_w && clamped_height == dst_max_h);
            bool has_src = src_pixels && src_pixels[0];
            if (full_frame && has_src) {
                g_dumped_surface_ppm = true;
                const char *ppm_path = "/private/tmp/bridge-dump.ppm";
                FILE *f = fopen(ppm_path, "wb");
                if (f) {
                    uint32_t dump_w = clamped_width;
                    uint32_t dump_h = clamped_height;
                    fprintf(f, "P6\n%u %u\n255\n", dump_w, dump_h);
                    for (uint32_t y = 0; y < dump_h; y++) {
                        const uint8_t *row = dst_base + (dest_y + y) * dst_stride + dest_x * 4;
                        for (uint32_t x = 0; x < dump_w; x++) {
                            uint32_t px = *(const uint32_t *)(row + x * 4);
                            uint8_t b = (uint8_t)(px & 0xff);
                            uint8_t g = (uint8_t)((px >> 8) & 0xff);
                            uint8_t r = (uint8_t)((px >> 16) & 0xff);
                            fputc(r, f);
                            fputc(g, f);
                            fputc(b, f);
                        }
                    }
                    fclose(f);
                    fprintf(stderr, "[Bridge] Wrote surface dump to %s (%ux%u)\n", ppm_path, dump_w, dump_h);
                } else {
                    fprintf(stderr, "[Bridge] Failed to open %s for surface dump\n", ppm_path);
                }
            } else {
                fprintf(stderr, "[Bridge] Skipping PPM dump (full_frame=%d has_src=%d)\n", full_frame, has_src);
            }
        }
    }
    copy_log_count++;

    qemu_mutex_unlock(&state->lock);

    /* dpy_gfx_update requires BQL, which we already have since this callback
     * is invoked from virtio-gpu command processing */
    dpy_gfx_update(state->con, dest_x, dest_y, clamped_width, clamped_height);

    metal_publish_iosurface(scanout_id);
}

void virtio_gpu_metal_scanout_init(struct VirtIOGPU *g) {
    if (!g) {
        return;
    }

    g_scanout_count = MIN(g->parent_obj.conf.max_outputs, METAL_BRIDGE_MAX_SCANOUTS);
    g_iosurface_supported = vrend_metal_scanout_supports_iosurface();
    for (uint32_t i = 0; i < g_scanout_count; i++) {
        struct MetalScanoutState *state = &g_scanout_states[i];
        state->con = g->parent_obj.scanout[i].con;
        state->surface = state->con ? qemu_console_surface(state->con) : NULL;
        if (!state->lock_initialized) {
            qemu_mutex_init(&state->lock);
            state->lock_initialized = true;
        }
        state->has_iosurface_info = false;
        memset(&state->last_iosurface_info, 0, sizeof(state->last_iosurface_info));
        state->last_ctx_id = 0;
    }

    vrend_metal_register_scanout_callback(metal_scanout_present, g_scanout_states);
    g_bridge_registered = true;
}

void virtio_gpu_metal_scanout_shutdown(void) {
    if (!g_bridge_registered) {
        return;
    }

    vrend_metal_register_scanout_callback(NULL, NULL);
    for (uint32_t i = 0; i < g_scanout_count; i++) {
        struct MetalScanoutState *state = &g_scanout_states[i];
        if (state->lock_initialized) {
            qemu_mutex_destroy(&state->lock);
            state->lock_initialized = false;
        }
        state->con = NULL;
        state->surface = NULL;
        state->has_iosurface_info = false;
        memset(&state->last_iosurface_info, 0, sizeof(state->last_iosurface_info));
        state->last_ctx_id = 0;
    }
    g_scanout_count = 0;
    g_bridge_registered = false;
    g_iosurface_supported = false;
}

void virtio_gpu_metal_register_iosurface_consumer(VirtioMetalIOSurfaceConsumer cb, void *opaque) {
    g_iosurface_consumer = cb;
    g_iosurface_consumer_opaque = opaque;

    if (!g_bridge_registered || !g_iosurface_supported || !g_iosurface_consumer) {
        return;
    }

    for (uint32_t i = 0; i < g_scanout_count; i++) {
        metal_publish_iosurface(i);
    }
}

void virtio_gpu_metal_register_shared_event_consumer(VirtioMetalSharedEventConsumer cb, void *opaque) {
    g_shared_event_consumer = cb;
    g_shared_event_consumer_opaque = opaque;
    if (cb) {
        vrend_metal_register_shared_event_listener(metal_shared_event_listener, NULL);
    } else {
        vrend_metal_register_shared_event_listener(NULL, NULL);
    }
}

void virtio_gpu_metal_set_scanout_throttle(uint32_t scanout_id, double max_fps) {
    vrend_metal_set_scanout_throttle(scanout_id, max_fps);
}
