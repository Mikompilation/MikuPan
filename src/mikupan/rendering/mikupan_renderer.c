#include "mikupan_renderer.h"
#include "../../../cmake-build-release-mingw/_deps/sdl3-src/include/SDL3/SDL_hints.h"
#include "../mikupan_types.h"
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_log.h"
#include "common/utility.h"
#include "graphics/graph2d/message.h"
#include "graphics/ui/imgui_window_c.h"
#include "gs/gs_server_c.h"
#include "gs/texture_manager_c.h"
#include "mikupan/logging_c.h"

#include <SDL3/SDL_gpu.h>
#include <SDL3_shadercross/SDL_shadercross.h>
#include <mikupan/mikupan_memory.h>
#include <stdlib.h>

#define PS2_RESOLUTION_X_FLOAT 640.0f
#define PS2_RESOLUTION_X_INT 640
#define PS2_RESOLUTION_Y_FLOAT 448.0f
#define PS2_RESOLUTION_Y_INT 448

float game_aspect_ratio = 4.0f/3.0f;
int window_width = 640;
int window_height = 448;
SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;
SDL_GPUDevice* device = NULL;
SDL_GPURenderPass* render_pass = NULL;
SDL_GPUCommandBuffer* command_buffer = NULL;
SDL_GPUTexture* swapchain_texture = NULL;
SDL_GPUTransferBuffer* transfer_buffer = NULL;
SDL_GPUGraphicsPipeline* graphicsPipeline = NULL;
SDL_GPUBuffer* vertexBuffer = NULL;

SDL_Texture* fnt_texture[6] = {0};
SDL_Texture *curr_fnt_texture = NULL;

// the vertex input layout
typedef struct
{
    float x, y, z;      //vec3 position
    float r, g, b, a;   //vec4 color
} Vertex;

// a list of vertices
Vertex vertices[3] =
{
    {0.0f, 0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f},     // top vertex
    {-0.5f, -0.5f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f},   // bottom left vertex
    {0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f}     // bottom right vertex
};

SDL_AppResult MikuPan_Init()
{
    SDL_SetAppMetadata("MikuPan", "1.0", "mikupan");
    SDL_SetHint(SDL_HINT_MAIN_CALLBACK_RATE, "60");

    if (!SDL_Init(
        SDL_INIT_VIDEO |
        SDL_INIT_GAMEPAD |
        SDL_INIT_JOYSTICK |
        SDL_INIT_HAPTIC
        ))
    {
        info_log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    window = SDL_CreateWindow("MikuPan", window_width, window_height, SDL_WINDOW_RESIZABLE);

    if (window == NULL)
    {
        info_log(SDL_GetError());
        return SDL_APP_FAILURE;
    }

    device = SDL_CreateGPUDevice(
        SDL_ShaderCross_GetSPIRVShaderFormats(),
        true,
        NULL
        );

    if (device == NULL)
    {
        info_log(SDL_GetError());
        return SDL_APP_FAILURE;
    }

    renderer = SDL_CreateGPURenderer(device, window);

    if (renderer == NULL)
    {
        info_log(SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_SetRenderLogicalPresentation(
        renderer,
        window_width,
        window_height,
        SDL_LOGICAL_PRESENTATION_DISABLED);

    InitImGuiWindow(window, renderer);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    if (!SDL_ShaderCross_Init())
    {
        info_log(SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // load the vertex & frag shader code
    size_t vertexCodeSize;
    size_t fragmentCodeSize;
    void* vertexCode = SDL_LoadFile("./shaders/vertex.vert.hlsl", &vertexCodeSize);
    void* fragmentCode = SDL_LoadFile("./shaders/vertex.frag.hlsl", &fragmentCodeSize);

    // Vertex shader code
    SDL_ShaderCross_HLSL_Info vertexInfo = {0};
    vertexInfo.entrypoint = "main";
    vertexInfo.shader_stage = SDL_SHADERCROSS_SHADERSTAGE_VERTEX;
    vertexInfo.source = vertexCode;

    void* spirv_bytecode = SDL_ShaderCross_CompileSPIRVFromHLSL(&vertexInfo, &vertexCodeSize);

    SDL_ShaderCross_SPIRV_Info vertexInfoSpirv = {0};
    vertexInfoSpirv.bytecode = (Uint8*)spirv_bytecode;
    vertexInfoSpirv.bytecode_size = vertexCodeSize;
    vertexInfoSpirv.entrypoint = "main";
    vertexInfoSpirv.shader_stage = SDL_SHADERCROSS_SHADERSTAGE_VERTEX;

    SDL_ShaderCross_GraphicsShaderMetadata* vertexMetadata = SDL_ShaderCross_ReflectGraphicsSPIRV((Uint8*)spirv_bytecode, vertexCodeSize, 0);

    SDL_GPUShader* vertexShader = SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(device, &vertexInfoSpirv, vertexMetadata, 0);

    SDL_free(vertexCode);
    SDL_free(spirv_bytecode);

    // Fragment shader code
    SDL_ShaderCross_HLSL_Info fragmentInfo = {0};
    fragmentInfo.entrypoint = "main";
    fragmentInfo.shader_stage = SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT;
    fragmentInfo.source = fragmentCode;

    SDL_GPUShader* spirv_frag_bytecode = SDL_ShaderCross_CompileSPIRVFromHLSL(&fragmentInfo, &fragmentCodeSize);

    SDL_ShaderCross_SPIRV_Info fragmentInfoSpirv = {0};
    fragmentInfoSpirv.bytecode = (Uint8*)spirv_frag_bytecode;
    fragmentInfoSpirv.bytecode_size = fragmentCodeSize;
    fragmentInfoSpirv.entrypoint = "main";
    fragmentInfoSpirv.shader_stage = SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT;

    SDL_ShaderCross_GraphicsShaderMetadata* fragmentMetadata = SDL_ShaderCross_ReflectGraphicsSPIRV((Uint8*)spirv_frag_bytecode, fragmentCodeSize, 0);
    SDL_GPUShader* fragmentShader = SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(device, &fragmentInfoSpirv, fragmentMetadata, 0);

    SDL_free(fragmentCode);
    SDL_free(spirv_frag_bytecode);

    // create the graphics pipeline
    SDL_GPUGraphicsPipelineCreateInfo pipelineInfo = {0};
    pipelineInfo.vertex_shader = vertexShader;
    pipelineInfo.fragment_shader = fragmentShader;
    pipelineInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

    // describe the vertex buffers
    SDL_GPUVertexBufferDescription vertexBufferDesctiptions[1];
    vertexBufferDesctiptions[0].slot = 0;
    vertexBufferDesctiptions[0].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    vertexBufferDesctiptions[0].instance_step_rate = 0;
    vertexBufferDesctiptions[0].pitch = sizeof(Vertex);

    pipelineInfo.vertex_input_state.num_vertex_buffers = 1;
    pipelineInfo.vertex_input_state.vertex_buffer_descriptions = vertexBufferDesctiptions;

    // describe the vertex attribute
    SDL_GPUVertexAttribute vertexAttributes[2];

    // a_position
    vertexAttributes[0].buffer_slot = 0;
    vertexAttributes[0].location = 0;
    vertexAttributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    vertexAttributes[0].offset = 0;

    // a_color
    vertexAttributes[1].buffer_slot = 0;
    vertexAttributes[1].location = 1;
    vertexAttributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
    vertexAttributes[1].offset = sizeof(float) * 3;

    pipelineInfo.vertex_input_state.num_vertex_attributes = 2;
    pipelineInfo.vertex_input_state.vertex_attributes = vertexAttributes;

    // describe the color target
    SDL_GPUColorTargetDescription colorTargetDescriptions[1] = {0};
    colorTargetDescriptions[0] = (SDL_GPUColorTargetDescription){0};
    colorTargetDescriptions[0].format = SDL_GetGPUSwapchainTextureFormat(device, window);

    pipelineInfo.target_info.num_color_targets = 1;
    pipelineInfo.target_info.color_target_descriptions = colorTargetDescriptions;

    // create the pipeline
    graphicsPipeline = SDL_CreateGPUGraphicsPipeline(device, &pipelineInfo);

    // we don't need to store the shaders after creating the pipeline
    SDL_ReleaseGPUShader(device, vertexShader);
    SDL_ReleaseGPUShader(device, fragmentShader);

    // create the vertex buffer
    SDL_GPUBufferCreateInfo buffer_info = {0};
    buffer_info.size = sizeof(vertices);
    buffer_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    vertexBuffer = SDL_CreateGPUBuffer(device, &buffer_info);

    // create a transfer buffer to upload to the vertex buffer
    SDL_GPUTransferBufferCreateInfo transfer_info = {0};
    transfer_info.size = sizeof(vertices);
    transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transfer_buffer = SDL_CreateGPUTransferBuffer(device, &transfer_info);

    // map the transfer buffer to a pointer
    Vertex* data = (Vertex*)SDL_MapGPUTransferBuffer(device, transfer_buffer, false);

    // or you can copy them all in one operation
    SDL_memcpy(data, vertices, sizeof(vertices));

    // unmap the pointer when you are done updating the transfer buffer
    SDL_UnmapGPUTransferBuffer(device, transfer_buffer);

    // start a copy pass
    SDL_GPUCommandBuffer* commandBuffer = SDL_AcquireGPUCommandBuffer(device);
    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(commandBuffer);

    // where is the data
    SDL_GPUTransferBufferLocation location = {0};
    location.transfer_buffer = transfer_buffer;
    location.offset = 0; // start from the beginning

    // where to upload the data
    SDL_GPUBufferRegion region = {0};
    region.buffer = vertexBuffer;
    region.size = sizeof(vertices); // size of the data in bytes
    region.offset = 0; // begin writing from the first vertex

    // upload the data
    SDL_UploadToGPUBuffer(copyPass, &location, &region, true);

    // end the copy pass
    SDL_EndGPUCopyPass(copyPass);
    SDL_SubmitGPUCommandBuffer(commandBuffer);

    return SDL_APP_CONTINUE;
}

void MikuPan_Clear()
{
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);
    command_buffer = SDL_AcquireGPUCommandBuffer(device);
}

void MikuPan_UpdateWindowSize(int width, int height)
{
    window_width = width;
    window_height = height;
}

void MikuPan_Render2DTexture(DISP_SPRT* sprite)
{
    if (!IsFirstUploadDone())
    {
        return;
    }

    sceGsTex0* tex0 = (sceGsTex0*)&sprite->tex0;

    SDL_Texture* texture = (SDL_Texture*)GetSDLTexture(tex0);

    if (texture == NULL)
    {
        texture = MikuPan_CreateTexture(tex0);
    }

    SDL_FRect dst_rect;
    SDL_FRect src_rect;

    src_rect.x = sprite->u;
    src_rect.y = sprite->v;

    src_rect.w = sprite->w;
    src_rect.h = sprite->h;

    dst_rect.x = (float)window_width * (sprite->x / PS2_RESOLUTION_X_FLOAT);
    dst_rect.y = (float)window_height * (sprite->y / PS2_RESOLUTION_Y_FLOAT);

    dst_rect.w = (float)window_width * (sprite->w / PS2_RESOLUTION_X_FLOAT);
    dst_rect.h = (float)window_height * (sprite->h / PS2_RESOLUTION_Y_FLOAT);

    SDL_SetTextureAlphaMod(texture, AdjustAlpha(sprite->alpha));
    SDL_SetTextureColorMod(texture, AdjustAlpha(sprite->r), AdjustAlpha(sprite->g), AdjustAlpha(sprite->b));

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    //SDL_RenderTextureRotated(renderer, texture, &src_rect, &dst_rect, sprite->rot, NULL, false);
    SDL_RenderTexture(renderer, texture, &src_rect, &dst_rect);
}

int GetTextureIndex(int fnt)
{
    if (fnt == 1)
    {
        return 0;
    }

    if (fnt == 0)
    {
        return 1;
    }

    return fnt;
}

void MikuPan_Render2DMessage(DISP_SPRT *sprite)
{
    SDL_FRect dst_rect;
    SDL_FRect src_rect;

    src_rect.x = sprite->u;
    src_rect.y = sprite->v;

    src_rect.w = sprite->w;
    src_rect.h = sprite->h;

    dst_rect.x = (float)window_width * (sprite->x / PS2_RESOLUTION_X_FLOAT);
    dst_rect.y = (float)window_height * (sprite->y / PS2_RESOLUTION_Y_FLOAT);

    dst_rect.w = (float)window_width * (sprite->w / PS2_RESOLUTION_X_FLOAT);
    dst_rect.h = (float)window_height * (sprite->h / PS2_RESOLUTION_Y_FLOAT);

    SDL_Texture* texture = curr_fnt_texture;

    SDL_SetTextureAlphaMod(texture, AdjustAlpha(sprite->alpha));
    SDL_SetTextureColorMod(texture, AdjustAlpha(sprite->r), AdjustAlpha(sprite->g), AdjustAlpha(sprite->b));
    SDL_RenderTexture(renderer, texture, &src_rect, &dst_rect);
}

void MikuPan_RenderSquare(float x1, float y1, float x2, float y2, float x3,
                          float y3, float x4, float y4, u_char r, u_char g, u_char b, u_char a)
{
    SDL_SetRenderDrawColor(renderer, AdjustAlpha(r), AdjustAlpha(g), AdjustAlpha(b), AdjustAlpha(a));

    SDL_FRect rect;

    rect.x = (float)window_width * (x1 / PS2_RESOLUTION_X_FLOAT);
    rect.y = (float)window_height * (y1 / PS2_RESOLUTION_Y_FLOAT);

    rect.w = (float)window_width * ((x4 - x1) / PS2_RESOLUTION_X_FLOAT);
    rect.h = (float)window_height * ((y4 - y1) / PS2_RESOLUTION_Y_FLOAT);
    SDL_RenderFillRect(renderer, &rect);
}

void MikuPan_RenderLine(float x1, float y1, float x2, float y2, u_char r, u_char g, u_char b, u_char a)
{
    SDL_SetRenderDrawColor(renderer, r, g, b, AdjustAlpha(a));

    float dst_x1 = (float)window_width *  (300.0f+x1) / PS2_RESOLUTION_X_FLOAT;
    float dst_y1 = (float)window_height * (200.0f+y1) / PS2_RESOLUTION_Y_FLOAT;
    float dst_x2 = (float)window_width *  (300.0f+x2) / PS2_RESOLUTION_X_FLOAT;
    float dst_y2 = (float)window_height * (200.0f+y2) / PS2_RESOLUTION_Y_FLOAT;

    SDL_RenderLine(renderer, dst_x1, dst_y1, dst_x2, dst_y2);
}

void MikuPan_SetupFntTexture()
{
    for (int i = 0; i < 6; i++)
    {
        if (fnt_texture[i] == NULL)
        {
            fnt_texture[i] = MikuPan_CreateTexture((sceGsTex0*)&fntdat[i].tex0);
        }
    }
}

SDL_Texture *MikuPan_CreateTexture(sceGsTex0 *tex0)
{
    int texture_width = 1 << tex0->TW;
    int texture_height = 1 << tex0->TH;

    unsigned char *image = DownloadGsTexture(tex0);

    SDL_Surface *surface =
        SDL_CreateSurfaceFrom(texture_width, texture_height,
                              SDL_PIXELFORMAT_RGBA32, image, texture_width * 4);

    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);

    AddSDLTexture(tex0, texture);

    return texture;
}

void MikuPan_SetFontTexture(int fnt)
{
    curr_fnt_texture = fnt_texture[fnt];
}

void MikuPan_DeleteTexture(void *texture)
{
    for (int i = 0; i < 6; i++)
    {
        if (fnt_texture[i] == texture)
        {
            return;
        }
    }

    SDL_DestroyTexture((SDL_Texture*)texture);
}

void MikuPan_Camera(const SgCAMERA *camera)
{
    struct GRA3DSCRATCHPADLAYOUT * scratchpad = (struct GRA3DSCRATCHPADLAYOUT *)ps2_virtual_scratchpad;
    //unsigned int viewLoc = gl_context->GetUniformLocation(shaderProgram, "view");
    //gl_context->UniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));

    // Projection
    //unsigned int projectionLoc = context->GetUniformLocation(shaderProgram, "projection");
    //context->UniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));
}

void MikuPan_Shutdown()
{
    // destroy the GPU device
    SDL_DestroyGPUDevice(device);

    // destroy the window
    SDL_DestroyWindow(window);
}

void MikuPan_EndFrame()
{
    SDL_GPUColorTargetInfo target_info = {0};
    target_info.texture = swapchain_texture;
    target_info.clear_color =( SDL_FColor ){ 0.0f, 0.0f, 0.0f, 0.0f };
    target_info.load_op = SDL_GPU_LOADOP_CLEAR;
    target_info.store_op = SDL_GPU_STOREOP_STORE;
    target_info.mip_level = 0;
    target_info.layer_or_depth_plane = 0;
    target_info.cycle = false;
    render_pass = SDL_BeginGPURenderPass(command_buffer, &target_info, 1, NULL);

    // bind the pipeline
    SDL_BindGPUGraphicsPipeline(render_pass, graphicsPipeline);

    // bind the vertex buffer
    SDL_GPUBufferBinding bufferBindings[1];
    bufferBindings[0].buffer = vertexBuffer;
    bufferBindings[0].offset = 0;


    SDL_BindGPUVertexBuffers(render_pass, 0, bufferBindings, 1);
    SDL_DrawGPUPrimitives(render_pass, 3, 1, 0, 0);

    DrawImGuiWindow();
    RenderImGuiWindow(renderer);
    //SDL_EndGPURenderPass(render_pass);
    SDL_SubmitGPUCommandBuffer(command_buffer);
    SDL_RenderPresent(renderer);
}

SDL_GPUShader* MikuPan_LoadShader(
	SDL_GPUDevice* device,
	const char* shaderFilename,
	u_int samplerCount,
	u_int uniformBufferCount,
	u_int storageBufferCount,
	u_int storageTextureCount
) {
	// Auto-detect the shader stage from the file name for convenience
	SDL_GPUShaderStage stage;
	if (SDL_strstr(shaderFilename, ".vert"))
	{
		stage = SDL_GPU_SHADERSTAGE_VERTEX;
	}
	else if (SDL_strstr(shaderFilename, ".frag"))
	{
		stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
	}
	else
	{
		SDL_Log("Invalid shader stage!");
		return NULL;
	}

	char fullPath[256];
	SDL_GPUShaderFormat backendFormats = SDL_GetGPUShaderFormats(device);
	SDL_GPUShaderFormat format = SDL_GPU_SHADERFORMAT_INVALID;
	const char *entrypoint;



	if (backendFormats & SDL_GPU_SHADERFORMAT_SPIRV) {
		SDL_snprintf(fullPath, sizeof(fullPath), "%sContent/Shaders/Compiled/SPIRV/%s.spv", "./", shaderFilename);
		format = SDL_GPU_SHADERFORMAT_SPIRV;
		entrypoint = "main";
	} else if (backendFormats & SDL_GPU_SHADERFORMAT_MSL) {
		SDL_snprintf(fullPath, sizeof(fullPath), "%sContent/Shaders/Compiled/MSL/%s.msl", "./", shaderFilename);
		format = SDL_GPU_SHADERFORMAT_MSL;
		entrypoint = "main0";
	} else if (backendFormats & SDL_GPU_SHADERFORMAT_DXIL) {
		SDL_snprintf(fullPath, sizeof(fullPath), "%sContent/Shaders/Compiled/DXIL/%s.dxil", "./", shaderFilename);
		format = SDL_GPU_SHADERFORMAT_DXIL;
		entrypoint = "main";
	} else {
		SDL_Log("%s", "Unrecognized backend shader format!");
		return NULL;
	}

	size_t codeSize;
	void* code = SDL_LoadFile(fullPath, &codeSize);
	if (code == NULL)
	{
		SDL_Log("Failed to load shader from disk! %s", fullPath);
		return NULL;
	}

	SDL_GPUShaderCreateInfo shaderInfo = {
		.code = code,
		.code_size = codeSize,
		.entrypoint = entrypoint,
		.format = format,
		.stage = stage,
		.num_samplers = samplerCount,
		.num_uniform_buffers = uniformBufferCount,
		.num_storage_buffers = storageBufferCount,
		.num_storage_textures = storageTextureCount
	};
	SDL_GPUShader* shader = SDL_CreateGPUShader(device, &shaderInfo);
	if (shader == NULL)
	{
		SDL_Log("Failed to create shader!");
		SDL_free(code);
		return NULL;
	}

	SDL_free(code);
	return shader;
}