#include "mikupan_ui_theme.h"

#include "typedefs.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"
#include "mikupan/mikupan_config.h"
#include "mikupan/io/mikupan_file_c.h"
#include "mikupan/mikupan_utils.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

const char* const MikuPan_UiThemeLabels[MIKUPAN_UI_THEME_COUNT] = {
    "Moonlit Blue", "Ghost Cyan", "Crimson",
    "FF1 Ritual",   "Mist Teal",  "Sepia Photo",
};

const char* const MikuPan_UiFontLabels[MIKUPAN_UI_FONT_COUNT] = {
    "ImGui Default Monospace",
    "Century Old Style",
    "Zapf Chancery",
};

static ImFont* ui_fonts[MIKUPAN_UI_FONT_COUNT] = {0};
static float ui_display_scale = 1.0f;
static float ui_font_default_size = 13.0f;
static float ui_font_regular_size = 14.0f;

int MikuPan_ClampThemeIndex(int theme)
{
    if (theme < 0 || theme >= MIKUPAN_UI_THEME_COUNT)
    {
        return 0;
    }

    return theme;
}

int MikuPan_ClampFontIndex(int font)
{
    if (font < 0 || font >= MIKUPAN_UI_FONT_COUNT)
    {
        return 1;
    }

    return font;
}

static float MikuPan_CalculateUiDisplayScale(SDL_Window* window)
{
    SDL_DisplayID display = 0;
    const SDL_DisplayMode* mode = NULL;
    float content_scale = 1.0f;
    float resolution_scale = 1.0f;
    float scale;

    if (window != NULL)
    {
        display = SDL_GetDisplayForWindow(window);
        content_scale = SDL_GetWindowDisplayScale(window);
    }

    if (display == 0)
    {
        display = SDL_GetPrimaryDisplay();
    }

    if (display != 0)
    {
        mode = SDL_GetCurrentDisplayMode(display);
        if (content_scale <= 0.0f)
        {
            content_scale = SDL_GetDisplayContentScale(display);
        }
    }

    if (mode != NULL && mode->w > 0 && mode->h > 0)
    {
        const float sx = (float) mode->w / 1920.0f;
        const float sy = (float) mode->h / 1080.0f;
        resolution_scale = sx < sy ? sx : sy;
    }

    if (content_scale <= 0.0f)
    {
        content_scale = 1.0f;
    }

    scale = content_scale > resolution_scale ? content_scale : resolution_scale;
    return MikuPan_ClampFloat(scale, 0.85f, 2.25f);
}

void MikuPan_ApplyUiFont(int font)
{
    ImGuiIO* io = igGetIO_Nil();
    font = MikuPan_ClampFontIndex(font);

    if (ui_fonts[font] != NULL)
    {
        io->FontDefault = ui_fonts[font];
    }
}

void MikuPan_ApplyUiFontScale(void)
{
    ImGuiStyle* style = igGetStyle();
    MikuPan_ConfigurationValidate();
    style->FontScaleMain = mikupan_configuration.font_scale;
}

ImFont* MikuPan_GetTitleBlockFont(void)
{
    return ui_fonts[1] != NULL ? ui_fonts[1] : ui_fonts[0];
}

ImFont* MikuPan_GetTitleFont(void)
{
    if (ui_fonts[2] != NULL)
    {
        return ui_fonts[2];
    }

    return MikuPan_GetTitleBlockFont();
}

static ImFont* MikuPan_LoadUiFontTtf(const char* relative_path,
                                     float size_pixels)
{
    ImGuiIO* io = igGetIO_Nil();
    char font_path[1024];

    if (!MikuPan_ResolveBasePath(relative_path, font_path, sizeof(font_path)))
    {
        return NULL;
    }

    if (strncmp(font_path, "assets://", 9) == 0
        || strncmp(font_path, "assets:/", 8) == 0)
    {
        u_int font_size = MikuPan_GetFileSize(font_path);
        if (font_size > 0 && font_size <= INT_MAX)
        {
            void *font_data = malloc(font_size);
            if (font_data != NULL)
            {
                MikuPan_ReadFullFile(font_path, (char *)font_data);
                return ImFontAtlas_AddFontFromMemoryTTF(
                    io->Fonts, font_data, (int) font_size, size_pixels, NULL,
                    ImFontAtlas_GetGlyphRangesDefault(io->Fonts));
            }
        }

        return NULL;
    }

    return ImFontAtlas_AddFontFromFileTTF(
        io->Fonts, font_path, size_pixels, NULL,
        ImFontAtlas_GetGlyphRangesDefault(io->Fonts));
}

static void MikuPan_LoadUiFonts(void)
{
    ImGuiIO* io = igGetIO_Nil();
    ImFontConfig* default_font_config = ImFontConfig_ImFontConfig();

    ui_font_default_size = 13.0f * ui_display_scale;
    ui_font_regular_size = 14.0f * ui_display_scale;

    if (default_font_config != NULL)
    {
        default_font_config->SizePixels = ui_font_default_size;
    }

    ui_fonts[0] = ImFontAtlas_AddFontDefault(io->Fonts, default_font_config);

    if (default_font_config != NULL)
    {
        ImFontConfig_destroy(default_font_config);
    }

    ui_fonts[1] = MikuPan_LoadUiFontTtf(
        "resources/fonts/CenturyOldStyle.ttf", ui_font_regular_size);

    if (ui_fonts[1] == NULL)
    {
        ui_fonts[1] = ui_fonts[0];
    }

    ui_fonts[2] = MikuPan_LoadUiFontTtf(
        "resources/fonts/zapfchancer.ttf", ui_font_regular_size);

    if (ui_fonts[2] == NULL)
    {
        ui_fonts[2] = ui_fonts[0];
    }

    MikuPan_ConfigurationValidate();
    MikuPan_ApplyUiFont(mikupan_configuration.selected_font);
}

static void MikuPan_ApplyThemeBaseline(ImVec4* c)
{
    c[ImGuiCol_Separator] = ImVec4{0.22f, 0.24f, 0.28f, 0.55f};
    c[ImGuiCol_SeparatorHovered] = ImVec4{0.40f, 0.45f, 0.52f, 0.85f};
    c[ImGuiCol_SeparatorActive] = ImVec4{0.58f, 0.66f, 0.76f, 1.00f};
    c[ImGuiCol_ResizeGrip] = ImVec4{0.24f, 0.28f, 0.34f, 0.25f};
    c[ImGuiCol_ResizeGripHovered] = ImVec4{0.42f, 0.50f, 0.60f, 0.70f};
    c[ImGuiCol_ResizeGripActive] = ImVec4{0.58f, 0.68f, 0.80f, 0.95f};
    c[ImGuiCol_TabDimmed] = ImVec4{0.06f, 0.08f, 0.11f, 0.95f};
    c[ImGuiCol_TabDimmedSelected] = ImVec4{0.14f, 0.18f, 0.24f, 1.00f};
    c[ImGuiCol_TabDimmedSelectedOverline] =
        ImVec4{0.36f, 0.48f, 0.62f, 1.00f};
    c[ImGuiCol_PlotLines] = ImVec4{0.58f, 0.70f, 0.84f, 1.00f};
    c[ImGuiCol_PlotLinesHovered] = ImVec4{0.76f, 0.86f, 0.98f, 1.00f};
    c[ImGuiCol_PlotHistogram] = ImVec4{0.44f, 0.54f, 0.68f, 1.00f};
    c[ImGuiCol_PlotHistogramHovered] = ImVec4{0.64f, 0.76f, 0.92f, 1.00f};
    c[ImGuiCol_TableRowBg] = ImVec4{0.00f, 0.00f, 0.00f, 0.00f};
    c[ImGuiCol_TableRowBgAlt] = ImVec4{0.12f, 0.16f, 0.22f, 0.25f};
    c[ImGuiCol_DragDropTarget] = ImVec4{0.76f, 0.86f, 0.98f, 0.90f};
    c[ImGuiCol_NavCursor] = ImVec4{0.58f, 0.70f, 0.84f, 1.00f};
    c[ImGuiCol_NavWindowingHighlight] = ImVec4{0.76f, 0.86f, 0.98f, 0.70f};
    c[ImGuiCol_NavWindowingDimBg] = ImVec4{0.03f, 0.04f, 0.06f, 0.55f};
    c[ImGuiCol_ModalWindowDimBg] = ImVec4{0.02f, 0.03f, 0.05f, 0.72f};
}

void MikuPan_ApplyFatalFrameStyle(int theme)
{
    theme = MikuPan_ClampThemeIndex(theme);

    ImGuiStyle* s = igGetStyle();
    ImVec4* c = s->Colors;

    s->Alpha = 0.96f;
    s->DisabledAlpha = 0.45f;
    s->WindowRounding = 0.0f;
    s->ChildRounding = 0.0f;
    s->PopupRounding = 0.0f;
    s->FrameRounding = 0.0f;
    s->ScrollbarRounding = 0.0f;
    s->GrabRounding = 0.0f;
    s->TabRounding = 0.0f;
    s->WindowBorderSize = 1.0f;
    s->ChildBorderSize = 1.0f;
    s->PopupBorderSize = 1.0f;
    s->FrameBorderSize = 1.0f;
    s->TabBorderSize = 1.0f;
    s->WindowPadding = ImVec2{10.0f, 10.0f};
    s->FramePadding = ImVec2{8.0f, 4.0f};
    s->ItemSpacing = ImVec2{8.0f, 5.0f};
    s->ItemInnerSpacing = ImVec2{6.0f, 4.0f};
    s->IndentSpacing = 18.0f;
    s->ScrollbarSize = 14.0f;
    s->GrabMinSize = 10.0f;

    if (theme != 2)
    {
        MikuPan_ApplyThemeBaseline(c);
    }

    switch (theme)
    {
        default:
        case 0:
            // Text: pale paper with a hint of purple-blue
            c[ImGuiCol_Text] = ImVec4{0.78f, 0.82f, 0.88f, 1.00f};
            c[ImGuiCol_TextDisabled] = ImVec4{0.32f, 0.36f, 0.42f, 1.00f};

            // Background: deep blue-black like moonlit mansion
            c[ImGuiCol_WindowBg] = ImVec4{0.04f, 0.06f, 0.10f, 0.96f};
            c[ImGuiCol_ChildBg] = ImVec4{0.06f, 0.08f, 0.12f, 0.85f};
            c[ImGuiCol_PopupBg] = ImVec4{0.03f, 0.05f, 0.08f, 0.98f};

            // Borders: slate with a cold purple lean
            c[ImGuiCol_Border] = ImVec4{0.24f, 0.28f, 0.35f, 0.65f};
            c[ImGuiCol_BorderShadow] = ImVec4{0, 0, 0, 0};

            // Frames: cool gray-blue
            c[ImGuiCol_FrameBg] = ImVec4{0.09f, 0.12f, 0.16f, 0.95f};
            c[ImGuiCol_FrameBgHovered] = ImVec4{0.16f, 0.21f, 0.28f, 0.95f};
            c[ImGuiCol_FrameBgActive] = ImVec4{0.23f, 0.30f, 0.40f, 1.00f};

            // Title bars: dark with cold highlights
            c[ImGuiCol_TitleBg] = ImVec4{0.05f, 0.07f, 0.10f, 1.00f};
            c[ImGuiCol_TitleBgActive] = ImVec4{0.10f, 0.14f, 0.18f, 1.00f};
            c[ImGuiCol_TitleBgCollapsed] =
                ImVec4{0.03f, 0.05f, 0.08f, 0.85f};

            c[ImGuiCol_MenuBarBg] = ImVec4{0.07f, 0.09f, 0.12f, 1.00f};

            // Scrollbars: soft azure
            c[ImGuiCol_ScrollbarBg] = ImVec4{0.03f, 0.04f, 0.06f, 0.85f};
            c[ImGuiCol_ScrollbarGrab] = ImVec4{0.18f, 0.24f, 0.30f, 1.00f};
            c[ImGuiCol_ScrollbarGrabHovered] =
                ImVec4{0.26f, 0.34f, 0.42f, 1.00f};
            c[ImGuiCol_ScrollbarGrabActive] =
                ImVec4{0.34f, 0.44f, 0.54f, 1.00f};

            // Accent: ghost-light cyan / pale purple
            c[ImGuiCol_CheckMark] = ImVec4{0.60f, 0.78f, 0.96f, 1.00f};
            c[ImGuiCol_SliderGrab] = ImVec4{0.42f, 0.64f, 0.88f, 1.00f};
            c[ImGuiCol_SliderGrabActive] =
                ImVec4{0.60f, 0.84f, 0.98f, 1.00f};

            // Buttons: twilight blue-gray
            c[ImGuiCol_Button] = ImVec4{0.08f, 0.10f, 0.14f, 0.95f};
            c[ImGuiCol_ButtonHovered] = ImVec4{0.18f, 0.24f, 0.32f, 1.00f};
            c[ImGuiCol_ButtonActive] = ImVec4{0.28f, 0.36f, 0.48f, 1.00f};

            // Headers
            c[ImGuiCol_Header] = ImVec4{0.12f, 0.16f, 0.22f, 0.85f};
            c[ImGuiCol_HeaderHovered] = ImVec4{0.20f, 0.28f, 0.38f, 0.95f};
            c[ImGuiCol_HeaderActive] = ImVec4{0.28f, 0.38f, 0.52f, 1.00f};

            // Tabs
            c[ImGuiCol_Tab] = ImVec4{0.08f, 0.10f, 0.14f, 0.95f};
            c[ImGuiCol_TabHovered] = ImVec4{0.20f, 0.28f, 0.38f, 0.95f};
            c[ImGuiCol_TabSelected] = ImVec4{0.26f, 0.34f, 0.46f, 1.00f};
            c[ImGuiCol_TabSelectedOverline] =
                ImVec4{0.60f, 0.78f, 0.96f, 1.00f};

            // Misc accents (plots, nav, tables, etc.)
            c[ImGuiCol_TextLink] = ImVec4{0.60f, 0.78f, 0.96f, 1.00f};
            c[ImGuiCol_TextSelectedBg] = ImVec4{0.25f, 0.32f, 0.42f, 0.65f};
            c[ImGuiCol_TableHeaderBg] = ImVec4{0.08f, 0.12f, 0.16f, 1.00f};
            c[ImGuiCol_TableBorderStrong] =
                ImVec4{0.22f, 0.28f, 0.34f, 1.00f};
            c[ImGuiCol_TableBorderLight] =
                ImVec4{0.14f, 0.18f, 0.24f, 1.00f};
            break;
        case 1:
            // Text: pale paper, slightly cold
            c[ImGuiCol_Text] = ImVec4{0.82f, 0.86f, 0.88f, 1.00f};
            c[ImGuiCol_TextDisabled] = ImVec4{0.38f, 0.42f, 0.45f, 1.00f};

            // Backgrounds: blue-black ink
            c[ImGuiCol_WindowBg] = ImVec4{0.05f, 0.07f, 0.09f, 0.96f};
            c[ImGuiCol_ChildBg] = ImVec4{0.07f, 0.09f, 0.11f, 0.85f};
            c[ImGuiCol_PopupBg] = ImVec4{0.04f, 0.06f, 0.08f, 0.98f};

            // Borders: cold slate
            c[ImGuiCol_Border] = ImVec4{0.22f, 0.28f, 0.32f, 0.65f};
            c[ImGuiCol_BorderShadow] = ImVec4{0, 0, 0, 0};

            // Frames: dusty wood in moonlight
            c[ImGuiCol_FrameBg] = ImVec4{0.10f, 0.13f, 0.16f, 0.95f};
            c[ImGuiCol_FrameBgHovered] = ImVec4{0.18f, 0.24f, 0.28f, 0.95f};
            c[ImGuiCol_FrameBgActive] = ImVec4{0.26f, 0.34f, 0.40f, 1.00f};

            // Titles: very dark blue-black
            c[ImGuiCol_TitleBg] = ImVec4{0.06f, 0.08f, 0.10f, 1.00f};
            c[ImGuiCol_TitleBgActive] = ImVec4{0.12f, 0.18f, 0.22f, 1.00f};
            c[ImGuiCol_TitleBgCollapsed] =
                ImVec4{0.05f, 0.07f, 0.09f, 0.85f};

            c[ImGuiCol_MenuBarBg] = ImVec4{0.08f, 0.10f, 0.12f, 1.00f};

            // Scrollbar: faint blue steel
            c[ImGuiCol_ScrollbarBg] = ImVec4{0.04f, 0.05f, 0.07f, 0.85f};
            c[ImGuiCol_ScrollbarGrab] = ImVec4{0.22f, 0.30f, 0.36f, 1.00f};
            c[ImGuiCol_ScrollbarGrabHovered] =
                ImVec4{0.30f, 0.40f, 0.46f, 1.00f};
            c[ImGuiCol_ScrollbarGrabActive] =
                ImVec4{0.38f, 0.50f, 0.58f, 1.00f};

            // Accent color: ghostly cyan (FF1 signature)
            c[ImGuiCol_CheckMark] = ImVec4{0.60f, 0.85f, 0.90f, 1.00f};
            c[ImGuiCol_SliderGrab] = ImVec4{0.50f, 0.78f, 0.84f, 1.00f};
            c[ImGuiCol_SliderGrabActive] =
                ImVec4{0.70f, 0.92f, 0.98f, 1.00f};

            // Buttons: foggy slate
            c[ImGuiCol_Button] = ImVec4{0.12f, 0.16f, 0.20f, 0.95f};
            c[ImGuiCol_ButtonHovered] = ImVec4{0.22f, 0.30f, 0.36f, 1.00f};
            c[ImGuiCol_ButtonActive] = ImVec4{0.32f, 0.42f, 0.50f, 1.00f};

            // Headers
            c[ImGuiCol_Header] = ImVec4{0.14f, 0.20f, 0.24f, 0.85f};
            c[ImGuiCol_HeaderHovered] = ImVec4{0.24f, 0.34f, 0.40f, 0.95f};
            c[ImGuiCol_HeaderActive] = ImVec4{0.34f, 0.46f, 0.54f, 1.00f};

            // Separators
            c[ImGuiCol_Separator] = ImVec4{0.22f, 0.28f, 0.32f, 0.55f};
            c[ImGuiCol_SeparatorHovered] =
                ImVec4{0.40f, 0.52f, 0.60f, 0.85f};
            c[ImGuiCol_SeparatorActive] = ImVec4{0.55f, 0.70f, 0.80f, 1.00f};

            // Tabs
            c[ImGuiCol_Tab] = ImVec4{0.10f, 0.14f, 0.18f, 0.95f};
            c[ImGuiCol_TabHovered] = ImVec4{0.24f, 0.34f, 0.40f, 0.95f};
            c[ImGuiCol_TabSelected] = ImVec4{0.30f, 0.42f, 0.50f, 1.00f};
            c[ImGuiCol_TabSelectedOverline] =
                ImVec4{0.65f, 0.90f, 0.95f, 1.00f};

            // Tables / misc accents
            c[ImGuiCol_TableHeaderBg] = ImVec4{0.12f, 0.18f, 0.22f, 1.00f};
            c[ImGuiCol_TableBorderStrong] =
                ImVec4{0.22f, 0.30f, 0.36f, 1.00f};
            c[ImGuiCol_TableBorderLight] =
                ImVec4{0.16f, 0.22f, 0.26f, 1.00f};
            c[ImGuiCol_TextLink] = ImVec4{0.65f, 0.90f, 0.95f, 1.00f};
            c[ImGuiCol_TextSelectedBg] = ImVec4{0.30f, 0.42f, 0.50f, 0.65f};

            // Nav / modal
            c[ImGuiCol_NavCursor] = ImVec4{0.65f, 0.90f, 0.95f, 1.00f};
            c[ImGuiCol_NavWindowingHighlight] =
                ImVec4{0.65f, 0.90f, 0.95f, 0.70f};
            c[ImGuiCol_NavWindowingDimBg] =
                ImVec4{0.04f, 0.05f, 0.07f, 0.50f};
            c[ImGuiCol_ModalWindowDimBg] =
                ImVec4{0.04f, 0.05f, 0.07f, 0.65f};
            break;
        case 2:
            c[ImGuiCol_Text] = ImVec4{0.86f, 0.78f, 0.62f, 1.00f};
            c[ImGuiCol_TextDisabled] = ImVec4{0.42f, 0.36f, 0.28f, 1.00f};
            c[ImGuiCol_WindowBg] = ImVec4{0.06f, 0.04f, 0.03f, 0.96f};
            c[ImGuiCol_ChildBg] = ImVec4{0.08f, 0.05f, 0.04f, 0.85f};
            c[ImGuiCol_PopupBg] = ImVec4{0.05f, 0.03f, 0.02f, 0.98f};
            c[ImGuiCol_Border] = ImVec4{0.40f, 0.10f, 0.08f, 0.65f};
            c[ImGuiCol_BorderShadow] = ImVec4{0.00f, 0.00f, 0.00f, 0.00f};
            c[ImGuiCol_FrameBg] = ImVec4{0.12f, 0.07f, 0.05f, 0.95f};
            c[ImGuiCol_FrameBgHovered] = ImVec4{0.32f, 0.10f, 0.08f, 0.85f};
            c[ImGuiCol_FrameBgActive] = ImVec4{0.52f, 0.12f, 0.10f, 0.95f};
            c[ImGuiCol_TitleBg] = ImVec4{0.08f, 0.04f, 0.03f, 1.00f};
            c[ImGuiCol_TitleBgActive] = ImVec4{0.35f, 0.05f, 0.05f, 1.00f};
            c[ImGuiCol_TitleBgCollapsed] =
                ImVec4{0.05f, 0.02f, 0.02f, 0.85f};
            c[ImGuiCol_MenuBarBg] = ImVec4{0.10f, 0.05f, 0.04f, 1.00f};
            c[ImGuiCol_ScrollbarBg] = ImVec4{0.04f, 0.02f, 0.02f, 0.85f};
            c[ImGuiCol_ScrollbarGrab] = ImVec4{0.28f, 0.16f, 0.10f, 1.00f};
            c[ImGuiCol_ScrollbarGrabHovered] =
                ImVec4{0.50f, 0.20f, 0.12f, 1.00f};
            c[ImGuiCol_ScrollbarGrabActive] =
                ImVec4{0.65f, 0.22f, 0.14f, 1.00f};
            c[ImGuiCol_CheckMark] = ImVec4{0.85f, 0.40f, 0.18f, 1.00f};
            c[ImGuiCol_SliderGrab] = ImVec4{0.60f, 0.20f, 0.14f, 1.00f};
            c[ImGuiCol_SliderGrabActive] =
                ImVec4{0.80f, 0.30f, 0.16f, 1.00f};
            c[ImGuiCol_Button] = ImVec4{0.18f, 0.08f, 0.06f, 0.95f};
            c[ImGuiCol_ButtonHovered] = ImVec4{0.45f, 0.12f, 0.10f, 1.00f};
            c[ImGuiCol_ButtonActive] = ImVec4{0.65f, 0.16f, 0.12f, 1.00f};
            c[ImGuiCol_Header] = ImVec4{0.22f, 0.08f, 0.06f, 0.85f};
            c[ImGuiCol_HeaderHovered] = ImVec4{0.45f, 0.12f, 0.10f, 0.95f};
            c[ImGuiCol_HeaderActive] = ImVec4{0.60f, 0.14f, 0.10f, 1.00f};
            c[ImGuiCol_Separator] = ImVec4{0.40f, 0.12f, 0.10f, 0.55f};
            c[ImGuiCol_SeparatorHovered] =
                ImVec4{0.65f, 0.18f, 0.12f, 0.85f};
            c[ImGuiCol_SeparatorActive] = ImVec4{0.85f, 0.24f, 0.14f, 1.00f};
            c[ImGuiCol_ResizeGrip] = ImVec4{0.28f, 0.12f, 0.08f, 0.50f};
            c[ImGuiCol_ResizeGripHovered] =
                ImVec4{0.55f, 0.16f, 0.10f, 0.85f};
            c[ImGuiCol_ResizeGripActive] =
                ImVec4{0.80f, 0.22f, 0.14f, 1.00f};
            c[ImGuiCol_Tab] = ImVec4{0.14f, 0.06f, 0.04f, 0.95f};
            c[ImGuiCol_TabHovered] = ImVec4{0.45f, 0.12f, 0.10f, 0.95f};
            c[ImGuiCol_TabSelected] = ImVec4{0.55f, 0.14f, 0.10f, 1.00f};
            c[ImGuiCol_TabSelectedOverline] =
                ImVec4{0.85f, 0.25f, 0.14f, 1.00f};
            c[ImGuiCol_TabDimmed] = ImVec4{0.10f, 0.05f, 0.03f, 0.95f};
            c[ImGuiCol_TabDimmedSelected] =
                ImVec4{0.30f, 0.08f, 0.06f, 1.00f};
            c[ImGuiCol_TabDimmedSelectedOverline] =
                ImVec4{0.55f, 0.14f, 0.10f, 1.00f};
            c[ImGuiCol_PlotLines] = ImVec4{0.85f, 0.50f, 0.18f, 1.00f};
            c[ImGuiCol_PlotLinesHovered] =
                ImVec4{1.00f, 0.55f, 0.20f, 1.00f};
            c[ImGuiCol_PlotHistogram] = ImVec4{0.65f, 0.20f, 0.14f, 1.00f};
            c[ImGuiCol_PlotHistogramHovered] =
                ImVec4{0.90f, 0.30f, 0.18f, 1.00f};
            c[ImGuiCol_TableHeaderBg] = ImVec4{0.18f, 0.08f, 0.06f, 1.00f};
            c[ImGuiCol_TableBorderStrong] =
                ImVec4{0.40f, 0.12f, 0.10f, 1.00f};
            c[ImGuiCol_TableBorderLight] =
                ImVec4{0.25f, 0.10f, 0.08f, 1.00f};
            c[ImGuiCol_TableRowBg] = ImVec4{0.00f, 0.00f, 0.00f, 0.00f};
            c[ImGuiCol_TableRowBgAlt] = ImVec4{0.95f, 0.50f, 0.18f, 0.04f};
            c[ImGuiCol_TextLink] = ImVec4{0.95f, 0.50f, 0.18f, 1.00f};
            c[ImGuiCol_TextSelectedBg] = ImVec4{0.55f, 0.14f, 0.10f, 0.65f};
            c[ImGuiCol_DragDropTarget] = ImVec4{0.95f, 0.50f, 0.18f, 0.90f};
            c[ImGuiCol_NavCursor] = ImVec4{0.85f, 0.25f, 0.14f, 1.00f};
            c[ImGuiCol_NavWindowingHighlight] =
                ImVec4{0.95f, 0.50f, 0.18f, 0.70f};
            c[ImGuiCol_NavWindowingDimBg] =
                ImVec4{0.04f, 0.02f, 0.02f, 0.50f};
            c[ImGuiCol_ModalWindowDimBg] =
                ImVec4{0.04f, 0.02f, 0.02f, 0.65f};
            break;

        case 3:
            /*
                Fatal Frame 1 PS2 inspired theme.

                Palette direction:
                - blackened wood / tatami shadows
                - dried blood red-browns
                - faded paper text
                - restrained bruised violet accents
                - sharp, square PS2-era UI shapes
            */
            // Text: faded paper / old photograph highlight
            c[ImGuiCol_Text] = ImVec4{0.86f, 0.80f, 0.70f, 1.00f};
            c[ImGuiCol_TextDisabled] = ImVec4{0.42f, 0.35f, 0.34f, 1.00f};

            // Backgrounds: blackened tatami / dark old wood
            c[ImGuiCol_WindowBg] = ImVec4{0.055f, 0.035f, 0.040f, 0.96f};
            c[ImGuiCol_ChildBg] = ImVec4{0.085f, 0.055f, 0.055f, 0.86f};
            c[ImGuiCol_PopupBg] = ImVec4{0.045f, 0.030f, 0.035f, 0.98f};

            // Borders: dusty red-brown lacquer
            c[ImGuiCol_Border] = ImVec4{0.31f, 0.19f, 0.18f, 0.68f};
            c[ImGuiCol_BorderShadow] = ImVec4{0.00f, 0.00f, 0.00f, 0.00f};

            // Frames: aged wood / stained cloth
            c[ImGuiCol_FrameBg] = ImVec4{0.13f, 0.075f, 0.080f, 0.95f};
            c[ImGuiCol_FrameBgHovered] = ImVec4{0.24f, 0.12f, 0.15f, 0.96f};
            c[ImGuiCol_FrameBgActive] = ImVec4{0.36f, 0.16f, 0.22f, 1.00f};

            // Titles: deep cover-shadow red-black
            c[ImGuiCol_TitleBg] = ImVec4{0.07f, 0.035f, 0.040f, 1.00f};
            c[ImGuiCol_TitleBgActive] = ImVec4{0.20f, 0.075f, 0.110f, 1.00f};
            c[ImGuiCol_TitleBgCollapsed] =
                ImVec4{0.055f, 0.030f, 0.035f, 0.86f};

            c[ImGuiCol_MenuBarBg] = ImVec4{0.10f, 0.060f, 0.060f, 1.00f};

            // Scrollbar: old cabinet / dark rust
            c[ImGuiCol_ScrollbarBg] = ImVec4{0.045f, 0.030f, 0.035f, 0.86f};
            c[ImGuiCol_ScrollbarGrab] = ImVec4{0.28f, 0.14f, 0.16f, 1.00f};
            c[ImGuiCol_ScrollbarGrabHovered] =
                ImVec4{0.40f, 0.18f, 0.23f, 1.00f};
            c[ImGuiCol_ScrollbarGrabActive] =
                ImVec4{0.54f, 0.22f, 0.31f, 1.00f};

            // Accent color: muted ritual violet / bruised crimson-purple
            c[ImGuiCol_CheckMark] = ImVec4{0.58f, 0.34f, 0.62f, 1.00f};
            c[ImGuiCol_SliderGrab] = ImVec4{0.42f, 0.23f, 0.48f, 1.00f};
            c[ImGuiCol_SliderGrabActive] =
                ImVec4{0.68f, 0.42f, 0.72f, 1.00f};

            // Buttons: muted blood-stained wood
            c[ImGuiCol_Button] = ImVec4{0.16f, 0.075f, 0.085f, 0.95f};
            c[ImGuiCol_ButtonHovered] = ImVec4{0.32f, 0.13f, 0.18f, 1.00f};
            c[ImGuiCol_ButtonActive] = ImVec4{0.40f, 0.15f, 0.22f, 1.00f};

            // Headers: carpet maroon / dried blood
            c[ImGuiCol_Header] = ImVec4{0.20f, 0.085f, 0.105f, 0.86f};
            c[ImGuiCol_HeaderHovered] = ImVec4{0.38f, 0.14f, 0.20f, 0.96f};
            c[ImGuiCol_HeaderActive] = ImVec4{0.42f, 0.15f, 0.24f, 1.00f};

            // Separators: worn red-brown edges with violet interaction highlight
            c[ImGuiCol_Separator] = ImVec4{0.31f, 0.19f, 0.18f, 0.58f};
            c[ImGuiCol_SeparatorHovered] =
                ImVec4{0.48f, 0.26f, 0.46f, 0.85f};
            c[ImGuiCol_SeparatorActive] = ImVec4{0.62f, 0.36f, 0.66f, 1.00f};

            // Resize grips: dim until interacted with
            c[ImGuiCol_ResizeGrip] = ImVec4{0.31f, 0.19f, 0.18f, 0.25f};
            c[ImGuiCol_ResizeGripHovered] =
                ImVec4{0.48f, 0.26f, 0.46f, 0.70f};
            c[ImGuiCol_ResizeGripActive] =
                ImVec4{0.62f, 0.36f, 0.66f, 0.95f};

            // Tabs: dark red-brown, selected with restrained shrine-violet
            c[ImGuiCol_Tab] = ImVec4{0.13f, 0.065f, 0.075f, 0.95f};
            c[ImGuiCol_TabHovered] = ImVec4{0.38f, 0.14f, 0.20f, 0.96f};
            c[ImGuiCol_TabSelected] = ImVec4{0.34f, 0.13f, 0.22f, 1.00f};
            c[ImGuiCol_TabSelectedOverline] =
                ImVec4{0.62f, 0.36f, 0.66f, 1.00f};
            c[ImGuiCol_TabDimmed] = ImVec4{0.09f, 0.045f, 0.050f, 0.95f};
            c[ImGuiCol_TabDimmedSelected] =
                ImVec4{0.18f, 0.075f, 0.105f, 1.00f};
            c[ImGuiCol_TabDimmedSelectedOverline] =
                ImVec4{0.38f, 0.22f, 0.42f, 1.00f};

            // Plot colors: subdued, readable, not too modern-looking
            c[ImGuiCol_PlotLines] = ImVec4{0.66f, 0.44f, 0.68f, 1.00f};
            c[ImGuiCol_PlotLinesHovered] =
                ImVec4{0.80f, 0.58f, 0.82f, 1.00f};
            c[ImGuiCol_PlotHistogram] = ImVec4{0.50f, 0.22f, 0.28f, 1.00f};
            c[ImGuiCol_PlotHistogramHovered] =
                ImVec4{0.68f, 0.30f, 0.40f, 1.00f};

            // Tables / misc accents
            c[ImGuiCol_TableHeaderBg] = ImVec4{0.16f, 0.075f, 0.090f, 1.00f};
            c[ImGuiCol_TableBorderStrong] =
                ImVec4{0.34f, 0.18f, 0.19f, 1.00f};
            c[ImGuiCol_TableBorderLight] =
                ImVec4{0.23f, 0.12f, 0.13f, 1.00f};
            c[ImGuiCol_TableRowBg] = ImVec4{0.00f, 0.00f, 0.00f, 0.00f};
            c[ImGuiCol_TableRowBgAlt] = ImVec4{0.12f, 0.065f, 0.070f, 0.32f};

            c[ImGuiCol_TextLink] = ImVec4{0.66f, 0.44f, 0.68f, 1.00f};
            c[ImGuiCol_TextSelectedBg] = ImVec4{0.34f, 0.18f, 0.36f, 0.65f};

            // Drag/drop and highlights
            c[ImGuiCol_DragDropTarget] = ImVec4{0.80f, 0.58f, 0.82f, 0.90f};

            // Nav / modal
            c[ImGuiCol_NavCursor] = ImVec4{0.66f, 0.44f, 0.68f, 1.00f};
            c[ImGuiCol_NavWindowingHighlight] =
                ImVec4{0.66f, 0.44f, 0.68f, 0.70f};
            c[ImGuiCol_NavWindowingDimBg] =
                ImVec4{0.045f, 0.030f, 0.035f, 0.55f};
            c[ImGuiCol_ModalWindowDimBg] =
                ImVec4{0.035f, 0.020f, 0.025f, 0.72f};
            break;
        case 4:
            // Text: pale moonlit cloth / ghost grey
            c[ImGuiCol_Text] = ImVec4{0.78f, 0.84f, 0.82f, 1.00f};
            c[ImGuiCol_TextDisabled] = ImVec4{0.36f, 0.43f, 0.42f, 1.00f};

            // Backgrounds: cold blue-green night fog
            c[ImGuiCol_WindowBg] = ImVec4{0.035f, 0.070f, 0.075f, 0.96f};
            c[ImGuiCol_ChildBg] = ImVec4{0.055f, 0.095f, 0.100f, 0.86f};
            c[ImGuiCol_PopupBg] = ImVec4{0.025f, 0.055f, 0.060f, 0.98f};

            // Borders: misty oxidized teal
            c[ImGuiCol_Border] = ImVec4{0.20f, 0.34f, 0.35f, 0.66f};
            c[ImGuiCol_BorderShadow] = ImVec4{0.00f, 0.00f, 0.00f, 0.00f};

            // Frames: dark wet wood under moonlight
            c[ImGuiCol_FrameBg] = ImVec4{0.070f, 0.120f, 0.125f, 0.95f};
            c[ImGuiCol_FrameBgHovered] =
                ImVec4{0.120f, 0.210f, 0.215f, 0.96f};
            c[ImGuiCol_FrameBgActive] =
                ImVec4{0.170f, 0.300f, 0.310f, 1.00f};

            // Titles: deep blue-green shadow
            c[ImGuiCol_TitleBg] = ImVec4{0.030f, 0.060f, 0.065f, 1.00f};
            c[ImGuiCol_TitleBgActive] =
                ImVec4{0.075f, 0.145f, 0.155f, 1.00f};
            c[ImGuiCol_TitleBgCollapsed] =
                ImVec4{0.030f, 0.055f, 0.060f, 0.86f};

            c[ImGuiCol_MenuBarBg] = ImVec4{0.050f, 0.095f, 0.100f, 1.00f};

            // Scrollbar: dull teal slate
            c[ImGuiCol_ScrollbarBg] = ImVec4{0.025f, 0.050f, 0.055f, 0.86f};
            c[ImGuiCol_ScrollbarGrab] =
                ImVec4{0.150f, 0.270f, 0.280f, 1.00f};
            c[ImGuiCol_ScrollbarGrabHovered] =
                ImVec4{0.210f, 0.370f, 0.380f, 1.00f};
            c[ImGuiCol_ScrollbarGrabActive] =
                ImVec4{0.290f, 0.500f, 0.510f, 1.00f};

            // Accent color: restrained spirit violet, less pink
            c[ImGuiCol_CheckMark] = ImVec4{0.56f, 0.45f, 0.72f, 1.00f};
            c[ImGuiCol_SliderGrab] = ImVec4{0.40f, 0.32f, 0.58f, 1.00f};
            c[ImGuiCol_SliderGrabActive] =
                ImVec4{0.66f, 0.55f, 0.82f, 1.00f};

            // Buttons: foggy teal-black
            c[ImGuiCol_Button] = ImVec4{0.075f, 0.130f, 0.135f, 0.95f};
            c[ImGuiCol_ButtonHovered] =
                ImVec4{0.145f, 0.255f, 0.265f, 1.00f};
            c[ImGuiCol_ButtonActive] = ImVec4{0.205f, 0.355f, 0.365f, 1.00f};

            // Headers: darker forest-fog teal
            c[ImGuiCol_Header] = ImVec4{0.095f, 0.175f, 0.180f, 0.86f};
            c[ImGuiCol_HeaderHovered] =
                ImVec4{0.165f, 0.300f, 0.310f, 0.96f};
            c[ImGuiCol_HeaderActive] = ImVec4{0.230f, 0.405f, 0.420f, 1.00f};

            // Separators: fogged teal edges with violet interaction highlight
            c[ImGuiCol_Separator] = ImVec4{0.20f, 0.34f, 0.35f, 0.55f};
            c[ImGuiCol_SeparatorHovered] =
                ImVec4{0.38f, 0.34f, 0.56f, 0.85f};
            c[ImGuiCol_SeparatorActive] = ImVec4{0.56f, 0.45f, 0.72f, 1.00f};

            // Resize grips: dim until interacted with
            c[ImGuiCol_ResizeGrip] = ImVec4{0.20f, 0.34f, 0.35f, 0.25f};
            c[ImGuiCol_ResizeGripHovered] =
                ImVec4{0.38f, 0.34f, 0.56f, 0.70f};
            c[ImGuiCol_ResizeGripActive] =
                ImVec4{0.56f, 0.45f, 0.72f, 0.95f};

            // Tabs: cold teal, selected with subtle spirit violet
            c[ImGuiCol_Tab] = ImVec4{0.065f, 0.120f, 0.125f, 0.95f};
            c[ImGuiCol_TabHovered] = ImVec4{0.165f, 0.300f, 0.310f, 0.96f};
            c[ImGuiCol_TabSelected] = ImVec4{0.145f, 0.240f, 0.270f, 1.00f};
            c[ImGuiCol_TabSelectedOverline] =
                ImVec4{0.56f, 0.45f, 0.72f, 1.00f};
            c[ImGuiCol_TabDimmed] = ImVec4{0.040f, 0.080f, 0.085f, 0.95f};
            c[ImGuiCol_TabDimmedSelected] =
                ImVec4{0.085f, 0.145f, 0.155f, 1.00f};
            c[ImGuiCol_TabDimmedSelectedOverline] =
                ImVec4{0.34f, 0.30f, 0.48f, 1.00f};

            // Plot colors: subdued ghost-violet and muted blood-red
            c[ImGuiCol_PlotLines] = ImVec4{0.56f, 0.45f, 0.72f, 1.00f};
            c[ImGuiCol_PlotLinesHovered] =
                ImVec4{0.70f, 0.62f, 0.86f, 1.00f};
            c[ImGuiCol_PlotHistogram] = ImVec4{0.42f, 0.18f, 0.22f, 1.00f};
            c[ImGuiCol_PlotHistogramHovered] =
                ImVec4{0.58f, 0.26f, 0.32f, 1.00f};

            // Tables / misc accents
            c[ImGuiCol_TableHeaderBg] =
                ImVec4{0.075f, 0.145f, 0.150f, 1.00f};
            c[ImGuiCol_TableBorderStrong] =
                ImVec4{0.20f, 0.34f, 0.35f, 1.00f};
            c[ImGuiCol_TableBorderLight] =
                ImVec4{0.120f, 0.230f, 0.235f, 1.00f};
            c[ImGuiCol_TableRowBg] = ImVec4{0.00f, 0.00f, 0.00f, 0.00f};
            c[ImGuiCol_TableRowBgAlt] =
                ImVec4{0.070f, 0.125f, 0.130f, 0.30f};

            c[ImGuiCol_TextLink] = ImVec4{0.62f, 0.56f, 0.78f, 1.00f};
            c[ImGuiCol_TextSelectedBg] = ImVec4{0.28f, 0.30f, 0.44f, 0.65f};

            // Drag/drop and highlights
            c[ImGuiCol_DragDropTarget] = ImVec4{0.70f, 0.62f, 0.86f, 0.90f};

            // Nav / modal
            c[ImGuiCol_NavCursor] = ImVec4{0.62f, 0.56f, 0.78f, 1.00f};
            c[ImGuiCol_NavWindowingHighlight] =
                ImVec4{0.62f, 0.56f, 0.78f, 0.70f};
            c[ImGuiCol_NavWindowingDimBg] =
                ImVec4{0.025f, 0.050f, 0.055f, 0.55f};
            c[ImGuiCol_ModalWindowDimBg] =
                ImVec4{0.020f, 0.040f, 0.045f, 0.72f};
            break;
        case 5:
            // Text: faded photographic paper
            c[ImGuiCol_Text] = ImVec4{0.86f, 0.80f, 0.68f, 1.00f};
            c[ImGuiCol_TextDisabled] = ImVec4{0.42f, 0.36f, 0.27f, 1.00f};

            // Backgrounds: dark sepia print / aged ink
            c[ImGuiCol_WindowBg] = ImVec4{0.055f, 0.045f, 0.030f, 0.96f};
            c[ImGuiCol_ChildBg] = ImVec4{0.080f, 0.065f, 0.045f, 0.86f};
            c[ImGuiCol_PopupBg] = ImVec4{0.045f, 0.035f, 0.024f, 0.98f};

            // Borders: old varnish / worn frame edges
            c[ImGuiCol_Border] = ImVec4{0.36f, 0.27f, 0.16f, 0.68f};
            c[ImGuiCol_BorderShadow] = ImVec4{0.00f, 0.00f, 0.00f, 0.00f};

            // Frames: cabinet wood and photo album paper
            c[ImGuiCol_FrameBg] = ImVec4{0.120f, 0.090f, 0.055f, 0.95f};
            c[ImGuiCol_FrameBgHovered] =
                ImVec4{0.230f, 0.170f, 0.095f, 0.96f};
            c[ImGuiCol_FrameBgActive] =
                ImVec4{0.330f, 0.240f, 0.130f, 1.00f};

            // Titles: dark exposed film
            c[ImGuiCol_TitleBg] = ImVec4{0.060f, 0.045f, 0.030f, 1.00f};
            c[ImGuiCol_TitleBgActive] =
                ImVec4{0.160f, 0.110f, 0.060f, 1.00f};
            c[ImGuiCol_TitleBgCollapsed] =
                ImVec4{0.045f, 0.035f, 0.024f, 0.86f};

            c[ImGuiCol_MenuBarBg] = ImVec4{0.090f, 0.065f, 0.040f, 1.00f};

            // Scrollbar: dark bronze
            c[ImGuiCol_ScrollbarBg] = ImVec4{0.040f, 0.032f, 0.022f, 0.86f};
            c[ImGuiCol_ScrollbarGrab] =
                ImVec4{0.230f, 0.165f, 0.090f, 1.00f};
            c[ImGuiCol_ScrollbarGrabHovered] =
                ImVec4{0.340f, 0.245f, 0.130f, 1.00f};
            c[ImGuiCol_ScrollbarGrabActive] =
                ImVec4{0.470f, 0.330f, 0.170f, 1.00f};

            // Accent color: amber candlelight on old paper
            c[ImGuiCol_CheckMark] = ImVec4{0.78f, 0.58f, 0.28f, 1.00f};
            c[ImGuiCol_SliderGrab] = ImVec4{0.58f, 0.40f, 0.18f, 1.00f};
            c[ImGuiCol_SliderGrabActive] =
                ImVec4{0.86f, 0.64f, 0.30f, 1.00f};

            // Buttons: aged wood
            c[ImGuiCol_Button] = ImVec4{0.135f, 0.095f, 0.055f, 0.95f};
            c[ImGuiCol_ButtonHovered] =
                ImVec4{0.280f, 0.190f, 0.095f, 1.00f};
            c[ImGuiCol_ButtonActive] = ImVec4{0.400f, 0.270f, 0.130f, 1.00f};

            // Headers: photo album separators
            c[ImGuiCol_Header] = ImVec4{0.170f, 0.120f, 0.065f, 0.86f};
            c[ImGuiCol_HeaderHovered] =
                ImVec4{0.320f, 0.220f, 0.110f, 0.96f};
            c[ImGuiCol_HeaderActive] = ImVec4{0.430f, 0.290f, 0.140f, 1.00f};

            // Separators and grips: warm paper edge highlights
            c[ImGuiCol_Separator] = ImVec4{0.36f, 0.27f, 0.16f, 0.58f};
            c[ImGuiCol_SeparatorHovered] =
                ImVec4{0.55f, 0.40f, 0.20f, 0.85f};
            c[ImGuiCol_SeparatorActive] = ImVec4{0.78f, 0.58f, 0.28f, 1.00f};
            c[ImGuiCol_ResizeGrip] = ImVec4{0.36f, 0.27f, 0.16f, 0.25f};
            c[ImGuiCol_ResizeGripHovered] =
                ImVec4{0.55f, 0.40f, 0.20f, 0.70f};
            c[ImGuiCol_ResizeGripActive] =
                ImVec4{0.78f, 0.58f, 0.28f, 0.95f};

            // Tabs: dark sepia with amber overline
            c[ImGuiCol_Tab] = ImVec4{0.115f, 0.080f, 0.045f, 0.95f};
            c[ImGuiCol_TabHovered] = ImVec4{0.320f, 0.220f, 0.110f, 0.96f};
            c[ImGuiCol_TabSelected] = ImVec4{0.300f, 0.205f, 0.105f, 1.00f};
            c[ImGuiCol_TabSelectedOverline] =
                ImVec4{0.78f, 0.58f, 0.28f, 1.00f};
            c[ImGuiCol_TabDimmed] = ImVec4{0.075f, 0.055f, 0.032f, 0.95f};
            c[ImGuiCol_TabDimmedSelected] =
                ImVec4{0.155f, 0.105f, 0.055f, 1.00f};
            c[ImGuiCol_TabDimmedSelectedOverline] =
                ImVec4{0.45f, 0.32f, 0.16f, 1.00f};

            // Plot colors: readable, warm, restrained
            c[ImGuiCol_PlotLines] = ImVec4{0.78f, 0.58f, 0.28f, 1.00f};
            c[ImGuiCol_PlotLinesHovered] =
                ImVec4{0.95f, 0.72f, 0.36f, 1.00f};
            c[ImGuiCol_PlotHistogram] = ImVec4{0.54f, 0.34f, 0.16f, 1.00f};
            c[ImGuiCol_PlotHistogramHovered] =
                ImVec4{0.80f, 0.50f, 0.22f, 1.00f};

            // Tables / misc accents
            c[ImGuiCol_TableHeaderBg] =
                ImVec4{0.150f, 0.105f, 0.060f, 1.00f};
            c[ImGuiCol_TableBorderStrong] =
                ImVec4{0.36f, 0.27f, 0.16f, 1.00f};
            c[ImGuiCol_TableBorderLight] =
                ImVec4{0.230f, 0.165f, 0.090f, 1.00f};
            c[ImGuiCol_TableRowBg] = ImVec4{0.00f, 0.00f, 0.00f, 0.00f};
            c[ImGuiCol_TableRowBgAlt] =
                ImVec4{0.120f, 0.085f, 0.048f, 0.30f};
            c[ImGuiCol_TextLink] = ImVec4{0.82f, 0.62f, 0.32f, 1.00f};
            c[ImGuiCol_TextSelectedBg] = ImVec4{0.42f, 0.29f, 0.14f, 0.65f};
            c[ImGuiCol_DragDropTarget] = ImVec4{0.95f, 0.72f, 0.36f, 0.90f};

            // Nav / modal
            c[ImGuiCol_NavCursor] = ImVec4{0.82f, 0.62f, 0.32f, 1.00f};
            c[ImGuiCol_NavWindowingHighlight] =
                ImVec4{0.95f, 0.72f, 0.36f, 0.70f};
            c[ImGuiCol_NavWindowingDimBg] =
                ImVec4{0.035f, 0.026f, 0.018f, 0.55f};
            c[ImGuiCol_ModalWindowDimBg] =
                ImVec4{0.030f, 0.022f, 0.014f, 0.72f};
            break;
    }

    if (ui_display_scale != 1.0f)
    {
        ImGuiStyle_ScaleAllSizes(s, ui_display_scale);
    }
}

void MikuPan_UiThemeInit(SDL_Window* window)
{
    MikuPan_ConfigurationValidate();
    ui_display_scale = MikuPan_CalculateUiDisplayScale(window);
    MikuPan_LoadUiFonts();
    MikuPan_ApplyUiFontScale();
    MikuPan_ApplyFatalFrameStyle(mikupan_configuration.selected_theme);
}

float MikuPan_UiThemeGetDisplayScale(void)
{
    return ui_display_scale;
}
