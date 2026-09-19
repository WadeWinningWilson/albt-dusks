// ============================================
// NEW CODE - ALBW Port (Dusklight 2.0 GfxService tear glow)
// Visual half of the Tear of Light: a world-space, camera-facing additive glow
// billboard drawn in the scene pass via GfxService. Position/alpha come from the
// custom tear actor (tear_actor.cpp). Plumbing follows the ao_mod reference:
//   init  : load tear_glow.wgsl, make a shader module, register a draw type + a
//           SCENE_AFTER_OPAQUE stage hook. Pipeline built lazily on layout.key.
//   scene : (game thread) snapshot the camera, fill the billboard uniform, push.
//   draw  : (render worker) bind the uniform and draw a 4-vert strip.
// ============================================
#include "tear_actor.hpp"

#include "albw_common.h"

#include "mods/svc/camera.h"
#include "mods/svc/gfx.h"
#include "mods/svc/resource.h"

#include <cmath>
#include <cstring>
#include <type_traits>

IMPORT_SERVICE(GfxService, svc_gfx);
IMPORT_SERVICE(CameraService, svc_camera);
// svc_resource is owned (IMPORT_SERVICE) by armogohma.cpp — reference it.
extern const ResourceService* svc_resource;

namespace {

struct BillboardUniforms {
    float view_proj[16];
    float center[4];
    float cam_right[4];
    float cam_up[4];
    float params[4];  // x=size, y=time, z=alpha, w=0
};
static_assert(sizeof(BillboardUniforms) % 16 == 0, "uniform must be 16-byte aligned");

struct DrawPayload {
    uint32_t uniform_offset;
    uint32_t uniform_size;
};
static_assert(std::is_trivially_copyable_v<DrawPayload>, "payload must be POD");
static_assert(sizeof(DrawPayload) <= GFX_INLINE_DRAW_PAYLOAD_SIZE, "payload too large");

GfxDeviceInfo      g_device = GFX_DEVICE_INFO_INIT;
WGPUShaderModule   g_shader = nullptr;
WGPURenderPipeline g_pipeline = nullptr;
WGPUBindGroupLayout g_bindLayout = nullptr;
uint64_t           g_pipelineKey = 0;
bool               g_havePipeline = false;
GfxDrawTypeHandle  g_drawType = 0;
GfxStageHookHandle g_stageHook = 0;
ResourceBuffer     g_wgsl = RESOURCE_BUFFER_INIT;
float              g_time = 0.0f;
volatile uint32_t  g_drawFired = 0;      // incremented on the render worker (no logging there)
volatile bool      g_drawPipelineOk = false;

WGPUShaderModule makeShader(const ResourceBuffer& src) {
    WGPUShaderSourceWGSL wgsl = WGPU_SHADER_SOURCE_WGSL_INIT;
    wgsl.code.data = static_cast<const char*>(src.data);
    wgsl.code.length = src.size;
    WGPUShaderModuleDescriptor desc = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
    desc.nextInChain = &wgsl.chain;
    desc.label = {"albw tear glow", WGPU_STRLEN};
    return wgpuDeviceCreateShaderModule(g_device.device, &desc);
}

// (Re)build the render pipeline for the current scene render-target layout.
bool ensurePipeline(const GfxRenderTargetLayout& layout) {
    if (g_havePipeline && g_pipelineKey == layout.key && g_pipeline != nullptr) {
        return true;
    }
    if (g_pipeline != nullptr) {
        wgpuRenderPipelineRelease(g_pipeline);
        g_pipeline = nullptr;
    }
    if (g_bindLayout != nullptr) {
        wgpuBindGroupLayoutRelease(g_bindLayout);
        g_bindLayout = nullptr;
    }
    if (g_shader == nullptr) {
        return false;
    }

    // Additive glow blend: src*srcAlpha + dst*1.
    WGPUBlendState blend = {};
    blend.color.operation = WGPUBlendOperation_Add;
    blend.color.srcFactor = WGPUBlendFactor_SrcAlpha;
    blend.color.dstFactor = WGPUBlendFactor_One;
    blend.alpha.operation = WGPUBlendOperation_Add;
    blend.alpha.srcFactor = WGPUBlendFactor_Zero;
    blend.alpha.dstFactor = WGPUBlendFactor_One;

    WGPUColorTargetState colorTargets[GFX_MAX_COLOR_ATTACHMENTS];
    const uint32_t colorCount =
        gfx_init_color_target_states(&layout, colorTargets, &blend, WGPUColorWriteMask_All);

    WGPUFragmentState fragment = WGPU_FRAGMENT_STATE_INIT;
    fragment.module = g_shader;
    fragment.entryPoint = {"fs_main", WGPU_STRLEN};
    fragment.targetCount = colorCount;
    fragment.targets = colorTargets;

    // Depth test against the scene (occluded by geometry) but no depth write.
    WGPUDepthStencilState depth = WGPU_DEPTH_STENCIL_STATE_INIT;
    depth.format = layout.depth_stencil_format;
    depth.depthWriteEnabled = WGPUOptionalBool_False;
    depth.depthCompare =
        g_device.uses_reversed_z ? WGPUCompareFunction_GreaterEqual : WGPUCompareFunction_LessEqual;

    WGPURenderPipelineDescriptor desc = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
    desc.label = {"albw tear glow", WGPU_STRLEN};
    desc.vertex.module = g_shader;
    desc.vertex.entryPoint = {"vs_main", WGPU_STRLEN};
    desc.primitive.topology = WGPUPrimitiveTopology_TriangleStrip;
    desc.depthStencil = (layout.depth_stencil_format != WGPUTextureFormat_Undefined) ? &depth : nullptr;
    desc.multisample.count = layout.sample_count;
    desc.fragment = &fragment;

    g_pipeline = wgpuDeviceCreateRenderPipeline(g_device.device, &desc);
    if (g_pipeline == nullptr) {
        return false;
    }
    g_bindLayout = wgpuRenderPipelineGetBindGroupLayout(g_pipeline, 0);
    g_pipelineKey = layout.key;
    g_havePipeline = true;
    return true;
}

// Render worker thread: bind the pushed uniform and draw the billboard.
void on_draw(ModContext*, const GfxDrawContext* ctx, const void* payload, size_t size, void*) {
    if (ctx == nullptr || size != sizeof(DrawPayload)) {
        return;
    }
    // NOTE: render worker thread — NO service calls (svc_log etc.) allowed here.
    // Record state in plain globals; on_scene (game thread) logs them.
    g_drawFired++;
    const bool haveP = ensurePipeline(ctx->layout);
    g_drawPipelineOk = haveP;
    if (!haveP) {
        return;
    }
    DrawPayload data;
    std::memcpy(&data, payload, sizeof(data));

    WGPUBindGroupEntry entry = WGPU_BIND_GROUP_ENTRY_INIT;
    entry.binding = 0;
    entry.buffer = ctx->uniform_buffer;
    entry.offset = data.uniform_offset;
    entry.size = data.uniform_size;

    WGPUBindGroupDescriptor bgd = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    bgd.layout = g_bindLayout;
    bgd.entryCount = 1;
    bgd.entries = &entry;
    WGPUBindGroup bg = wgpuDeviceCreateBindGroup(ctx->device, &bgd);

    wgpuRenderPassEncoderSetPipeline(ctx->pass, g_pipeline);
    wgpuRenderPassEncoderSetBindGroup(ctx->pass, 0, bg, 0, nullptr);
    wgpuRenderPassEncoderDraw(ctx->pass, 4, 1, 0, 0);
    wgpuBindGroupRelease(bg);
}

// Game thread: snapshot camera + tear state, push the uniform + a draw.
void on_scene(ModContext*, const GfxStageContext* stageCtx, void*) {
    const bool hasView = stageCtx != nullptr && stageCtx->game_view != nullptr;
    cXyz pos;
    f32 sizeWorld = 26.0f;
    f32 alpha = 1.0f;
    const bool hasTear = albw_tear_actor_get_draw(&pos, &sizeWorld, &alpha);
    if (!hasView || !hasTear) {
        return;
    }

    CameraInfo cam = CAMERA_INFO_INIT;
    const ModResult camRc = svc_camera->get_camera(mod_ctx, stageCtx->game_view, &cam);
    if (camRc != MOD_OK) {
        return;
    }

    g_time += 1.0f / 60.0f;

    BillboardUniforms u = {};
    std::memcpy(u.view_proj, cam.proj_from_world, sizeof(u.view_proj));
    u.center[0] = pos.x;
    u.center[1] = pos.y;
    u.center[2] = pos.z;
    u.center[3] = 1.0f;
    // world_from_view column 0 = right, column 1 = up (column-major float[16]).
    u.cam_right[0] = cam.world_from_view[0];
    u.cam_right[1] = cam.world_from_view[1];
    u.cam_right[2] = cam.world_from_view[2];
    u.cam_up[0] = cam.world_from_view[4];
    u.cam_up[1] = cam.world_from_view[5];
    u.cam_up[2] = cam.world_from_view[6];
    u.params[0] = sizeWorld;
    u.params[1] = g_time;
    u.params[2] = alpha;

    GfxRange range = {0, 0};
    if (svc_gfx->push_uniform(mod_ctx, &u, sizeof(u), &range) != MOD_OK) {
        return;
    }
    DrawPayload payload = {range.offset, range.size};
    svc_gfx->push_draw(mod_ctx, g_drawType, &payload, sizeof(payload));
}

}  // namespace

ModResult albw_tear_glow_init(ModError*) {
    if (svc_resource->load(mod_ctx, "tear_glow.wgsl", &g_wgsl) != MOD_OK) {
        svc_log->error(mod_ctx, "tear glow: failed to load tear_glow.wgsl");
        return MOD_ERROR;
    }
    if (svc_gfx->get_device_info(mod_ctx, &g_device) != MOD_OK) {
        svc_log->error(mod_ctx, "tear glow: get_device_info failed");
        return MOD_ERROR;
    }
    g_shader = makeShader(g_wgsl);
    if (g_shader == nullptr) {
        svc_log->error(mod_ctx, "tear glow: shader module creation failed");
        return MOD_ERROR;
    }

    GfxDrawTypeDesc drawDesc = GFX_DRAW_TYPE_DESC_INIT;
    drawDesc.label = "albw tear glow";
    drawDesc.draw = on_draw;
    if (svc_gfx->register_draw_type(mod_ctx, &drawDesc, &g_drawType) != MOD_OK) {
        svc_log->error(mod_ctx, "tear glow: register_draw_type failed");
        return MOD_ERROR;
    }

    GfxStageHookDesc stageDesc = GFX_STAGE_HOOK_DESC_INIT;
    stageDesc.callback = on_scene;
    if (svc_gfx->register_stage_hook(mod_ctx, GFX_STAGE_SCENE_AFTER_OPAQUE, &stageDesc,
                                     &g_stageHook) != MOD_OK) {
        svc_log->error(mod_ctx, "tear glow: register_stage_hook failed");
        return MOD_ERROR;
    }
    return MOD_OK;
}

ModResult albw_tear_glow_shutdown(ModError*) {
    if (g_pipeline != nullptr) {
        wgpuRenderPipelineRelease(g_pipeline);
        g_pipeline = nullptr;
    }
    if (g_bindLayout != nullptr) {
        wgpuBindGroupLayoutRelease(g_bindLayout);
        g_bindLayout = nullptr;
    }
    if (g_shader != nullptr) {
        wgpuShaderModuleRelease(g_shader);
        g_shader = nullptr;
    }
    if (g_wgsl.data != nullptr) {
        svc_resource->free(mod_ctx, &g_wgsl);
    }
    return MOD_OK;
}
