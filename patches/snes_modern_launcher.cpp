/* =============================================================================
 * SUPER NINTENDO - Modern Setup & Configuration Launcher
 * Estilo visual moderno de alto impacto inspirado en Sega Rally Revo / Recomp Modern UI
 * Soporta Pantalla Completa Adaptativa High-DPI, Presets 16:9 / Retro / Steam Deck,
 * detección de ROMs, mandos en tiempo real, shaders y sonido nativo.
 * ============================================================================= */

#if defined(LNG_SDL3)
  #include <SDL3/SDL.h>
  #include <SDL3/SDL_main.h>
  #include <SDL3/SDL_opengl.h>
  #include "imgui_impl_sdl3.h"
#else
  #include <SDL.h>
  #include <SDL_main.h>
  #include <SDL_opengl.h>
  #include <SDL_syswm.h>
  #include "imgui_impl_sdl2.h"
#endif

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "recomp_launcher.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#else
#include <unistd.h>
#include "launcher_files.h"
#endif

#include <stdio.h>
#include <string.h>
#include <string>

#ifndef MAX_PATH
#define MAX_PATH 1024
#endif

static bool snes_file_exists(const char *path) {
    if (!path || !path[0]) return false;
#ifdef _WIN32
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
#else
    return access(path, F_OK) == 0;
#endif
}

#ifdef _WIN32
static UINT_PTR CALLBACK BrowseHookProc(HWND hdlg, UINT uiMsg, WPARAM wParam, LPARAM lParam) {
    if (uiMsg == WM_INITDIALOG) {
        HWND hParent = GetParent(hdlg);
        HWND target = hParent ? hParent : hdlg;
        SetWindowPos(target, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        BringWindowToTop(target);
        SetForegroundWindow(target);
        SetActiveWindow(target);
    }
    return 0;
}

static bool prompt_browse_rom(SDL_Window *window, char *out_path, size_t max_len) {
    HWND hwnd = NULL;
#if defined(LNG_SDL3)
    if (window) {
        hwnd = (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
    }
#else
    if (window) {
        SDL_SysWMinfo wmInfo;
        SDL_VERSION(&wmInfo.version);
        if (SDL_GetWindowWMInfo(window, &wmInfo)) {
            hwnd = wmInfo.info.win.window;
        }
    }
#endif
    if (!hwnd) {
        hwnd = GetActiveWindow();
        if (!hwnd) hwnd = GetForegroundWindow();
    }

    if (hwnd) {
        EnableWindow(hwnd, FALSE);
    }

    char file_buf[MAX_PATH] = { 0 };
    if (out_path && out_path[0]) {
        strncpy(file_buf, out_path, sizeof(file_buf) - 1);
    }

    OPENFILENAMEA ofn;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrTitle = "Selecciona la ROM de Super Nintendo (.sfc, .smc)";
    ofn.lpstrFilter = "ROMs de Super Nintendo (*.sfc;*.smc)\0*.sfc;*.smc\0Todos los archivos (*.*)\0*.*\0";
    ofn.lpstrFile = file_buf;
    ofn.nMaxFile = (DWORD)sizeof(file_buf);
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_ENABLEHOOK | OFN_ENABLESIZING;
    ofn.lpfnHook = BrowseHookProc;

    BOOL ok = GetOpenFileNameA(&ofn);

    if (hwnd) {
        EnableWindow(hwnd, TRUE);
        SetForegroundWindow(hwnd);
        SetActiveWindow(hwnd);
    }

    if (ok && file_buf[0]) {
        strncpy(out_path, file_buf, max_len - 1);
        out_path[max_len - 1] = '\0';
        return true;
    }
    return false;
}
#else
static bool prompt_browse_rom(SDL_Window *window, char *out_path, size_t max_len) {
    (void)window;
    return launcher_pick_rom(out_path, max_len);
}
#endif

static GLuint load_texture_from_bmp(const char *path, int *out_w, int *out_h) {
    SDL_Surface *surface = SDL_LoadBMP(path);
    if (!surface) return 0;

    GLuint tex_id = 0;
    glGenTextures(1, &tex_id);
    glBindTexture(GL_TEXTURE_2D, tex_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

#if defined(LNG_SDL3)
    SDL_Surface *rgba_surf = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
    if (rgba_surf) {
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rgba_surf->w, rgba_surf->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba_surf->pixels);
        if (out_w) *out_w = rgba_surf->w;
        if (out_h) *out_h = rgba_surf->h;
        SDL_DestroySurface(rgba_surf);
    }
    SDL_DestroySurface(surface);
#else
    GLenum format = GL_BGR;
    if (surface->format->BytesPerPixel == 4) {
        format = GL_BGRA;
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surface->w, surface->h, 0, format, GL_UNSIGNED_BYTE, surface->pixels);
    if (out_w) *out_w = surface->w;
    if (out_h) *out_h = surface->h;
    SDL_FreeSurface(surface);
#endif
    return tex_id;
}

static void save_snes_config(const char *config_path, int fullscreen, int window_scale, int display_aspect,
                             int widescreen, int linear_filter, int vsync, int volume, const char *shader,
                             int integer_scaling, int color_mode) {
    const char *cfg_target = (config_path && config_path[0]) ? config_path : "config.ini";
    FILE *f = fopen(cfg_target, "w");
    if (!f) return;

    const char *aspect_str = "4:3";
    if (display_aspect == 1) aspect_str = "16:9";
    else if (display_aspect == 2) aspect_str = "16:10";
    else if (display_aspect == 3) aspect_str = "1:1";

    fprintf(f, "[General]\n");
    fprintf(f, "Autosave = 0\n");
    fprintf(f, "DisableFrameDelay = 0\n");
    fprintf(f, "SkipLauncher = 0\n");
    fprintf(f, "RunAhead = 0\n\n");

    fprintf(f, "[Graphics]\n");
    fprintf(f, "DisplayAspect = %s\n", aspect_str);
    fprintf(f, "Widescreen = %d\n", (display_aspect == 1 || display_aspect == 2) ? 1 : 0);
    fprintf(f, "IgnoreAspectRatio = %d\n", (display_aspect == 3) ? 1 : 0);
    fprintf(f, "Fullscreen = %d\n", fullscreen ? 1 : 0);
    fprintf(f, "WindowScale = %d\n", window_scale);
    fprintf(f, "NewRenderer = 0\n");
    fprintf(f, "LinearFiltering = %d\n", (linear_filter == 1) ? 1 : 0);
    fprintf(f, "Shader = %s\n", (linear_filter == 2) ? "assets/shaders/crt-soft.glslp" : (shader ? shader : ""));
    fprintf(f, "NoSpriteLimits = 1\n");
    fprintf(f, "OutputMethod = OpenGL\n");
    fprintf(f, "FrameBlend = 0\n");
    fprintf(f, "VSync = %d\n", vsync ? 1 : 0);
    fprintf(f, "Renderer = opengl\n\n");

    fprintf(f, "[Sound]\n");
    fprintf(f, "EnableAudio = 1\n");
    fprintf(f, "Volume = %d\n", volume);
    fprintf(f, "AudioFreq = 48000\n");
    fprintf(f, "AudioChannels = 2\n");
    fprintf(f, "AudioSamples = 2048\n\n");

    fprintf(f, "[KeyMap]\n");
    fprintf(f, "Fullscreen = Alt+Return\n");
    fprintf(f, "Turbo = Tab\n");
    fprintf(f, "Reset = Ctrl+r\n");
    fprintf(f, "Pause = Shift+p\n");
    fprintf(f, "ToggleWidescreen = Alt+w\n");
    fprintf(f, "SaveStateMenu = F11\n");
    fprintf(f, "Rewind = F12\n\n");

    fprintf(f, "[GamepadMap]\n");
    fprintf(f, "EnableGamepad1 = true\n");
    fprintf(f, "EnableGamepad2 = false\n");
    fprintf(f, "Controls = DpadUp, DpadDown, DpadLeft, DpadRight, Back, Start, B, A, Y, X, Lb, Rb\n");

    fclose(f);
}

extern "C" int run_snes_modern_launcher(
    const char *window_title,
    RecompLauncherCSettings *io,
    const RecompLauncherCGameInfo *game,
    const char *assets_dir,
    const char *initial_rom,
    char *out_rom_path, size_t out_rom_path_len)
{
    (void)assets_dir;

    // 1. Inicializar configuración inicial desde io y config.ini si existe
    int fullscreen = (io && io->fullscreen > 0) ? 1 : 1;
    int window_scale = (io && io->window_scale > 0) ? io->window_scale : 3;
    int widescreen = (io && io->widescreen > 0) ? 1 : 1;
    int display_aspect = (io && io->aspect_index >= 0) ? io->aspect_index : 1;
    int linear_filter = (io && io->linear_filter > 0) ? 1 : 1;
    int volume = (io && io->volume >= 0 && io->volume <= 100) ? io->volume : 100;
    int vsync = 1;
    int integer_scaling = 0;
    int color_mode = 1; // 1 = TV Cálido
    int refresh_rate = 0; // 0 = Auto monitor Hz

    const char *cfg_file = (game && game->config_path && game->config_path[0]) ? game->config_path : "config.ini";
    FILE *fcfg = fopen(cfg_file, "r");
    if (fcfg) {
        char line[256];
        while (fgets(line, sizeof(line), fcfg)) {
            int v = 0;
            char s[128];
            if (sscanf(line, "Fullscreen = %d", &v) == 1) fullscreen = v;
            else if (sscanf(line, "WindowScale = %d", &v) == 1) window_scale = v;
            else if (sscanf(line, "Widescreen = %d", &v) == 1) widescreen = v;
            else if (sscanf(line, "LinearFiltering = %d", &v) == 1) linear_filter = v;
            else if (sscanf(line, "VSync = %d", &v) == 1) vsync = v;
            else if (sscanf(line, "Volume = %d", &v) == 1) volume = v;
            else if (sscanf(line, "DisplayAspect = %127s", s) == 1) {
                if (strstr(s, "16:10") || strstr(s, "Deck") || strcmp(s, "2") == 0) display_aspect = 2;
                else if (strstr(s, "16:9") || strstr(s, "1")) display_aspect = 1;
                else if (strstr(s, "1:1") || strstr(s, "3") || strstr(s, "Stretch")) display_aspect = 3;
                else display_aspect = 0;
            } else if (strstr(line, "crt-soft") != NULL) {
                linear_filter = 2; // CRT Scanlines
            }
        }
        fclose(fcfg);
    }

    char current_rom[MAX_PATH] = { 0 };
    if (initial_rom && initial_rom[0]) {
        strncpy(current_rom, initial_rom, sizeof(current_rom) - 1);
    } else {
        FILE *from = fopen("rom.cfg", "r");
        if (from) {
            if (fgets(current_rom, sizeof(current_rom), from)) {
                size_t l = strlen(current_rom);
                while (l > 0 && (current_rom[l - 1] == '\r' || current_rom[l - 1] == '\n')) {
                    current_rom[--l] = '\0';
                }
            }
            fclose(from);
        }
    }

    // Auto-detección por nombre de archivo si aún está vacío
    if (current_rom[0] == '\0') {
        const char *candidates[] = {
            "Illusion of Time (ESP).sfc", "Illusion of Time.sfc", "Illusion of Gaia.sfc",
            "Terranigma (ESP).sfc", "Terranigma.sfc",
            "Secret of Evermore (ESP).sfc", "Secret of Evermore.sfc",
            "Ys III - Wanderers from Ys (ESP).sfc", "Ys III - Wanderers from Ys (ESP).smc",
            "Ys III - Wanderers from Ys.sfc", "Ys III.sfc",
            "The Legend of Zelda - A Link to the Past (ESP).sfc", "The Legend of Zelda - A Link to the Past (ESP).smc",
            "The Legend of Zelda - A Link to the Past.sfc", "Zelda.sfc",
            "Final Fantasy - Magic Quest (ESP).smc", "Final Fantasy - Magic Quest (ESP).sfc", "Final Fantasy - Mystic Quest.sfc",
            "Final Fantasy II (ESP).smc", "Final Fantasy II (ESP).sfc", "Final Fantasy II.sfc",
            "Final Fantasy III (ESP).smc", "Final Fantasy III (ESP).sfc", "Final Fantasy III.sfc",
            "Final Fantasy IV (ESP).smc", "Final Fantasy IV (ESP).sfc", "Final Fantasy IV.sfc",
            "Final Fantasy V (ESP).smc", "Final Fantasy V (ESP).sfc", "Final Fantasy V.sfc",
            "Final Fantasy VI (ESP).smc", "Final Fantasy VI (ESP).sfc", "Final Fantasy VI.sfc",
            "Lufia & The Fortress of Doom (ESP).SMC", "Lufia & The Fortress of Doom (ESP).sfc", "Lufia & The Fortress of Doom.sfc",
            "Lufia II - Rise of the Sinistrals (ESP).smc", "Lufia II - Rise of the Sinistrals (ESP).sfc", "Lufia II - Rise of the Sinistrals.sfc",
            "Lufia (ESP).sfc", "Lufia (ESP).smc", "Lufia.sfc",
            "rom.sfc", "rom.smc", NULL
        };
        for (int i = 0; candidates[i]; i++) {
            if (snes_file_exists(candidates[i])) {
                strncpy(current_rom, candidates[i], sizeof(current_rom) - 1);
                break;
            }
        }
    }

    // 2. Inicializar SDL y crear ventana a Pantalla Completa Adaptativa
#if defined(LNG_SDL3)
    SDL_SetMainReady();
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD);
#else
    SDL_SetMainReady();
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK);
#endif

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    int win_w = 1920;
    int win_h = 1080;
    int detected_hz = 60;

#if defined(LNG_SDL3)
    SDL_DisplayID primary_disp = SDL_GetPrimaryDisplay();
    if (primary_disp != 0) {
        const SDL_DisplayMode *dm = SDL_GetDesktopDisplayMode(primary_disp);
        if (dm && dm->w > 0 && dm->h > 0) {
            win_w = dm->w;
            win_h = dm->h;
            if (dm->refresh_rate > 0.0f) detected_hz = (int)(dm->refresh_rate + 0.5f);
        }
    }
#else
    SDL_DisplayMode dm;
    if (SDL_GetDesktopDisplayMode(0, &dm) == 0 && dm.w > 0 && dm.h > 0) {
        win_w = dm.w;
        win_h = dm.h;
        if (dm.refresh_rate > 0) detected_hz = dm.refresh_rate;
    }
#endif

    const char *title_str = window_title ? window_title : (game && game->name ? game->name : "Super Nintendo - Modern Launcher");

#if defined(LNG_SDL3)
    SDL_Window *window = SDL_CreateWindow(
        title_str, win_w, win_h,
        SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN | SDL_WINDOW_HIGH_PIXEL_DENSITY
    );
    if (!window) {
        window = SDL_CreateWindow(title_str, 1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    }
#else
    SDL_Window *window = SDL_CreateWindow(
        title_str, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, win_w, win_h,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_ALLOW_HIGHDPI
    );
    if (!window) {
        window = SDL_CreateWindow(title_str, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720,
            SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI);
    }
#endif
    if (!window) return RECOMP_LAUNCHER_RESULT_UNAVAILABLE;

    SDL_GetWindowSize(window, &win_w, &win_h);

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // VSync activado para un renderizado ultra suave

#if defined(LNG_SDL3)
    SDL_Cursor *cursor_hand = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_POINTER);
    SDL_Cursor *cursor_arrow = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);
#else
    SDL_Cursor *cursor_hand = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);
    SDL_Cursor *cursor_arrow = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
#endif

    // Factor de escalado de interfaz relativo a 1080p
    float scale = (float)win_h / 1080.0f;
    if (scale < 0.80f) scale = 0.80f;
    if (scale > 2.50f) scale = 2.50f;

    // 3. Inicializar Dear ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io_imgui = ImGui::GetIO();
    io_imgui.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io_imgui.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    io_imgui.IniFilename = NULL;

    ImFont *font_title = NULL;
    ImFont *font_large = NULL;
    ImFont *font_bold = NULL;
    ImFont *font_regular = NULL;
    ImFont *font_small = NULL;

    const char *bold_candidates[] = {
        "assets/fonts/LatoLatin-Bold.ttf",
        "recomp-ui/assets/common/fonts/LatoLatin-Bold.ttf",
        "../assets/fonts/LatoLatin-Bold.ttf",
        NULL
    };
    const char *reg_candidates[] = {
        "assets/fonts/LatoLatin-Regular.ttf",
        "recomp-ui/assets/common/fonts/LatoLatin-Regular.ttf",
        "../assets/fonts/LatoLatin-Regular.ttf",
        NULL
    };

    const char *found_bold = NULL;
    for (int i = 0; bold_candidates[i]; i++) {
        if (snes_file_exists(bold_candidates[i])) {
            found_bold = bold_candidates[i];
            break;
        }
    }
    const char *found_reg = NULL;
    for (int i = 0; reg_candidates[i]; i++) {
        if (snes_file_exists(reg_candidates[i])) {
            found_reg = reg_candidates[i];
            break;
        }
    }

    if (found_bold) {
        font_title = io_imgui.Fonts->AddFontFromFileTTF(found_bold, 40.0f * scale);
        font_large = io_imgui.Fonts->AddFontFromFileTTF(found_bold, 28.0f * scale);
        font_bold  = io_imgui.Fonts->AddFontFromFileTTF(found_bold, 22.0f * scale);
    }
    if (found_reg) {
        font_regular = io_imgui.Fonts->AddFontFromFileTTF(found_reg, 19.0f * scale);
        font_small   = io_imgui.Fonts->AddFontFromFileTTF(found_reg, 15.0f * scale);
    }
    if (!font_regular) {
        io_imgui.Fonts->AddFontDefault();
    }

#if defined(LNG_SDL3)
    ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
#else
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
#endif
    ImGui_ImplOpenGL3_Init("#version 330");

    // 4. Cargar póster vertical del juego
    int poster_w = 0, poster_h = 0;
    const char *poster_candidates[] = {
        "assets/img/poster.bmp",
        (game && game->boxart_path) ? game->boxart_path : NULL,
        "assets/img/boxart.bmp",
        "assets/img/boxart.tga",
        "../assets/img/poster.bmp",
        NULL
    };
    GLuint poster_tex = 0;
    for (int i = 0; poster_candidates[i]; i++) {
        if (poster_candidates[i] && snes_file_exists(poster_candidates[i])) {
            poster_tex = load_texture_from_bmp(poster_candidates[i], &poster_w, &poster_h);
            if (poster_tex) break;
        }
    }

    // 5. Estilo visual: Deep Navy + Slate Card + Vibrant Orange (Estilo Sega Rally Revo)
    ImGuiStyle &style = ImGui::GetStyle();
    style.WindowRounding    = 0.0f;
    style.FrameRounding     = 6.0f * scale;
    style.ItemSpacing       = ImVec2(12.0f * scale, 12.0f * scale);
    style.ItemInnerSpacing  = ImVec2(10.0f * scale, 8.0f * scale);
    style.ScrollbarRounding = 4.0f * scale;
    style.GrabRounding      = 5.0f * scale;
    style.FramePadding      = ImVec2(12.0f * scale, 8.0f * scale);

    ImVec4 *colors = style.Colors;
    colors[ImGuiCol_WindowBg]             = ImVec4(0.047f, 0.071f, 0.122f, 1.00f); // #0C121F Deep Navy
    colors[ImGuiCol_ChildBg]              = ImVec4(0.063f, 0.094f, 0.157f, 1.00f); // #101828 Slate Card
    colors[ImGuiCol_Border]               = ImVec4(0.141f, 0.200f, 0.314f, 1.00f);
    colors[ImGuiCol_FrameBg]              = ImVec4(0.090f, 0.133f, 0.220f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.125f, 0.180f, 0.290f, 1.00f);
    colors[ImGuiCol_FrameBgActive]        = ImVec4(0.160f, 0.224f, 0.353f, 1.00f);
    colors[ImGuiCol_TitleBg]              = ImVec4(0.047f, 0.071f, 0.122f, 1.00f);
    colors[ImGuiCol_TitleBgActive]        = ImVec4(0.047f, 0.071f, 0.122f, 1.00f);
    colors[ImGuiCol_CheckMark]            = ImVec4(1.000f, 0.463f, 0.000f, 1.00f); // Vibrant Orange
    colors[ImGuiCol_SliderGrab]           = ImVec4(1.000f, 0.463f, 0.000f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]     = ImVec4(1.000f, 0.600f, 0.200f, 1.00f);
    colors[ImGuiCol_Button]               = ImVec4(0.094f, 0.141f, 0.231f, 1.00f);
    colors[ImGuiCol_ButtonHovered]        = ImVec4(0.137f, 0.200f, 0.322f, 1.00f);
    colors[ImGuiCol_ButtonActive]         = ImVec4(0.180f, 0.259f, 0.412f, 1.00f);
    colors[ImGuiCol_Header]               = ImVec4(1.000f, 0.463f, 0.000f, 0.25f);
    colors[ImGuiCol_HeaderHovered]        = ImVec4(1.000f, 0.463f, 0.000f, 0.40f);
    colors[ImGuiCol_HeaderActive]         = ImVec4(1.000f, 0.463f, 0.000f, 0.60f);
    colors[ImGuiCol_Text]                 = ImVec4(0.950f, 0.960f, 0.980f, 1.00f);
    colors[ImGuiCol_TextDisabled]         = ImVec4(0.500f, 0.580f, 0.680f, 1.00f);

    int active_tab = 0; // 0: Play, 1: Picture, 2: Controls, 3: Audio
    int active_preset = (widescreen && display_aspect == 1 && window_scale >= 3) ? 0 : ((widescreen && display_aspect == 2) ? 1 : ((!widescreen || display_aspect == 0) ? 2 : 3));
    bool running = true;
    int result_action = RECOMP_LAUNCHER_RESULT_LAUNCH;

    // Detectar título del juego
    const char *game_display_title = (game && game->name) ? game->name : "Super Nintendo Recomp";
    bool is_iot = (strstr(game_display_title, "Illusion") != NULL);
    bool is_terra = (strstr(game_display_title, "Terranigma") != NULL);
    bool is_soe = (strstr(game_display_title, "Evermore") != NULL);
    bool is_ys = (strstr(game_display_title, "Ys") != NULL);
    bool is_zelda = (strstr(game_display_title, "Zelda") != NULL || strstr(game_display_title, "Link") != NULL);
    bool is_ff = (strstr(game_display_title, "Final Fantasy") != NULL);
    bool is_lufia = (strstr(game_display_title, "Lufia") != NULL);

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
#if defined(LNG_SDL3)
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
                result_action = RECOMP_LAUNCHER_RESULT_QUIT;
            } else if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.scancode == SDL_SCANCODE_ESCAPE) {
                    running = false;
                    result_action = RECOMP_LAUNCHER_RESULT_QUIT;
                } else if (event.key.scancode == SDL_SCANCODE_RETURN || event.key.scancode == SDL_SCANCODE_KP_ENTER) {
                    running = false;
                    result_action = RECOMP_LAUNCHER_RESULT_LAUNCH;
                } else if (event.key.scancode == SDL_SCANCODE_F11 ||
                           (event.key.scancode == SDL_SCANCODE_RETURN && (event.key.mod & SDL_KMOD_ALT))) {
                    Uint32 wflags = SDL_GetWindowFlags(window);
                    if (wflags & SDL_WINDOW_FULLSCREEN) {
                        SDL_SetWindowFullscreen(window, false);
                        SDL_SetWindowSize(window, (int)(1280 * scale), (int)(720 * scale));
                        SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
                    } else {
                        SDL_SetWindowFullscreen(window, true);
                    }
                }
            } else if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED || event.type == SDL_EVENT_WINDOW_RESIZED) {
                win_w = event.window.data1;
                win_h = event.window.data2;
            }
#else
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) {
                running = false;
                result_action = RECOMP_LAUNCHER_RESULT_QUIT;
            } else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
                    running = false;
                    result_action = RECOMP_LAUNCHER_RESULT_QUIT;
                } else if (event.key.keysym.scancode == SDL_SCANCODE_RETURN || event.key.keysym.scancode == SDL_SCANCODE_KP_ENTER) {
                    running = false;
                    result_action = RECOMP_LAUNCHER_RESULT_LAUNCH;
                } else if (event.key.keysym.scancode == SDL_SCANCODE_F11 ||
                           (event.key.keysym.scancode == SDL_SCANCODE_RETURN && (event.key.mod & KMOD_ALT))) {
                    Uint32 wflags = SDL_GetWindowFlags(window);
                    if (wflags & SDL_WINDOW_FULLSCREEN_DESKTOP) {
                        SDL_SetWindowFullscreen(window, 0);
                        SDL_SetWindowSize(window, (int)(1280 * scale), (int)(720 * scale));
                        SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
                    } else {
                        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
                    }
                }
            } else if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED) {
                win_w = event.window.data1;
                win_h = event.window.data2;
            }
#endif
        }

        ImGui_ImplOpenGL3_NewFrame();
#if defined(LNG_SDL3)
        ImGui_ImplSDL3_NewFrame();
#else
        ImGui_ImplSDL2_NewFrame();
#endif
        ImGui::NewFrame();

        if (io_imgui.DisplaySize.x > 0 && io_imgui.DisplaySize.y > 0) {
            win_w = (int)io_imgui.DisplaySize.x;
            win_h = (int)io_imgui.DisplaySize.y;
        }

        // Lienzo principal de pantalla completa
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2((float)win_w, (float)win_h));
        ImGui::Begin("SNESLauncherMain", NULL,
                     ImGuiWindowFlags_NoTitleBar |
                     ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoBringToFrontOnFocus);

        ImDrawList *draw = ImGui::GetWindowDrawList();

        // ── Columna Izquierda: Póster de la Portada (~32% ancho) ───────────
        float left_w = (float)win_w * 0.32f;
        if (left_w < 380.0f * scale) left_w = 380.0f * scale;
        if (left_w > 640.0f * scale) left_w = 640.0f * scale;

        ImGui::BeginChild("LeftBanner", ImVec2(left_w, (float)win_h), false, ImGuiWindowFlags_NoScrollbar);
        if (poster_tex && poster_w > 0 && poster_h > 0) {
            float target_aspect = left_w / (float)win_h;
            float img_aspect = (float)poster_w / (float)poster_h;
            ImVec2 uv0(0.0f, 0.0f);
            ImVec2 uv1(1.0f, 1.0f);
            if (img_aspect > target_aspect) {
                float f = target_aspect / img_aspect;
                float crop = (1.0f - f) * 0.5f;
                uv0.x = crop;
                uv1.x = 1.0f - crop;
            } else {
                float f = img_aspect / target_aspect;
                float crop = (1.0f - f) * 0.5f;
                uv0.y = crop;
                uv1.y = 1.0f - crop;
            }
            ImGui::Image((ImTextureID)(intptr_t)poster_tex, ImVec2(left_w, (float)win_h), uv0, uv1);
        } else {
            draw->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(left_w, (float)win_h),
                                          IM_COL32(14, 24, 45, 255), IM_COL32(20, 38, 70, 255),
                                          IM_COL32(10, 15, 28, 255), IM_COL32(5, 8, 16, 255));
        }

        // Overlay degradado inferior con tipografía nítida
        float overlay_h = 165.0f * scale;
        draw->AddRectFilledMultiColor(ImVec2(0, win_h - overlay_h), ImVec2(left_w, (float)win_h),
                                      IM_COL32(12, 19, 32, 0), IM_COL32(12, 19, 32, 0),
                                      IM_COL32(12, 19, 32, 250), IM_COL32(12, 19, 32, 250));

        if (font_large) ImGui::PushFont(font_large);
        draw->AddText(ImVec2(28.0f * scale, win_h - 135.0f * scale), IM_COL32(255, 255, 255, 255), game_display_title);
        if (font_large) ImGui::PopFont();

        if (font_bold) ImGui::PushFont(font_bold);
        const char *game_sub = is_iot ? "Super Nintendo | Enix / Quintet [Español]" :
                               (is_terra ? "Super Nintendo | Enix / Quintet [Español]" :
                               (is_soe ? "Super Nintendo | Squaresoft [Español]" :
                               (is_ys ? "Super Nintendo | Nihon Falcom / Tonkin House [Español]" :
                               (is_zelda ? "Super Nintendo | Nintendo [Español]" :
                               (is_ff ? "Super Nintendo | Squaresoft [Español]" :
                               (is_lufia ? "Super Nintendo | Taito / Neverland [Español]" : "Super Nintendo - PAL/ESP"))))));
        draw->AddText(ImVec2(28.0f * scale, win_h - 90.0f * scale), IM_COL32(255, 130, 0, 255), game_sub);
        if (font_bold) ImGui::PopFont();

        if (font_regular) ImGui::PushFont(font_regular);
        draw->AddText(ImVec2(28.0f * scale, win_h - 52.0f * scale), IM_COL32(148, 163, 184, 230),
                      "PC Native Recompilation | Auto / 60 / 144 / 240 / 360 Hz");
        if (font_regular) ImGui::PopFont();

        ImGui::EndChild();

        // ── Columna Derecha: Configuración y Presets ───────────────────────
        ImGui::SameLine();
        float pad_x = 36.0f * scale;
        ImGui::SetCursorPosX(left_w + pad_x);
        float right_w = (float)win_w - left_w - pad_x * 1.5f;
        ImGui::BeginChild("RightPanel", ImVec2(right_w, (float)win_h), false, ImGuiWindowFlags_NoScrollbar);

        // Decoración superior: Rayas diagonales estilo Racing / Cyberpunk
        float right_top_x = right_w - (110.0f * scale);
        draw->AddTriangleFilled(ImVec2(left_w + pad_x + right_top_x + 24 * scale, 0),
                                ImVec2(left_w + pad_x + right_top_x + 48 * scale, 0),
                                ImVec2(left_w + pad_x + right_top_x + 12 * scale, 60 * scale), IM_COL32(255, 115, 0, 230));
        draw->AddTriangleFilled(ImVec2(left_w + pad_x + right_top_x + 48 * scale, 0),
                                ImVec2(left_w + pad_x + right_top_x + 12 * scale, 60 * scale),
                                ImVec2(left_w + pad_x + right_top_x - 12 * scale, 60 * scale), IM_COL32(255, 115, 0, 230));
        draw->AddTriangleFilled(ImVec2(left_w + pad_x + right_top_x + 60 * scale, 0),
                                ImVec2(left_w + pad_x + right_top_x + 84 * scale, 0),
                                ImVec2(left_w + pad_x + right_top_x + 48 * scale, 60 * scale), IM_COL32(255, 140, 0, 230));
        draw->AddTriangleFilled(ImVec2(left_w + pad_x + right_top_x + 84 * scale, 0),
                                ImVec2(left_w + pad_x + right_top_x + 48 * scale, 60 * scale),
                                ImVec2(left_w + pad_x + right_top_x + 24 * scale, 60 * scale), IM_COL32(255, 140, 0, 230));

        ImGui::Dummy(ImVec2(0, 14.0f * scale));

        if (font_title) ImGui::PushFont(font_title);
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%s", game_display_title);
        if (font_title) ImGui::PopFont();

        if (font_regular) ImGui::PushFont(font_regular);
        ImGui::TextColored(ImVec4(0.65f, 0.72f, 0.82f, 1.0f), "Listo para jugar. Pulsa el bot\xc3\xb3n Jugar o la tecla Intro (Enter).");
        if (font_regular) ImGui::PopFont();

        ImGui::Dummy(ImVec2(0, 10.0f * scale));

        // Pestañas (Play, Picture, Controls, Audio)
        const char *tabs[] = { "Play", "Picture", "Controls", "Audio" };
        if (font_large) ImGui::PushFont(font_large);
        for (int t = 0; t < 4; t++) {
            if (t > 0) ImGui::SameLine(0, 24.0f * scale);
            bool is_selected = (active_tab == t);
            if (is_selected) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.50f, 0.05f, 1.0f));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.70f, 0.76f, 0.85f, 1.0f));
            }

            if (ImGui::Button(tabs[t], ImVec2(0, 36.0f * scale))) {
                active_tab = t;
            }
            ImGui::PopStyleColor();

            if (is_selected) {
                ImVec2 bmin = ImGui::GetItemRectMin();
                ImVec2 bmax = ImGui::GetItemRectMax();
                draw->AddLine(ImVec2(bmin.x, bmax.y + 2), ImVec2(bmax.x, bmax.y + 2), IM_COL32(255, 120, 0, 255), 3.0f * scale);
            }
        }
        if (font_large) ImGui::PopFont();

        ImVec2 sep_pos = ImGui::GetCursorScreenPos();
        draw->AddLine(ImVec2(sep_pos.x, sep_pos.y + 8 * scale), ImVec2(sep_pos.x + right_w - 40.0f * scale, sep_pos.y + 8 * scale), IM_COL32(36, 51, 80, 255), 1.5f);
        ImGui::Dummy(ImVec2(0, 18.0f * scale));

        // ── TAB 0: PLAY (JUGAR) ─────────────────────────────────────────────
        if (active_tab == 0) {
            if (font_bold) ImGui::PushFont(font_bold);
            ImGui::TextColored(ImVec4(0.78f, 0.84f, 0.92f, 1.0f), "Archivo ROM de Super Nintendo:");
            if (font_bold) ImGui::PopFont();

            if (font_regular) ImGui::PushFont(font_regular);
            float input_w = right_w - 180.0f * scale;
            if (input_w < 300.0f) input_w = 300.0f;
            ImGui::PushItemWidth(input_w);
            ImGui::InputText("##snes_rom_path", current_rom, sizeof(current_rom), ImGuiInputTextFlags_ReadOnly);
            ImGui::PopItemWidth();

            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.18f, 0.30f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 0.46f, 0.0f, 0.8f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
            if (ImGui::Button("Examinar...", ImVec2(120.0f * scale, 0))) {
                char picked[MAX_PATH] = { 0 };
                if (prompt_browse_rom(window, picked, sizeof(picked))) {
                    strncpy(current_rom, picked, sizeof(current_rom) - 1);
                }
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);

            if (current_rom[0] != '\0' && snes_file_exists(current_rom)) {
                ImGui::TextColored(ImVec4(0.20f, 0.88f, 0.45f, 1.0f), "ROM detectada y verificada: %s [OK]", current_rom);
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.20f, 1.0f), "Aviso: Copia tu ROM (.sfc/.smc) en la carpeta del juego o pulsa Examinar.");
            }
            if (font_regular) ImGui::PopFont();

            ImGui::Dummy(ImVec2(0, 16.0f * scale));

            // Presets de Aspecto (Look)
            if (font_bold) ImGui::PushFont(font_bold);
            ImGui::TextColored(ImVec4(0.90f, 0.93f, 0.98f, 1.0f), "Aspecto (Presets recomendados):");
            if (font_bold) ImGui::PopFont();

            struct PresetCard {
                const char *badge;
                const char *title;
                const char *sub1;
                const char *sub2;
            } cards[4] = {
                { "Recomendado", "Moderna", "1920x1080 (16:9)", "144 Hz, FOV Panorámico, Suave" },
                { NULL, "Steam Deck", "1280x800 (16:10)", "60 Hz, FOV 16:10 Nativo" },
                { NULL, "Original 1994", "1280x960 (4:3)", "60 Hz, Píxel Nítido / Retro" },
                { NULL, "Personalizado", "Ajustable", "Configura en Picture o Play" }
            };

            float card_w = 210.0f * scale;
            float card_h = 135.0f * scale;

            for (int i = 0; i < 4; i++) {
                if (i > 0) ImGui::SameLine(0, 16.0f * scale);
                bool sel = (active_preset == i);

                ImGui::PushID(i);
                ImVec2 p_min = ImGui::GetCursorScreenPos();
                ImVec2 p_max = ImVec2(p_min.x + card_w, p_min.y + card_h);

                bool clicked = ImGui::InvisibleButton("##card_btn", ImVec2(card_w, card_h));
                bool card_hovered = ImGui::IsItemHovered();
                if (card_hovered) {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                }

                ImU32 bg_col = sel ? IM_COL32(20, 34, 58, 255) : (card_hovered ? IM_COL32(22, 32, 52, 255) : IM_COL32(15, 23, 38, 255));
                ImU32 border_col = sel ? IM_COL32(255, 115, 0, 255) : (card_hovered ? IM_COL32(255, 150, 40, 255) : IM_COL32(40, 58, 90, 255));

                draw->AddRectFilled(p_min, p_max, bg_col, 8.0f * scale);
                draw->AddRect(p_min, p_max, border_col, 8.0f * scale, 0, sel ? 2.5f * scale : (card_hovered ? 2.0f * scale : 1.0f));

                if (cards[i].badge) {
                    if (font_small) ImGui::PushFont(font_small);
                    draw->AddText(ImVec2(p_min.x + 14.0f * scale, p_min.y + 12.0f * scale), IM_COL32(255, 120, 0, 255), cards[i].badge);
                    if (font_small) ImGui::PopFont();
                }

                float text_y = cards[i].badge ? 38.0f * scale : 20.0f * scale;
                if (font_bold) ImGui::PushFont(font_bold);
                draw->AddText(ImVec2(p_min.x + 14.0f * scale, p_min.y + text_y), IM_COL32(255, 255, 255, 255), cards[i].title);
                if (font_bold) ImGui::PopFont();

                if (font_regular) ImGui::PushFont(font_regular);
                draw->AddText(ImVec2(p_min.x + 14.0f * scale, p_min.y + text_y + 30.0f * scale), IM_COL32(148, 163, 184, 255), cards[i].sub1);
                if (font_regular) ImGui::PopFont();

                if (font_small) ImGui::PushFont(font_small);
                draw->AddText(ImVec2(p_min.x + 14.0f * scale, p_min.y + text_y + 54.0f * scale), IM_COL32(100, 116, 139, 255), cards[i].sub2);
                if (font_small) ImGui::PopFont();

                if (clicked) {
                    active_preset = i;
                    if (i == 0) { // Modern
                        fullscreen = 1; window_scale = 3; widescreen = 1; display_aspect = 1; linear_filter = 1; vsync = 1; refresh_rate = 0; integer_scaling = 0; color_mode = 1;
                    } else if (i == 1) { // Steam Deck
                        fullscreen = 1; window_scale = 2; widescreen = 1; display_aspect = 2; linear_filter = 1; vsync = 1; refresh_rate = 60; integer_scaling = 0; color_mode = 1;
                    } else if (i == 2) { // Original 1994
                        fullscreen = 0; window_scale = 3; widescreen = 0; display_aspect = 0; linear_filter = 0; vsync = 1; refresh_rate = 60; integer_scaling = 1; color_mode = 1;
                    }
                }
                ImGui::PopID();
            }

            ImGui::Dummy(ImVec2(0, 18.0f * scale));

            // Mejoras y opciones
            if (font_bold) ImGui::PushFont(font_bold);
            ImGui::TextColored(ImVec4(0.90f, 0.93f, 0.98f, 1.0f), "Mejoras del Motor:");
            if (font_bold) ImGui::PopFont();

            // Modo Panorámico (FOV Real)
            if (font_regular) ImGui::PushFont(font_regular);
            ImGui::TextColored(ImVec4(0.75f, 0.80f, 0.90f, 1.0f), "Modo Panor\xc3\xa1mico (FOV Real):");
            ImGui::SameLine(320.0f * scale);
            const char *ws_modes[] = { "16:9 (PC/TV)", "16:10 (Deck)", "4:3 Original" };
            for (int m = 0; m < 3; m++) {
                if (m > 0) ImGui::SameLine(0, 8.0f * scale);
                int m_aspect = (m == 0) ? 1 : ((m == 1) ? 2 : 0);
                bool is_cur = (display_aspect == m_aspect);
                float bw = (m == 0) ? 130.0f * scale : ((m == 1) ? 130.0f * scale : 110.0f * scale);
                if (is_cur) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 0.45f, 0.0f, 0.95f));
                    ImGui::Button(ws_modes[m], ImVec2(bw, 36.0f * scale));
                    ImGui::PopStyleColor();
                } else {
                    if (ImGui::Button(ws_modes[m], ImVec2(bw, 36.0f * scale))) {
                        display_aspect = m_aspect;
                        widescreen = (m_aspect == 1 || m_aspect == 2) ? 1 : 0;
                        active_preset = 3;
                    }
                }
            }
            ImGui::TextColored(ImVec4(0.55f, 0.63f, 0.74f, 1.0f),
                               "Extiende el campo de visi\xc3\xb3n de la c\xc3\xa1mara a 16:9 o 16:10 nativo sin deformar sprites ni estirar la imagen.");

            ImGui::Dummy(ImVec2(0, 6.0f * scale));

            // Frecuencia de Refresco (Hz)
            ImGui::TextColored(ImVec4(0.75f, 0.80f, 0.90f, 1.0f), "Tasa de Refresco (Hz):");
            ImGui::SameLine(320.0f * scale);
            int hz_options[] = { 60, 144, 240, 360, 0 };
            for (int h = 0; h < 5; h++) {
                if (h > 0) ImGui::SameLine(0, 8.0f * scale);
                bool is_cur = (refresh_rate == hz_options[h]);
                char hlabel[48];
                if (hz_options[h] == 0) {
                    snprintf(hlabel, sizeof(hlabel), "Auto (%d Hz)##hz_auto", detected_hz);
                } else {
                    snprintf(hlabel, sizeof(hlabel), "%d Hz##hz_%d", hz_options[h], hz_options[h]);
                }
                float btn_w = (hz_options[h] == 0) ? 140.0f * scale : 75.0f * scale;
                if (is_cur) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 0.45f, 0.0f, 0.95f));
                    ImGui::Button(hlabel, ImVec2(btn_w, 36.0f * scale));
                    ImGui::PopStyleColor();
                } else {
                    if (ImGui::Button(hlabel, ImVec2(btn_w, 36.0f * scale))) {
                        refresh_rate = hz_options[h];
                        active_preset = 3;
                    }
                }
            }
            ImGui::TextColored(ImVec4(0.55f, 0.63f, 0.74f, 1.0f),
                               "Sincronizaci\xc3\xb3n de c\xc3\xa1mara y cuadros: 60Hz cl\xc3\xa1sico, 144/240/360Hz gaming, o Auto (%d Hz de tu pantalla).", detected_hz);
            if (font_regular) ImGui::PopFont();
        }

        // ── TAB 1: PICTURE (PANTALLA) ───────────────────────────────────────
        else if (active_tab == 1) {
            if (font_large) ImGui::PushFont(font_large);
            ImGui::TextColored(ImVec4(0.90f, 0.93f, 0.98f, 1.0f), "Ajustes de Pantalla (Display & Motion)");
            if (font_large) ImGui::PopFont();
            ImGui::Dummy(ImVec2(0, 6.0f * scale));

            if (font_regular) ImGui::PushFont(font_regular);

            if (font_bold) ImGui::PushFont(font_bold);
            ImGui::TextColored(ImVec4(1.0f, 0.50f, 0.05f, 1.0f), "Display");
            if (font_bold) ImGui::PopFont();

            // Fullscreen
            ImGui::Text("Fullscreen (Pantalla completa):");
            ImGui::SameLine(320.0f * scale);
            if (fullscreen) {
                if (ImGui::Button("Off##fs_off", ImVec2(75.0f * scale, 30.0f * scale))) { fullscreen = 0; active_preset = 3; }
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 0.45f, 0.0f, 0.95f));
                ImGui::Button("On##fs_on", ImVec2(75.0f * scale, 30.0f * scale));
                ImGui::PopStyleColor();
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.28f, 0.42f, 0.95f));
                ImGui::Button("Off##fs_off", ImVec2(75.0f * scale, 30.0f * scale));
                ImGui::PopStyleColor();
                ImGui::SameLine();
                if (ImGui::Button("On##fs_on", ImVec2(75.0f * scale, 30.0f * scale))) { fullscreen = 1; active_preset = 3; }
            }

            // VSync
            ImGui::Text("VSync (Sincronizaci\xc3\xb3n vertical):");
            ImGui::SameLine(320.0f * scale);
            if (vsync) {
                if (ImGui::Button("Off##vs_off", ImVec2(75.0f * scale, 30.0f * scale))) { vsync = 0; active_preset = 3; }
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 0.45f, 0.0f, 0.95f));
                ImGui::Button("On##vs_on", ImVec2(75.0f * scale, 30.0f * scale));
                ImGui::PopStyleColor();
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.28f, 0.42f, 0.95f));
                ImGui::Button("Off##vs_off", ImVec2(75.0f * scale, 30.0f * scale));
                ImGui::PopStyleColor();
                ImGui::SameLine();
                if (ImGui::Button("On##vs_on", ImVec2(75.0f * scale, 30.0f * scale))) { vsync = 1; active_preset = 3; }
            }

            // Aspecto
            ImGui::Text("Modo de Pantalla / Aspecto:");
            ImGui::SameLine(320.0f * scale);
            const char *asp_opts[] = { "4:3 Cl\xc3\xa1sico", "16:9 Panor\xc3\xa1mico", "16:10 Steam Deck", "Estirado Completo" };
            for (int a = 0; a < 4; a++) {
                if (a > 0) ImGui::SameLine(0, 8.0f * scale);
                bool is_a = (display_aspect == a);
                float btn_w = (a == 1) ? 165.0f * scale : ((a == 2) ? 150.0f * scale : 120.0f * scale);
                if (is_a) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 0.45f, 0.0f, 0.95f));
                    ImGui::Button(asp_opts[a], ImVec2(btn_w, 30.0f * scale));
                    ImGui::PopStyleColor();
                } else {
                    if (ImGui::Button(asp_opts[a], ImVec2(btn_w, 30.0f * scale))) {
                        display_aspect = a;
                        widescreen = (a == 1 || a == 2) ? 1 : 0;
                        active_preset = 3;
                    }
                }
            }

            // Filtro Gráfico
            ImGui::Text("Filtro Gr\xc3\xa1" "fico:");
            ImGui::SameLine(320.0f * scale);
            const char *filter_labels[] = { "Sharp (N\xc3\xadtido)", "Smooth (Suave)", "CRT Scanlines" };
            for (int f = 0; f < 3; f++) {
                if (f > 0) ImGui::SameLine();
                bool f_cur = (linear_filter == f);
                float f_w = (f == 2) ? 140.0f * scale : 120.0f * scale;
                if (f_cur) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 0.45f, 0.0f, 0.95f));
                    ImGui::Button(filter_labels[f], ImVec2(f_w, 30.0f * scale));
                    ImGui::PopStyleColor();
                } else {
                    if (ImGui::Button(filter_labels[f], ImVec2(f_w, 30.0f * scale))) {
                        linear_filter = f;
                        active_preset = 3;
                    }
                }
            }

            // Escalado Entero
            ImGui::Text("Integer scaling (Escalado entero):");
            ImGui::SameLine(320.0f * scale);
            if (integer_scaling) {
                if (ImGui::Button("Off##is_off", ImVec2(75.0f * scale, 30.0f * scale))) { integer_scaling = 0; active_preset = 3; }
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 0.45f, 0.0f, 0.95f));
                ImGui::Button("On##is_on", ImVec2(75.0f * scale, 30.0f * scale));
                ImGui::PopStyleColor();
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.28f, 0.42f, 0.95f));
                ImGui::Button("Off##is_off", ImVec2(75.0f * scale, 30.0f * scale));
                ImGui::PopStyleColor();
                ImGui::SameLine();
                if (ImGui::Button("On##is_on", ImVec2(75.0f * scale, 30.0f * scale))) { integer_scaling = 1; active_preset = 3; }
            }

            // Paleta de color
            ImGui::Text("Colours (Paleta de color):");
            ImGui::SameLine(320.0f * scale);
            if (color_mode) {
                if (ImGui::Button("Raw (Original)##col_raw", ImVec2(130.0f * scale, 30.0f * scale))) { color_mode = 0; active_preset = 3; }
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 0.45f, 0.0f, 0.95f));
                ImGui::Button("As on a TV (C\xc3\xa1lido)##col_tv", ImVec2(165.0f * scale, 30.0f * scale));
                ImGui::PopStyleColor();
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.28f, 0.42f, 0.95f));
                ImGui::Button("Raw (Original)##col_raw", ImVec2(130.0f * scale, 30.0f * scale));
                ImGui::PopStyleColor();
                ImGui::SameLine();
                if (ImGui::Button("As on a TV (C\xc3\xa1lido)##col_tv", ImVec2(165.0f * scale, 30.0f * scale))) { color_mode = 1; active_preset = 3; }
            }

            // Escala interna de resolución
            ImGui::Text("Internal Resolution / Scale:");
            ImGui::SameLine(320.0f * scale);
            const char *scales[] = { "1x (256x224)", "2x (720p)", "3x (1080p)", "4x (1440p 2K)", "5x (2160p 4K)" };
            int cur_scale_idx = window_scale - 1;
            if (cur_scale_idx < 0) cur_scale_idx = 0;
            if (cur_scale_idx > 4) cur_scale_idx = 4;
            ImGui::PushItemWidth(200.0f * scale);
            if (ImGui::Combo("##win_scale", &cur_scale_idx, scales, 5)) {
                window_scale = cur_scale_idx + 1;
                active_preset = 3;
            }
            ImGui::PopItemWidth();

            ImGui::Dummy(ImVec2(0, 8.0f * scale));

            if (font_bold) ImGui::PushFont(font_bold);
            ImGui::TextColored(ImVec4(1.0f, 0.50f, 0.05f, 1.0f), "Motion");
            if (font_bold) ImGui::PopFont();

            ImGui::Text("Refresh Rate / High Frame Rate:");
            ImGui::SameLine(320.0f * scale);
            int hz_opts_pic[] = { 60, 144, 240, 360, 0 };
            for (int h = 0; h < 5; h++) {
                if (h > 0) ImGui::SameLine();
                bool is_c = (refresh_rate == hz_opts_pic[h]);
                char h_lbl[48];
                if (hz_opts_pic[h] == 0) {
                    snprintf(h_lbl, sizeof(h_lbl), "Auto (%d Hz)##hz_pic_auto", detected_hz);
                } else {
                    snprintf(h_lbl, sizeof(h_lbl), "%d Hz##hz_pic_%d", hz_opts_pic[h], hz_opts_pic[h]);
                }
                float b_w = (hz_opts_pic[h] == 0) ? 140.0f * scale : 75.0f * scale;
                if (is_c) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 0.45f, 0.0f, 0.95f));
                    ImGui::Button(h_lbl, ImVec2(b_w, 30.0f * scale));
                    ImGui::PopStyleColor();
                } else {
                    if (ImGui::Button(h_lbl, ImVec2(b_w, 30.0f * scale))) {
                        refresh_rate = hz_opts_pic[h];
                        active_preset = 3;
                    }
                }
            }
            ImGui::TextColored(ImVec4(0.55f, 0.63f, 0.74f, 1.0f),
                               "La opci\xc3\xb3n Auto sincroniza a la tasa de refresco detectada de tu monitor (%d Hz).", detected_hz);

            if (font_regular) ImGui::PopFont();
        }

        // ── TAB 2: CONTROLS (CONTROLES) ─────────────────────────────────────
        else if (active_tab == 2) {
            if (font_large) ImGui::PushFont(font_large);
            ImGui::TextColored(ImVec4(0.90f, 0.93f, 0.98f, 1.0f), "Mapeo de Mandos y Teclado");
            if (font_large) ImGui::PopFont();
            ImGui::Dummy(ImVec2(0, 8.0f * scale));

            if (font_regular) ImGui::PushFont(font_regular);
            const char *pad_name = NULL;
#if defined(LNG_SDL3)
            int pad_count = 0;
            SDL_JoystickID *pads = SDL_GetGamepads(&pad_count);
            if (pad_count > 0 && pads) {
                pad_name = SDL_GetGamepadNameForID(pads[0]);
            }
#else
            int pad_count = SDL_NumJoysticks();
            if (pad_count > 0) {
                pad_name = SDL_JoystickNameForIndex(0);
            }
#endif
            if (pad_name && pad_name[0]) {
                ImGui::TextColored(ImVec4(0.20f, 0.88f, 0.45f, 1.0f), "Mando detectado: %s", pad_name);
            } else {
                ImGui::TextColored(ImVec4(0.70f, 0.76f, 0.85f, 1.0f),
                                   "Teclado activo (Conecta cualquier mando Xbox, PlayStation o Switch por USB/Bluetooth).");
            }
#if defined(LNG_SDL3)
            if (pads) SDL_free(pads);
#endif

            ImGui::Dummy(ImVec2(0, 10.0f * scale));

            if (ImGui::BeginTable("KeymapTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn("Acci\xc3\xb3n en el Juego", ImGuiTableColumnFlags_WidthFixed, 240.0f * scale);
                ImGui::TableSetupColumn("Teclado", ImGuiTableColumnFlags_WidthFixed, 300.0f * scale);
                ImGui::TableSetupColumn("Gamepad (Xbox / PlayStation / Switch)", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableHeadersRow();

                const char *actions_iot[][3] = {
                    { "Mover personaje", "Flechas de dirección / W A S D", "D-Pad y Stick Izquierdo" },
                    { "Atacar / Correr", "Z  /  Espacio", "Botón A (PlayStation Cruz)" },
                    { "Flauta / Hablar", "X", "Botón B (PlayStation Círculo)" },
                    { "Defensa telequinética", "C  /  V", "Gatillos LB / RB (L / R)" },
                    { "Menú / Inventario", "Enter / Intro", "Botón Start / Options" },
                    { "Cambiar transformación", "Shift / S", "Botón Y / X" },
                    { "Pantalla Completa", "Alt + Enter  /  F11", "Guía / Touchpad" },
                    { "Salir del Juego", "Escape (Esc)", "Botón Back / Select" }
                };

                const char *actions_terra[][3] = {
                    { "Mover a Ark", "Flechas de dirección / W A S D", "D-Pad y Stick Izquierdo" },
                    { "Atacar / Golpear", "Z  /  Espacio", "Botón A (PlayStation Cruz)" },
                    { "Saltar", "X", "Botón B (PlayStation Círculo)" },
                    { "Correr / Empujar", "A  /  Shift", "Botón X (PlayStation Cuadrado)" },
                    { "Bloquear / Escudo", "C  /  V", "Gatillos LB / RB (L / R)" },
                    { "Caja de Pandora / Ítems", "Right Shift", "Botón Select / Share" },
                    { "Menú de Armas", "Enter / Intro", "Botón Start / Options" },
                    { "Salir del Juego", "Escape (Esc)", "Botón Back / Select" }
                };

                const char *actions_soe[][3] = {
                    { "Mover al Chico o Perro", "Flechas de dirección / W A S D", "D-Pad y Stick Izquierdo" },
                    { "Atacar con arma cargada", "Z  /  Espacio", "Botón A (PlayStation Cruz)" },
                    { "Anillo de Alquimia / Menú", "X", "Botón B (PlayStation Círculo)" },
                    { "Correr a gran velocidad", "A  /  Shift", "Botón X (PlayStation Cuadrado)" },
                    { "Alternar Chico / Perro", "Right Shift", "Botón Select / Share" },
                    { "Pausa", "Enter / Intro", "Botón Start / Options" },
                    { "Pantalla Completa", "Alt + Enter  /  F11", "Guía / Touchpad" },
                    { "Salir del Juego", "Escape (Esc)", "Botón Back / Select" }
                };

                const char *actions_ys[][3] = {
                    { "Mover a Adol", "Flechas de direcci\xc3\xb3n / W A S D", "D-Pad y Stick Izquierdo" },
                    { "Atacar con espada", "Z  /  Espacio", "Bot\xc3\xb3n A (PlayStation Cruz)" },
                    { "Saltar", "X", "Bot\xc3\xb3n B (PlayStation C\xc3\xad" "rculo)" },
                    { "Agacharse / Defender", "Flecha Abajo / S", "D-Pad Abajo / Stick Abajo" },
                    { "Usar Anillo / Magia", "A  /  Shift", "Bot\xc3\xb3n X (PlayStation Cuadrado)" },
                    { "Men\xc3\xba de Estado / \xc3\x8dtems", "Enter / Intro", "Bot\xc3\xb3n Start / Options" },
                    { "Pantalla Completa", "Alt + Enter  /  F11", "Gu\xc3\xad" "a / Touchpad" },
                    { "Salir del Juego", "Escape (Esc)", "Bot\xc3\xb3n Back / Select" }
                };

                const char *actions_zelda[][3] = {
                    { "Mover a Link", "Flechas de direcci\xc3\xb3n / W A S D", "D-Pad y Stick Izquierdo" },
                    { "Espada / Ataque Giro", "Z  /  Espacio", "Bot\xc3\xb3n A (PlayStation Cruz)" },
                    { "Usar \xc3\x8dtem Secundario", "X", "Bot\xc3\xb3n B (PlayStation C\xc3\xad" "rculo)" },
                    { "Correr (Botas) / Agarrar", "A  /  Shift", "Bot\xc3\xb3n X (PlayStation Cuadrado)" },
                    { "Hablar / Levantar", "C  /  V", "Bot\xc3\xb3n Y (PlayStation Tri\xc3\xa1ngulo)" },
                    { "Mapa del Mundo", "Right Shift", "Bot\xc3\xb3n Select / Share" },
                    { "Men\xc3\xba de Inventario", "Enter / Intro", "Bot\xc3\xb3n Start / Options" },
                    { "Salir del Juego", "Escape (Esc)", "Bot\xc3\xb3n Back / Select" }
                };

                const char *actions_rpg[][3] = {
                    { "Mover personaje", "Flechas de direcci\xc3\xb3n / W A S D", "D-Pad y Stick Izquierdo" },
                    { "Confirmar / Hablar / Acci\xc3\xb3n", "Z  /  Espacio", "Bot\xc3\xb3n A (PlayStation Cruz)" },
                    { "Cancelar / Correr / Volver", "X", "Bot\xc3\xb3n B (PlayStation C\xc3\xad" "rculo)" },
                    { "Men\xc3\xba de Objetos / Magia", "C  /  V", "Bot\xc3\xb3n Y (PlayStation Tri\xc3\xa1ngulo)" },
                    { "Cambiar l\xc3\xadder / Fila", "A  /  Shift", "Gatillos LB / RB (L / R)" },
                    { "Men\xc3\xba Principal / Pausa", "Enter / Intro", "Bot\xc3\xb3n Start / Options" },
                    { "Pantalla Completa", "Alt + Enter  /  F11", "Gu\xc3\xad" "a / Touchpad" },
                    { "Salir del Juego", "Escape (Esc)", "Bot\xc3\xb3n Back / Select" }
                };

                const char *(*act_table)[3] = is_iot ? actions_iot :
                                              (is_terra ? actions_terra :
                                              (is_soe ? actions_soe :
                                              (is_ys ? actions_ys :
                                              (is_zelda ? actions_zelda : actions_rpg))));

                for (int r = 0; r < 8; r++) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    if (r == 7) {
                        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.0f, 1.0f), "%s", act_table[r][0]);
                    } else {
                        ImGui::Text("%s", act_table[r][0]);
                    }
                    ImGui::TableSetColumnIndex(1);
                    if (r == 7) {
                        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.0f, 1.0f), "%s", act_table[r][1]);
                    } else {
                        ImGui::Text("%s", act_table[r][1]);
                    }
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%s", act_table[r][2]);
                }
                ImGui::EndTable();
            }

            ImGui::Dummy(ImVec2(0, 10.0f * scale));
            ImGui::TextColored(ImVec4(0.55f, 0.63f, 0.74f, 1.0f),
                               "Consejo: Pulsa la tecla Escape (Esc) en cualquier momento del juego para salir instant\xc3\xa1neamente.");
            if (font_regular) ImGui::PopFont();
        }

        // ── TAB 3: AUDIO ────────────────────────────────────────────────────
        else if (active_tab == 3) {
            if (font_large) ImGui::PushFont(font_large);
            ImGui::TextColored(ImVec4(0.90f, 0.93f, 0.98f, 1.0f), "Ajustes de Sonido");
            if (font_large) ImGui::PopFont();
            ImGui::Dummy(ImVec2(0, 12.0f * scale));

            if (font_regular) ImGui::PushFont(font_regular);
            ImGui::Text("Volumen Maestro:");
            ImGui::PushItemWidth(450.0f * scale);
            ImGui::SliderInt("##vol_slider", &volume, 0, 100, "%d%%");
            ImGui::PopItemWidth();

            ImGui::Dummy(ImVec2(0, 14.0f * scale));
            ImGui::TextColored(ImVec4(0.70f, 0.76f, 0.85f, 1.0f),
                               "Salida de audio: 48.000 Hz / 16-bit Est\xc3\xa9reo (SNES S-DSP + SPC700 Nativo).");
            ImGui::TextColored(ImVec4(0.55f, 0.63f, 0.74f, 1.0f),
                               "Mezcla y s\xc3\xadntesis de audio x64 en tiempo real con latencia m\xc3\xadnima.");
            if (font_regular) ImGui::PopFont();
        }

        // ── BARRA INFERIOR / FOOTER (PLAY + EXIT) ───────────────────────────
        ImGui::SetCursorPosY((float)win_h - 110.0f * scale);
        if (font_small) ImGui::PushFont(font_small);
        ImGui::TextColored(ImVec4(0.55f, 0.63f, 0.74f, 1.0f),
                           "Los ajustes se guardan autom\xc3\xa1ticamente al pulsar Jugar. Puedes pulsar Alt+Enter o F11 para alternar ventana.");
        if (font_small) ImGui::PopFont();

        ImGui::SetCursorPosY((float)win_h - 85.0f * scale);

        // Botón Jugar (Play)
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.000f, 0.463f, 0.000f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.000f, 0.580f, 0.150f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.850f, 0.380f, 0.000f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f * scale);

        if (font_large) ImGui::PushFont(font_large);
        if (ImGui::Button("  JUGAR (Play)  ", ImVec2(240.0f * scale, 56.0f * scale))) {
            running = false;
            result_action = RECOMP_LAUNCHER_RESULT_LAUNCH;
        }
        if (font_large) ImGui::PopFont();

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(4);

        ImGui::SameLine(0, 20.0f * scale);

        // Botón Salir (Quit)
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.16f, 0.25f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.24f, 0.36f, 1.0f));
        if (font_bold) ImGui::PushFont(font_bold);
        if (ImGui::Button("Salir", ImVec2(130.0f * scale, 56.0f * scale))) {
            running = false;
            result_action = RECOMP_LAUNCHER_RESULT_QUIT;
        }
        if (font_bold) ImGui::PopFont();
        ImGui::PopStyleColor(2);

        ImGui::EndChild();
        ImGui::End();

        // Actualizar cursor del ratón
        bool is_hovering = ImGui::IsAnyItemHovered() || ImGui::IsAnyItemActive() || (ImGui::GetMouseCursor() == ImGuiMouseCursor_Hand);
        if (is_hovering) {
            SDL_SetCursor(cursor_hand);
        } else {
            SDL_SetCursor(cursor_arrow);
        }

        ImGui::Render();
        glViewport(0, 0, (int)io_imgui.DisplaySize.x, (int)io_imgui.DisplaySize.y);
        glClearColor(0.047f, 0.071f, 0.122f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    // 6. Limpieza y guardado
    if (poster_tex) {
        glDeleteTextures(1, &poster_tex);
    }

    ImGui_ImplOpenGL3_Shutdown();
#if defined(LNG_SDL3)
    ImGui_ImplSDL3_Shutdown();
#else
    ImGui_ImplSDL2_Shutdown();
#endif
    ImGui::DestroyContext();
#if defined(LNG_SDL3)
    SDL_GL_DestroyContext(gl_context);
    SDL_DestroyWindow(window);
    if (cursor_hand) SDL_DestroyCursor(cursor_hand);
    if (cursor_arrow) SDL_DestroyCursor(cursor_arrow);
#else
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    if (cursor_hand) SDL_FreeCursor(cursor_hand);
    if (cursor_arrow) SDL_FreeCursor(cursor_arrow);
#endif

    // Guardar ajustes seleccionados
    save_snes_config(cfg_file, fullscreen, window_scale, display_aspect, widescreen,
                     linear_filter, vsync, volume, NULL, integer_scaling, color_mode);

    // Guardar rom.cfg
    if (current_rom[0] != '\0') {
        FILE *from = fopen("rom.cfg", "w");
        if (from) {
            fprintf(from, "%s\n", current_rom);
            fclose(from);
        }
    }

    // Actualizar io y out_rom_path para el llamador host_main
    if (io) {
        io->fullscreen = fullscreen;
        io->window_scale = window_scale;
        io->aspect_index = display_aspect;
        io->widescreen = widescreen;
        io->linear_filter = (linear_filter == 1) ? 1 : 0;
        io->volume = volume;
        io->vsync = vsync;
    }
    if (out_rom_path && out_rom_path_len > 0 && current_rom[0] != '\0') {
        strncpy(out_rom_path, current_rom, out_rom_path_len - 1);
        out_rom_path[out_rom_path_len - 1] = '\0';
    }

    return result_action;
}
