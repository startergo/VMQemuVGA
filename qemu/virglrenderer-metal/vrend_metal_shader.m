/*
 * virglrenderer Metal Backend - Shader Translation Implementation
 * 
 * This ports the GLSL→MSL translation code from metal_server.m (lines 764-930)
 * to work with virglrenderer's TGSI pipeline.
 */

#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#include <stdlib.h>
#include <string.h>
#include "vrend_metal_shader.h"

#if defined(VREND_METAL_USE_MGL_TOOLCHAIN) && defined(__has_include) && __has_include("mgl_toolchain.h")
#include "mgl_toolchain.h"
#define VREND_METAL_HAVE_MGL_TOOLCHAIN 1
#else
#define VREND_METAL_HAVE_MGL_TOOLCHAIN 0
#endif

/* Forward declare Metal device accessor */
id<MTLDevice> vrend_metal_get_device(void);

static NSString *vrend_msl_type_for_glsl_type(NSString *glslType) {
    static NSDictionary<NSString *, NSString *> *typeMap = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        typeMap = @{ 
            @"float": @"float",
            @"vec2": @"float2",
            @"vec3": @"float3",
            @"vec4": @"float4",
            @"mat3": @"float3x3",
            @"mat4": @"float4x4",
            @"int": @"int",
            @"ivec2": @"int2",
            @"ivec3": @"int3",
            @"ivec4": @"int4",
            @"uint": @"uint",
            @"uvec2": @"uint2",
            @"uvec3": @"uint3",
            @"uvec4": @"uint4",
        };
    });
    NSString *mapped = typeMap[glslType];
    return mapped ? mapped : glslType;
}

typedef NS_ENUM(NSInteger, vrend_texture_modifier) {
    VREND_TEXTURE_MOD_NONE = 0,
    VREND_TEXTURE_MOD_LOD,
    VREND_TEXTURE_MOD_BIAS,
};

static NSString *vrend_trim(NSString *value) {
    return [value stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
}

static inline NSString *vrend_string_or_default(NSString *value, NSString *fallback) {
    return value ? value : fallback;
}

static NSString *vrend_metal_texture_type_for_sampler(NSString *samplerType) {
    static NSDictionary<NSString *, NSString *> *samplerMap = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        samplerMap = @{
            @"sampler2D": @"texture2d<float>",
            @"sampler2DShadow": @"depth2d<float>",
            @"sampler3D": @"texture3d<float>",
            @"samplerCube": @"texturecube<float>",
        };
    });
    return samplerMap[samplerType];
}

static BOOL vrend_parse_texture_call_arguments(NSString *source,
                                               NSUInteger openParenIndex,
                                               NSString **outFirstArg,
                                               NSString **outCoordExpr,
                                               NSString **outExtraExpr,
                                               NSUInteger *outClosingParenIndex) {
    if (openParenIndex >= [source length]) {
        return NO;
    }
    NSCharacterSet *whitespace = [NSCharacterSet whitespaceAndNewlineCharacterSet];
    NSUInteger idx = openParenIndex + 1;
    NSUInteger length = [source length];
    while (idx < length && [whitespace characterIsMember:[source characterAtIndex:idx]]) {
        idx++;
    }
    NSUInteger samplerStart = idx;
    while (idx < length) {
        unichar ch = [source characterAtIndex:idx];
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_') {
            idx++;
            continue;
        }
        break;
    }
    if (idx <= samplerStart) {
        return NO;
    }
    NSString *firstArg = vrend_trim([source substringWithRange:NSMakeRange(samplerStart, idx - samplerStart)]);
    while (idx < length && [whitespace characterIsMember:[source characterAtIndex:idx]]) {
        idx++;
    }
    if (idx >= length || [source characterAtIndex:idx] != ',') {
        return NO;
    }
    idx++; /* skip comma */
    NSUInteger coordStart = idx;
    NSInteger depth = 0;
    BOOL hasExtra = NO;
    NSUInteger coordEnd = coordStart;
    while (idx < length) {
        unichar ch = [source characterAtIndex:idx];
        if (ch == '(' || ch == '[' || ch == '{') {
            depth++;
        } else if (ch == ')' || ch == ']' || ch == '}') {
            if (depth == 0) {
                if (ch != ')') {
                    return NO;
                }
                coordEnd = idx;
                break;
            }
            depth--;
        } else if (ch == ',' && depth == 0) {
            hasExtra = YES;
            coordEnd = idx;
            idx++;
            break;
        }
        idx++;
    }
    if (coordEnd < coordStart) {
        coordEnd = coordStart;
    }
    if (coordEnd > length) {
        coordEnd = length;
    }
    NSString *coordExpr = vrend_trim([source substringWithRange:NSMakeRange(coordStart, coordEnd - coordStart)]);
    if (coordExpr.length == 0) {
        return NO;
    }
    NSString *extraExpr = nil;
    if (hasExtra) {
        NSUInteger extraStart = idx;
        while (extraStart < length && [whitespace characterIsMember:[source characterAtIndex:extraStart]]) {
            extraStart++;
        }
        idx = extraStart;
        depth = 0;
        while (idx < length) {
            unichar ch = [source characterAtIndex:idx];
            if (ch == '(' || ch == '[' || ch == '{') {
                depth++;
            } else if (ch == ')' || ch == ']' || ch == '}') {
                if (depth == 0) {
                    if (ch != ')') {
                        return NO;
                    }
                    break;
                }
                depth--;
            }
            idx++;
        }
        if (idx >= length || [source characterAtIndex:idx] != ')') {
            return NO;
        }
        NSUInteger extraEnd = idx;
        extraExpr = vrend_trim([source substringWithRange:NSMakeRange(extraStart, extraEnd - extraStart)]);
    } else {
        if (idx >= length || [source characterAtIndex:idx] != ')') {
            return NO;
        }
    }
    if (outFirstArg) {
        *outFirstArg = firstArg;
    }
    if (outCoordExpr) {
        *outCoordExpr = coordExpr;
    }
    if (outExtraExpr) {
        *outExtraExpr = extraExpr;
    }
    if (outClosingParenIndex) {
        *outClosingParenIndex = idx;
    }
    return YES;
}

static NSDictionary *vrend_sampler_element_for_argument(NSDictionary *sampler,
                                                        NSString *argument) {
    if (!sampler || [argument length] == 0) {
        return nil;
    }
    NSArray *elements = sampler[@"elements"];
    if ([elements count] == 0) {
        NSString *textureParam = sampler[@"textureParam"];
        NSString *samplerParam = sampler[@"samplerParam"];
        if (textureParam && samplerParam) {
            return @{ @"textureParam": textureParam,
                      @"samplerParam": samplerParam,
                      @"index": @0 };
        }
        return nil;
    }
    NSString *samplerName = vrend_string_or_default(sampler[@"name"], @"");
    if ([argument isEqualToString:samplerName]) {
        return [elements firstObject];
    }
    NSRange bracketRange = NSMakeRange(NSNotFound, 0);
    for (NSUInteger scan = 0; scan < [argument length]; scan++) {
        if ([argument characterAtIndex:scan] == '[') {
            bracketRange.location = scan;
            bracketRange.length = 1;
            break;
        }
    }
    if (bracketRange.location == NSNotFound || ![argument hasSuffix:@"]"]) {
        return nil;
    }
    NSString *baseName = [argument substringToIndex:bracketRange.location];
    if (![baseName isEqualToString:samplerName]) {
        return nil;
    }
    NSString *indexString = [argument substringWithRange:NSMakeRange(bracketRange.location + 1,
                                                                    [argument length] - bracketRange.location - 2)];
    indexString = vrend_trim(indexString);
    NSCharacterSet *nonDigits = [[NSCharacterSet decimalDigitCharacterSet] invertedSet];
    if ([indexString rangeOfCharacterFromSet:nonDigits].location != NSNotFound) {
        return nil;
    }
    NSInteger elementIndex = [indexString integerValue];
    if (elementIndex < 0 || elementIndex >= (NSInteger)[elements count]) {
        return nil;
    }
    return elements[(NSUInteger)elementIndex];
}

static NSString *vrend_metal_replace_sampler_function(NSString *line,
                                                      NSDictionary *sampler,
                                                      NSString *functionName,
                                                      BOOL projection,
                                                      vrend_texture_modifier modifier) {
    if (line.length == 0) {
        return line;
    }
    NSMutableString *mutableLine = [line mutableCopy];
    NSString *samplerName = sampler[@"name"];
    if (![samplerName length]) {
        return line;
    }
    NSUInteger searchIndex = 0;
    while (searchIndex < [mutableLine length]) {
        NSRange funcRange = [mutableLine rangeOfString:functionName
                                                options:0
                                                  range:NSMakeRange(searchIndex, [mutableLine length] - searchIndex)];
        if (funcRange.location == NSNotFound) {
            break;
        }
        NSUInteger parenIndex = funcRange.location + funcRange.length;
        if (parenIndex >= [mutableLine length] || [mutableLine characterAtIndex:parenIndex] != '(') {
            searchIndex = funcRange.location + funcRange.length;
            continue;
        }
        NSString *currentString = [mutableLine copy];
        NSString *firstArg = nil;
        NSString *coordExpr = nil;
        NSString *extraExpr = nil;
        NSUInteger closingIndex = 0;
        if (!vrend_parse_texture_call_arguments(currentString,
                                                parenIndex,
                                                &firstArg,
                                                &coordExpr,
                                                &extraExpr,
                                                &closingIndex)) {
            searchIndex = funcRange.location + funcRange.length;
            continue;
        }
        NSDictionary *elementInfo = vrend_sampler_element_for_argument(sampler, firstArg);
        if (!elementInfo) {
            searchIndex = funcRange.location + funcRange.length;
            continue;
        }
        NSString *textureParam = elementInfo[@"textureParam"];
        NSString *samplerParam = elementInfo[@"samplerParam"];
        if (![textureParam length] || ![samplerParam length]) {
            searchIndex = funcRange.location + funcRange.length;
            continue;
        }
        NSString *sampleCoord = coordExpr;
        if (projection) {
            sampleCoord = [NSString stringWithFormat:@"vrend_project_coord(%@)", coordExpr];
        }
        NSString *replacement = nil;
        switch (modifier) {
            case VREND_TEXTURE_MOD_NONE:
                replacement = [NSString stringWithFormat:@"%@.sample(%@, %@)",
                                textureParam, samplerParam, sampleCoord];
                break;
            case VREND_TEXTURE_MOD_LOD:
                if ([extraExpr length] == 0) {
                    searchIndex = funcRange.location + funcRange.length;
                    continue;
                }
                replacement = [NSString stringWithFormat:@"%@.sample(%@, %@, level(%@))",
                                textureParam, samplerParam, sampleCoord, extraExpr];
                break;
            case VREND_TEXTURE_MOD_BIAS:
                if ([extraExpr length] == 0) {
                    searchIndex = funcRange.location + funcRange.length;
                    continue;
                }
                replacement = [NSString stringWithFormat:@"%@.sample(%@, %@, bias(%@))",
                                textureParam, samplerParam, sampleCoord, extraExpr];
                break;
            default:
                break;
        }
        if (![replacement length]) {
            searchIndex = funcRange.location + funcRange.length;
            continue;
        }
        NSRange replaceRange = NSMakeRange(funcRange.location, closingIndex - funcRange.location + 1);
        [mutableLine replaceCharactersInRange:replaceRange withString:replacement];
        searchIndex = funcRange.location + [replacement length];
    }
    return mutableLine;
}

static NSArray<NSDictionary *> *vrend_metal_texture_function_mappings(void) {
    static NSArray<NSDictionary *> *mappings = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        mappings = @[
            @{ @"name": @"texture2DProjLod", @"projection": @YES, @"modifier": @(VREND_TEXTURE_MOD_LOD) },
            @{ @"name": @"textureProjLod", @"projection": @YES, @"modifier": @(VREND_TEXTURE_MOD_LOD) },
            @{ @"name": @"texture2DProjBias", @"projection": @YES, @"modifier": @(VREND_TEXTURE_MOD_BIAS) },
            @{ @"name": @"textureProjBias", @"projection": @YES, @"modifier": @(VREND_TEXTURE_MOD_BIAS) },
            @{ @"name": @"texture2DProj", @"projection": @YES, @"modifier": @(VREND_TEXTURE_MOD_NONE) },
            @{ @"name": @"textureProj", @"projection": @YES, @"modifier": @(VREND_TEXTURE_MOD_NONE) },
            @{ @"name": @"texture2DLod", @"projection": @NO, @"modifier": @(VREND_TEXTURE_MOD_LOD) },
            @{ @"name": @"textureLod", @"projection": @NO, @"modifier": @(VREND_TEXTURE_MOD_LOD) },
            @{ @"name": @"texture2DBias", @"projection": @NO, @"modifier": @(VREND_TEXTURE_MOD_BIAS) },
            @{ @"name": @"textureBias", @"projection": @NO, @"modifier": @(VREND_TEXTURE_MOD_BIAS) },
            @{ @"name": @"texture2D", @"projection": @NO, @"modifier": @(VREND_TEXTURE_MOD_NONE) },
            @{ @"name": @"texture", @"projection": @NO, @"modifier": @(VREND_TEXTURE_MOD_NONE) },
        ];
    });
    return mappings;
}

static NSString *vrend_metal_convert_texture_calls(NSString *line, NSArray<NSDictionary *> *samplers) {
    NSString *converted = line;
    NSArray<NSDictionary *> *mappings = vrend_metal_texture_function_mappings();
    for (NSDictionary *sampler in samplers) {
        for (NSDictionary *entry in mappings) {
            NSString *name = entry[@"name"];
            BOOL projection = [entry[@"projection"] boolValue];
            vrend_texture_modifier modifier = (vrend_texture_modifier)[entry[@"modifier"] integerValue];
            converted = vrend_metal_replace_sampler_function(converted, sampler, name, projection, modifier);
        }
    }
    return converted;
}

/* GLSL → MSL translation - ported from your metal_server.m */
char* vrend_metal_translate_glsl_to_msl(
    const char *glsl_source,
    enum vrend_shader_type type) {
    
    if (!glsl_source) {
        return NULL;
    }

#if VREND_METAL_HAVE_MGL_TOOLCHAIN
    {
        mgl_toolchain_stage stage = MGL_TOOLCHAIN_STAGE_VERTEX;
        switch (type) {
            case VREND_SHADER_VERTEX:
                stage = MGL_TOOLCHAIN_STAGE_VERTEX;
                break;
            case VREND_SHADER_FRAGMENT:
                stage = MGL_TOOLCHAIN_STAGE_FRAGMENT;
                break;
            case VREND_SHADER_COMPUTE:
                stage = MGL_TOOLCHAIN_STAGE_COMPUTE;
                break;
        }

        char *msl_out = NULL;
        char *err = NULL;
        int rc = mgl_toolchain_glsl_to_msl(stage, glsl_source, 0, &msl_out, NULL, &err);
        if (rc == 0 && msl_out) {
            const char *marker = "/* VREND_MGL_TOOLCHAIN */\n";
            size_t marker_len = strlen(marker);
            size_t msl_len = strlen(msl_out);
            char *merged = (char *)malloc(marker_len + msl_len + 1);
            if (merged) {
                memcpy(merged, marker, marker_len);
                memcpy(merged + marker_len, msl_out, msl_len + 1);
            }
            mgl_toolchain_free(msl_out);
            if (err) {
                mgl_toolchain_free(err);
            }
            return merged;
        }

        if (err) {
            NSLog(@"[Shader] MGL toolchain failed, falling back: %s", err);
            mgl_toolchain_free(err);
        }
        if (msl_out) {
            mgl_toolchain_free(msl_out);
        }
    }
#endif
    
    @autoreleasepool {
                NSString *glsl = [NSString stringWithUTF8String:glsl_source];
                NSError *mainRegexError = nil;
                NSRegularExpression *mainRegex = [NSRegularExpression regularExpressionWithPattern:@"void\\s+main\\s*(\\([^)]*\\))"
                                                                                                                                                                         options:0
                                                                                                                                                                             error:&mainRegexError];
                if (!mainRegexError && mainRegex) {
                        NSString *renamed = [mainRegex stringByReplacingMatchesInString:glsl
                                                                                                                                        options:0
                                                                                                                                            range:NSMakeRange(0, [glsl length])
                                                                                                                             withTemplate:@"void vrend_glsl_main$1"];
                        if ([renamed length] > 0) {
                                glsl = renamed;
                        }
                }
        NSMutableString *msl = [NSMutableString string];
        
        /* Add Metal standard library header */
        [msl appendString:@"#include <metal_stdlib>\n"];
        [msl appendString:@"using namespace metal;\n\n"];
        [msl appendString:@"static inline float2 vrend_project_coord(float3 coord) { return coord.xy / coord.z; }\n"];
        [msl appendString:@"static inline float2 vrend_project_coord(float4 coord) { return coord.xy / coord.w; }\n\n"];
        
        /* Split into lines for processing */
        NSArray *lines = [glsl componentsSeparatedByString:@"\n"];
        NSMutableArray *mutable_lines = [lines mutableCopy];
        NSMutableArray<NSDictionary *> *vertex_attributes = [NSMutableArray array];
        NSMutableArray<NSDictionary *> *varyings = [NSMutableArray array];
        NSMutableArray<NSMutableDictionary *> *sampler_uniforms = [NSMutableArray array];
        BOOL uses_frag_color = NO;
        
        /* Track constants to prevent duplicates */
        NSMutableSet *declared_constants = [NSMutableSet set];
        
        /* Process each line */
        for (NSInteger i = 0; i < [mutable_lines count]; i++) {
            NSString *line = mutable_lines[i];
            NSString *trimmed = [line stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
            
            /* Skip GLSL version directives */
            if ([trimmed hasPrefix:@"#version"]) {
                mutable_lines[i] = @"";
                continue;
            }

            /* Handle const declarations */
            if ([trimmed hasPrefix:@"const "]) {
                NSError *error = nil;
                NSRegularExpression *regex =
                    [NSRegularExpression regularExpressionWithPattern:@"const\\s+\\w+\\s+(\\w+)\\s*="
                                                              options:0
                                                                error:&error];
                NSTextCheckingResult *match =
                    [regex firstMatchInString:trimmed
                                      options:0
                                        range:NSMakeRange(0, [trimmed length])];
                if (match && [match numberOfRanges] > 1) {
                    NSRange nameRange = [match rangeAtIndex:1];
                    NSString *constName = [trimmed substringWithRange:nameRange];
                    if ([declared_constants containsObject:constName]) {
                        NSLog(@"[Shader] Skipping duplicate const: %@", constName);
                        mutable_lines[i] = @"";
                        continue;
                    }
                    [declared_constants addObject:constName];
                    NSString *metalConst =
                        [trimmed stringByReplacingOccurrencesOfString:@"const "
                                                           withString:@"constant "];
                    mutable_lines[i] = metalConst;
                } else {
                    NSString *metalConst =
                        [line stringByReplacingOccurrencesOfString:@"const "
                                                        withString:@"constant "];
                    mutable_lines[i] = metalConst;
                }
                continue;
            }

            /* Handle varying → stage_in/out */
            if ([trimmed hasPrefix:@"varying "]) {
                static NSRegularExpression *varyingRegex = nil;
                static dispatch_once_t varyingOnce;
                dispatch_once(&varyingOnce, ^{
                    varyingRegex = [NSRegularExpression regularExpressionWithPattern:@"varying\\s+([\\w\\d_]+)\\s+([\\w\\d_]+)"
                                                                              options:0
                                                                                error:NULL];
                });
                NSTextCheckingResult *match =
                    [varyingRegex firstMatchInString:trimmed
                                              options:0
                                                range:NSMakeRange(0, [trimmed length])];
                if (match && [match numberOfRanges] >= 3) {
                    NSString *glslType = [trimmed substringWithRange:[match rangeAtIndex:1]];
                    NSString *name = [trimmed substringWithRange:[match rangeAtIndex:2]];
                    NSString *metalType = vrend_msl_type_for_glsl_type(glslType);
                    NSString *varyingType = vrend_string_or_default(metalType, glslType);
                    NSString *varyingName = vrend_string_or_default(name, @"varying");
                    [varyings addObject:@{ @"type": varyingType,
                                            @"name": varyingName }];
                }
                if (type == VREND_SHADER_VERTEX) {
                    mutable_lines[i] =
                        [line stringByReplacingOccurrencesOfString:@"varying "
                                                        withString:@"/* vertex_out */ "];
                } else {
                    mutable_lines[i] =
                        [line stringByReplacingOccurrencesOfString:@"varying "
                                                        withString:@"/* fragment_in */ "];
                }
                continue;
            }

            /* Handle attribute → stage_in */
            if ([trimmed hasPrefix:@"attribute "]) {
                static NSRegularExpression *attributeRegex = nil;
                static dispatch_once_t attrOnce;
                dispatch_once(&attrOnce, ^{
                    attributeRegex = [NSRegularExpression regularExpressionWithPattern:@"attribute\\s+([\\w\\d_]+)\\s+([\\w\\d_]+)"
                                                                                 options:0
                                                                                   error:NULL];
                });
                NSTextCheckingResult *match =
                    [attributeRegex firstMatchInString:trimmed
                                                options:0
                                                  range:NSMakeRange(0, [trimmed length])];
                if (match && [match numberOfRanges] >= 3) {
                    NSString *glslType = [trimmed substringWithRange:[match rangeAtIndex:1]];
                    NSString *name = [trimmed substringWithRange:[match rangeAtIndex:2]];
                    NSString *metalType = vrend_msl_type_for_glsl_type(glslType);
                    NSString *attributeType = vrend_string_or_default(metalType, glslType);
                    NSString *attributeName = vrend_string_or_default(name, @"attr");
                    [vertex_attributes addObject:@{ @"type": attributeType,
                                                    @"name": attributeName }];
                }
                mutable_lines[i] =
                    [line stringByReplacingOccurrencesOfString:@"attribute "
                                                    withString:@"/* vertex_in */ "];
                continue;
            }

            /* Handle uniform → constant or sampler bindings */
            if ([trimmed hasPrefix:@"uniform "]) {
                static NSRegularExpression *samplerRegex = nil;
                static dispatch_once_t samplerOnce;
                dispatch_once(&samplerOnce, ^{
                    samplerRegex = [NSRegularExpression regularExpressionWithPattern:@"uniform\\s+(sampler\\w+)\\s+([\\w\\d_]+)(?:\\s*\\[\\s*(\\d+)\\s*\\])?"
                                                                                options:0
                                                                                  error:NULL];
                });
                NSTextCheckingResult *samplerMatch =
                    [samplerRegex firstMatchInString:trimmed
                                              options:0
                                                range:NSMakeRange(0, [trimmed length])];
                if (samplerMatch && [samplerMatch numberOfRanges] >= 3) {
                    NSString *samplerType = [trimmed substringWithRange:[samplerMatch rangeAtIndex:1]];
                    NSString *samplerName = [trimmed substringWithRange:[samplerMatch rangeAtIndex:2]];
                    NSString *arraySizeString = nil;
                    if ([samplerMatch numberOfRanges] >= 4) {
                        NSRange sizeRange = [samplerMatch rangeAtIndex:3];
                        if (sizeRange.location != NSNotFound) {
                            arraySizeString = [trimmed substringWithRange:sizeRange];
                        }
                    }
                    NSString *metalTextureType = vrend_metal_texture_type_for_sampler(samplerType);
                    if (metalTextureType) {
                        NSUInteger arraySize = (NSUInteger)[arraySizeString integerValue];
                        if (arraySize < 1) {
                            arraySize = 1;
                        }
                        NSMutableArray *elements = [NSMutableArray array];
                        for (NSUInteger elemIndex = 0; elemIndex < arraySize; elemIndex++) {
                            NSString *textureParamName = [NSString stringWithFormat:@"%@_%lu_tex", samplerName, (unsigned long)elemIndex];
                            NSString *samplerParamName = [NSString stringWithFormat:@"%@_%lu_samp", samplerName, (unsigned long)elemIndex];
                            NSMutableDictionary *elementInfo = [@{ @"index": @(elemIndex),
                                                                   @"textureParam": textureParamName,
                                                                   @"samplerParam": samplerParamName } mutableCopy];
                            [elements addObject:elementInfo];
                        }
                        NSMutableDictionary *samplerInfo = [@{ @"name": samplerName,
                                                               @"textureType": metalTextureType,
                                                               @"arraySize": @(arraySize),
                                                               @"elements": elements } mutableCopy];
                        NSDictionary *firstElement = [elements firstObject];
                        if (firstElement) {
                            samplerInfo[@"textureParam"] = firstElement[@"textureParam"];
                            samplerInfo[@"samplerParam"] = firstElement[@"samplerParam"];
                        }
                        [sampler_uniforms addObject:samplerInfo];
                        mutable_lines[i] = @"";
                        continue;
                    }
                }
                NSString *metalUniform =
                    [line stringByReplacingOccurrencesOfString:@"uniform "
                                                    withString:@"constant "];
                mutable_lines[i] = metalUniform;
                continue;
            }

            /* Handle gl_FragColor */
            if ([trimmed containsString:@"gl_FragColor"]) {
                uses_frag_color = YES;
                line = [line stringByReplacingOccurrencesOfString:@"gl_FragColor"
                                                       withString:@"out_color"];
                trimmed = [line stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
            }

            if ([line length] > 0 && [sampler_uniforms count] > 0) {
                line = vrend_metal_convert_texture_calls(line, sampler_uniforms);
            }
            mutable_lines[i] = line;
        }
        
        if (type == VREND_SHADER_VERTEX) {
            [msl appendString:@"float4 gl_Position;\n"];
        }
        if (type == VREND_SHADER_FRAGMENT) {
            [msl appendString:@"float4 out_color;\n"];
        }
        
        if ([vertex_attributes count] > 0 && type == VREND_SHADER_VERTEX) {
            [msl appendString:@"struct VrMetalVertexIn {\n"];
            for (NSUInteger idx = 0; idx < [vertex_attributes count]; idx++) {
                NSDictionary *entry = vertex_attributes[idx];
                NSString *attrType = entry[@"type"];
                if (!attrType) {
                    attrType = @"float4";
                }
                NSString *attrName = entry[@"name"];
                if (!attrName) {
                    attrName = [NSString stringWithFormat:@"attr%lu", (unsigned long)idx];
                }
                [msl appendFormat:@"    %@ %@ [[attribute(%lu)]];\n", attrType, attrName, (unsigned long)idx];
            }
            [msl appendString:@"};\n\n"];
        }

        if (type == VREND_SHADER_VERTEX || type == VREND_SHADER_FRAGMENT) {
            [msl appendString:@"struct VrMetalVaryingPayload {\n"];
            [msl appendString:@"    float4 position [[position]];\n"];
            for (NSUInteger idx = 0; idx < [varyings count]; idx++) {
                NSDictionary *entry = varyings[idx];
                NSString *varType = entry[@"type"];
                if (!varType) {
                    varType = @"float4";
                }
                NSString *varName = entry[@"name"];
                if (!varName) {
                    varName = [NSString stringWithFormat:@"varying%lu", (unsigned long)idx];
                }
                [msl appendFormat:@"    %@ %@ [[user(locn%lu)]];\n", varType, varName, (unsigned long)idx];
            }
            [msl appendString:@"};\n\n"];
        }

        /* Reassemble shader */
        for (NSString *line in mutable_lines) {
            if ([line length] > 0) {
                [msl appendString:line];
                [msl appendString:@"\n"];
            }
        }
        
        /* Add function signature based on type */
        if (type == VREND_SHADER_VERTEX) {
            [msl appendString:@"\n/* Vertex shader entry point */\n"];
            NSMutableArray<NSString *> *vertexParams = [NSMutableArray array];
            if ([vertex_attributes count] > 0) {
                [vertexParams addObject:@"VrMetalVertexIn in [[stage_in]]"];
            } else {
                [vertexParams addObject:@"uint vertex_id [[vertex_id]]"];
            }
            NSUInteger vertexTextureSlot = 0;
            for (NSDictionary *sampler in sampler_uniforms) {
                NSString *textureType = vrend_string_or_default(sampler[@"textureType"], @"texture2d<float>");
                NSArray *elements = sampler[@"elements"];
                NSString *samplerBaseName = vrend_string_or_default(sampler[@"name"], @"tex");
                if ([elements count] == 0) {
                    NSString *textureParam = sampler[@"textureParam"];
                    if (!textureParam) {
                        textureParam = [NSString stringWithFormat:@"%@_%lu_tex", samplerBaseName, (unsigned long)vertexTextureSlot];
                    }
                    NSString *samplerParam = sampler[@"samplerParam"];
                    if (!samplerParam) {
                        samplerParam = [NSString stringWithFormat:@"%@_%lu_samp", samplerBaseName, (unsigned long)vertexTextureSlot];
                    }
                    [vertexParams addObject:[NSString stringWithFormat:@"%@ %@ [[texture(%lu)]]",
                                             textureType, textureParam, (unsigned long)vertexTextureSlot]];
                    [vertexParams addObject:[NSString stringWithFormat:@"sampler %@ [[sampler(%lu)]]",
                                             samplerParam, (unsigned long)vertexTextureSlot]];
                    vertexTextureSlot++;
                    continue;
                }
                for (NSDictionary *element in elements) {
                    NSUInteger elementIndex = [element[@"index"] unsignedIntegerValue];
                    NSString *textureParam = element[@"textureParam"];
                    if (!textureParam) {
                        textureParam = [NSString stringWithFormat:@"%@_%lu_tex", samplerBaseName, (unsigned long)elementIndex];
                    }
                    NSString *samplerParam = element[@"samplerParam"];
                    if (!samplerParam) {
                        samplerParam = [NSString stringWithFormat:@"%@_%lu_samp", samplerBaseName, (unsigned long)elementIndex];
                    }
                    [vertexParams addObject:[NSString stringWithFormat:@"%@ %@ [[texture(%lu)]]",
                                             textureType, textureParam, (unsigned long)vertexTextureSlot]];
                    [vertexParams addObject:[NSString stringWithFormat:@"sampler %@ [[sampler(%lu)]]",
                                             samplerParam, (unsigned long)vertexTextureSlot]];
                    vertexTextureSlot++;
                }
            }
            NSString *vertexParamList = [vertexParams count] ? [vertexParams componentsJoinedByString:@", "] : @"void";
            [msl appendFormat:@"vertex VrMetalVaryingPayload vertex_main(%@) {\n", vertexParamList];
            if ([vertex_attributes count] > 0) {
                for (NSDictionary *entry in vertex_attributes) {
                    NSString *attrName = entry[@"name"];
                    if (!attrName) {
                        attrName = @"attr";
                    }
                    [msl appendFormat:@"    %@ = in.%@;\n", attrName, attrName];
                }
            } else {
                [msl appendString:@"    (void)vertex_id;\n"];
            }
            [msl appendString:@"    gl_Position = float4(0.0);\n"];
            [msl appendString:@"    vrend_glsl_main();\n"];
            [msl appendString:@"    VrMetalVaryingPayload outPayload;\n"];
            [msl appendString:@"    outPayload.position = gl_Position;\n"];
            for (NSDictionary *entry in varyings) {
                NSString *varName = entry[@"name"];
                if (!varName) {
                    varName = @"varying";
                }
                [msl appendFormat:@"    outPayload.%@ = %@;\n", varName, varName];
            }
            [msl appendString:@"    return outPayload;\n"];
            [msl appendString:@"}\n"];
        } else if (type == VREND_SHADER_FRAGMENT) {
            [msl appendString:@"\n/* Fragment shader entry point */\n"];
            NSMutableArray<NSString *> *fragmentParams = [NSMutableArray array];
            [fragmentParams addObject:@"VrMetalVaryingPayload in [[stage_in]]"];
            NSUInteger fragmentTextureSlot = 0;
            for (NSDictionary *sampler in sampler_uniforms) {
                NSString *textureType = vrend_string_or_default(sampler[@"textureType"], @"texture2d<float>");
                NSArray *elements = sampler[@"elements"];
                NSString *samplerBaseName = vrend_string_or_default(sampler[@"name"], @"tex");
                if ([elements count] == 0) {
                    NSString *textureParam = sampler[@"textureParam"];
                    if (!textureParam) {
                        textureParam = [NSString stringWithFormat:@"%@_%lu_tex", samplerBaseName, (unsigned long)fragmentTextureSlot];
                    }
                    NSString *samplerParam = sampler[@"samplerParam"];
                    if (!samplerParam) {
                        samplerParam = [NSString stringWithFormat:@"%@_%lu_samp", samplerBaseName, (unsigned long)fragmentTextureSlot];
                    }
                    [fragmentParams addObject:[NSString stringWithFormat:@"%@ %@ [[texture(%lu)]]",
                                               textureType, textureParam, (unsigned long)fragmentTextureSlot]];
                    [fragmentParams addObject:[NSString stringWithFormat:@"sampler %@ [[sampler(%lu)]]",
                                               samplerParam, (unsigned long)fragmentTextureSlot]];
                    fragmentTextureSlot++;
                    continue;
                }
                for (NSDictionary *element in elements) {
                    NSUInteger elementIndex = [element[@"index"] unsignedIntegerValue];
                    NSString *textureParam = element[@"textureParam"];
                    if (!textureParam) {
                        textureParam = [NSString stringWithFormat:@"%@_%lu_tex", samplerBaseName, (unsigned long)elementIndex];
                    }
                    NSString *samplerParam = element[@"samplerParam"];
                    if (!samplerParam) {
                        samplerParam = [NSString stringWithFormat:@"%@_%lu_samp", samplerBaseName, (unsigned long)elementIndex];
                    }
                    [fragmentParams addObject:[NSString stringWithFormat:@"%@ %@ [[texture(%lu)]]",
                                               textureType, textureParam, (unsigned long)fragmentTextureSlot]];
                    [fragmentParams addObject:[NSString stringWithFormat:@"sampler %@ [[sampler(%lu)]]",
                                               samplerParam, (unsigned long)fragmentTextureSlot]];
                    fragmentTextureSlot++;
                }
            }
            NSString *fragmentParamList = [fragmentParams componentsJoinedByString:@", "];
            [msl appendFormat:@"fragment float4 fragment_main(%@) {\n", fragmentParamList];
            for (NSDictionary *entry in varyings) {
                NSString *varName = entry[@"name"];
                if (!varName) {
                    varName = @"varying";
                }
                [msl appendFormat:@"    %@ = in.%@;\n", varName, varName];
            }
            [msl appendString:@"    out_color = float4(0.0);\n"];
            [msl appendString:@"    vrend_glsl_main();\n"];
            [msl appendString:@"    return out_color;\n"];
            [msl appendString:@"}\n"];
        }
        
        /* Convert to C string */
        const char *cstr = [msl UTF8String];
        return cstr ? strdup(cstr) : NULL;
    }
}

/* Compile MSL source to Metal function */
id<MTLFunction> vrend_metal_compile_shader(
    const char *msl_source,
    const char *entry_point) {
    
    if (!msl_source || !entry_point) {
        return nil;
    }
    
    @autoreleasepool {
        id<MTLDevice> device = vrend_metal_get_device();
        if (!device) {
            NSLog(@"[Shader] Metal device not initialized");
            return nil;
        }
        
        NSError *error = nil;
        NSString *source = [NSString stringWithUTF8String:msl_source];
        
        /* Compile shader library */
        id<MTLLibrary> library = 
            [device newLibraryWithSource:source
                                 options:nil
                                   error:&error];
        
        if (!library) {
            NSLog(@"[Shader] Compilation failed: %@", error);
            return nil;
        }
        
        /* Get function from library */
        NSString *entryName = [NSString stringWithUTF8String:entry_point];
        id<MTLFunction> function = [library newFunctionWithName:entryName];
        
        if (!function) {
            NSLog(@"[Shader] Function '%s' not found in library", entry_point);
            return nil;
        }
        
        NSLog(@"[Shader] Compiled shader: %s", entry_point);
        return function;
    }
}

/* Create shader from GLSL source */
struct vrend_metal_shader* vrend_metal_shader_create(
    uint32_t shader_id,
    enum vrend_shader_type type,
    const char *glsl_source,
    uint32_t glsl_len) {
    
    @autoreleasepool {
        struct vrend_metal_shader *shader = calloc(1, sizeof(*shader));
        if (!shader) {
            return NULL;
        }
        
        shader->shader_id = shader_id;
        shader->type = type;
        
        /* Translate GLSL → MSL */
        shader->msl_source = vrend_metal_translate_glsl_to_msl(glsl_source, type);
        if (!shader->msl_source) {
            free(shader);
            return NULL;
        }
        
        /* Compile Metal shader */
        const char *entry_point =
            (type == VREND_SHADER_VERTEX) ? "vertex_main" : "fragment_main";

        if (shader->msl_source && strncmp(shader->msl_source, "/* VREND_MGL_TOOLCHAIN */", 24) == 0) {
            /* SPIRV-Cross MSL backend typically uses main0/main entrypoint names. */
            shader->metal_function = vrend_metal_compile_shader(shader->msl_source, "main0");
            if (!shader->metal_function) {
                shader->metal_function = vrend_metal_compile_shader(shader->msl_source, "main");
            }
        } else {
            shader->metal_function = vrend_metal_compile_shader(shader->msl_source, entry_point);
        }

        if (!shader->metal_function) {
            free(shader->msl_source);
            free(shader);
            return NULL;
        }
        
        NSLog(@"[Shader] Created shader %u: %s", 
              shader_id, (type == VREND_SHADER_VERTEX) ? "vertex" : "fragment");
        
        return shader;
    }
}

/* Destroy shader */
void vrend_metal_shader_destroy(struct vrend_metal_shader *shader) {
    if (!shader) {
        return;
    }
    
    @autoreleasepool {
        if (shader->metal_function) {
            shader->metal_function = nil;
        }
        
        if (shader->msl_source) {
            free(shader->msl_source);
            shader->msl_source = NULL;
        }
        
        free(shader);
    }
}
