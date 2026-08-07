#!/bin/bash
#
# Build libGL.dylib wrapper that exports all OpenGL symbols
# This allows glmark2 to dlopen() our library instead of system OpenGL
#

echo "========================================"
echo "  Building libGL.dylib Wrapper"
echo "========================================"

# Create build directory
mkdir -p build/metal

echo "📦 Compiling libGL.dylib wrapper (full OpenGL replacement)..."

# Create exported symbols list (only export glXXX names, hide my_glXXX)
cat > build/metal/exported_symbols.txt << 'EOF'
_glClear _my_glClear
_glClearColor _my_glClearColor
_glViewport _my_glViewport
_glEnable _my_glEnable
_glDisable _my_glDisable
_glDepthFunc _my_glDepthFunc
_glBlendFunc _my_glBlendFunc
_glCullFace _my_glCullFace
_glGenBuffers _my_glGenBuffers
_glBindBuffer _my_glBindBuffer
_glBufferData _my_glBufferData
_glDeleteBuffers _my_glDeleteBuffers
_glGenVertexArrays _my_glGenVertexArrays
_glBindVertexArray _my_glBindVertexArray
_glDeleteVertexArrays _my_glDeleteVertexArrays
_glVertexAttribPointer _my_glVertexAttribPointer
_glEnableVertexAttribArray _my_glEnableVertexAttribArray
_glDisableVertexAttribArray _my_glDisableVertexAttribArray
_glDrawArrays _my_glDrawArrays
_glDrawElements _my_glDrawElements
_glCreateShader _my_glCreateShader
_glShaderSource _my_glShaderSource
_glCompileShader _my_glCompileShader
_glGetShaderiv _my_glGetShaderiv
_glGetShaderInfoLog _my_glGetShaderInfoLog
_glDeleteShader _my_glDeleteShader
_glCreateProgram _my_glCreateProgram
_glAttachShader _my_glAttachShader
_glLinkProgram _my_glLinkProgram
_glGetProgramiv _my_glGetProgramiv
_glGetProgramInfoLog _my_glGetProgramInfoLog
_glUseProgram _my_glUseProgram
_glDeleteProgram _my_glDeleteProgram
_glGetUniformLocation _my_glGetUniformLocation
_glUniform1f _my_glUniform1f
_glUniform2f _my_glUniform2f
_glUniform3f _my_glUniform3f
_glUniform4f _my_glUniform4f
_glUniform1i _my_glUniform1i
_glUniformMatrix4fv _my_glUniformMatrix4fv
_glGetAttribLocation _my_glGetAttribLocation
_glBindAttribLocation _my_glBindAttribLocation
_glGenTextures _my_glGenTextures
_glBindTexture _my_glBindTexture
_glTexImage2D _my_glTexImage2D
_glTexParameteri _my_glTexParameteri
_glDeleteTextures _my_glDeleteTextures
_glActiveTexture _my_glActiveTexture
_glGenFramebuffers _my_glGenFramebuffers
_glBindFramebuffer _my_glBindFramebuffer
_glFramebufferTexture2D _my_glFramebufferTexture2D
_glCheckFramebufferStatus _my_glCheckFramebufferStatus
_glDeleteFramebuffers _my_glDeleteFramebuffers
_glGetString _my_glGetString
_glGetIntegerv _my_glGetIntegerv
_glGetError _my_glGetError
_glFlush _my_glFlush
_glFinish _my_glFinish
EOF

# Build dylib and create symbol aliases using ld
clang -arch x86_64 \
    -dynamiclib \
    -o build/metal/libGL.1.dylib \
    -install_name /usr/local/lib/libGL.1.dylib \
    -compatibility_version 1.0 \
    -current_version 1.0 \
    -DGL_SILENCE_DEPRECATION \
    -I/opt/X11/include \
    -L/opt/X11/lib \
    -framework OpenGL \
    -framework CoreFoundation \
    -Wl,-alias_list,build/metal/symbol_aliases.txt \
    SharedGL/metal/gl_to_metal_client.c \
    SharedGL/metal/fishhook.c

if [ $? -eq 0 ]; then
    echo "✅ libGL.1.dylib wrapper built successfully"
    
    # Create symlink for libGL.dylib
    cd build/metal
    ln -sf libGL.1.dylib libGL.dylib
    cd ../..
    
    echo "✅ Created symlink: libGL.dylib -> libGL.1.dylib"
    echo ""
    echo "📋 Usage on VM:"
    echo "   DYLD_LIBRARY_PATH=~ DISPLAY=:0 glmark2"
    echo ""
    echo "📦 Files created:"
    ls -lh build/metal/libGL*.dylib
else
    echo "❌ Build failed"
    exit 1
fi
