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
#include "../../SDL_internal.h"

#if SDL_VIDEO_DRIVER_WIIU

#include "SDL_assert.h"
#include "SDL_surface.h"
#include "SDL_hints.h"
#include "SDL_render.h"

#include "SDL_wiiumouse.h"

#include "../SDL_sysvideo.h"
#include "../../events/SDL_mouse_c.h"
#include "../../events/default_cursor.h"
#include "SDL_render.h"
#include "SDL_events.h"

#include <nsyshid/hid.h>
#include <malloc.h>

static SDL_Cursor *WIIU_CreateCursor(SDL_Surface * surface, int hot_x, int hot_y);
static int WIIU_ShowCursor(SDL_Cursor * cursor);
static void WIIU_MoveCursor(SDL_Cursor * cursor);
static void WIIU_FreeCursor(SDL_Cursor * cursor);
static void WIIU_WarpMouse(SDL_Window * window, int x, int y);
static int WIIU_WarpMouseGlobal(int x, int y);
static void WIIU_MouseInputCallback(uint32_t handle, int32_t error, uint8_t *buffer, uint32_t bytesTransferred, void *userContext);
static int32_t WIIU_MouseAttachCallback(HIDClient *client, HIDDevice *device, HIDAttachEvent attach);

static HIDClient WIIU_MouseClient;

void
WIIU_RenderCursor(SDL_Renderer * renderer)
{
	SDL_Mouse *mouse = SDL_GetMouse();
	WIIU_MouseData *mdata = mouse->driverdata;
	WIIU_CursorData *cdata;
	
	/* check if the cursor should be rendered */
	if (!mdata->position_valid || !mdata->cursor_visible ||
		!mouse->cur_cursor || !mouse->cur_cursor->driverdata) {
		return;
	}
	cdata = mouse->cur_cursor->driverdata;
	
	/* first time cursor is drawn, create the texture */
	if (!cdata->texture) {
		cdata->texture = SDL_CreateTextureFromSurface(renderer, cdata->surface);
	}
	
	/* draw cursor */
	SDL_RenderCopy(renderer, cdata->texture, NULL, &cdata->rect);
}

/* Create a cursor from a surface */
static SDL_Cursor *
WIIU_CreateCursor(SDL_Surface * surface, int hot_x, int hot_y)
{
	WIIU_CursorData *cdata;

	SDL_Cursor *cursor = SDL_calloc(1, sizeof(*cursor));
    if (!cursor) {
        SDL_OutOfMemory();
		return NULL;
    }

    cursor->driverdata = SDL_calloc(1, sizeof(*cdata));
	if (!cursor->driverdata) {
        SDL_free(cursor);
		SDL_OutOfMemory();
        return NULL;
    }

    cdata = cursor->driverdata;
	cdata->surface = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA8888, 0);
	if (!cdata->surface) {
		SDL_free(cdata);
		SDL_free(cursor);
		SDL_OutOfMemory();
		return NULL;
	}
	cdata->hot_x = hot_x;
	cdata->hot_y = hot_y;
	cdata->rect.w = cdata->surface->w;
	cdata->rect.h = cdata->surface->h;
    
    return cursor;
}

/* Show the specified cursor, or hide if cursor is NULL */
static int
WIIU_ShowCursor(SDL_Cursor * cursor)
{
    WIIU_MouseData *mdata = SDL_GetMouse()->driverdata;
    mdata->cursor_visible = (cursor != NULL);
	return 0;
}

/* This is called when a mouse motion event occurs */
static void
WIIU_MoveCursor(SDL_Cursor * cursor)
{
    SDL_Mouse *mouse = SDL_GetMouse();
    WIIU_MouseData *mdata = mouse->driverdata;
    WIIU_CursorData *cdata = cursor->driverdata;
    
    /* The new cursor position is valid */
    mdata->position_valid = 1;
	
	/* Update cursor position */
	cdata->rect.x = mouse->x - cdata->hot_x;
	cdata->rect.y = mouse->y - cdata->hot_y;
}

/* Free a window manager cursor */
static void
WIIU_FreeCursor(SDL_Cursor * cursor)
{
    WIIU_CursorData *cdata = cursor->driverdata;
	if (cdata->texture) {
		SDL_DestroyTexture(cdata->texture);
	}
	SDL_free(cdata->surface);
	SDL_free(cdata);
    SDL_free(cursor);
}

/* Warp the mouse to (x,y) within a window */
static void
WIIU_WarpMouse(SDL_Window * window, int x, int y)
{
    WIIU_WarpMouseGlobal(x, y);
}

/* Warp the mouse to (x,y) in screen space */
static int
WIIU_WarpMouseGlobal(int x, int y)
{
    SDL_Mouse *mouse = SDL_GetMouse();
    SDL_SendMouseMotion(mouse->focus, mouse->mouseID, 0, x, y);
	return 0;
}

/* Called when a new input packet is availablel from the mouse */
static void
WIIU_MouseInputCallback(uint32_t handle, int32_t error, uint8_t *buffer, uint32_t bytesTransferred, void *userContext)
{
    static const Uint8 mouseButtonMap[3] = {SDL_BUTTON_LEFT, SDL_BUTTON_MIDDLE, SDL_BUTTON_RIGHT};
	uint8_t status, pressed, released;
	int x, y;
    SDL_Mouse *mouse = SDL_GetMouse();
    WIIU_MouseData *mdata = mouse->driverdata;
    
    if (error) {
        mdata->position_valid = 0;
        free(buffer);
        return;
    }
    
    /* process mouse motion */
    x = (int)((int8_t)buffer[1]);
    y = (int)((int8_t)buffer[2]);
    if (x || y) {
        SDL_SendMouseMotion(mouse->focus, mouse->mouseID, SDL_TRUE, x, y);
    }
    
    /* process mouse buttons */
    status = buffer[0];
	pressed = status & ~mdata->status;
	released = mdata->status & ~status;
    for (int i = 0; i < 3; i++) {
        if (pressed & (1 << i)) {
            SDL_SendMouseButton(mouse->focus, mouse->mouseID, SDL_PRESSED, mouseButtonMap[i]);
        }
        if (released & (1 << i)) {
            SDL_SendMouseButton(mouse->focus, mouse->mouseID, SDL_RELEASED, mouseButtonMap[i]);
        }
    }
    mdata->status = status;

    /* wait for the next input packet */
    HIDRead(handle, buffer, bytesTransferred, WIIU_MouseInputCallback, NULL);
}

/* Called when a new HID device is attached */
static int32_t
WIIU_MouseAttachCallback(HIDClient *client, HIDDevice *device, HIDAttachEvent attach)
{
	uint8_t *mouseBuffer;
    if ((device->subClass != 1) || (device->protocol != 2) || (attach == HID_DEVICE_DETACH)) {
        return 0;
    }

	mouseBuffer = memalign(64, device->maxPacketSizeRx);
    HIDSetProtocol(device->handle, device->interfaceIndex, 0, NULL, NULL);
    HIDSetIdle(device->handle, device->interfaceIndex, 0, NULL, NULL);
    HIDRead(device->handle, mouseBuffer, device->maxPacketSizeRx, WIIU_MouseInputCallback, NULL);

    return 1;
}

void
WIIU_InitMouse(_THIS)
{
	SDL_Cursor *default_cursor;
    SDL_Mouse *mouse = SDL_GetMouse();
	mouse->driverdata = SDL_calloc(1, sizeof(WIIU_MouseData));
	
    mouse->CreateCursor = WIIU_CreateCursor;
	mouse->ShowCursor = WIIU_ShowCursor;
	mouse->MoveCursor = WIIU_MoveCursor;
    mouse->FreeCursor = WIIU_FreeCursor;
    mouse->WarpMouse = WIIU_WarpMouse;
    mouse->WarpMouseGlobal = WIIU_WarpMouseGlobal;
    
	default_cursor = SDL_CreateCursor(
        default_cdata, default_cmask, 
        DEFAULT_CWIDTH, DEFAULT_CHEIGHT,
        DEFAULT_CHOTX, DEFAULT_CHOTY
    );
	SDL_SetDefaultCursor(default_cursor);
    
    HIDSetup();
	HIDAddClient(&WIIU_MouseClient, WIIU_MouseAttachCallback);
}

void
WIIU_QuitMouse(_THIS)
{
	SDL_Mouse *mouse = SDL_GetMouse();
	
	HIDDelClient(&WIIU_MouseClient);
    HIDTeardown();
	
	SDL_free(mouse->driverdata);
}


#endif /* SDL_VIDEO_DRIVER_WIIU */

/* vi: set ts=4 sw=4 expandtab: */
