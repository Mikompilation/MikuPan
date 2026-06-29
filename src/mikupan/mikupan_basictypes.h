#ifndef MIKUPAN_MIKUPAN_BASICTYPES_H
#define MIKUPAN_MIKUPAN_BASICTYPES_H
#include "typedefs.h"

typedef struct
{
    u_int id;
} MikuPan_RenderBufferObject;

typedef struct
{
    u_int id;
} MikuPan_FrameBufferObject;

typedef struct
{
    int width;
    int height;
    u_int id;
    uint64_t tex0;
    uint64_t hash;
} MikuPan_TextureInfo;

typedef struct
{
    int msaa;
    MikuPan_FrameBufferObject framebuffer;
    MikuPan_RenderBufferObject depth;
    MikuPan_RenderBufferObject colour;
    MikuPan_TextureInfo texture;
    MikuPan_RenderBufferObject framebuffer_readback;
} MikuPan_MsaaBufferObject;

typedef struct
{
    float x;
    float y;
    float w;
    float h;
} MikuPan_Rect;

typedef struct
{
    /// Number of components in the vertex attribute.
    int size;
    u_int index;
    int stride;
    u_int offset;
} MikuPan_AttributeInfo;

typedef struct
{
    u_int id;
    /// Size of the buffer allocation in bytes.
    int buffer_length;
    u_int num_attributes;
    MikuPan_AttributeInfo* attributes;
} MikuPan_BufferObjectInfo;

typedef struct
{
    u_int vao;
    u_int ibo;
    u_int num_buffers;
    MikuPan_BufferObjectInfo* buffers;
} MikuPan_PipelineInfo;

typedef struct
{
    int width;
    int height;
} MikuPan_Resolution;

#define MIKUPAN_RENDER_RESOLUTION_FIT_WINDOW (-1)

typedef struct
{
    MikuPan_Resolution window;
    /* Use MIKUPAN_RENDER_RESOLUTION_FIT_WINDOW for both render dimensions to
     * follow the current window size. */
    MikuPan_Resolution render;
    int is_fullscreen;   /* legacy; kept in sync with window_mode != windowed */
    int window_mode;     /* 0 = windowed, 1 = fullscreen, 2 = borderless */
    int vsync;
    int lighting_mode;
    int msaa_index;
    int shadow_resolution;
    float brightness;
    float gamma;
    /* SDL_GPU driver to request at startup ("vulkan", "direct3d12", ...).
     * Empty = let SDL pick. The device is created once, so a change only
     * applies on the next launch. */
    char gpu_driver[32];
    int gpu_debug;
} MikuPan_ConfigRenderer;

/* Apply a window display mode to the SDL window.
 *   Windowed   - leave fullscreen, restore the window border.
 *   Fullscreen - exclusive fullscreen using the display's desktop mode.
 *   Borderless - a normal borderless WINDOW sized to fill the display (NOT SDL
 *                fullscreen), so alt-tab stays instant and other windows can
 *                overlap. */
enum MikuPan_WindowMode
{
    MIKUPAN_WINDOW_WINDOWED = 0,
    MIKUPAN_WINDOW_FULLSCREEN = 1,
    MIKUPAN_WINDOW_BORDERLESS = 2,
};

typedef struct
{
    int enabled;
    float strength;
    float curvature;
    float overscan;
    float scanline_strength;
    float scanline_scale;
    float scanline_thickness;
    float mask_strength;
    float mask_scale;
    float vignette_strength;
    float vignette_size;
    float chroma_offset;
    float blend_strength;
    float blend_radius;
    float noise_strength;
    float flicker_strength;
    float glow_strength;
} MikuPan_ConfigCrt;

typedef struct
{
    int selected_gamepad_index;

    /* Saved controller / keyboard bindings. Sizes mirror
     * MIKUPAN_CONTROLLER_LOGICAL_COUNT (16) and MIKUPAN_STICK_COUNT (4) from
     * mikupan_controller.h. bindings_saved is 0 until the user saves, so a
     * fresh config keeps the runtime defaults instead of these zeroed arrays. */
    int bindings_saved;
    int controller_kind[16];
    int controller_code[16];
    int keyboard_scancode[16];
    int stick_axis[4];
    int stick_invert[4];
    int stick_kb_neg[4];
    int stick_kb_pos[4];

    /* Optional gameplay action profile. These maps are action slot -> game
     * button slot, using the same 16-slot sizes as key_cnf.c. */
    int action_profile_saved;
    int action_profile_layout;
    int action_profile_enabled;
    int action_profile_subjective_move;
    int action_profile_dpad_subjective_move;
    int action_profile_stick_subjective_move;
    int action_profile_finder_reverse_y;
    int action_profile_finder_swap_sticks;
    int action_profile_normal[16];
    int action_profile_finder[16];

    /* Finder (first-person) mouse-look. enabled defaults to on and sensitivity
     * to 1.0 at runtime; these are only read back when bindings_saved is set. */
    int finder_mouse_enabled;
    float finder_mouse_sensitivity;
} MikuPan_ConfigInput;

typedef struct
{
    int enabled;
    float distance;
    float height;
    float side;
    float look_ahead;
    float interest_height;
    float fov_deg;
} MikuPan_ConfigThirdPersonCamera;

typedef struct
{
    int enabled;
    int r3_toggle_enabled;
    int auto_run_enabled;
    float eye_height;
    float eye_forward;
    float look_distance;
    float fov_deg;
    float stick_yaw_speed_deg;
    float stick_pitch_speed_deg;
} MikuPan_ConfigFirstPersonCamera;

typedef struct
{
    MikuPan_ConfigRenderer renderer;
    MikuPan_ConfigCrt crt;
    int selected_theme;
    int selected_font;
    float font_scale;
    int show_fps;
    int title_room_background;
    MikuPan_ConfigThirdPersonCamera third_person_camera;
    MikuPan_ConfigFirstPersonCamera first_person_camera;
    MikuPan_ConfigInput input;
    char data_folder[512];
} MikuPan_Config;

enum MikuPan_PipelineType
{
    /// BOUNDING BOX SHADER
    POSITION4,

    /// Shadow silhouette position-only stream
    SHADOW_POSITION4,

    /// MESH 0x12 SHADER
    POSITION3_NORMAL3_UV2,

    /// MESH 0x12 SHADER (SoA layout — used by mesh_type 0x32 only).
    /// Same shader & attribute locations as POSITION3_NORMAL3_UV2 but
    /// positions / normals / UVs / colors live in 4 separate VBOs so that
    /// 0x32 uploads can avoid the per-vertex CPU interleaving step.
    POSITION3_NORMAL3_UV2_SOA,

    /// MESH 0x2 SHADER
    POSITION4_NORMAL4_UV2,

    /// GPU-SKINNED MESH 0x2 / 0xA. Static bind-pose data uploaded once:
    /// buffer0 holds four float4 per vertex (bone-0 pos+weight, bone-1
    /// pos+packed bone indices, bone-0 normal+normal-weight, bone-1 normal);
    /// buffer1 holds the float2 UV. The bone-matrix palette arrives separately
    /// in a vertex storage buffer.
    SKIN_POSITION4X2_NORMAL4X2_UV2,

    /// SPRITE
    UV4_COLOUR4_POSITION4,

    /// UNTEXTURED COLOURED SPRITE
    COLOUR4_POSITION4,

    /// Lighting Data
    LIGHTING_DATA,

    /// Per-material colour block (Ambient/Diffuse/Specular/Emission). Pushed
    /// by MikuPan_SetMaterial when SetMaterialData fires for a new material.
    MATERIAL_DATA,

    /// THE MAXIMUM NUM OF PIPELINES, USEFUL FOR LOOPS
    MAX_NUMBER_OF_PIPELINES
};

#endif//MIKUPAN_MIKUPAN_BASICTYPES_H
