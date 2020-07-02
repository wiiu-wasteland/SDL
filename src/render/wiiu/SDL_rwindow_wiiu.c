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

#if SDL_VIDEO_RENDER_WIIU

#include "../SDL_sysrender.h"
#include "SDL_render_wiiu.h"

void WIIU_SDL_WindowEvent(SDL_Renderer * renderer, const SDL_WindowEvent *event)
{
    if (event->event == SDL_WINDOWEVENT_SIZE_CHANGED) {
        WIIU_SDL_GetWindowColorBuffer();
    }
}

/* We always output at whatever res the window is.
   This may need to change if SDL_wiiuvideo is ever folded into SDL_render -
   see SDL_*WindowTexture from SDL_video.c for how this could be done */
int WIIU_SDL_GetOutputSize(SDL_Renderer * renderer, int *w, int *h)
{
    SDL_GetWindowSize(renderer->window, w, h);
    return 0;
}

/* We handle all viewport changes in the render functions and shaders, so we
   don't actually have to do anything here. SDL still requires we implement it. */
int WIIU_SDL_UpdateViewport(SDL_Renderer * renderer)
{
    return 0;
}

/* Ideally this should change the GX2SetScissor values, but SetRenderTarget
   needs refactoring first or these get overwritten. */
int WIIU_SDL_UpdateClipRect(SDL_Renderer * renderer)
{
    return 0;
}

#endif //SDL_VIDEO_RENDER_WIIU
