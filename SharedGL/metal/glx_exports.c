//
// GLX Symbol Exports
// Creates glXXX wrappers that call my_glXXX implementations
//

#include <OpenGL/gl.h>

// Forward declarations
extern void* my_glXQueryVersion(void *dpy, int *major, int *minor);
extern void* my_glXQueryExtension(void *dpy, int *errorBase, int *eventBase);
extern const char* my_glXQueryExtensionsString(void *dpy, int screen);
extern const char* my_glXGetClientString(void *dpy, int name);
extern void* my_glXChooseVisual(void *dpy, int screen, int *attribList);
extern void* my_glXCreateContext(void *dpy, void *vis, void *shareList, int direct);
extern void my_glXDestroyContext(void *dpy, void *ctx);
extern int my_glXMakeCurrent(void *dpy, unsigned long drawable, void *ctx);
extern void my_glXSwapBuffers(void *dpy, unsigned long drawable);
extern void* my_glXGetProcAddressARB(const unsigned char *procName);
extern int my_glXGetConfig(void *dpy, void *vis, int attrib, int *value);

// Export actual glXXX symbols
void* glXQueryVersion(void *dpy, int *major, int *minor) {
    return my_glXQueryVersion(dpy, major, minor);
}

void* glXQueryExtension(void *dpy, int *errorBase, int *eventBase) {
    return my_glXQueryExtension(dpy, errorBase, eventBase);
}

const char* glXQueryExtensionsString(void *dpy, int screen) {
    return my_glXQueryExtensionsString(dpy, screen);
}

const char* glXGetClientString(void *dpy, int name) {
    return my_glXGetClientString(dpy, name);
}

void* glXChooseVisual(void *dpy, int screen, int *attribList) {
    return my_glXChooseVisual(dpy, screen, attribList);
}

void* glXCreateContext(void *dpy, void *vis, void *shareList, int direct) {
    return my_glXCreateContext(dpy, vis, shareList, direct);
}

void glXDestroyContext(void *dpy, void *ctx) {
    my_glXDestroyContext(dpy, ctx);
}

int glXMakeCurrent(void *dpy, unsigned long drawable, void *ctx) {
    return my_glXMakeCurrent(dpy, drawable, ctx);
}

void glXSwapBuffers(void *dpy, unsigned long drawable) {
    my_glXSwapBuffers(dpy, drawable);
}

void* glXGetProcAddressARB(const unsigned char *procName) {
    return my_glXGetProcAddressARB(procName);
}

void* glXGetProcAddress(const unsigned char *procName) {
    return my_glXGetProcAddressARB(procName);
}

int glXGetConfig(void *dpy, void *vis, int attrib, int *value) {
    return my_glXGetConfig(dpy, vis, attrib, value);
}
