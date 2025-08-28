#ifndef __VIRTIO_GPU_H__
#define __VIRTIO_GPU_H__

#include <stdint.h>

/* VirtIO GPU device configuration */
#define VIRTIO_GPU_F_VIRGL                0
#define VIRTIO_GPU_F_EDID                 1
#define VIRTIO_GPU_F_RESOURCE_UUID        2
#define VIRTIO_GPU_F_RESOURCE_BLOB        3

/* VirtIO GPU feature support */
#define VIRTIO_GPU_FEATURE_3D             (1 << 0)
#define VIRTIO_GPU_FEATURE_VIRGL          (1 << 1)
#define VIRTIO_GPU_FEATURE_RESOURCE_BLOB  (1 << 2)
#define VIRTIO_GPU_FEATURE_CONTEXT_INIT   (1 << 3)
#define VIRTIO_GPU_FEATURE_CROSS_DEVICE   (1 << 4)
#define VIRTIO_GPU_FEATURE_RESOURCE_SYNC  (1 << 5)

/* OpenGL capability query parameters */
#define VIRTIO_GPU_GL_VERSION             0x1001
#define VIRTIO_GPU_GL_VENDOR              0x1002
#define VIRTIO_GPU_GL_RENDERER            0x1003

/* Context initialization flags */
#define VIRTIO_GPU_CONTEXT_INIT_QUERY_CAPS  0x01
#define VIRTIO_GPU_CONTEXT_INIT_3D          0x02

/* Resource target types */
#define VIRTIO_GPU_RESOURCE_TARGET_BUFFER    1
#define VIRTIO_GPU_RESOURCE_TARGET_TEXTURE_1D  2
#define VIRTIO_GPU_RESOURCE_TARGET_TEXTURE_2D  3
#define VIRTIO_GPU_RESOURCE_TARGET_TEXTURE_3D  4

/* VirtIO GPU configuration space */
struct virtio_gpu_config {
    uint32_t events_read;
    uint32_t events_clear;
    uint32_t num_scanouts;
    uint32_t num_capsets;
};

/* Control commands */
enum virtio_gpu_ctrl_type {
    /* 2D commands */
    VIRTIO_GPU_CMD_GET_DISPLAY_INFO = 0x0100,
    VIRTIO_GPU_CMD_RESOURCE_CREATE_2D,
    VIRTIO_GPU_CMD_RESOURCE_UNREF,
    VIRTIO_GPU_CMD_SET_SCANOUT,
    VIRTIO_GPU_CMD_RESOURCE_FLUSH,
    VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D,
    VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING,
    VIRTIO_GPU_CMD_RESOURCE_DETACH_BACKING,
    VIRTIO_GPU_CMD_GET_CAPSET_INFO,
    VIRTIO_GPU_CMD_GET_CAPSET,
    VIRTIO_GPU_CMD_GET_EDID,

    /* 3D commands */
    VIRTIO_GPU_CMD_CTX_CREATE = 0x0200,
    VIRTIO_GPU_CMD_CTX_DESTROY,
    VIRTIO_GPU_CMD_CTX_ATTACH_RESOURCE,
    VIRTIO_GPU_CMD_CTX_DETACH_RESOURCE,
    VIRTIO_GPU_CMD_RESOURCE_CREATE_3D,
    VIRTIO_GPU_CMD_TRANSFER_TO_HOST_3D,
    VIRTIO_GPU_CMD_TRANSFER_FROM_HOST_3D,
    VIRTIO_GPU_CMD_SUBMIT_3D,
    
    /* Extended 3D commands for expanded functionality */
    VIRTIO_GPU_CMD_BIND_TEXTURE,
    VIRTIO_GPU_CMD_UPDATE_TEXTURE,
    VIRTIO_GPU_CMD_DESTROY_SURFACE,
    VIRTIO_GPU_CMD_CREATE_FRAMEBUFFER,
    
    /* Cursor commands */
    VIRTIO_GPU_CMD_UPDATE_CURSOR,
    VIRTIO_GPU_CMD_MOVE_CURSOR,

    /* Success responses */
    VIRTIO_GPU_RESP_OK_NODATA = 0x1100,
    VIRTIO_GPU_RESP_OK_DISPLAY_INFO,
    VIRTIO_GPU_RESP_OK_CAPSET_INFO,
    VIRTIO_GPU_RESP_OK_CAPSET,
    VIRTIO_GPU_RESP_OK_EDID,

    /* Error responses */
    VIRTIO_GPU_RESP_ERR_UNSPEC = 0x1200,
    VIRTIO_GPU_RESP_ERR_OUT_OF_MEMORY,
    VIRTIO_GPU_RESP_ERR_INVALID_SCANOUT_ID,
    VIRTIO_GPU_RESP_ERR_INVALID_RESOURCE_ID,
    VIRTIO_GPU_RESP_ERR_INVALID_CONTEXT_ID,
    VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER,
};

/* Pixel formats */
enum virtio_gpu_formats {
    VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM  = 1,
    VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM  = 2,
    VIRTIO_GPU_FORMAT_A8R8G8B8_UNORM  = 3,
    VIRTIO_GPU_FORMAT_X8R8G8B8_UNORM  = 4,
    VIRTIO_GPU_FORMAT_R8G8B8A8_UNORM  = 67,
    VIRTIO_GPU_FORMAT_X8B8G8R8_UNORM  = 68,
    VIRTIO_GPU_FORMAT_A8B8G8R8_UNORM  = 121,
    VIRTIO_GPU_FORMAT_R8G8B8X8_UNORM  = 134,
    
    /* Depth/stencil formats */
    VIRTIO_GPU_FORMAT_D16_UNORM       = 55,
    VIRTIO_GPU_FORMAT_D32_FLOAT       = 71,
    VIRTIO_GPU_FORMAT_D24_UNORM_S8_UINT = 49,
};

/* Resource targets for 3D */
#define VIRTIO_GPU_RESOURCE_TARGET_2D      1
#define VIRTIO_GPU_RESOURCE_TARGET_3D      2
#define VIRTIO_GPU_RESOURCE_TARGET_CUBE    3
#define VIRTIO_GPU_RESOURCE_TARGET_1D_ARRAY 4
#define VIRTIO_GPU_RESOURCE_TARGET_2D_ARRAY 5
#define VIRTIO_GPU_RESOURCE_TARGET_CUBE_ARRAY 6

/* Common header for all commands */
struct virtio_gpu_ctrl_hdr {
    uint32_t type;
    uint32_t flags;
    uint64_t fence_id;
    uint32_t ctx_id;
    uint32_t padding;
};

/* Display info structures */
struct virtio_gpu_rect {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
};

struct virtio_gpu_display_one {
    struct virtio_gpu_rect r;
    uint32_t enabled;
    uint32_t flags;
};

struct virtio_gpu_resp_display_info {
    struct virtio_gpu_ctrl_hdr hdr;
    struct virtio_gpu_display_one pmodes[16];
};

/* 2D Resource commands */
struct virtio_gpu_resource_create_2d {
    struct virtio_gpu_ctrl_hdr hdr;
    uint32_t resource_id;
    uint32_t format;
    uint32_t width;
    uint32_t height;
};

struct virtio_gpu_resource_create_3d {
    struct virtio_gpu_ctrl_hdr hdr;
    uint32_t resource_id;
    uint32_t target;
    uint32_t format;
    uint32_t bind;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    uint32_t array_size;
    uint32_t last_level;
    uint32_t nr_samples;
    uint32_t flags;
    uint32_t padding;
};

struct virtio_gpu_resource_unref {
    struct virtio_gpu_ctrl_hdr hdr;
    uint32_t resource_id;
    uint32_t padding;
};

struct virtio_gpu_set_scanout {
    struct virtio_gpu_ctrl_hdr hdr;
    struct virtio_gpu_rect r;
    uint32_t scanout_id;
    uint32_t resource_id;
};

struct virtio_gpu_resource_flush {
    struct virtio_gpu_ctrl_hdr hdr;
    struct virtio_gpu_rect r;
    uint32_t resource_id;
    uint32_t padding;
};

struct virtio_gpu_transfer_to_host_2d {
    struct virtio_gpu_ctrl_hdr hdr;
    struct virtio_gpu_rect r;
    uint64_t offset;
    uint32_t resource_id;
    uint32_t padding;
};

struct virtio_gpu_transfer_to_host_3d {
    struct virtio_gpu_ctrl_hdr hdr;
    struct virtio_gpu_rect r;
    uint64_t offset;
    uint32_t resource_id;
    uint32_t level;
    uint32_t stride;
    uint32_t layer_stride;
};

/* Memory backing */
struct virtio_gpu_mem_entry {
    uint64_t addr;
    uint32_t length;
    uint32_t padding;
};

struct virtio_gpu_resource_attach_backing {
    struct virtio_gpu_ctrl_hdr hdr;
    uint32_t resource_id;
    uint32_t nr_entries;
};

struct virtio_gpu_resource_detach_backing {
    struct virtio_gpu_ctrl_hdr hdr;
    uint32_t resource_id;
    uint32_t padding;
};

/* 3D Context commands */
struct virtio_gpu_ctx_create {
    struct virtio_gpu_ctrl_hdr hdr;
    uint32_t nlen;
    uint32_t padding;
    char debug_name[64];
};

struct virtio_gpu_ctx_destroy {
    struct virtio_gpu_ctrl_hdr hdr;
};

struct virtio_gpu_ctx_resource {
    struct virtio_gpu_ctrl_hdr hdr;
    uint32_t resource_id;
    uint32_t padding;
};

struct virtio_gpu_cmd_submit {
    struct virtio_gpu_ctrl_hdr hdr;
    uint32_t size;
    uint32_t padding;
};

/* Capability set queries */
struct virtio_gpu_get_capset_info {
    struct virtio_gpu_ctrl_hdr hdr;
    uint32_t capset_index;
    uint32_t padding;
};

struct virtio_gpu_resp_capset_info {
    struct virtio_gpu_ctrl_hdr hdr;
    uint32_t capset_id;
    uint32_t capset_max_version;
    uint32_t capset_max_size;
    uint32_t padding;
};

struct virtio_gpu_get_capset {
    struct virtio_gpu_ctrl_hdr hdr;
    uint32_t capset_id;
    uint32_t capset_version;
};

struct virtio_gpu_resp_capset {
    struct virtio_gpu_ctrl_hdr hdr;
    uint8_t capset_data[];
};

/* Cursor structures */
struct virtio_gpu_cursor_pos {
    uint32_t scanout_id;
    uint32_t x;
    uint32_t y;
    uint32_t padding;
};

struct virtio_gpu_update_cursor {
    struct virtio_gpu_ctrl_hdr hdr;
    struct virtio_gpu_cursor_pos pos;
    uint32_t resource_id;
    uint32_t hot_x;
    uint32_t hot_y;
    uint32_t padding;
};

#endif /* __VIRTIO_GPU_H__ */
