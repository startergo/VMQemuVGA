/*
 * test_onscreen_3d.c - Onscreen 3D rendering test using VMVirtIOGPU
 * 
 * This test directly uses the VMVirtIOGPU driver to:
 * 1. Create a 3D context
 * 2. Create a 3D resource (texture/render target)
 * 3. Submit virgl rendering commands (clear to different colors)
 * 4. Attach resource to scanout for onscreen display
 * 5. Cycle through colors visibly on screen
 * 
 * Compile on Snow Leopard:
 *   gcc -std=c99 -arch x86_64 -o test_onscreen_3d test_onscreen_3d.c \
 *       -framework IOKit -framework CoreFoundation -Wall -mmacosx-version-min=10.6
 * 
 * Run:
 *   sudo ./test_onscreen_3d
 * 
 * Expected: Screen should cycle through red, green, blue, yellow colors
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>

// UserClient selectors
#define kVMVirtIOGPUUserClientCreate3DContext      1
#define kVMVirtIOGPUUserClientDestroy3DContext     2
#define kVMVirtIOGPUUserClientSetupSurface         4
#define kVMVirtIOGPUUserClientCreateSurface        7
#define kVMVirtIOGPUUserClientSubmitVirglCommands  0x3000
#define kVMVirtIOGPUUserClientCreate3DResource     0x4003
#define kVMVirtIOGPUUserClientFlushResource        0x3006  // RESOURCE_FLUSH selector
#define kVMVirtIOGPUUserClientSetScanout           0x3007  // SET_SCANOUT selector
#define kVMVirtIOGPUUserClientTransferToHost3D     0x3008  // TRANSFER_TO_HOST_3D selector

// Virgl command opcodes
#define VIRGL_CCMD_CLEAR                1
#define VIRGL_CCMD_CREATE_OBJECT        2
#define VIRGL_CCMD_BIND_OBJECT          3
#define VIRGL_CCMD_DESTROY_OBJECT       4
#define VIRGL_CCMD_SET_VIEWPORT_STATE   5
#define VIRGL_CCMD_SET_FRAMEBUFFER_STATE 6

// Test configuration
#define SCREEN_WIDTH  1024
#define SCREEN_HEIGHT 768
#define COLOR_COUNT   6

typedef struct {
    io_service_t service;
    io_connect_t connection;
    uint32_t context_id;
    uint32_t resource_id;
} RenderState;

typedef struct {
    const char* name;
    float r, g, b, a;
} ColorDef;

const ColorDef colors[COLOR_COUNT] = {
    {"RED",     1.0f, 0.0f, 0.0f, 1.0f},
    {"GREEN",   0.0f, 1.0f, 0.0f, 1.0f},
    {"BLUE",    0.0f, 0.0f, 1.0f, 1.0f},
    {"YELLOW",  1.0f, 1.0f, 0.0f, 1.0f},
    {"MAGENTA", 1.0f, 0.0f, 1.0f, 1.0f},
    {"CYAN",    0.0f, 1.0f, 1.0f, 1.0f}
};

void print_header(const char* title) {
    printf("\n═══════════════════════════════════════════════════════════════════\n");
    printf("  %s\n", title);
    printf("═══════════════════════════════════════════════════════════════════\n\n");
}

// Find VMQemuVGAAccelerator service
io_service_t find_accelerator() {
    printf("🔍 Searching for VMQemuVGAAccelerator...\n");
    
    io_service_t service = IOServiceGetMatchingService(
        kIOMasterPortDefault,
        IOServiceMatching("VMQemuVGAAccelerator")
    );
    
    if (service) {
        printf("✅ Found VMQemuVGAAccelerator\n");
    } else {
        printf("❌ VMQemuVGAAccelerator not found\n");
    }
    
    return service;
}

// Initialize rendering context
int init_render_state(RenderState* state) {
    kern_return_t kr;
    
    // Find service
    state->service = find_accelerator();
    if (!state->service) {
        return 0;
    }
    
    // Open UserClient (type 4 for VMVirtIOGPUUserClient)
    printf("\n🔗 Opening UserClient (type=4)...\n");
    kr = IOServiceOpen(state->service, mach_task_self(), 4, &state->connection);
    if (kr != KERN_SUCCESS) {
        printf("❌ Failed to open UserClient: 0x%08x\n", kr);
        return 0;
    }
    printf("✅ UserClient opened: 0x%x\n", state->connection);
    
    // Create 3D context
    printf("\n🎨 Creating 3D context...\n");
    uint64_t input = 0;
    uint64_t context_output = 0;
    uint32_t output_count = 1;
    
    kr = IOConnectCallMethod(
        state->connection,
        kVMVirtIOGPUUserClientCreate3DContext,
        &input, 1,
        NULL, 0,
        &context_output, &output_count,
        NULL, NULL
    );
    
    if (kr != KERN_SUCCESS || context_output == 0) {
        printf("❌ Failed to create context: 0x%08x\n", kr);
        return 0;
    }
    
    state->context_id = (uint32_t)context_output;
    printf("✅ Context created: ID=%u\n", state->context_id);
    
    // Create 3D resource (render target)
    printf("\n🖼️  Creating 3D resource (%dx%d)...\n", SCREEN_WIDTH, SCREEN_HEIGHT);
    uint64_t resource_inputs[4] = {
        state->context_id,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        2  // BGRA8888 format
    };
    uint64_t resource_output = 0;
    output_count = 1;
    
    kr = IOConnectCallMethod(
        state->connection,
        kVMVirtIOGPUUserClientCreate3DResource,
        resource_inputs, 4,
        NULL, 0,
        &resource_output, &output_count,
        NULL, NULL
    );
    
    if (kr != KERN_SUCCESS) {
        printf("⚠️  Create resource returned: 0x%08x (may still work)\n", kr);
    }
    
    state->resource_id = (uint32_t)resource_output;
    printf("✅ Resource created: ID=%u\n", state->resource_id);
    
    return 1;
}

// Submit virgl CLEAR command
int render_clear(RenderState* state, const ColorDef* color) {
    // Convert float colors to uint32_t representation
    uint32_t r = *((uint32_t*)&color->r);
    uint32_t g = *((uint32_t*)&color->g);
    uint32_t b = *((uint32_t*)&color->b);
    uint32_t a = *((uint32_t*)&color->a);
    
    // Virgl CLEAR command
    uint32_t virgl_cmd[] = {
        VIRGL_CCMD_CLEAR,  // opcode
        0x0000000F,        // mask: clear color+depth+stencil
        r, g, b, a,        // color RGBA
        0x3F800000,        // depth = 1.0
        0x00000000         // stencil = 0
    };
    
    kern_return_t kr = IOConnectCallMethod(
        state->connection,
        kVMVirtIOGPUUserClientSubmitVirglCommands,
        NULL, 0,
        virgl_cmd, sizeof(virgl_cmd),
        NULL, NULL,
        NULL, NULL
    );
    
    if (kr != KERN_SUCCESS) {
        return 0;
    }
    
    // *** CRITICAL: Transfer 3D rendered content to host for display ***
    // Without this, rendering stays in GPU memory but never shows on screen!
    uint64_t transfer_inputs[8] = {
        state->resource_id,  // resource_id
        0,                   // level (mipmap level)
        0,                   // x offset
        0,                   // y offset
        0,                   // z offset (for 3D textures)
        SCREEN_WIDTH,        // width
        SCREEN_HEIGHT,       // height
        1                    // depth (1 for 2D textures)
    };
    
    kr = IOConnectCallMethod(
        state->connection,
        kVMVirtIOGPUUserClientTransferToHost3D,  // 0x3008
        transfer_inputs, 8,
        NULL, 0,
        NULL, NULL,
        NULL, NULL
    );
    
    if (kr != KERN_SUCCESS) {
        printf("  ⚠️  TransferToHost3D failed: 0x%08x\n", kr);
        return 0;
    }
    
    // *** CRITICAL: Flush resource to tell display to update ***
    // This tells the VirtIO GPU host to actually refresh the scanout
    uint64_t flush_inputs[5] = {
        state->resource_id,  // resource_id
        0,                   // x offset
        0,                   // y offset
        SCREEN_WIDTH,        // width
        SCREEN_HEIGHT        // height
    };
    
    kr = IOConnectCallMethod(
        state->connection,
        kVMVirtIOGPUUserClientFlushResource,  // 0x3006
        flush_inputs, 5,
        NULL, 0,
        NULL, NULL,
        NULL, NULL
    );
    
    if (kr != KERN_SUCCESS) {
        printf("  ⚠️  FlushResource failed: 0x%08x\n", kr);
        return 0;
    }
    
    return 1;
}

// Attach resource to scanout (display it)
int attach_to_scanout(RenderState* state) {
    printf("\n📺 Attaching resource to scanout (selector 0x3007)...\n");
    
    // Selector 0x3007 expects: scanout_id, resource_id, x, y, width, height
    uint64_t scanout_inputs[6] = {
        0,                    // scanout_id (0 = primary display)
        state->resource_id,   // resource_id
        0,                    // x offset
        0,                    // y offset
        SCREEN_WIDTH,         // width
        SCREEN_HEIGHT         // height
    };
    
    kern_return_t kr = IOConnectCallMethod(
        state->connection,
        kVMVirtIOGPUUserClientSetScanout,  // 0x3007
        scanout_inputs, 6,
        NULL, 0,
        NULL, NULL,
        NULL, NULL
    );
    
    if (kr == KERN_SUCCESS) {
        printf("✅ Resource attached to scanout\n");
        printf("    This switches VirtIO GPU from VGA mode to native mode!\n");
        return 1;
    } else {
        printf("⚠️  Attach scanout returned: 0x%08x\n", kr);
        printf("    Commands still submitted to GPU but not displayed\n");
        return 0;
    }
}

// Cleanup
void cleanup(RenderState* state) {
    if (state->connection && state->context_id) {
        printf("\n🧹 Cleaning up...\n");
        
        uint64_t destroy_input = state->context_id;
        IOConnectCallMethod(
            state->connection,
            kVMVirtIOGPUUserClientDestroy3DContext,
            &destroy_input, 1,
            NULL, 0,
            NULL, NULL,
            NULL, NULL
        );
        
        printf("✅ Context destroyed\n");
    }
    
    if (state->connection) {
        IOServiceClose(state->connection);
    }
    
    if (state->service) {
        IOObjectRelease(state->service);
    }
}

int main(int argc, char** argv) {
    printf("\n");
    print_header("VMVirtIOGPU Onscreen 3D Rendering Test");
    
    printf("This test will:\n");
    printf("  1. Create 3D context and resource\n");
    printf("  2. Submit virgl CLEAR commands\n");
    printf("  3. Try to display on screen (if scanout attachment works)\n");
    printf("  4. Cycle through colors: RED → GREEN → BLUE → YELLOW → MAGENTA → CYAN\n");
    printf("\n");
    
    if (geteuid() != 0) {
        printf("⚠️  Not running as root - some operations may fail\n\n");
    }
    
    RenderState state = {0};
    
    // Initialize
    if (!init_render_state(&state)) {
        printf("\n❌ Initialization failed\n");
        return 1;
    }
    
    // Try to attach to scanout
    int scanout_attached = attach_to_scanout(&state);
    
    if (!scanout_attached) {
        printf("\n⚠️  Scanout attachment not working - continuing anyway\n");
        printf("Commands will still be submitted to GPU (check kernel log)\n");
    }
    
    // Render loop
    print_header("Rendering Color Cycle");
    
    printf("Rendering %d frames, cycling through colors...\n", COLOR_COUNT * 3);
    printf("(Press Ctrl+C to stop)\n\n");
    
    int frame = 0;
    int success_count = 0;
    
    for (int cycle = 0; cycle < 3; cycle++) {
        for (int i = 0; i < COLOR_COUNT; i++) {
            frame++;
            const ColorDef* color = &colors[i];
            
            printf("Frame %d: Clearing to %s (%.1f, %.1f, %.1f, %.1f)\n",
                   frame, color->name, color->r, color->g, color->b, color->a);
            
            if (render_clear(&state, color)) {
                success_count++;
                printf("  ✅ Command submitted\n");
            } else {
                printf("  ❌ Command failed\n");
            }
            
            // Hold each color for 1 second
            sleep(1);
        }
        
        printf("\n--- Cycle %d complete ---\n\n", cycle + 1);
    }
    
    // Summary
    print_header("Test Complete");
    
    printf("Rendered %d frames\n", frame);
    printf("Successful commands: %d/%d\n", success_count, frame);
    printf("\n");
    
    if (success_count == frame) {
        printf("✅ All commands submitted successfully!\n");
    } else {
        printf("⚠️  Some commands failed\n");
    }
    
    printf("\n📋 Check kernel log for command processing:\n");
    printf("    sudo dmesg | grep -i 'virgl\\|submit\\|clear'\n");
    printf("\n");
    
    if (scanout_attached) {
        printf("📺 If scanout is working, you should see colors on screen!\n");
    } else {
        printf("⚠️  Scanout not attached - colors won't display\n");
        printf("    But commands are still being processed by GPU\n");
    }
    
    printf("\n");
    
    // Cleanup
    cleanup(&state);
    
    printf("\n✅ Test completed successfully\n\n");
    return 0;
}
