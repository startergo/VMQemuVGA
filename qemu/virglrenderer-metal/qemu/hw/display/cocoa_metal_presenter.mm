#import "cocoa_metal_presenter.h"

#import <AppKit/AppKit.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <IOSurface/IOSurface.h>
#import <CoreVideo/CoreVideo.h>
#import <dispatch/dispatch.h>
#include <mach/mach_port.h>
#include <math.h>
#include <stdint.h>

#import "virgl_metal_scanout_bridge.h"

static void cocoa_iosurface_consumer(const struct vrend_metal_scanout_surface_info *info, void *opaque);
static void cocoa_shared_event_consumer(const struct vrend_metal_shared_event_info *info, void *opaque);
static CVReturn _CocoaDisplayLinkCallback(CVDisplayLinkRef displayLink,
                                          const CVTimeStamp *now,
                                          const CVTimeStamp *outputTime,
                                          CVOptionFlags flagsIn,
                                          CVOptionFlags *flagsOut,
                                          void *displayLinkContext);

@interface CocoaMetalPresenter ()
@property (nonatomic, readonly) uint32_t maxScanouts;
@property (nonatomic, strong) id<MTLDevice> device;
@property (nonatomic, strong) id<MTLCommandQueue> commandQueue;
@property (nonatomic, strong) NSMutableDictionary<NSNumber *, NSView *> *viewTable;
@property (nonatomic, strong) NSMutableDictionary<NSNumber *, CAMetalLayer *> *layerTable;
@property (nonatomic, strong) NSMutableDictionary<NSNumber *, NSNumber *> *frameIdTable;
@property (nonatomic, strong) NSMutableDictionary<NSNumber *, id<MTLSharedEvent>> *sharedEventTable;
@property (nonatomic, strong) NSMutableDictionary<NSNumber *, NSNumber *> *sharedEventSignals;
@property (nonatomic, strong) NSMutableDictionary<NSNumber *, NSNumber *> *scanoutContextMap;
@property (nonatomic, assign) BOOL bridgeRegistered;
@property (nonatomic, assign) BOOL vsyncEnabled;
@property (nonatomic, assign) double targetFrameRate;
@property (nonatomic, assign) CVDisplayLinkRef displayLink;
@property (nonatomic, strong) NSLock *pendingLock;
@property (nonatomic, strong) NSMutableDictionary<NSNumber *, NSData *> *pendingInfo;

- (void)handleIOSurfaceInfo:(struct vrend_metal_scanout_surface_info)info;
- (void)handleSharedEventInfoStruct:(struct vrend_metal_shared_event_info)info;
- (CAMetalLayer *)ensureLayerForScanout:(NSNumber *)key
                                   view:(NSView *)view
                                   size:(CGSize)size
                             pixelFormat:(MTLPixelFormat)pixelFormat;
- (void)presentSurfaceInfo:(const struct vrend_metal_scanout_surface_info)info
                    onLayer:(CAMetalLayer *)layer;
- (void)presentImmediate:(struct vrend_metal_scanout_surface_info)info;
- (void)startDisplayLink;
- (void)stopDisplayLink;
- (void)displayLinkDidFire;
- (void)applyThrottleForScanout:(uint32_t)scanout;
- (void)applyThrottleForAllScanouts;
@end

@implementation CocoaMetalPresenter

- (instancetype)initWithMaxScanouts:(uint32_t)maxScanouts {
    self = [super init];
    if (!self) {
        return nil;
    }

    _maxScanouts = maxScanouts;
#if TARGET_OS_OSX
    if (@available(macOS 10.14, *)) {
        _device = MTLCreateSystemDefaultDevice();
        if (_device) {
            _commandQueue = [_device newCommandQueue];
        } else {
            NSLog(@"[CocoaMetalPresenter] Failed to create default Metal device");
        }
    } else {
        NSLog(@"[CocoaMetalPresenter] macOS 10.14+ is required for Metal scanouts");
    }
#else
    NSLog(@"[CocoaMetalPresenter] Metal scanouts are not supported on this platform");
#endif

    _viewTable = [NSMutableDictionary dictionary];
    _layerTable = [NSMutableDictionary dictionary];
    _frameIdTable = [NSMutableDictionary dictionary];
    _sharedEventTable = [NSMutableDictionary dictionary];
    _sharedEventSignals = [NSMutableDictionary dictionary];
    _scanoutContextMap = [NSMutableDictionary dictionary];
    _pendingInfo = [NSMutableDictionary dictionary];
    _pendingLock = [[NSLock alloc] init];
    _vsyncEnabled = YES;
    _targetFrameRate = 60.0;
    _displayLink = NULL;

    return self;
}

- (void)dealloc {
    [self stopDisplayLink];
}

- (void)setView:(NSView *)view forScanout:(uint32_t)scanout {
    if (!view || scanout >= self.maxScanouts) {
        return;
    }

    dispatch_async(dispatch_get_main_queue(), ^{
        self.viewTable[@(scanout)] = view;
        [self.layerTable removeObjectForKey:@(scanout)];
        [self.frameIdTable removeObjectForKey:@(scanout)];
        [self applyThrottleForScanout:scanout];
    });
}

- (void)detachScanout:(uint32_t)scanout {
    if (scanout >= self.maxScanouts) {
        return;
    }

    dispatch_async(dispatch_get_main_queue(), ^{
        NSNumber *key = @(scanout);
        [self.viewTable removeObjectForKey:key];
        [self.layerTable removeObjectForKey:key];
        [self.frameIdTable removeObjectForKey:key];
        [self.scanoutContextMap removeObjectForKey:key];
        [self.sharedEventSignals removeObjectForKey:key];
    });
    virtio_gpu_metal_set_scanout_throttle(scanout, 0.0);
}

- (void)layoutLayers {
    dispatch_async(dispatch_get_main_queue(), ^{
        [self.layerTable enumerateKeysAndObjectsUsingBlock:^(NSNumber *key, CAMetalLayer *layer, BOOL *stop) {
            NSView *view = self.viewTable[key];
            if (!layer || !view) {
                return;
            }
            layer.frame = view.bounds;
            NSRect backingRect = [view convertRectToBacking:view.bounds];
            layer.drawableSize = backingRect.size;
        }];
    });
}

- (void)registerWithMetalBridge {
    if (self.bridgeRegistered) {
        return;
    }
    if (!vrend_metal_scanout_supports_iosurface()) {
        NSLog(@"[CocoaMetalPresenter] IOSurface scanouts not available on current host");
        return;
    }
    self.bridgeRegistered = YES;
    virtio_gpu_metal_register_iosurface_consumer(cocoa_iosurface_consumer, (__bridge void *)self);
    virtio_gpu_metal_register_shared_event_consumer(cocoa_shared_event_consumer, (__bridge void *)self);
}

- (void)unregisterFromMetalBridge {
    if (!self.bridgeRegistered) {
        return;
    }
    virtio_gpu_metal_register_iosurface_consumer(NULL, NULL);
    virtio_gpu_metal_register_shared_event_consumer(NULL, NULL);
    [self.layerTable removeAllObjects];
    [self.frameIdTable removeAllObjects];
    [self.sharedEventTable removeAllObjects];
    [self.sharedEventSignals removeAllObjects];
    [self.scanoutContextMap removeAllObjects];
    self.bridgeRegistered = NO;
}

- (void)setVsyncEnabled:(BOOL)enabled {
    if (_vsyncEnabled == enabled) {
        return;
    }
    _vsyncEnabled = enabled;
    if (_vsyncEnabled) {
        [self startDisplayLink];
    } else {
        [self stopDisplayLink];
        [_pendingLock lock];
        [_pendingInfo removeAllObjects];
        [_pendingLock unlock];
    }
    [self applyThrottleForAllScanouts];
}

- (BOOL)vsyncEnabled {
    return _vsyncEnabled;
}

- (void)setTargetFrameRate:(double)hz {
    double clamped = hz > 0.0 ? hz : 0.0;
    if (fabs(_targetFrameRate - clamped) < 0.01) {
        return;
    }
    _targetFrameRate = clamped;
    [self applyThrottleForAllScanouts];
}

- (double)targetFrameRate {
    return _targetFrameRate;
}

#pragma mark - Private helpers

- (void)handleIOSurfaceInfo:(struct vrend_metal_scanout_surface_info)info {
    if (info.scanout_id >= self.maxScanouts) {
        return;
    }

    NSNumber *key = @(info.scanout_id);
    if (info.iosurface_id == 0) {
        [self.layerTable removeObjectForKey:key];
        [self.frameIdTable removeObjectForKey:key];
        [self.scanoutContextMap removeObjectForKey:key];
        [_pendingLock lock];
        [_pendingInfo removeObjectForKey:key];
        [_pendingLock unlock];
        return;
    }

    if (info.ctx_id) {
        self.scanoutContextMap[key] = @(info.ctx_id);
    }

    if (self.vsyncEnabled) {
        [_pendingLock lock];
        NSData *blob = [NSData dataWithBytes:&info length:sizeof(info)];
        _pendingInfo[key] = blob;
        [_pendingLock unlock];
    } else {
        [self presentImmediate:info];
    }
}

- (void)handleSharedEventInfoStruct:(struct vrend_metal_shared_event_info)info {
    if (info.ctx_id == 0) {
        return;
    }
    NSNumber *key = @(info.ctx_id);
#if TARGET_OS_OSX
    if (@available(macOS 10.14, *)) {
        if (info.mach_port == MACH_PORT_NULL && info.shared_event_handle == 0) {
            [self.sharedEventTable removeObjectForKey:key];
            [self.sharedEventSignals removeObjectForKey:key];
            return;
        }
        id<MTLSharedEvent> sharedEvent = self.sharedEventTable[key];
        if (!sharedEvent) {
            MTLSharedEventHandle *handle = nil;
            if (info.mach_port != MACH_PORT_NULL &&
                [MTLSharedEventHandle instancesRespondToSelector:@selector(initWithMachPort:)]) {
                handle = [[MTLSharedEventHandle alloc] initWithMachPort:info.mach_port];
            } else if (info.shared_event_handle) {
                handle = (__bridge MTLSharedEventHandle *)(void *)(uintptr_t)info.shared_event_handle;
            }
            if (!handle || !self.device) {
                [self.sharedEventTable removeObjectForKey:key];
                [self.sharedEventSignals removeObjectForKey:key];
                return;
            }
            sharedEvent = [self.device newSharedEventWithHandle:handle];
            if (!sharedEvent) {
                return;
            }
            self.sharedEventTable[key] = sharedEvent;
        }
        if ([sharedEvent respondsToSelector:@selector(setSignaledValue:)]) {
            sharedEvent.signaledValue = info.signal_value;
        }
        self.sharedEventSignals[key] = @(info.signal_value);
    }
#else
    (void)info;
#endif
}

- (CAMetalLayer *)ensureLayerForScanout:(NSNumber *)key
                                   view:(NSView *)view
                                   size:(CGSize)size
                             pixelFormat:(MTLPixelFormat)pixelFormat {
    CAMetalLayer *layer = self.layerTable[key];
    if (!layer) {
        view.wantsLayer = YES;
        if ([view.layer isKindOfClass:[CAMetalLayer class]]) {
            layer = (CAMetalLayer *)view.layer;
        } else {
            layer = [CAMetalLayer layer];
            view.layer = layer;
        }
        self.layerTable[key] = layer;
    }

    if (layer.device != self.device) {
        layer.device = self.device;
    }
    if (pixelFormat != MTLPixelFormatInvalid && layer.pixelFormat != pixelFormat) {
        layer.pixelFormat = pixelFormat;
    }

    layer.frame = view.bounds;
    CGFloat scale = 1.0f;
    if (view.window) {
        scale = view.window.backingScaleFactor;
    } else if (NSScreen.mainScreen) {
        scale = NSScreen.mainScreen.backingScaleFactor;
    }
    layer.contentsScale = scale;
    layer.drawableSize = size;
    if (@available(macOS 10.13, *)) {
        layer.maximumDrawableCount = 3;
        layer.displaySyncEnabled = NO;
    }

    return layer;
}

- (void)presentSurfaceInfo:(const struct vrend_metal_scanout_surface_info)info onLayer:(CAMetalLayer *)layer {
    if (!self.device || !self.commandQueue) {
        return;
    }

    IOSurfaceRef surface = IOSurfaceLookup(info.iosurface_id);
    if (!surface) {
        return;
    }

    MTLTextureDescriptor *descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:(MTLPixelFormat)info.metal_pixel_format
                                                                                           width:info.width
                                                                                          height:info.height
                                                                                       mipmapped:NO];
    descriptor.storageMode = MTLStorageModeShared;
    descriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageBlitSource;

    id<MTLTexture> sourceTexture = [self.device newTextureWithDescriptor:descriptor iosurface:surface plane:0];
    if (!sourceTexture) {
        CFRelease(surface);
        return;
    }

    id<CAMetalDrawable> drawable = [layer nextDrawable];
    if (!drawable) {
        CFRelease(surface);
        return;
    }

    id<MTLCommandBuffer> commandBuffer = [self.commandQueue commandBuffer];
    id<MTLBlitCommandEncoder> blit = [commandBuffer blitCommandEncoder];
    MTLOrigin origin = {0, 0, 0};
    MTLSize size = {info.width, info.height, 1};
    [blit copyFromTexture:sourceTexture
              sourceSlice:0
              sourceLevel:0
             sourceOrigin:origin
               sourceSize:size
                toTexture:drawable.texture
         destinationSlice:0
         destinationLevel:0
        destinationOrigin:origin];
    [blit endEncoding];
    [commandBuffer presentDrawable:drawable];
    [commandBuffer commit];

    CFRelease(surface);
}

- (void)presentImmediate:(struct vrend_metal_scanout_surface_info)info {
    NSNumber *key = @(info.scanout_id);
    NSNumber *lastFrame = self.frameIdTable[key];
    if (info.frame_id && lastFrame && lastFrame.unsignedLongLongValue == info.frame_id) {
        return;
    }

    NSView *view = self.viewTable[key];
    if (!view) {
        return;
    }

    CGSize desiredSize = CGSizeMake(info.width, info.height);
    CAMetalLayer *layer = [self ensureLayerForScanout:key
                                                 view:view
                                                 size:desiredSize
                                           pixelFormat:(MTLPixelFormat)info.metal_pixel_format];
    if (!layer) {
        return;
    }

    [self presentSurfaceInfo:info onLayer:layer];
    self.frameIdTable[key] = @(info.frame_id);
}

- (void)startDisplayLink {
    if (!self.vsyncEnabled || self.displayLink) {
        return;
    }
    CVDisplayLinkRef link = NULL;
    if (CVDisplayLinkCreateWithActiveCGDisplays(&link) == kCVReturnSuccess) {
        CVDisplayLinkSetOutputCallback(link, &_CocoaDisplayLinkCallback, (__bridge void *)self);
        if (CVDisplayLinkStart(link) == kCVReturnSuccess) {
            self.displayLink = link;
            CVTimeStamp period = CVDisplayLinkGetNominalOutputVideoRefreshPeriod(link);
            if (period.videoTimeScale != 0 && period.videoRefreshPeriod != 0) {
                double hz = (double)period.videoTimeScale / (double)period.videoRefreshPeriod;
                [self setTargetFrameRate:hz];
            }
        } else {
            CVDisplayLinkRelease(link);
        }
    }
}

- (void)stopDisplayLink {
    if (!self.displayLink) {
        return;
    }
    CVDisplayLinkStop(self.displayLink);
    CVDisplayLinkRelease(self.displayLink);
    self.displayLink = NULL;
}

- (void)displayLinkDidFire {
    if (!self.vsyncEnabled) {
        return;
    }
    NSDictionary<NSNumber *, NSData *> *snapshot = nil;
    [_pendingLock lock];
    if (_pendingInfo.count) {
        snapshot = [_pendingInfo copy];
        [_pendingInfo removeAllObjects];
    }
    [_pendingLock unlock];
    if (!snapshot.count) {
        return;
    }

    dispatch_async(dispatch_get_main_queue(), ^{
        [snapshot enumerateKeysAndObjectsUsingBlock:^(NSNumber *key, NSData *data, BOOL *stop) {
            struct vrend_metal_scanout_surface_info info;
            if (data.length == sizeof(info)) {
                [data getBytes:&info length:sizeof(info)];
                [self presentImmediate:info];
            }
        }];
    });
}

- (void)applyThrottleForScanout:(uint32_t)scanout {
    double fps = (self.vsyncEnabled && self.targetFrameRate > 0.0) ? self.targetFrameRate : 0.0;
    virtio_gpu_metal_set_scanout_throttle(scanout, fps);
}

- (void)applyThrottleForAllScanouts {
    NSArray<NSNumber *> *keys = [self.viewTable allKeys];
    for (NSNumber *key in keys) {
        [self applyThrottleForScanout:key.unsignedIntValue];
    }
}

@end

static void cocoa_iosurface_consumer(const struct vrend_metal_scanout_surface_info *info, void *opaque) {
    CocoaMetalPresenter *presenter = (__bridge CocoaMetalPresenter *)opaque;
    if (!presenter || !info) {
        return;
    }

    struct vrend_metal_scanout_surface_info copied = *info;
    dispatch_async(dispatch_get_main_queue(), ^{
        [presenter handleIOSurfaceInfo:copied];
    });
}

static void cocoa_shared_event_consumer(const struct vrend_metal_shared_event_info *info, void *opaque) {
    CocoaMetalPresenter *presenter = (__bridge CocoaMetalPresenter *)opaque;
    if (!presenter || !info) {
        return;
    }
    struct vrend_metal_shared_event_info copied = *info;
    dispatch_async(dispatch_get_main_queue(), ^{
        [presenter handleSharedEventInfoStruct:copied];
    });
}

static CVReturn _CocoaDisplayLinkCallback(CVDisplayLinkRef displayLink,
                                          const CVTimeStamp *now,
                                          const CVTimeStamp *outputTime,
                                          CVOptionFlags flagsIn,
                                          CVOptionFlags *flagsOut,
                                          void *displayLinkContext) {
    @autoreleasepool {
        CocoaMetalPresenter *presenter = (__bridge CocoaMetalPresenter *)displayLinkContext;
        [presenter displayLinkDidFire];
    }
    return kCVReturnSuccess;
}

static CocoaMetalPresenter *gSharedPresenter = nil;

static void RunOnMain(dispatch_block_t block) {
    if (!block) {
        return;
    }
    if ([NSThread isMainThread]) {
        block();
    } else {
        dispatch_async(dispatch_get_main_queue(), block);
    }
}

static CocoaMetalPresenter *EnsurePresenter(void) {
    if (!gSharedPresenter) {
        gSharedPresenter = [[CocoaMetalPresenter alloc] initWithMaxScanouts:VIRTIO_GPU_MAX_SCANOUTS];
        [gSharedPresenter registerWithMetalBridge];
        [gSharedPresenter setVsyncEnabled:YES];
    }
    return gSharedPresenter;
}

void cocoa_metal_presenter_start(void) {
    RunOnMain(^{
        (void)EnsurePresenter();
    });
}

void cocoa_metal_presenter_stop(void) {
    RunOnMain(^{
        if (!gSharedPresenter) {
            return;
        }
        [gSharedPresenter unregisterFromMetalBridge];
        gSharedPresenter = nil;
    });
}

void cocoa_metal_presenter_attach_view(NSView *view, uint32_t scanout) {
    if (!view) {
        return;
    }
    RunOnMain(^{
        CocoaMetalPresenter *presenter = EnsurePresenter();
        [presenter setView:view forScanout:scanout];
    });
}

void cocoa_metal_presenter_detach_view(uint32_t scanout) {
    RunOnMain(^{
        if (!gSharedPresenter) {
            return;
        }
        [gSharedPresenter detachScanout:scanout];
    });
}

void cocoa_metal_presenter_layout_views(void) {
    RunOnMain(^{
        if (!gSharedPresenter) {
            return;
        }
        [gSharedPresenter layoutLayers];
    });
}

void cocoa_metal_presenter_set_vsync(bool enabled) {
    RunOnMain(^{
        CocoaMetalPresenter *presenter = EnsurePresenter();
        [presenter setVsyncEnabled:enabled ? YES : NO];
    });
}

void cocoa_metal_presenter_set_target_frame_rate(double hz) {
    RunOnMain(^{
        CocoaMetalPresenter *presenter = EnsurePresenter();
        [presenter setTargetFrameRate:hz];
    });
}
