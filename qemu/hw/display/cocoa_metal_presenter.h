#import <Foundation/Foundation.h>
#include <stdbool.h>
#include <stdint.h>

@class NSView;

NS_ASSUME_NONNULL_BEGIN

/**
 * CocoaMetalPresenter wires virglrenderer-metal scanout IOSurfaces into
 * CAMetalLayer-backed NSViews so QEMU's Cocoa UI can present guest frames
 * without memcpy fallbacks.
 */
@interface CocoaMetalPresenter : NSObject

- (instancetype)initWithMaxScanouts:(uint32_t)maxScanouts;
- (void)setView:(NSView *)view forScanout:(uint32_t)scanout;
- (void)detachScanout:(uint32_t)scanout;
- (void)layoutLayers;
- (void)registerWithMetalBridge;
- (void)unregisterFromMetalBridge;
- (void)setVsyncEnabled:(BOOL)enabled;
- (BOOL)vsyncEnabled;
- (void)setTargetFrameRate:(double)hz;
- (double)targetFrameRate;

@end

#ifdef __cplusplus
extern "C" {
#endif

FOUNDATION_EXPORT void cocoa_metal_presenter_start(void);
FOUNDATION_EXPORT void cocoa_metal_presenter_stop(void);
FOUNDATION_EXPORT void cocoa_metal_presenter_attach_view(NSView *view, uint32_t scanout);
FOUNDATION_EXPORT void cocoa_metal_presenter_detach_view(uint32_t scanout);
FOUNDATION_EXPORT void cocoa_metal_presenter_layout_views(void);
FOUNDATION_EXPORT void cocoa_metal_presenter_set_vsync(bool enabled);
FOUNDATION_EXPORT void cocoa_metal_presenter_set_target_frame_rate(double hz);

#ifdef __cplusplus
}
#endif

NS_ASSUME_NONNULL_END
