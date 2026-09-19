// Tear of Light — world-space, camera-facing additive glow billboard.
// Drawn by tear_glow.cpp via Dusklight 2.0 GfxService. 4-vertex triangle strip,
// corners generated procedurally from vertex_index (no vertex buffer).

struct Billboard {
    view_proj : mat4x4f,  // CameraInfo.proj_from_world
    center    : vec4f,    // tear world position (xyz)
    cam_right : vec4f,    // world_from_view column 0
    cam_up    : vec4f,    // world_from_view column 1
    params    : vec4f,    // x = size (world units), y = time (s), z = alpha, w = free
};

@group(0) @binding(0) var<uniform> u : Billboard;

struct VsOut {
    @builtin(position) clip : vec4f,
    @location(0)       uv   : vec2f,
};

@vertex
fn vs_main(@builtin(vertex_index) vid : u32) -> VsOut {
    var corners = array<vec2f, 4>(
        vec2f(-1.0, -1.0), vec2f(1.0, -1.0),
        vec2f(-1.0,  1.0), vec2f(1.0,  1.0));
    let c = corners[vid];

    let size = u.params.x;
    let world = u.center.xyz
              + c.x * u.cam_right.xyz * size
              + c.y * u.cam_up.xyz    * size;

    var out : VsOut;
    out.clip = u.view_proj * vec4f(world, 1.0);
    out.uv   = c;
    return out;
}

@fragment
fn fs_main(in : VsOut) -> @location(0) vec4f {
    let t     = u.params.y;
    let alpha = u.params.z;
    let r     = length(in.uv);            // 0 at center

    let core  = clamp(1.0 - r, 0.0, 1.0);
    let glow  = pow(core, 2.5);           // tight hot center
    let pulse = 0.78 + 0.22 * sin(t * 3.0);
    let intensity = glow * pulse * alpha;

    // Bright blue tear.
    let col = vec3f(0.30, 0.60, 1.0) * intensity;
    return vec4f(col, intensity);
}
