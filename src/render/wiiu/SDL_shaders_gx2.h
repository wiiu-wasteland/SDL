/*
 *  Simple DirectMedia Layer
 *  Copyright (C) 1997-2020 Sam Lantinga <slouken@libsdl.org>
 * 
 *  This software is provided 'as-is', without any express or implied
 *  warranty.  In no event will the authors be held liable for any damages
 *  arising from the use of this software.
 * 
 *  Permission is granted to anyone to use this software for any purpose,
 *  including commercial applications, and to alter it and redistribute it
 *  freely, subject to the following restrictions:
 * 
 *  1. The origin of this software must not be misrepresented; you must not
 *     claim that you wrote the original software. If you use this software
 *     in a product, an acknowledgment in the product documentation would be
 *     appreciated but is not required.
 *  2. Altered source versions must be plainly marked as such, and must not be
 *     misrepresented as being the original software.
 *  3. This notice may not be removed or altered from any source distribution.
 */
#include "../../SDL_internal.h"

#ifndef SDL_shaders_gx2_h_
#define SDL_shaders_gx2_h_

#if SDL_VIDEO_RENDER_WIIU

/* GX2 shader implementation */

typedef enum {
    SHADER_INVALID = -1,
    SHADER_TEXTURE,
    SHADER_COLOR,
    NUM_SHADERS
} GX2_Shader;

typedef struct GX2_ShaderContext GX2_ShaderContext;

extern GX2_ShaderContext * GX2_CreateShaderContext(void);
extern void GX2_SelectShader(GX2_ShaderContext *ctx, GX2_Shader shader);
extern void GX2_SetAttributeBuffer(GX2_ShaderContext *ctx, uint32_t index, uint32_t count, uint32_t stride, void *data);
extern void GX2_SetVertexShaderUniform(GX2_ShaderContext *ctx, uint32_t index, uint32_t count, void *data);
extern void GX2_SetFragmentShaderUniform(GX2_ShaderContext *ctx, uint32_t index, uint32_t count, void *data);
extern void GX2_DestroyShaderContext(GX2_ShaderContext *ctx);

#endif /* SDL_VIDEO_RENDER_WIIU */

#endif /* SDL_shaders_gx2_h_ */

/* vi: set ts=4 sw=4 expandtab: */


