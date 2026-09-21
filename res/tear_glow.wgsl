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

    let core = clamp(1.0 - r, 0.0, 1.0);

    // ---- TUNABLES (edit these four, rebuild, look) ----
    // BODY_FALLOFF was 2.5, which is why it read as haze rather than an orb:
    // at half radius pow(0.5, 2.5) = 0.177, so the whole outer half of the
    // sphere sat under 18% intensity - a hot pip in a faint cloud. 1.2 gives
    // 0.435 at the same point, so the body actually carries light. Raise it
    // toward 2.5 for wispier, lower toward 1.0 for a harder ball.
    let BODY_FALLOFF = 1.2;
    let BODY_GAIN    = 1.00;   // overall fill brightness
    let HOT_GAIN     = 1.40;   // nucleus punch; >1 is intentional (additive)
    let PULSE_DEPTH  = 0.12;   // was 0.22 - the old dip cost up to 22% of the body

    let body = pow(core, BODY_FALLOFF);   // the filled volume
    let hot  = pow(core, 6.0);            // small bright nucleus
    let pulse = (1.0 - PULSE_DEPTH) + PULSE_DEPTH * sin(t * 3.0);

    let intensity = (body * BODY_GAIN + hot * HOT_GAIN) * pulse * alpha;

    // White-hot centre falling off to blue at the rim. A real light orb is
    // near-white in the middle; the old flat vec3(0.30,0.60,1.0) capped the
    // brightest pixel at 30% red, which reads washed-out against bright
    // scenery no matter how high the alpha goes.
    let tint = mix(vec3f(0.30, 0.62, 1.0), vec3f(0.88, 0.96, 1.0), hot);

    let col = tint * intensity;
    return vec4f(col, intensity);
}
