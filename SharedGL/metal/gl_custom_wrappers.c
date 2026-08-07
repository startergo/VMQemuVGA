//
// Custom GL function wrappers that need special handling
// These override the default forwarding behavior
//
// NOTE: This file is compiled together with gl_to_metal_client.c
//       to ensure my_glEnable/my_glDisable/my_glIsEnabled are visible
//

#include <OpenGL/gl.h>
#include <OpenGL/OpenGL.h>

// Declarations are in gl_to_metal_client.c - these are just wrappers
// that export the correct symbol names

void glEnable(GLenum cap);
void glDisable(GLenum cap);
GLboolean glIsEnabled(GLenum cap);

// Actual implementations
void glEnable(GLenum cap) {
    my_glEnable(cap);
}

void glDisable(GLenum cap) {
    my_glDisable(cap);
}

GLboolean glIsEnabled(GLenum cap) {
    return my_glIsEnabled(cap);
}
