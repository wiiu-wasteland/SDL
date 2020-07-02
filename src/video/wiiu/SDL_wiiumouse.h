/*
  Simple DirectMedia Layer
  Copyright (C) 2018-2019 Ash Logan <ash@heyquark.com>
  Copyright (C) 2018-2019 Roberto Van Eeden <r.r.qwertyuiop.r.r@gmail.com>

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
#ifndef SDL_WIIU_mouse_h_
#define SDL_WIIU_mouse_h_

#include "../SDL_sysvideo.h"
#include "SDL_render.h"

typedef struct _WIIU_CursorData WIIU_CursorData;
struct _WIIU_CursorData
{
	int hot_x, hot_y;
	SDL_Surface *surface;
	SDL_Texture *texture;
	SDL_Rect rect;
};

typedef struct _WIIU_MouseData WIIU_MouseData;
struct _WIIU_MouseData
{
	uint8_t status;
	int position_valid;
	int cursor_visible;
};

extern void WIIU_RenderCursor(SDL_Renderer * renderer);
extern void WIIU_InitMouse(_THIS);
extern void WIIU_QuitMouse(_THIS);

#endif /* SDL_WIIU_mouse_h_ */

/* vi: set ts=4 sw=4 expandtab: */
