/*
  Simple DirectMedia Layer
  Copyright (C) 2018-2018 Ash Logan <ash@heyquark.com>
  Copyright (C) 2018-2018 Roberto Van Eeden <r.r.qwertyuiop.r.r@gmail.com>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

#include "../../SDL_internal.h"

#ifndef SDL_wiiuvideo_h
#define SDL_wiiuvideo_h

#include <gx2/texture.h>
#include "SDL_surface.h"

#define WIIU_WINDOW_DATA "_SDL_WiiUData"

struct VWIIUVec2
{
	union { float x, r; };
	union { float y, g; };
};

struct VWIIUVec3
{
	union { float x, r; };
	union { float y, g; };
	union { float z, b; };
};

struct VWIIUVec4
{
	union { float x, r; };
	union { float y, g; };
	union { float z, b; };
	union { float w, a; };
};

typedef struct
{
	GX2Sampler sampler;
	GX2Texture texture;
	GX2ColorBuffer cbuf;
	WIIUVec4 u_texSize;
	WIIUVec4 u_mod;
} WIIU_WindowBufferData;

typedef struct
{
    GX2Sampler sampler;
    GX2RBuffer a_position;
    GX2RBuffer a_texCoord;
    WIIUVec4 u_texSize;
	struct {
		GX2Texture texture;
		GX2ColorBuffer cbuf;
	} buffer[2];
	int currentBuffer;
} SDL_WindowData;

#endif //SDL_wiiuvideo_h
