/*
 * focus_interpose.m — NSApplication sendEvent: swizzle for focus-state diagnosis
 *
 * Hypothesis under test: PowerFox receives NSKeyDown but the text-insertion
 * path is silently dropped because one of the three IsFocused() preconditions
 * is false at dispatch time. IsFocused() requires:
 *   [window firstResponder] == mView  AND
 *   [window isKeyWindow]                AND
 *   [[NSApplication sharedApplication] isActive]
 *
 * If any of those is false while the window looks visually active, that's
 * the bug — and it's a VM/WindowServer interaction, not a Gecko bug.
 *
 * Deploy:
 *   DYLD_INSERT_LIBRARIES=/tmp/focus_interpose.dylib \
 *     /Applications/PowerFox.app/Contents/MacOS/powerfox
 *
 * Mechanism: +load category on NSApplication swaps sendEvent: with our
 * category method via method_exchangeImplementations. Inside the category
 * method, calling [self fi_sendEvent:event] invokes the original IMP.
 */

#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>

#define LOG_TAG "[focus]"

@interface NSApplication (FocusInterpose)
- (void)fi_sendEvent:(NSEvent *)event;
@end

@implementation NSApplication (FocusInterpose)

+ (void)load {
    @try {
        Method original = class_getInstanceMethod(self, @selector(sendEvent:));
        Method swizzled = class_getInstanceMethod(self, @selector(fi_sendEvent:));
        if (!original || !swizzled) {
            fprintf(stderr, LOG_TAG " +load: FAILED — method not found (orig=%p swiz=%p)\n",
                    original, swizzled);
            fflush(stderr);
            return;
        }
        method_exchangeImplementations(original, swizzled);
        fprintf(stderr, LOG_TAG " +load: swizzled -[NSApplication sendEvent:]\n");
        fflush(stderr);
    } @catch (NSException *e) {
        fprintf(stderr, LOG_TAG " +load: EXCEPTION %s: %s\n",
                [[e name] UTF8String], [[e reason] UTF8String]);
        fflush(stderr);
    }
}

- (void)fi_sendEvent:(NSEvent *)event {
    if ([event type] == NSKeyDown) {
        NSApplication *app = [NSApplication sharedApplication];
        NSWindow *kw = [app keyWindow];
        NSResponder *fr = [kw firstResponder];
        const char *frClass = fr ? class_getName([fr class]) : "(nil)";
        const char *winClass = kw ? class_getName([kw class]) : "(nil)";
        NSString *title = kw ? [kw title] : nil;
        const char *titleC = title ? [title UTF8String] : "(no title)";
        NSString *chars = [event characters];
        const char *charsC = chars ? [chars UTF8String] : "";

        fprintf(stderr,
            LOG_TAG " NSKeyDown chars=\"%s\" keycode=0x%x  "
                    "appActive=%d  keyWindow=%p(%s:\"%s\")  isKeyWindow=%d  "
                    "firstResponder=%p(%s)\n",
            charsC,
            (unsigned)[event keyCode],
            (int)[app isActive],
            kw, winClass, titleC,
            (int)(kw ? [kw isKeyWindow] : NO),
            fr, frClass);
        fflush(stderr);
    }
    /* Under swizzle, fi_sendEvent: selector now points at original IMP,
     * so this calls the original sendEvent:. */
    [self fi_sendEvent:event];
}

@end
