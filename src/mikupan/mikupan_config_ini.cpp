#include "mikupan_config.h"

#include "mikupan_logging.h"

#include <SDL3/SDL_filesystem.h>
#ifdef __ANDROID__
#include <SDL3/SDL_system.h>
#endif

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <system_error>
#include <exception>

#include "inipp.h"

namespace
{
using Ini = inipp::Ini<char>;

bool PathExistsNoThrow(const std::filesystem::path& path, const char *context)
{
    std::error_code error;
    const bool exists = std::filesystem::exists(path, error);
    if (error)
    {
        info_log("Failed to check %s %s: %s",
                 context,
                 path.generic_string().c_str(),
                 error.message().c_str());
        return false;
    }

    return exists;
}

bool CreateDirectoriesNoThrow(const std::filesystem::path& path,
                              const char *context)
{
    std::error_code error;
    std::filesystem::create_directories(path, error);
    if (error)
    {
        info_log("Failed to create %s %s: %s",
                 context,
                 path.generic_string().c_str(),
                 error.message().c_str());
        return false;
    }

    return true;
}

std::filesystem::path GetDefaultConfigurationPath()
{
    char *base = SDL_GetPrefPath("Mikompilation", "MikuPan");
    if (base != nullptr && base[0] != '\0')
    {
        std::filesystem::path path(base);
        SDL_free(base);
        return path / "mikupan.ini";
    }

    if (base != nullptr)
    {
        SDL_free(base);
    }

    return std::filesystem::path("MikuPan") / "mikupan.ini";
}

std::filesystem::path GetDefaultConfigurationDirectory()
{
    return GetDefaultConfigurationPath().parent_path();
}

template <typename T>
std::string ConfigValueToString(T value)
{
    std::ostringstream stream;
    stream << value;
    return stream.str();
}

template <>
std::string ConfigValueToString(float value)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(3) << value;
    return stream.str();
}

template <typename T>
void SetValue(Ini& ini, const char *section, const char *key, T value)
{
    ini.sections[section][key] = ConfigValueToString(value);
}

template <typename T>
void ApplyValue(const Ini& ini, const char *section, const char *key, T& dst)
{
    const auto sec = ini.sections.find(section);
    if (sec == ini.sections.end())
    {
        return;
    }

    T value = dst;
    if (inipp::get_value(sec->second, key, value))
    {
        dst = value;
    }
    else if (sec->second.find(key) != sec->second.end())
    {
        info_log("Invalid config value ignored: [%s] %s", section, key);
    }
}

void ApplyString(const Ini& ini, const char *section, const char *key,
                 char *dst, size_t dst_size)
{
    const auto sec = ini.sections.find(section);
    if (sec == ini.sections.end())
    {
        return;
    }

    const auto entry = sec->second.find(key);
    if (entry == sec->second.end())
    {
        return;
    }

    std::strncpy(dst, entry->second.c_str(), dst_size - 1);
    dst[dst_size - 1] = '\0';
}

bool TryLoadConfigurationFile(const std::filesystem::path& path)
{
    if (!PathExistsNoThrow(path, "configuration file"))
    {
        return false;
    }

    std::ifstream stream(path);
    if (!stream.is_open())
    {
        info_log("Failed to open configuration file %s",
                 path.generic_string().c_str());
        return false;
    }

    Ini ini;
    ini.parse(stream);
    ini.strip_trailing_comments();
    ini.default_section(ini.sections["DEFAULT"]);
    ini.interpolate();

    if (!ini.errors.empty())
    {
        for (const auto& line : ini.errors)
        {
            info_log("Configuration parse error in %s: %s",
                     path.generic_string().c_str(), line.c_str());
        }
    }

    ApplyValue(ini, "renderer.window", "width", mikupan_configuration.renderer.window.width);
    ApplyValue(ini, "renderer.window", "height", mikupan_configuration.renderer.window.height);
    ApplyValue(ini, "renderer.render", "width", mikupan_configuration.renderer.render.width);
    ApplyValue(ini, "renderer.render", "height", mikupan_configuration.renderer.render.height);
    ApplyValue(ini, "renderer", "is_fullscreen", mikupan_configuration.renderer.is_fullscreen);
    ApplyValue(ini, "renderer", "window_mode", mikupan_configuration.renderer.window_mode);
    ApplyValue(ini, "renderer", "vsync", mikupan_configuration.renderer.vsync);
    ApplyValue(ini, "renderer", "lighting_mode", mikupan_configuration.renderer.lighting_mode);
    ApplyValue(ini, "renderer", "msaa_index", mikupan_configuration.renderer.msaa_index);
    ApplyValue(ini, "renderer", "shadow_resolution", mikupan_configuration.renderer.shadow_resolution);
    ApplyValue(ini, "renderer", "brightness", mikupan_configuration.renderer.brightness);
    ApplyValue(ini, "renderer", "gamma", mikupan_configuration.renderer.gamma);
    ApplyString(ini, "renderer", "gpu_driver",
                mikupan_configuration.renderer.gpu_driver,
                sizeof(mikupan_configuration.renderer.gpu_driver));
    ApplyValue(ini, "renderer", "gpu_debug", mikupan_configuration.renderer.gpu_debug);
    ApplyValue(ini, "crt", "enabled", mikupan_configuration.crt.enabled);
    ApplyValue(ini, "crt", "strength", mikupan_configuration.crt.strength);
    ApplyValue(ini, "crt", "curvature", mikupan_configuration.crt.curvature);
    ApplyValue(ini, "crt", "overscan", mikupan_configuration.crt.overscan);
    ApplyValue(ini, "crt", "scanline_strength", mikupan_configuration.crt.scanline_strength);
    ApplyValue(ini, "crt", "scanline_scale", mikupan_configuration.crt.scanline_scale);
    ApplyValue(ini, "crt", "scanline_thickness", mikupan_configuration.crt.scanline_thickness);
    ApplyValue(ini, "crt", "mask_strength", mikupan_configuration.crt.mask_strength);
    ApplyValue(ini, "crt", "mask_scale", mikupan_configuration.crt.mask_scale);
    ApplyValue(ini, "crt", "vignette_strength", mikupan_configuration.crt.vignette_strength);
    ApplyValue(ini, "crt", "vignette_size", mikupan_configuration.crt.vignette_size);
    ApplyValue(ini, "crt", "chroma_offset", mikupan_configuration.crt.chroma_offset);
    ApplyValue(ini, "crt", "blend_strength", mikupan_configuration.crt.blend_strength);
    ApplyValue(ini, "crt", "blend_radius", mikupan_configuration.crt.blend_radius);
    ApplyValue(ini, "crt", "noise_strength", mikupan_configuration.crt.noise_strength);
    ApplyValue(ini, "crt", "flicker_strength", mikupan_configuration.crt.flicker_strength);
    ApplyValue(ini, "crt", "glow_strength", mikupan_configuration.crt.glow_strength);
    ApplyValue(ini, "ui", "selected_theme", mikupan_configuration.selected_theme);
    ApplyValue(ini, "ui", "selected_font", mikupan_configuration.selected_font);
    ApplyValue(ini, "ui", "font_scale", mikupan_configuration.font_scale);
    ApplyValue(ini, "ui", "show_fps", mikupan_configuration.show_fps);
    ApplyValue(ini, "third_person_camera", "enabled",
               mikupan_configuration.third_person_camera.enabled);
    ApplyValue(ini, "third_person_camera", "distance",
               mikupan_configuration.third_person_camera.distance);
    ApplyValue(ini, "third_person_camera", "height",
               mikupan_configuration.third_person_camera.height);
    ApplyValue(ini, "third_person_camera", "side",
               mikupan_configuration.third_person_camera.side);
    ApplyValue(ini, "third_person_camera", "look_ahead",
               mikupan_configuration.third_person_camera.look_ahead);
    ApplyValue(ini, "third_person_camera", "interest_height",
               mikupan_configuration.third_person_camera.interest_height);
    ApplyValue(ini, "third_person_camera", "fov_deg",
               mikupan_configuration.third_person_camera.fov_deg);
    ApplyValue(ini, "first_person_camera", "enabled",
               mikupan_configuration.first_person_camera.enabled);
    ApplyValue(ini, "first_person_camera", "r3_toggle_enabled",
               mikupan_configuration.first_person_camera.r3_toggle_enabled);
    ApplyValue(ini, "first_person_camera", "auto_run_enabled",
               mikupan_configuration.first_person_camera.auto_run_enabled);
    ApplyValue(ini, "first_person_camera", "eye_height",
               mikupan_configuration.first_person_camera.eye_height);
    ApplyValue(ini, "first_person_camera", "eye_forward",
               mikupan_configuration.first_person_camera.eye_forward);
    ApplyValue(ini, "first_person_camera", "look_distance",
               mikupan_configuration.first_person_camera.look_distance);
    ApplyValue(ini, "first_person_camera", "fov_deg",
               mikupan_configuration.first_person_camera.fov_deg);
    ApplyValue(ini, "first_person_camera", "stick_yaw_speed_deg",
               mikupan_configuration.first_person_camera.stick_yaw_speed_deg);
    ApplyValue(ini, "first_person_camera", "stick_pitch_speed_deg",
               mikupan_configuration.first_person_camera.stick_pitch_speed_deg);
    ApplyValue(ini, "input", "selected_gamepad_index",
               mikupan_configuration.input.selected_gamepad_index);
    ApplyValue(ini, "input", "bindings_saved",
               mikupan_configuration.input.bindings_saved);
    for (int i = 0; i < 16; i++)
    {
        const std::string base = "btn" + std::to_string(i) + "_";
        ApplyValue(ini, "input", (base + "kind").c_str(),
                   mikupan_configuration.input.controller_kind[i]);
        ApplyValue(ini, "input", (base + "code").c_str(),
                   mikupan_configuration.input.controller_code[i]);
        ApplyValue(ini, "input", (base + "kb").c_str(),
                   mikupan_configuration.input.keyboard_scancode[i]);
    }
    for (int i = 0; i < 4; i++)
    {
        const std::string base = "stick" + std::to_string(i) + "_";
        ApplyValue(ini, "input", (base + "axis").c_str(),
                   mikupan_configuration.input.stick_axis[i]);
        ApplyValue(ini, "input", (base + "invert").c_str(),
                   mikupan_configuration.input.stick_invert[i]);
        ApplyValue(ini, "input", (base + "kbneg").c_str(),
                   mikupan_configuration.input.stick_kb_neg[i]);
        ApplyValue(ini, "input", (base + "kbpos").c_str(),
                   mikupan_configuration.input.stick_kb_pos[i]);
    }
    ApplyValue(ini, "input.action_profile", "saved",
               mikupan_configuration.input.action_profile_saved);
    ApplyValue(ini, "input.action_profile", "layout",
               mikupan_configuration.input.action_profile_layout);
    ApplyValue(ini, "input.action_profile", "enabled",
               mikupan_configuration.input.action_profile_enabled);
    ApplyValue(ini, "input.action_profile", "subjective_move",
               mikupan_configuration.input.action_profile_subjective_move);
    ApplyValue(ini, "input.action_profile", "dpad_subjective_move",
               mikupan_configuration.input.action_profile_dpad_subjective_move);
    ApplyValue(ini, "input.action_profile", "stick_subjective_move",
               mikupan_configuration.input.action_profile_stick_subjective_move);
    ApplyValue(ini, "input.action_profile", "finder_reverse_y",
               mikupan_configuration.input.action_profile_finder_reverse_y);
    ApplyValue(ini, "input.action_profile", "finder_swap_sticks",
               mikupan_configuration.input.action_profile_finder_swap_sticks);
    ApplyValue(ini, "input", "finder_mouse_enabled",
               mikupan_configuration.input.finder_mouse_enabled);
    ApplyValue(ini, "input", "finder_mouse_sensitivity",
               mikupan_configuration.input.finder_mouse_sensitivity);
    for (int i = 0; i < 16; i++)
    {
        ApplyValue(ini, "input.action_profile",
                   ("normal_action" + std::to_string(i)).c_str(),
                   mikupan_configuration.input.action_profile_normal[i]);
        ApplyValue(ini, "input.action_profile",
                   ("finder_action" + std::to_string(i)).c_str(),
                   mikupan_configuration.input.action_profile_finder[i]);
    }

    ApplyString(ini, "paths", "data_folder", mikupan_configuration.data_folder,
                sizeof(mikupan_configuration.data_folder));

    info_log("Loaded configuration from %s", path.generic_string().c_str());
    return true;
}

bool TrySaveConfigurationFile(const std::filesystem::path& path)
{
    const auto parent = path.parent_path();
    if (!parent.empty() && !PathExistsNoThrow(parent, "configuration directory"))
    {
        if (!CreateDirectoriesNoThrow(parent, "configuration directory"))
        {
            return false;
        }
    }

    try
    {
    std::ofstream stream(path, std::ios::trunc);
    if (!stream.is_open())
    {
        info_log("Failed to open configuration file for writing %s",
                 path.generic_string().c_str());
        return false;
    }

    Ini ini;
    SetValue(ini, "renderer.window", "width", mikupan_configuration.renderer.window.width);
    SetValue(ini, "renderer.window", "height", mikupan_configuration.renderer.window.height);
    SetValue(ini, "renderer.render", "width", mikupan_configuration.renderer.render.width);
    SetValue(ini, "renderer.render", "height", mikupan_configuration.renderer.render.height);
    SetValue(ini, "renderer", "is_fullscreen", mikupan_configuration.renderer.is_fullscreen);
    SetValue(ini, "renderer", "window_mode", mikupan_configuration.renderer.window_mode);
    SetValue(ini, "renderer", "vsync", mikupan_configuration.renderer.vsync);
    SetValue(ini, "renderer", "lighting_mode", mikupan_configuration.renderer.lighting_mode);
    SetValue(ini, "renderer", "msaa_index", mikupan_configuration.renderer.msaa_index);
    SetValue(ini, "renderer", "shadow_resolution", mikupan_configuration.renderer.shadow_resolution);
    SetValue(ini, "renderer", "brightness", mikupan_configuration.renderer.brightness);
    SetValue(ini, "renderer", "gamma", mikupan_configuration.renderer.gamma);
    ini.sections["renderer"]["gpu_driver"] =
        mikupan_configuration.renderer.gpu_driver;
    SetValue(ini, "renderer", "gpu_debug", mikupan_configuration.renderer.gpu_debug);
    SetValue(ini, "crt", "enabled", mikupan_configuration.crt.enabled);
    SetValue(ini, "crt", "strength", mikupan_configuration.crt.strength);
    SetValue(ini, "crt", "curvature", mikupan_configuration.crt.curvature);
    SetValue(ini, "crt", "overscan", mikupan_configuration.crt.overscan);
    SetValue(ini, "crt", "scanline_strength", mikupan_configuration.crt.scanline_strength);
    SetValue(ini, "crt", "scanline_scale", mikupan_configuration.crt.scanline_scale);
    SetValue(ini, "crt", "scanline_thickness", mikupan_configuration.crt.scanline_thickness);
    SetValue(ini, "crt", "mask_strength", mikupan_configuration.crt.mask_strength);
    SetValue(ini, "crt", "mask_scale", mikupan_configuration.crt.mask_scale);
    SetValue(ini, "crt", "vignette_strength", mikupan_configuration.crt.vignette_strength);
    SetValue(ini, "crt", "vignette_size", mikupan_configuration.crt.vignette_size);
    SetValue(ini, "crt", "chroma_offset", mikupan_configuration.crt.chroma_offset);
    SetValue(ini, "crt", "blend_strength", mikupan_configuration.crt.blend_strength);
    SetValue(ini, "crt", "blend_radius", mikupan_configuration.crt.blend_radius);
    SetValue(ini, "crt", "noise_strength", mikupan_configuration.crt.noise_strength);
    SetValue(ini, "crt", "flicker_strength", mikupan_configuration.crt.flicker_strength);
    SetValue(ini, "crt", "glow_strength", mikupan_configuration.crt.glow_strength);
    SetValue(ini, "ui", "selected_theme", mikupan_configuration.selected_theme);
    SetValue(ini, "ui", "selected_font", mikupan_configuration.selected_font);
    SetValue(ini, "ui", "font_scale", mikupan_configuration.font_scale);
    SetValue(ini, "ui", "show_fps", mikupan_configuration.show_fps);
    SetValue(ini, "third_person_camera", "enabled",
             mikupan_configuration.third_person_camera.enabled);
    SetValue(ini, "third_person_camera", "distance",
             mikupan_configuration.third_person_camera.distance);
    SetValue(ini, "third_person_camera", "height",
             mikupan_configuration.third_person_camera.height);
    SetValue(ini, "third_person_camera", "side",
             mikupan_configuration.third_person_camera.side);
    SetValue(ini, "third_person_camera", "look_ahead",
             mikupan_configuration.third_person_camera.look_ahead);
    SetValue(ini, "third_person_camera", "interest_height",
             mikupan_configuration.third_person_camera.interest_height);
    SetValue(ini, "third_person_camera", "fov_deg",
             mikupan_configuration.third_person_camera.fov_deg);
    SetValue(ini, "first_person_camera", "enabled",
             mikupan_configuration.first_person_camera.enabled);
    SetValue(ini, "first_person_camera", "r3_toggle_enabled",
             mikupan_configuration.first_person_camera.r3_toggle_enabled);
    SetValue(ini, "first_person_camera", "auto_run_enabled",
             mikupan_configuration.first_person_camera.auto_run_enabled);
    SetValue(ini, "first_person_camera", "eye_height",
             mikupan_configuration.first_person_camera.eye_height);
    SetValue(ini, "first_person_camera", "eye_forward",
             mikupan_configuration.first_person_camera.eye_forward);
    SetValue(ini, "first_person_camera", "look_distance",
             mikupan_configuration.first_person_camera.look_distance);
    SetValue(ini, "first_person_camera", "fov_deg",
             mikupan_configuration.first_person_camera.fov_deg);
    SetValue(ini, "first_person_camera", "stick_yaw_speed_deg",
             mikupan_configuration.first_person_camera.stick_yaw_speed_deg);
    SetValue(ini, "first_person_camera", "stick_pitch_speed_deg",
             mikupan_configuration.first_person_camera.stick_pitch_speed_deg);
    SetValue(ini, "input", "selected_gamepad_index",
             mikupan_configuration.input.selected_gamepad_index);
    SetValue(ini, "input", "bindings_saved",
             mikupan_configuration.input.bindings_saved);
    if (mikupan_configuration.input.bindings_saved)
    {
        for (int i = 0; i < 16; i++)
        {
            const std::string base = "btn" + std::to_string(i) + "_";
            SetValue(ini, "input", (base + "kind").c_str(),
                     mikupan_configuration.input.controller_kind[i]);
            SetValue(ini, "input", (base + "code").c_str(),
                     mikupan_configuration.input.controller_code[i]);
            SetValue(ini, "input", (base + "kb").c_str(),
                     mikupan_configuration.input.keyboard_scancode[i]);
        }
        for (int i = 0; i < 4; i++)
        {
            const std::string base = "stick" + std::to_string(i) + "_";
            SetValue(ini, "input", (base + "axis").c_str(),
                     mikupan_configuration.input.stick_axis[i]);
            SetValue(ini, "input", (base + "invert").c_str(),
                     mikupan_configuration.input.stick_invert[i]);
            SetValue(ini, "input", (base + "kbneg").c_str(),
                     mikupan_configuration.input.stick_kb_neg[i]);
            SetValue(ini, "input", (base + "kbpos").c_str(),
                     mikupan_configuration.input.stick_kb_pos[i]);
        }
        SetValue(ini, "input.action_profile", "saved",
                 mikupan_configuration.input.action_profile_saved);
        SetValue(ini, "input.action_profile", "layout",
                 mikupan_configuration.input.action_profile_layout);
        SetValue(ini, "input.action_profile", "enabled",
                 mikupan_configuration.input.action_profile_enabled);
        SetValue(ini, "input.action_profile", "subjective_move",
                 mikupan_configuration.input.action_profile_subjective_move);
        SetValue(ini, "input.action_profile", "dpad_subjective_move",
                 mikupan_configuration.input.action_profile_dpad_subjective_move);
        SetValue(ini, "input.action_profile", "stick_subjective_move",
                 mikupan_configuration.input.action_profile_stick_subjective_move);
        SetValue(ini, "input.action_profile", "finder_reverse_y",
                 mikupan_configuration.input.action_profile_finder_reverse_y);
        SetValue(ini, "input.action_profile", "finder_swap_sticks",
                 mikupan_configuration.input.action_profile_finder_swap_sticks);
        SetValue(ini, "input", "finder_mouse_enabled",
                 mikupan_configuration.input.finder_mouse_enabled);
        SetValue(ini, "input", "finder_mouse_sensitivity",
                 mikupan_configuration.input.finder_mouse_sensitivity);
        for (int i = 0; i < 16; i++)
        {
            SetValue(ini, "input.action_profile",
                     ("normal_action" + std::to_string(i)).c_str(),
                     mikupan_configuration.input.action_profile_normal[i]);
            SetValue(ini, "input.action_profile",
                     ("finder_action" + std::to_string(i)).c_str(),
                     mikupan_configuration.input.action_profile_finder[i]);
        }
    }

    ini.sections["paths"]["data_folder"] = mikupan_configuration.data_folder;

    ini.generate(stream);
    if (!stream.good())
    {
        info_log("Failed to write configuration file %s", path.generic_string().c_str());
        return false;
    }

    stream.close();

    if (!stream.good())
    {
        info_log("Failed to write configuration file %s", path.generic_string().c_str());
        return false;
    }

    info_log("Saved configuration to %s", path.generic_string().c_str());
    return true;
    }
    catch (const std::exception& error)
    {
        info_log("Failed to save configuration file %s: %s",
                 path.generic_string().c_str(), error.what());
        return false;
    }
    catch (...)
    {
        info_log("Failed to save configuration file %s",
                 path.generic_string().c_str());
        return false;
    }
}
}

extern "C" void MikuPan_LoadConfiguration(const char *filename)
{
    if (filename != nullptr && filename[0] != '\0')
    {
        TryLoadConfigurationFile(filename);
    }
    else
    {
        const std::filesystem::path default_path = GetDefaultConfigurationPath();
        info_log("Default configuration path: %s",
                 default_path.generic_string().c_str());
        if (!TryLoadConfigurationFile(default_path))
        {
            TryLoadConfigurationFile("mikupan.ini");
        }
    }

    if (mikupan_configuration.data_folder[0] == '\0')
    {
#ifdef __ANDROID__
        const char *base = SDL_GetAndroidExternalStoragePath();
        if (base == nullptr || base[0] == '\0')
        {
            base = SDL_GetAndroidInternalStoragePath();
        }
#else
        const char *base = SDL_GetBasePath();
#endif
        if (base != nullptr)
        {
            std::strncpy(mikupan_configuration.data_folder, base,
                         sizeof(mikupan_configuration.data_folder) - 1);
            mikupan_configuration.data_folder
                [sizeof(mikupan_configuration.data_folder) - 1] = '\0';
        }
    }

    MikuPan_ConfigurationValidate();
}

extern "C" int MikuPan_SaveConfiguration(const char *filename)
{
    MikuPan_ConfigurationValidate();

    if (filename != nullptr && filename[0] != '\0')
    {
        return TrySaveConfigurationFile(filename) ? 1 : 0;
    }

    const std::filesystem::path default_path = GetDefaultConfigurationPath();
    info_log("Default configuration path: %s",
             default_path.generic_string().c_str());
    return TrySaveConfigurationFile(default_path) ? 1 : 0;
}

extern "C" int MikuPan_ResolveUserPath(const char *path, char *buffer,
                                        size_t buffer_size)
{
    if (path == nullptr || buffer == nullptr || buffer_size == 0)
    {
        return 0;
    }

    std::filesystem::path relative_path;
    for (const auto& part : std::filesystem::path(path).relative_path())
    {
        if (part.empty() || part == "." || part == "..")
        {
            continue;
        }

        relative_path /= part;
    }

    const std::filesystem::path resolved_path =
        (GetDefaultConfigurationDirectory() / relative_path).lexically_normal();
    const std::filesystem::path parent = resolved_path.parent_path();
    if (!parent.empty() && !PathExistsNoThrow(parent, "user data directory"))
    {
        if (!CreateDirectoriesNoThrow(parent, "user data directory"))
        {
            return 0;
        }
    }

    const std::string resolved = resolved_path.generic_string();

    if (resolved.size() + 1 > buffer_size)
    {
        info_log("User path too long: %s", resolved.c_str());
        return 0;
    }

    std::strcpy(buffer, resolved.c_str());
    return 1;
}
