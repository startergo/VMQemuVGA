/*
 * text_interpose.m — ChildView NSTextInputClient dispatch tracer
 *
 * Three-way discriminator for the PowerFox text-insertion bug:
 *   insertText:replacementRange: fires   -> Gecko dispatch is the culprit
 *   doCommandBySelector: fires instead   -> interpretKeyEvents misrouting
 *   neither fires                        -> interpretKeyEvents not called
 *
 * Plus hasMarkedText / markedRange for the composition-stuck theory.
 *
 * Build:
 *   clang -arch x86_64 -mmacosx-version-min=10.6 -dynamiclib \
 *         -o text_interpose.dylib text_interpose.m \
 *         -framework Cocoa -Wno-deprecated-declarations
 *
 * Deploy:
 *   DYLD_INSERT_LIBRARIES=/tmp/text_interpose.dylib \
 *     /Applications/PowerFox.app/Contents/MacOS/powerfox
 *
 * Mechanism: XUL is dlopen'd by libmozglue after main() starts, so at
 * constructor time ChildView isn't registered. Defer the lookup via
 * dispatch_after on the main queue — by the time it fires, the run loop
 * is spinning, XUL is loaded, and ChildView exists. Hooks installed via
 * method_setImplementation; originals saved and forwarded through.
 */

#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>
#import <dispatch/dispatch.h>
#import <mach-o/dyld.h>
#import <dlfcn.h>
#import <string.h>

#define LOG_TAG "[text]"

static IMP orig_insertText    = NULL;
static IMP orig_doCommand     = NULL;
static IMP orig_hasMarkedText = NULL;
static IMP orig_markedRange   = NULL;

static bool g_installed = false;

static void install_hooks(void);

/* ---- Replacement IMPs ---- */

static void
my_insertText(id self, SEL _cmd, id aString, NSRange range)
{
    NSString *str = nil;
    if ([aString isKindOfClass:[NSAttributedString class]]) {
        str = [(NSAttributedString *)aString string];
    } else if ([aString isKindOfClass:[NSString class]]) {
        str = (NSString *)aString;
    }
    fprintf(stderr, LOG_TAG " insertText:replacementRange: str=\"%s\" range={%lu,%lu}\n",
            str ? [str UTF8String] : "(non-string)",
            (unsigned long)range.location, (unsigned long)range.length);
    fflush(stderr);
    if (orig_insertText) {
        ((void (*)(id, SEL, id, NSRange))orig_insertText)(self, _cmd, aString, range);
    }
}

static void
my_doCommand(id self, SEL _cmd, SEL aSelector)
{
    fprintf(stderr, LOG_TAG " doCommandBySelector: %s\n",
            aSelector ? sel_getName(aSelector) : "(null)");
    fflush(stderr);
    if (orig_doCommand) {
        ((void (*)(id, SEL, SEL))orig_doCommand)(self, _cmd, aSelector);
    }
}

static BOOL
my_hasMarkedText(id self, SEL _cmd)
{
    BOOL r = NO;
    if (orig_hasMarkedText) {
        r = ((BOOL (*)(id, SEL))orig_hasMarkedText)(self, _cmd);
    }
    fprintf(stderr, LOG_TAG " hasMarkedText -> %d\n", (int)r);
    fflush(stderr);
    return r;
}

static NSRange
my_markedRange(id self, SEL _cmd)
{
    NSRange r = NSMakeRange(NSNotFound, 0);
    if (orig_markedRange) {
        r = ((NSRange (*)(id, SEL))orig_markedRange)(self, _cmd);
    }
    fprintf(stderr, LOG_TAG " markedRange -> {%lu,%lu}\n",
            (unsigned long)r.location, (unsigned long)r.length);
    fflush(stderr);
    return r;
}

/* ---- Install ---- */

static void
install_hooks(void)
{
    if (g_installed) return;
    Class cls = objc_getClass("ChildView");
    if (!cls) {
        fprintf(stderr, LOG_TAG " install: ChildView still not registered\n");
        fflush(stderr);
        return;
    }
    g_installed = true;
    fprintf(stderr, LOG_TAG " install: ChildView=%p\n", cls);
    fflush(stderr);

    struct { SEL sel; IMP *orig_slot; IMP new_imp; const char *name; } hooks[] = {
        { @selector(insertText:replacementRange:), &orig_insertText,    (IMP)my_insertText,    "insertText:replacementRange:" },
        { @selector(doCommandBySelector:),         &orig_doCommand,     (IMP)my_doCommand,     "doCommandBySelector:" },
        { @selector(hasMarkedText),                &orig_hasMarkedText, (IMP)my_hasMarkedText, "hasMarkedText" },
        { @selector(markedRange),                  &orig_markedRange,   (IMP)my_markedRange,   "markedRange" },
    };

    for (size_t i = 0; i < sizeof(hooks) / sizeof(hooks[0]); i++) {
        Method m = class_getInstanceMethod(cls, hooks[i].sel);
        if (!m) {
            fprintf(stderr, LOG_TAG " install: SKIP %s — not found on ChildView\n",
                    hooks[i].name);
            fflush(stderr);
            continue;
        }
        *(hooks[i].orig_slot) = method_getImplementation(m);
        method_setImplementation(m, hooks[i].new_imp);
        fprintf(stderr, LOG_TAG " install: hooked -[ChildView %s] (orig IMP=%p)\n",
                hooks[i].name, (void *)*(hooks[i].orig_slot));
        fflush(stderr);
    }
}

/* ---- Trigger install via several mechanisms (first one to succeed wins) ---- */

static void
on_image_added(const struct mach_header *mh, intptr_t slide)
{
    Dl_info info;
    if (dladdr(mh, &info) && info.dli_fname &&
        (strstr(info.dli_fname, "XUL") || strstr(info.dli_fname, "xul"))) {
        fprintf(stderr, LOG_TAG " image-added: %s\n", info.dli_fname);
        fflush(stderr);
        /* Defer — image-add fires before the image's +load phase
         * completes, so ChildView may not yet be registered. */
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.5 * NSEC_PER_SEC)),
                       dispatch_get_main_queue(), ^{
            install_hooks();
        });
    }
}

__attribute__((constructor))
static void text_interpose_init(void)
{
    fprintf(stderr, LOG_TAG " ctor: registering\n");
    fflush(stderr);
    install_hooks();                                  /* in case XUL already loaded */
    _dyld_register_func_for_add_image(on_image_added); /* covers dlopen case */
}
