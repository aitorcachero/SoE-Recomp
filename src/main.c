/*
 * Secret of Evermore — desktop host shim.
 *
 * Identity and hooks, nothing else. The host itself is the framework's
 * (snesrecomp/runner/src/desktop/host_main.h).
 */

#include "host_main.h"
#include "game_rtl.h"
#include "snesrecomp_rom_identity.h"  /* generated from rom_identity.txt */
#include "desktop/config.h"
#include "snes/ppu.h"
#include "widescreen.h"
#include "snes/ws_shadow.h"

#ifndef __ANDROID__
#define SDL_MAIN_HANDLED 1
#endif
#include "desktop/sdl_compat.h"
#ifdef __ANDROID__
/* SDLActivity loads libmain.so and dlsyms its SDL_main. */
#define main SDL_main
#endif

#ifndef SNES_GAME_VERSION
#define SNES_GAME_VERSION "dev"
#endif

extern Ppu *g_ppu;
extern uint8 g_ram[0x20000];

static const char *const kSoeAspectLabels[] = {
    "4:3 (Original Cl\xc3\xa1sico)",
    "16:9 Panor\xc3\xa1mico (Hack FOV)",
    "16:10 Panor\xc3\xa1mico (Steam Deck)",
    "Pantalla Completa (Estirado)",
};

static void SoePrepareFrame(int dw, int dh, int *w, int *h)
{
    (void)dw; (void)dh;
    if (g_config.display_aspect == 1) {
        *w = 384; /* 16:9 widescreen canvas: 256 + 2 * 64 */
    } else if (g_config.display_aspect == 2) {
        *w = 352; /* 16:10 widescreen canvas: 256 + 2 * 48 (Steam Deck 1280x800) */
    } else {
        *w = 256;
    }
    *h = 224;
}

static void SoeComputeViewport(int frame_w, int frame_h, int drawable_w, int drawable_h,
                              SnesDisplayViewport *viewport)
{
    (void)frame_w; (void)frame_h;
    if (!viewport) return;
    if (g_config.display_aspect == 0) {
        *viewport = SnesDisplayAspect_FitViewport(4.0 / 3.0, drawable_w, drawable_h);
    } else if (g_config.display_aspect == 1) {
        *viewport = SnesDisplayAspect_FitViewport(16.0 / 9.0, drawable_w, drawable_h);
    } else if (g_config.display_aspect == 2) {
        *viewport = SnesDisplayAspect_FitViewport(16.0 / 10.0, drawable_w, drawable_h);
    } else {
        viewport->x = 0;
        viewport->y = 0;
        viewport->width = drawable_w > 0 ? drawable_w : 1;
        viewport->height = drawable_h > 0 ? drawable_h : 1;
    }
}


static void SoeBeginFrame(unsigned number)
{
    (void)number;
    if (!g_ppu) return;
    if (g_config.display_aspect != 1 && g_config.display_aspect != 2 && !g_config.widescreen) {
        PpuSetExtraSpace(g_ppu, 0);
        return;
    }

    int budget = (g_config.display_aspect == 2) ? 48 : 64;
    PpuSetExtraSpaceCentered(g_ppu, budget);

    /* Mode 5 and 6 are high-resolution 512px menus/dialogue/name-entry screens.
     * When active, clamp margins to 0 so menus are centered and 100% clean with black pillarboxes. */
    if (PPU_mode(g_ppu) == 5 || PPU_mode(g_ppu) == 6) {
        PpuSetExtraSideSpace(g_ppu, 0, 0, 0);
        return;
    }

    if (PPU_mode(g_ppu) == 7) {
        PpuSetExtraSpace(g_ppu, budget);
        return;
    }

    /* Standard gameplay (Mode 1).
     * Camera coordinates: Direct Page  (X),  (Y) in bank .
     * Current map dimensions:  (width in 16x16 tiles),  (height in 16x16 tiles). */
    uint32_t camX = (uint32_t)(g_ram[0x59] | (g_ram[0x5a] << 8));
    uint32_t camY = (uint32_t)(g_ram[0x5b] | (g_ram[0x5c] << 8));
    uint16_t mapW = (uint16_t)(g_ram[0x8ee] | (g_ram[0x8ef] << 8));

    /* budget already defined */
    int extraLeft = budget;
    int extraRight = budget;

    /* Clamp viewport to map boundaries if map dimensions are valid */
    if (mapW > 0 && mapW < 1000) {
        int mapWidthPx = mapW * 16;
        int maxCamX = (mapWidthPx > 256) ? (mapWidthPx - 256) : 0;
        if (mapWidthPx <= 256) {
            extraLeft = 0;
            extraRight = 0;
        } else {
            extraLeft = (int)camX;
            if (extraLeft > budget) extraLeft = budget;
            if (extraLeft < 0) extraLeft = 0;

            int rightSpace = maxCamX - (int)camX;
            extraRight = rightSpace;
            if (extraRight > budget) extraRight = budget;
            if (extraRight < 0) extraRight = 0;
        }
    }

    PpuSetExtraSideSpace(g_ppu, extraLeft, extraRight, 0);

    static uint16_t s_prev_mapW = 0;
    if (mapW != s_prev_mapW) {
        WsShadowReset();
        s_prev_mapW = mapW;
    }

    /* Configure WsShadow for Layer 0 (BG1) and Layer 1 (BG2) in pure world space */
    for (int l = 0; l < 2; l++) {
        WsShadowSetWorld(l, camX, camY);
        WsShadowSetScroll(l, g_ppu->hScroll[l], g_ppu->vScroll[l]);
    }
    WsShadowFrame(g_ppu);
}

static const SnesDesktopHostGame kGameHost = {
    .display_name        = "Secret of Evermore",
    .window_title        = "Secret of Evermore",
    .region              = SNESRECOMP_ROM_REGION,
    .rom_file            = SNESRECOMP_ROM_FILE,
    .expected_sha256_hex = SNESRECOMP_ROM_EXPECTED_SHA256,
    .expected_crc32_hex  = SNESRECOMP_ROM_EXPECTED_CRC32,
    .game_id             = SNESRECOMP_ROM_GAME_ID,
    .build_version       = SNES_GAME_VERSION,
    .game_info           = &kGameInfo,
    .num_players         = 2,
    /* Battery-backed SRAM shows the launcher's SAVES panel. Leave NULL for a
     * title without one. The path is exe-relative. */
    .sram_path           = "saves/save.srm",
    .widescreen_supported = 1,
    .native_widescreen   = 1,
    .display_aspect_supported = 1,
    .shader_supported    = 1,
    .aspect_labels       = kSoeAspectLabels,
    .num_aspect_labels   = 4,
    .aspect_setting_label = "Modo de Pantalla",
    .aspect_setting_help  = "4:3 clasico original, 16:9 Panoramico, 16:10 Steam Deck o Pantalla Completa estirada.",
    .prepare_frame       = SoePrepareFrame,
    .compute_viewport    = SoeComputeViewport,
    .begin_sim_frame     = SoeBeginFrame,
};

#ifndef __ANDROID__
#undef main   /* desktop: keep plain main() even if SDL_main.h remapped it */
#endif
int main(int argc, char **argv)
{
    return snesrecomp_desktop_main(&kGameHost, argc, argv);
}
