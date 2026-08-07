/*
 * text_interpose.m — ChildView NSTextInputClient + NSTextInputContext tracer
 *
 * Three-way discriminator for the PowerFox text-insertion bug:
 *   insertText:replacementRange: fires   -> Gecko dispatch is the culprit
 *   doCommandBySelector: fires instead   -> interpretKeyEvents misrouting
 *   neither fires                        -> interpretKeyEvents not called
 *
 * Plus hasMarkedText / markedRange for the composition-stuck theory.
 *
 * Plus NSTextInputContext -activate / -deactivate to test the reentrant-flush
 * hypothesis. TextInputHandler::NotifyIMEOfFocusChangeInGecko() does:
 *   [inputContext deactivate];
 *   [inputContext activate];
 * inside the mIsInFocusProcessing window (lines 2641-2642, gated by
 * #if MAC_OS_X_VERSION_10_6). If a dropped insertText: lands between such a
 * pair, IgnoreIMEComposition() at TextInputHandler.mm:2147 is the drop site
 * and mIsInFocusProcessing was true. If the pair never fires during typing,
 * the reentrant theory dies and the drop is elsewhere in InsertText
 * (isEditable / mIMEState.mEnabled being the next candidate).
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
 * Mechanism: ChildView lives in XUL, which libmozglue.dylib dlopens after
 * main(). A dylib constructor runs before that, so objc_getClass("ChildView")
 * returns nil. Defer via _dyld_register_func_for_add_image + dispatch_after.
 * NSTextInputContext is in AppKit and is available at constructor time, so
 * those hooks install immediately. Hooks installed via method_setImplementation;
 * originals saved and forwarded through.
 */

#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>
#import <dispatch/dispatch.h>
#import <mach-o/dyld.h>
#import <dlfcn.h>
#import <string.h>
#import <sys/time.h>
#import <stdarg.h>

#define LOG_TAG "[text]"

static IMP orig_insertText    = NULL;
static IMP orig_doCommand     = NULL;
static IMP orig_hasMarkedText = NULL;
static IMP orig_markedRange   = NULL;
static IMP orig_activate      = NULL;
static IMP orig_deactivate    = NULL;

static bool g_childview_installed      = false;
static bool g_nsinputcontext_installed = false;

static void install_hooks(void);

/* ---- Logging helper ----
 *
 * Single-line, atomic log: "[text] <sec.usec> <msg>\n" flushed to BOTH stderr
 * and /tmp/text_trace.log. GUI-launched apps on 10.6 have stderr connected to
 * /dev/null, so the file is the reliable capture path; stderr is kept for
 * Console.app / direct-process-stderr cases.
 *
 * gettimeofday() is used because clock_gettime() is 10.12+ and the build
 * targets 10.6.
 */
static FILE *g_logfile = NULL;

static void
log_line(const char *fmt, ...)
{
    if (!g_logfile) {
        g_logfile = fopen("/tmp/text_trace.log", "a");
    }
    struct timeval tv;
    gettimeofday(&tv, NULL);
    char prefix[64];
    snprintf(prefix, sizeof(prefix), LOG_TAG " %ld.%06ld ",
             (long)tv.tv_sec, (long)tv.tv_usec);

    va_list ap;
    va_start(ap, fmt);
    fputs(prefix, stderr);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    fflush(stderr);
    va_end(ap);

    if (g_logfile) {
        va_list ap2;
        va_start(ap2, fmt);
        fputs(prefix, g_logfile);
        vfprintf(g_logfile, fmt, ap2);
        fputc('\n', g_logfile);
        fflush(g_logfile);
        va_end(ap2);
    }
}

/* ---- Replacement IMPs: ChildView NSTextInputClient ---- */

static void
my_insertText(id self, SEL _cmd, id aString, NSRange range)
{
    NSString *str = nil;
    if ([aString isKindOfClass:[NSAttributedString class]]) {
        str = [(NSAttributedString *)aString string];
    } else if ([aString isKindOfClass:[NSString class]]) {
        str = (NSString *)aString;
    }
    log_line("insertText:replacementRange: self=%p str=\"%s\" range={%lu,%lu}",
             self, str ? [str UTF8String] : "(non-string)",
             (unsigned long)range.location, (unsigned long)range.length);
    if (orig_insertText) {
        ((void (*)(id, SEL, id, NSRange))orig_insertText)(self, _cmd, aString, range);
    }
}

static void
my_doCommand(id self, SEL _cmd, SEL aSelector)
{
    log_line("doCommandBySelector: self=%p sel=%s",
             self, aSelector ? sel_getName(aSelector) : "(null)");
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
    log_line("hasMarkedText self=%p -> %d", self, (int)r);
    return r;
}

static NSRange
my_markedRange(id self, SEL _cmd)
{
    NSRange r = NSMakeRange(NSNotFound, 0);
    if (orig_markedRange) {
        r = ((NSRange (*)(id, SEL))orig_markedRange)(self, _cmd);
    }
    log_line("markedRange self=%p -> {%lu,%lu}",
             self, (unsigned long)r.location, (unsigned long)r.length);
    return r;
}

/* ---- Replacement IMPs: NSTextInputContext ---- */

static void
my_activate(id self, SEL _cmd)
{
    log_line("NSTIC -[activate] self=%p", self);
    if (orig_activate) {
        ((void (*)(id, SEL))orig_activate)(self, _cmd);
    }
}

static void
my_deactivate(id self, SEL _cmd)
{
    log_line("NSTIC -[deactivate] self=%p", self);
    if (orig_deactivate) {
        ((void (*)(id, SEL))orig_deactivate)(self, _cmd);
    }
}

/* ---- Install: ChildView hooks (XUL-loaded) ---- */

static void
install_hooks(void)
{
    if (g_childview_installed) return;
    Class cls = objc_getClass("ChildView");
    if (!cls) {
        log_line("install: ChildView still not registered");
        return;
    }
    g_childview_installed = true;
    log_line("install: ChildView=%p", cls);

    struct { SEL sel; IMP *orig_slot; IMP new_imp; const char *name; } hooks[] = {
        { @selector(insertText:replacementRange:), &orig_insertText,    (IMP)my_insertText,    "insertText:replacementRange:" },
        { @selector(doCommandBySelector:),         &orig_doCommand,     (IMP)my_doCommand,     "doCommandBySelector:" },
        { @selector(hasMarkedText),                &orig_hasMarkedText, (IMP)my_hasMarkedText, "hasMarkedText" },
        { @selector(markedRange),                  &orig_markedRange,   (IMP)my_markedRange,   "markedRange" },
    };

    for (size_t i = 0; i < sizeof(hooks) / sizeof(hooks[0]); i++) {
        Method m = class_getInstanceMethod(cls, hooks[i].sel);
        if (!m) {
            log_line("install: SKIP %s — not found on ChildView", hooks[i].name);
            continue;
        }
        *(hooks[i].orig_slot) = method_getImplementation(m);
        method_setImplementation(m, hooks[i].new_imp);
        log_line("install: hooked -[ChildView %s] (orig IMP=%p)",
                 hooks[i].name, (void *)*(hooks[i].orig_slot));
    }
}

/* ---- Install: NSTextInputContext hooks (AppKit — available at ctor) ---- */

static void
install_nsinputcontext_hooks(void)
{
    if (g_nsinputcontext_installed) return;
    Class cls = objc_getClass("NSTextInputContext");
    if (!cls) {
        log_line("install: NSTextInputContext not available");
        return;
    }
    g_nsinputcontext_installed = true;
    log_line("install: NSTextInputContext=%p", cls);

    struct { SEL sel; IMP *orig_slot; IMP new_imp; const char *name; } hooks[] = {
        { @selector(activate),   &orig_activate,   (IMP)my_activate,   "activate" },
        { @selector(deactivate), &orig_deactivate, (IMP)my_deactivate, "deactivate" },
    };

    for (size_t i = 0; i < sizeof(hooks) / sizeof(hooks[0]); i++) {
        Method m = class_getInstanceMethod(cls, hooks[i].sel);
        if (!m) {
            log_line("install: SKIP %s — not found on NSTextInputContext", hooks[i].name);
            continue;
        }
        *(hooks[i].orig_slot) = method_getImplementation(m);
        method_setImplementation(m, hooks[i].new_imp);
        log_line("install: hooked -[NSTextInputContext %s] (orig IMP=%p)",
                 hooks[i].name, (void *)*(hooks[i].orig_slot));
    }
}

/* ---- Trigger install via several mechanisms (first one to succeed wins) ---- */

static void
on_image_added(const struct mach_header *mh, intptr_t slide)
{
    Dl_info info;
    if (dladdr(mh, &info) && info.dli_fname &&
        (strstr(info.dli_fname, "XUL") || strstr(info.dli_fname, "xul"))) {
        log_line("image-added: %s", info.dli_fname);
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
    log_line("ctor: registering");
    install_nsinputcontext_hooks();                    /* AppKit — available now */
    install_hooks();                                   /* ChildView — in case XUL already loaded */
    _dyld_register_func_for_add_image(on_image_added); /* covers dlopen case */
}
