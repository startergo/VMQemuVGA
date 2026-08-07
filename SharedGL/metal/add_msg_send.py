#!/usr/bin/env python3
"""Add msg_send() calls after send_metal_command() blocks"""

def process_file(filename):
    with open(filename, 'r') as f:
        content = f.read()
    
    # Simple replacements for known patterns
    replacements = [
        # Enable/Disable
        ('send_u32(cap);\n    }\n}\n\nvoid my_glDisable', 'send_u32(cap);\n        msg_send();\n    }\n}\n\nvoid my_glDisable'),
        ('send_u32(cap);\n    }\n}\n\n// Depth testing', 'send_u32(cap);\n        msg_send();\n    }\n}\n\n// Depth testing'),
        
        # Draw commands
        ('send_u32(count);\n    }\n}\n\nvoid my_glDrawElements', 'send_u32(count);\n        msg_send();\n    }\n}\n\nvoid my_glDrawElements'),
        ('send_u64((uint64_t)indices);  // offset when using index buffer\n    }\n}', 'send_u64((uint64_t)indices);  // offset when using index buffer\n        msg_send();\n    }\n}'),
        
        # Enable/Disable Vertex Attrib Array
        ('send_u32(index);\n    }\n}\n\nvoid my_glDisableVertexAttribArray', 'send_u32(index);\n        msg_send();\n    }\n}\n\nvoid my_glDisableVertexAttribArray'),
        ('send_u32(index);\n    }\n}\n\n//\n// Modern Drawing', 'send_u32(index);\n        msg_send();\n    }\n}\n\n//\n// Modern Drawing'),
        
        # Shader commands
        ('send_u32(shader);\n    }\n}\n\nGLuint my_glCreateProgram', 'send_u32(shader);\n        msg_send();\n    }\n}\n\nGLuint my_glCreateProgram'),
        ('send_u32(shaderType);\n    }\n}\n\nvoid my_glDeleteShader', 'send_u32(shaderType);\n        msg_send();\n    }\n}\n\nvoid my_glDeleteShader'),
        ('send_u32(program);\n    }\n}\n\nvoid my_glDeleteProgram', 'send_u32(program);\n        msg_send();\n    }\n}\n\nvoid my_glDeleteProgram'),
        ('send_u32(program);\n    }\n}\n\n// Shader/Program query', 'send_u32(program);\n        msg_send();\n    }\n}\n\n// Shader/Program query'),
        
        # Uniform and Attrib Location (need to send before recv)
        ('send_string(name);\n    \n    GLint location = (GLint)recv_i32();', 'send_string(name);\n    msg_send();\n    \n    GLint location = (GLint)recv_i32();'),
        
        # UniformMatrix4fv
        ('send_data(value, count * 16 * sizeof(GLfloat));\n    }\n}\n\n//\n// Textures', 'send_data(value, count * 16 * sizeof(GLfloat));\n        msg_send();\n    }\n}\n\n//\n// Textures'),
        
        # Textures
        ('send_u32(texture);\n    }\n}\n\nvoid my_glTexImage2D', 'send_u32(texture);\n        msg_send();\n    }\n}\n\nvoid my_glTexImage2D'),
        ('send_data(pixels, pixelSize);\n    }\n}\n\nvoid my_glTexParameteri', 'send_data(pixels, pixelSize);\n        msg_send();\n    }\n}\n\nvoid my_glTexParameteri'),
        ('send_i32(param);\n    }\n}\n\nvoid my_glDeleteTextures', 'send_i32(param);\n        msg_send();\n    }\n}\n\nvoid my_glDeleteTextures'),
        ('send_data(textures, n * sizeof(GLuint));\n    }\n}\n\n//\n// Framebuffer', 'send_data(textures, n * sizeof(GLuint));\n        msg_send();\n    }\n}\n\n//\n// Framebuffer'),
        
        # Framebuffers
        ('send_u32(framebuffer);\n    }\n}\n\nvoid my_glFramebufferTexture2D', 'send_u32(framebuffer);\n        msg_send();\n    }\n}\n\nvoid my_glFramebufferTexture2D'),
        ('send_i32(level);\n    }\n}\n\nGLenum my_glCheckFramebufferStatus', 'send_i32(level);\n        msg_send();\n    }\n}\n\nGLenum my_glCheckFramebufferStatus'),
        ('send_u32(target);\n        status = recv_u32();', 'send_u32(target);\n        msg_send();\n        status = recv_u32();'),
        ('send_data(framebuffers, n * sizeof(GLuint));\n    }\n}\n\n//\n// Legacy OpenGL', 'send_data(framebuffers, n * sizeof(GLuint));\n        msg_send();\n    }\n}\n\n//\n// Legacy OpenGL'),
        
        # Blend func
        ('send_u32(dfactor);\n    }\n}\n\n// Enable/Disable features', 'send_u32(dfactor);\n        msg_send();\n    }\n}\n\n// Enable/Disable features'),
        
        # Depth func
        ('send_u32(func);\n    }\n}\n\nvoid my_glDepthMask', 'send_u32(func);\n        msg_send();\n    }\n}\n\nvoid my_glDepthMask'),
        
        # Cull face
        ('send_u32(mode);\n    }\n}\n\nvoid my_glFrontFace', 'send_u32(mode);\n        msg_send();\n    }\n}\n\nvoid my_glFrontFace'),
        
        # glDrawArrays with client data
        ('}\n            }\n        }\n    } else {\n        // VBO path', '}\n            }\n        }\n        msg_send();\n    } else {\n        // VBO path'),
    ]
    
    for old, new in replacements:
        if old in content:
            content = content.replace(old, new)
            print(f"✅ Applied fix")
    
    with open(filename, 'w') as f:
        f.write(content)
    
    print(f"\n✅ Processed {filename}")

if __name__ == '__main__':
    process_file('gl_to_metal_client.c')
