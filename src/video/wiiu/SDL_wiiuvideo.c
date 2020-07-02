/*
  Simple DirectMedia Layer
  Copyright (C) 2018-2018 Ash Logan <ash@heyquark.com>
  Copyright (C) 2018-2018 rw-r-r-0644 <r.r.qwertyuiop.r.r@gmail.com>

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

/* SDL internals */
#include "../SDL_sysvideo.h"
#include "SDL_version.h"
#include "SDL_syswm.h"
#include "SDL_loadso.h"
#include "SDL_events.h"
#include "../../events/SDL_mouse_c.h"
#include "../../events/SDL_keyboard_c.h"
#include "SDL_wiiuvideo.h"
#include "SDL_wiiumouse.h"

#include <whb/proc.h>
#include <whb/gfx.h>
#include <string.h>
#include <stdint.h>

#include "wiiu_shaders.h"

static int WIIU_VideoInit(_THIS);
static int WIIU_SetDisplayMode(_THIS, SDL_VideoDisplay *display, SDL_DisplayMode *mode);
static void WIIU_VideoQuit(_THIS);
static void WIIU_PumpEvents(_THIS);

#define SCREEN_WIDTH    1280
#define SCREEN_HEIGHT   720



static GX2ContextState *currentState = NULL;

void
WIIU_SetGX2ContextState(GX2ContextState *state)
{
    if (currentState == state) {
        return;
    }
    currentState = state;
    GX2SetContextState(state);
}

static void
WIIU_DrawWindow(_THIS, SDL_Window *window)
{
    static const VWIIUVec4 u_viewSize = {(float)SCREEN_WIDTH, (float)SCREEN_HEIGHT,};
    static const VWIIUVec4 u_mod = {1.0f, 1.0f, 1.0f, 1.0f,};
    SDL_WindowData *data = (SDL_WindowData *) window->driverdata;
    GX2Texture *texture = &data->buffer[data->currentBuffer].texture;
    
    if (!texture->surface.image) {
        return;
    }
    
    wiiuSetTextureShader();
    
    GX2SetPixelUniformReg(wiiuTextureShader.pixelShader->uniformVars[0].offset, 4, (uint32_t*)&u_mod);
    GX2SetVertexUniformReg(wiiuTextureShader.vertexShader->uniformVars[0].offset, 4, (uint32_t *)&u_viewSize);
    GX2SetVertexUniformReg(wiiuTextureShader.vertexShader->uniformVars[1].offset, 4, (uint32_t *)&data->u_texSize);
    
    GX2RSetAttributeBuffer(&data->a_position, 0, data->a_position.elemSize, 0);
    GX2RSetAttributeBuffer(&data->a_texCoord, 1, data->a_texCoord.elemSize, 0);
    
    GX2SetPixelTexture(texture, wiiuTextureShader.pixelShader->samplerVars[0].location);
    GX2SetPixelSampler(&data->sampler, wiiuTextureShader.pixelShader->samplerVars[0].location);
    
    GX2SetColorControl(GX2_LOGIC_OP_COPY, 0x00, FALSE, TRUE);
    
    GX2DrawEx(GX2_PRIMITIVE_MODE_QUADS, 4, 0, 1);
}

static void
WIIU_GX2CallbackVSYNC(GX2EventType type, void *userData)
{
    SDL_VideoDevice *_this = (SDL_VideoDevice*) userData;
    SDL_Window *window;

    WHBGfxBeginRender();
    
    /* Render all TV windows */
    WHBGfxBeginRenderTV();
    WHBGfxClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    for (window = _this->windows; window; window = window->next) {
        if (!(flags & SDL_WINDOW_WIIU_GAMEPAD_ONLY)) {
            WIIU_DrawWindow(_this, window);
        }
    }
    WHBGfxFinishRenderTV();
    
    /* Render all DRC windows */
    WHBGfxBeginRenderDRC();
    WHBGfxClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    for (window = _this->windows; window; window = window->next) {
        if (!(flags & SDL_WINDOW_WIIU_TV_ONLY)) {
            WIIU_DrawWindow(_this, window);
        }
    }
    WHBGfxFinishRenderDRC();
    
    WHBGfxFinishRender();

    /* Restore previous context state */
    WIIU_SetGX2ContextState(savedState);
}

static SDL_bool
WIIU_GetWindowWMInfo(_THIS, SDL_Window * window, SDL_SysWMinfo * info)
{
    SDL_WindowData *data = (SDL_WindowData *) window->driverdata;

    if (info->version.major == SDL_MAJOR_VERSION &&
        info->version.minor == SDL_MINOR_VERSION) {
        info->subsystem = SDL_SYSWM_WIIU;
        info->info.wiiu.WCurrentBuffer = &data->currentBuffer;
        info->info.wiiu.WColorBuffers = {
            &data->buffer[0].cbuf,
            &data->buffer[1].cbuf,
        };
        return SDL_TRUE;
    }

    SDL_SetError("Application not compiled with SDL %d.%d",
                 SDL_MAJOR_VERSION, SDL_MINOR_VERSION);
    return SDL_FALSE;
}

/* Initialize window texture/color buffer
 * Called when creating/resizing the window */
static void
WIIU_CreateWindowTexture(SDL_Window *window)
{
    SDL_WindowData *data = (SDL_WindowData *) window->driverdata;
    BOOL res = TRUE;
    WIIUVec2 *a_texCoord_vals;
    GX2RResourceFlags surface_flags =
        GX2R_RESOURCE_BIND_TEXTURE | GX2R_RESOURCE_BIND_COLOR_BUFFER |
        GX2R_RESOURCE_USAGE_CPU_WRITE | GX2R_RESOURCE_USAGE_CPU_READ |
        GX2R_RESOURCE_USAGE_GPU_WRITE | GX2R_RESOURCE_USAGE_GPU_READ;
    
    data->buffer[0].texture.surface.width = window->w;
    data->buffer[0].texture.surface.height = window->h;
    data->buffer[0].texture.surface.format = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
    data->buffer[0].texture.surface.depth = 1;
    data->buffer[0].texture.surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
    data->buffer[0].texture.surface.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
    data->buffer[0].texture.surface.mipLevels = 1;
    data->buffer[0].texture.viewNumMips = 1;
    data->buffer[0].texture.viewNumSlices = 1;
    data->buffer[0].texture.compMap = 0x00010203;
    GX2CalcSurfaceSizeAndAlignment(&data->buffer[0].texture.surface);
    GX2InitTextureRegs(&data->buffer[0].texture);
    
    data->buffer[0].cbuf.surface = data->buffer[0].texture.surface;
    data->buffer[0].cbuf.viewNumSlices = 1;
    GX2InitColorBufferRegs(&data->buffer[0].cbuf);

    data->buffer[1].texture = data->buffer[0].texture;
    data->buffer[1].cbuf = data->buffer[0].cbuf;

    res &= GX2RCreateSurface(&data->buffer[0].texture.surface, surface_flags);
    res &= GX2RCreateSurface(&data->buffer[1].texture.surface, surface_flags);
    res &= GX2RCreateSurfaceUserMemory(
        &data->buffer[0].cbuf.surface,
        data->buffer[0].texture.surface.image,
        data->buffer[0].texture.surface.mipmaps,
        data->buffer[0].texture.surface.resourceFlags
    );
    res &= GX2RCreateSurfaceUserMemory(
        &data->buffer[1].cbuf.surface,
        data->buffer[1].texture.surface.image,
        data->buffer[1].texture.surface.mipmaps,
        data->buffer[1].texture.surface.resourceFlags
    );
    if (!res) {
        GX2RDestroySurfaceEx(&data->buffer[0].cbuf.surface, 0);
        GX2RDestroySurfaceEx(&data->buffer[1].cbuf.surface, 0);
        GX2RDestroySurfaceEx(&data->buffer[0].texture.surface, 0);
        GX2RDestroySurfaceEx(&data->buffer[1].texture.surface, 0);
        SDL_OutOfMemory();
        return;
    }

    data->u_texSize = (WIIUVec4) {window->w, window->h,};
    
    a_texCoord_vals = GX2RLockBufferEx(&data->a_texCoord, 0);
    a_texCoord_vals[0] = (WIIUVec2) {0.0f            , (float)window->h,};
    a_texCoord_vals[1] = (WIIUVec2) {(float)window->w, (float)window->h,};
    a_texCoord_vals[2] = (WIIUVec2) {(float)window->w, 0.0f            ,};
    a_texCoord_vals[3] = (WIIUVec2) {0.0f            , 0.0f            ,};
    GX2RUnlockBufferEx(&data->a_texCoord, 0);

    return 0;
}

static void
WIIU_DestroyWindowTexture()
{
    SDL_WindowData *data = (SDL_WindowData *) window->driverdata;
    GX2RDestroySurfaceEx(&data->buffer[0].cbuf.surface, 0);
    GX2RDestroySurfaceEx(&data->buffer[1].cbuf.surface, 0);
    GX2RDestroySurfaceEx(&data->buffer[0].texture.surface, 0);
    GX2RDestroySurfaceEx(&data->buffer[1].texture.surface, 0);
}

static void
WIIU_Update() {
    a_position_vals = GX2RLockBufferEx(a_position, 0);
    a_position_vals[0] = (WIIUVec2) {
        .x = win_x, .y = win_y
    };
    a_position_vals[1] = (WIIUVec2) {
        .x = win_x + win_w, .y = win_y
    };
    a_position_vals[2] = (WIIUVec2) {
        .x = win_x + win_w, .y = win_y + win_h
    };
    a_position_vals[3] = (WIIUVec2) {
        .x = win_x, .y = win_y + win_h
    };
    GX2RUnlockBufferEx(a_position, 0);
    
    /* Compute texture coords */

}


static void
WIIU_SetWindowSize(_THIS, SDL_Window * window)
{
    VWIIUVec2 *a_texCoord_vals;
    WIIU_WindowData *wdata = (WIIU_WindowData *)window->driverdata;
    
    /* Recreate window texture with the new size */
    WIIU_DestroyWindowTexture();
    WIIU_CreateWindowTexture();
    
    a_texCoord_vals = GX2RLockBufferEx(&wdata->a_texCoord, 0);
    a_texCoord_vals[0] = (VWIIUVec2) {.x = 0.0f,      .y = window->h};
    a_texCoord_vals[1] = (VWIIUVec2) {.x = window->w, .y = window->h};
    a_texCoord_vals[2] = (VWIIUVec2) {.x = window->w, .y = 0.0f     };
    a_texCoord_vals[3] = (VWIIUVec2) {.x = 0.0f,      .y = 0.0f     };
    GX2RUnlockBufferEx(&wdata->a_texCoord, 0);
    
    tdata->u_texSize = (VWIIUVec4) {
        .x = window->w,
        .y = window->h,
    };
}


static int WIIU_CreateSDLWindow(_THIS, SDL_Window *window)
{
    const char *s_hint;
    SDL_WindowData *data = (SDL_WindowData *) SDL_calloc(1, sizeof(*data));
    if (!data) {
        return SDL_OutOfMemory();
    }
    window->driverdata = data;

    /* Initialize texture sampler */
    s_hint = SDL_GetHint(SDL_HINT_RENDER_SCALE_QUALITY);
    if (!s_hint || !SDL_strcasecmp(s_hint, "nearest") || !SDL_strcasecmp(s_hint, "0")) {
        GX2InitSampler(&data->sampler, GX2_TEX_CLAMP_MODE_CLAMP, GX2_TEX_XY_FILTER_MODE_POINT);
    } else {
        GX2InitSampler(&data->sampler, GX2_TEX_CLAMP_MODE_CLAMP, GX2_TEX_XY_FILTER_MODE_LINEAR);
    }
    
    /* Create attribute buffers */
    data->a_position.flags = GX2R_RESOURCE_BIND_VERTEX_BUFFER | GX2R_RESOURCE_USAGE_CPU_WRITE;
    data->a_position.elemSize = sizeof(WIIUVec2);
    data->a_position.elemCount = 4;
    GX2RCreateBuffer(&data->a_position);
    
    data->a_texCoord.flags = GX2R_RESOURCE_BIND_VERTEX_BUFFER | GX2R_RESOURCE_USAGE_CPU_WRITE;
    data->a_texCoord.elemSize = sizeof(WIIUVec2);
    data->a_texCoord.elemCount = 4;
    GX2RCreateBuffer(&data->a_texCoord);
    
    /* Initialize window texture */
    WIIU_CreateWindowTexture(window);
    
    /* Set focus on window */
    SDL_SetKeyboardFocus(window);
    SDL_SetMouseFocus(window);
    
    return 0;
}

static int WIIU_VideoInit(_THIS)
{
    SDL_VideoDisplay tvDisplay, drcDisplay;
    SDL_DisplayMode mode;

    WHBProcInit();
    WHBGfxInit();

    WIIU_InitMouse(_this);
    
    // setup shader
    wiiuInitTextureShader();

    // setup render callback
    GX2SetEventCallback(GX2_EVENT_TYPE_VSYNC, &render_callback, _this);

    
    
    
    // add tv display
    GX2TVScanMode currentTvMode = GX2GetSystemTVScanMode();
    
    tvDisplay.name = "tv";

    mode.format = SDL_PIXELFORMAT_RGBA8888;
    mode.refresh_rate = 60;

    mode.w = 854; mode.h = 480;
    
    if ((currentTvMode == GX2_TV_SCAN_MODE_480I) ||
        (currentTvMode == GX2_TV_SCAN_MODE_480P)) {
        tvDisplay.
    }
    
    SDL_AddDisplayMode(&tvDisplay, &mode);

    mode.w = 1280; mode.h = 720;
    SDL_AddDisplayMode(&tvDisplay, &mode);

    mode.w = 1920; mode.h = 1080;
    SDL_AddDisplayMode(&tvDisplay, &mode);
    
    {
        case GX2_TV_SCAN_MODE_480I:
        case GX2_TV_SCAN_MODE_480P:
            sTvRenderMode = GX2_TV_RENDER_MODE_WIDE_480P;
            tvWidth = 854;
            tvHeight = 480;
            break;
        case GX2_TV_SCAN_MODE_1080I:
        case GX2_TV_SCAN_MODE_1080P:
            sTvRenderMode = GX2_TV_RENDER_MODE_WIDE_1080P;
            tvWidth = 1920;
            tvHeight = 1080;
            break;
        case GX2_TV_SCAN_MODE_720P:
        default:
            sTvRenderMode = GX2_TV_RENDER_MODE_WIDE_720P;
            tvWidth = 1280;
            tvHeight = 720;
            break;
    }
    
    SDL_AddVideoDisplay(&tvDisplay);
    
    
    tvMode[0] = (SDL_DisplayMode) {
        .w = 854, .h = 480,
    }
    
    
    switch(GX2GetSystemTVScanMode())
    {
        case GX2_TV_SCAN_MODE_480I:
        case GX2_TV_SCAN_MODE_480P:
            sTvRenderMode = GX2_TV_RENDER_MODE_WIDE_480P;
            tvWidth = 854;
            tvHeight = 480;
            break;
        case GX2_TV_SCAN_MODE_1080I:
        case GX2_TV_SCAN_MODE_1080P:
            sTvRenderMode = GX2_TV_RENDER_MODE_WIDE_1080P;
            tvWidth = 1920;
            tvHeight = 1080;
            break;
        case GX2_TV_SCAN_MODE_720P:
        default:
            sTvRenderMode = GX2_TV_RENDER_MODE_WIDE_720P;
            tvWidth = 1280;
            tvHeight = 720;
            break;
    }
    
    // add default mode (1280x720)
    mode.format = SDL_PIXELFORMAT_RGBA8888;
    mode.w = SCREEN_WIDTH;
    mode.h = SCREEN_HEIGHT;
    mode.refresh_rate = 60;
    mode.driverdata = NULL;
    if (SDL_AddBasicVideoDisplay(&mode) < 0) {
        return -1;
    }
    SDL_AddDisplayMode(&_this->displays[0], &mode);

    return 0;
}

static void WIIU_VideoQuit(_THIS)
{
    WIIU_QuitMouse(_this);

    wiiuFreeTextureShader();
    WHBGfxShutdown();
    WHBProcShutdown();
}

static int WIIU_SetDisplayMode(_THIS, SDL_VideoDisplay *display, SDL_DisplayMode *mode)
{
    return 0;
}

static void WIIU_PumpEvents(_THIS)
{
}

static int WIIU_Available(void)
{
    return 1;
}

static void WIIU_DeleteDevice(SDL_VideoDevice *device)
{
    SDL_free(device);
}

static SDL_VideoDevice *WIIU_CreateDevice(int devindex)
{
    SDL_VideoDevice *device;

    device = (SDL_VideoDevice*) SDL_calloc(1, sizeof(SDL_VideoDevice));
    if(!device) {
        SDL_OutOfMemory();
        return NULL;
    }

    device->VideoInit = WIIU_VideoInit;
    device->VideoQuit = WIIU_VideoQuit;
    device->SetDisplayMode = WIIU_SetDisplayMode;
    device->PumpEvents = WIIU_PumpEvents;
    device->CreateSDLWindow = WIIU_CreateSDLWindow;

    device->free = WIIU_DeleteDevice;

    return device;
}


struct SDL_VideoDevice
{
    /* * * */
    /* The name of this video driver */
    const char *name;
    
    /* * * */
    /* Initialization/Query functions */
    
    /*
     * Initialize the native video subsystem, filling in the list of
     * displays for this driver, returning 0 or -1 if there's an error.
     */
    int (*VideoInit) (_THIS);
    
    /*
     * Reverse the effects VideoInit() -- called if VideoInit() fails or
     * if the application is shutting down the video subsystem.
     */
    void (*VideoQuit) (_THIS);
    
    /*
     * Reinitialize the touch devices -- called if an unknown touch ID occurs.
     */
    void (*ResetTouch) (_THIS);
    
    /* * * */
    /*
     * Display functions
     */
    
    /*
     * Get the bounds of a display
     */
    int (*GetDisplayBounds) (_THIS, SDL_VideoDisplay * display, SDL_Rect * rect);
    
    /*
     * Get the usable bounds of a display (bounds minus menubar or whatever)
     */
    int (*GetDisplayUsableBounds) (_THIS, SDL_VideoDisplay * display, SDL_Rect * rect);
    
    /*
     * Get the dots/pixels-per-inch of a display
     */
    int (*GetDisplayDPI) (_THIS, SDL_VideoDisplay * display, float * ddpi, float * hdpi, float * vdpi);
    
    /*
     * Get a list of the available display modes for a display.
     */
    void (*GetDisplayModes) (_THIS, SDL_VideoDisplay * display);
    
    /*
     * Setting the display mode is independent of creating windows, so
     * when the display mode is changed, all existing windows should have
     * their data updated accordingly, including the display surfaces
     * associated with them.
     */
    int (*SetDisplayMode) (_THIS, SDL_VideoDisplay * display, SDL_DisplayMode * mode);
    
    /* * * */
    /*
     * Window functions
     */
    int (*CreateSDLWindow) (_THIS, SDL_Window * window);
    int (*CreateSDLWindowFrom) (_THIS, SDL_Window * window, const void *data);
    void (*SetWindowTitle) (_THIS, SDL_Window * window);
    void (*SetWindowIcon) (_THIS, SDL_Window * window, SDL_Surface * icon);
    void (*SetWindowPosition) (_THIS, SDL_Window * window);
    void (*SetWindowSize) (_THIS, SDL_Window * window);
    void (*SetWindowMinimumSize) (_THIS, SDL_Window * window);
    void (*SetWindowMaximumSize) (_THIS, SDL_Window * window);
    int (*GetWindowBordersSize) (_THIS, SDL_Window * window, int *top, int *left, int *bottom, int *right);
    int (*SetWindowOpacity) (_THIS, SDL_Window * window, float opacity);
    int (*SetWindowModalFor) (_THIS, SDL_Window * modal_window, SDL_Window * parent_window);
    int (*SetWindowInputFocus) (_THIS, SDL_Window * window);
    void (*ShowWindow) (_THIS, SDL_Window * window);
    void (*HideWindow) (_THIS, SDL_Window * window);
    void (*RaiseWindow) (_THIS, SDL_Window * window);
    void (*MaximizeWindow) (_THIS, SDL_Window * window);
    void (*MinimizeWindow) (_THIS, SDL_Window * window);
    void (*RestoreWindow) (_THIS, SDL_Window * window);
    void (*SetWindowBordered) (_THIS, SDL_Window * window, SDL_bool bordered);
    void (*SetWindowResizable) (_THIS, SDL_Window * window, SDL_bool resizable);
    void (*SetWindowFullscreen) (_THIS, SDL_Window * window, SDL_VideoDisplay * display, SDL_bool fullscreen);
    int (*SetWindowGammaRamp) (_THIS, SDL_Window * window, const Uint16 * ramp);
    int (*GetWindowGammaRamp) (_THIS, SDL_Window * window, Uint16 * ramp);
    void (*SetWindowGrab) (_THIS, SDL_Window * window, SDL_bool grabbed);
    void (*DestroyWindow) (_THIS, SDL_Window * window);
    int (*CreateWindowFramebuffer) (_THIS, SDL_Window * window, Uint32 * format, void ** pixels, int *pitch);
    int (*UpdateWindowFramebuffer) (_THIS, SDL_Window * window, const SDL_Rect * rects, int numrects);
    void (*DestroyWindowFramebuffer) (_THIS, SDL_Window * window);
    void (*OnWindowEnter) (_THIS, SDL_Window * window);
    
    /* * * */
    /*
     * Shaped-window functions
     */
    SDL_ShapeDriver shape_driver;
    
    /* Get some platform dependent window information */
    SDL_bool(*GetWindowWMInfo) (_THIS, SDL_Window * window,
                                struct SDL_SysWMinfo * info);
    
    /* * * */
    /*
     * OpenGL support
     */
    int (*GL_LoadLibrary) (_THIS, const char *path);
    void *(*GL_GetProcAddress) (_THIS, const char *proc);
    void (*GL_UnloadLibrary) (_THIS);
    SDL_GLContext(*GL_CreateContext) (_THIS, SDL_Window * window);
    int (*GL_MakeCurrent) (_THIS, SDL_Window * window, SDL_GLContext context);
    void (*GL_GetDrawableSize) (_THIS, SDL_Window * window, int *w, int *h);
    int (*GL_SetSwapInterval) (_THIS, int interval);
    int (*GL_GetSwapInterval) (_THIS);
    int (*GL_SwapWindow) (_THIS, SDL_Window * window);
    void (*GL_DeleteContext) (_THIS, SDL_GLContext context);
    void (*GL_DefaultProfileConfig) (_THIS, int *mask, int *major, int *minor);
    
    /* * * */
    /*
     * Vulkan support
     */
    int (*Vulkan_LoadLibrary) (_THIS, const char *path);
    void (*Vulkan_UnloadLibrary) (_THIS);
    SDL_bool (*Vulkan_GetInstanceExtensions) (_THIS, SDL_Window *window, unsigned *count, const char **names);
    SDL_bool (*Vulkan_CreateSurface) (_THIS, SDL_Window *window, VkInstance instance, VkSurfaceKHR *surface);
    void (*Vulkan_GetDrawableSize) (_THIS, SDL_Window * window, int *w, int *h);
    
    /* * * */
    /*
     * Event manager functions
     */
    void (*PumpEvents) (_THIS);
    
    /* Suspend the screensaver */
    void (*SuspendScreenSaver) (_THIS);
    
    /* Text input */
    void (*StartTextInput) (_THIS);
    void (*StopTextInput) (_THIS);
    void (*SetTextInputRect) (_THIS, SDL_Rect *rect);
    
    /* Screen keyboard */
    SDL_bool (*HasScreenKeyboardSupport) (_THIS);
    void (*ShowScreenKeyboard) (_THIS, SDL_Window *window);
    void (*HideScreenKeyboard) (_THIS, SDL_Window *window);
    SDL_bool (*IsScreenKeyboardShown) (_THIS, SDL_Window *window);
    
    /* Clipboard */
    int (*SetClipboardText) (_THIS, const char *text);
    char * (*GetClipboardText) (_THIS);
    SDL_bool (*HasClipboardText) (_THIS);
    
    /* MessageBox */
    int (*ShowMessageBox) (_THIS, const SDL_MessageBoxData *messageboxdata, int *buttonid);
    
    /* Hit-testing */
    int (*SetWindowHitTest)(SDL_Window * window, SDL_bool enabled);
    
    /* Tell window that app enabled drag'n'drop events */
    void (*AcceptDragAndDrop)(SDL_Window * window, SDL_bool accept);
    
    /* * * */
    /* Data common to all drivers */
    SDL_bool is_dummy;
    SDL_bool suspend_screensaver;
    int num_displays;
    SDL_VideoDisplay *displays;
    SDL_Window *windows;
    SDL_Window *grabbed_window;
    Uint8 window_magic;
    Uint32 next_object_id;
    char *clipboard_text;
    
    /* * * */
    /* Data used by the GL drivers */
    struct
    {
        int red_size;
        int green_size;
        int blue_size;
        int alpha_size;
        int depth_size;
        int buffer_size;
        int stencil_size;
        int double_buffer;
        int accum_red_size;
        int accum_green_size;
        int accum_blue_size;
        int accum_alpha_size;
        int stereo;
        int multisamplebuffers;
        int multisamplesamples;
        int accelerated;
        int major_version;
        int minor_version;
        int flags;
        int profile_mask;
        int share_with_current_context;
        int release_behavior;
        int reset_notification;
        int framebuffer_srgb_capable;
        int no_error;
        int retained_backing;
        int driver_loaded;
        char driver_path[256];
        void *dll_handle;
    } gl_config;
    
    /* * * */
    /* Cache current GL context; don't call the OS when it hasn't changed. */
    /* We have the global pointers here so Cocoa continues to work the way
     *     it always has, and the thread-local storage for the general case.
     */
    SDL_Window *current_glwin;
    SDL_GLContext current_glctx;
    SDL_TLSID current_glwin_tls;
    SDL_TLSID current_glctx_tls;
    
    /* * * */
    /* Data used by the Vulkan drivers */
    struct
    {
        PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
        PFN_vkEnumerateInstanceExtensionProperties vkEnumerateInstanceExtensionProperties;
        int loader_loaded;
        char loader_path[256];
        void *loader_handle;
    } vulkan_config;
    
    /* * * */
    /* Data private to this driver */
    void *driverdata;
    struct SDL_GLDriverData *gl_data;
    
    #if SDL_VIDEO_OPENGL_EGL
    struct SDL_EGL_VideoData *egl_data;
    #endif
    
    #if SDL_VIDEO_OPENGL_ES || SDL_VIDEO_OPENGL_ES2
    struct SDL_PrivateGLESData *gles_data;
    #endif
    
    /* * * */
    /* The function used to dispose of this structure */
    void (*free) (_THIS);
};

VideoBootStrap WIIU_bootstrap = {
    "WiiU", "Video driver for Nintendo WiiU",
    WIIU_Available, WIIU_CreateDevice
};

#endif /* SDL_VIDEO_DRIVER_WIIU */
