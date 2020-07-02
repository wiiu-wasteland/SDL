
static GX2ContextState *currentState;

void WIIU_GX2SetContextState(GX2ContextState *state)
{
    if (currentState == state) {
        return;
    }
    if (state) {
        currentState = state;
    }
    GX2SetContextState(currentState);
}

GX2ContextState *WIIU_GX2GetContextState(GX2ContextState *state)
{
    return currentState;
}

GX2RBuffer * WIIU_AllocRenderData(WIIU_RenderData *r, GX2RBuffer buffer)
{
    WIIU_RenderAllocData *rdata = SDL_malloc(sizeof(WIIU_RenderAllocData));
    
    rdata->buffer = buffer;
    if (!GX2RCreateBuffer(&rdata->buffer)) {
        SDL_free(rdata);
        return 0;
    }
    
    rdata->next = r->listfree;
    r->listfree = rdata;
    return &rdata->buffer;
}

void WIIU_FreeRenderData(WIIU_RenderData *r)
{
    while (r->listfree) {
        WIIU_RenderAllocData *ptr = r->listfree;
        r->listfree = r->listfree->next;
        GX2RDestroyBufferEx(&ptr->buffer, 0);
        SDL_free(ptr);
    }
}

void WIIU_TextureStartRendering(WIIU_RenderData *r, WIIU_TextureData *t)
{
    WIIU_TextureDrawData *d = SDL_malloc(sizeof(WIIU_TextureDrawData));
    t->isRendering = 1;
    d->texdata = t;
    d->next = r->listdraw;
    r->listdraw = d;
}

void WIIU_TextureDoneRendering(WIIU_RenderData *r)
{
    while (r->listdraw) {
        WIIU_TextureDrawData *d = r->listdraw;
        r->listdraw = r->listdraw->next;
        d->texdata->isRendering = 0;
        SDL_free(d);
    }
}

/* If the texture is currently being rendered and we change the content
   before the rendering is finished, the GPU will end up partially drawing
   the new data, so we wait for the GPU to finish rendering before
   updating the texture */
void WIIU_TextureCheckWaitRendering(WIIU_RenderData *r, WIIU_TextureData *t)
{
    if (t->isRendering) {
        GX2DrawDone();
        WIIU_TextureDoneRendering(r);
        WIIU_FreeRenderData(r);
    }
}

SDL_Texture * WIIU_GetRenderTarget(SDL_Renderer* renderer)
{
    WIIU_RenderData *data = (WIIU_RenderData *) renderer->driverdata;

    if (renderer->target) {
        return renderer->target;
    }

    return &data->windowTex;
}

WIIUPixFmt SDLFormatToWIIUFormat(Uint32 format)
{
    WIIUPixFmt outFmt = { /* sane defaults? */
        .fmt = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8,
        .compMap = 0x00010203,
    };

    switch (format) {
        /* packed16 formats: 4 bits/channel */
        case SDL_PIXELFORMAT_RGB444: /* aka XRGB4444 */
        case SDL_PIXELFORMAT_ARGB4444: {
            outFmt.fmt = GX2_SURFACE_FORMAT_UNORM_R4_G4_B4_A4;
            outFmt.compMap = 0x01020300;
            break;
        }
        case SDL_PIXELFORMAT_RGBA4444: {
            outFmt.fmt = GX2_SURFACE_FORMAT_UNORM_R4_G4_B4_A4;
            outFmt.compMap = 0x00010203;
            break;
        }
        case SDL_PIXELFORMAT_ABGR4444: {
            outFmt.fmt = GX2_SURFACE_FORMAT_UNORM_R4_G4_B4_A4;
            outFmt.compMap = 0x03020100;
            break;
        }
        case SDL_PIXELFORMAT_BGRA4444: {
            outFmt.fmt = GX2_SURFACE_FORMAT_UNORM_R4_G4_B4_A4;
            outFmt.compMap = 0x02010003;
            break;
        }

        /* packed16 formats: 5 bits/channel */
        case SDL_PIXELFORMAT_RGB555: /* aka XRGB1555 */
        case SDL_PIXELFORMAT_ARGB1555: {
            outFmt.fmt = GX2_SURFACE_FORMAT_UNORM_R5_G5_B5_A1;
            outFmt.compMap = 0x01020300;
            break;
        }
        case SDL_PIXELFORMAT_BGR555: /* aka XRGB1555 */
        case SDL_PIXELFORMAT_ABGR1555: {
            outFmt.fmt = GX2_SURFACE_FORMAT_UNORM_R5_G5_B5_A1;
            outFmt.compMap = 0x03020100;
            break;
        }
        case SDL_PIXELFORMAT_RGBA5551: {
            outFmt.fmt = GX2_SURFACE_FORMAT_UNORM_R5_G5_B5_A1;
            outFmt.compMap = 0x00010203;
            break;
        }
        case SDL_PIXELFORMAT_BGRA5551: {
            outFmt.fmt = GX2_SURFACE_FORMAT_UNORM_R5_G5_B5_A1;
            outFmt.compMap = 0x02010003;
            break;
        }

        /* packed16 formats: 565 */
        case SDL_PIXELFORMAT_RGB565: {
            outFmt.fmt = GX2_SURFACE_FORMAT_UNORM_R5_G6_B5;
            outFmt.compMap = 0x00010203;
            break;
        }
        case SDL_PIXELFORMAT_BGR565: {
            outFmt.fmt = GX2_SURFACE_FORMAT_UNORM_R5_G6_B5;
            outFmt.compMap = 0x02010003;
            break;
        }

        /* packed32 formats */
        case SDL_PIXELFORMAT_RGBA8888:
        case SDL_PIXELFORMAT_RGBX8888: {
            outFmt.fmt = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
            outFmt.compMap = 0x00010203;
            break;
        }
        case SDL_PIXELFORMAT_ARGB8888: {
            outFmt.fmt = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
            outFmt.compMap = 0x01020300;
            break;
        }
        case SDL_PIXELFORMAT_BGRA8888:
        case SDL_PIXELFORMAT_BGRX8888: {
            outFmt.fmt = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
            outFmt.compMap = 0x02010003;
            break;
        }
        case SDL_PIXELFORMAT_ABGR8888:
        case SDL_PIXELFORMAT_BGR888: {
            outFmt.fmt = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
            outFmt.compMap = 0x03020100;
            break;
        }
        case SDL_PIXELFORMAT_ARGB2101010: {
            outFmt.fmt = GX2_SURFACE_FORMAT_UNORM_R10_G10_B10_A2;
            outFmt.compMap = 0x01020300;
            break;
        }
        default: {
            /* TODO return an error */
            printf("SDL: WiiU format not recognised (SDL: %08X)\n", format);
            break;
        }
    }

    return outFmt;
}
