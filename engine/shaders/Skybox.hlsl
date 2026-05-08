// Skybox.hlsl - render a cubemap centred on the camera.

#include "Common.hlsli"

struct VSOut {
    float4 pos_cs : SV_POSITION;
    float3 dir    : TEXCOORD0;
};

TextureCube  t_skybox : register(t9);
SamplerState s_linear : register(s0);

VSOut SkyboxVS(uint vid : SV_VERTEXID) {
    // Fullscreen triangle technique - 3 verts, no input.
    float2 uv = float2((vid << 1) & 2, vid & 2);
    float4 ndc = float4(uv * 2.0 - 1.0, 1.0, 1.0); // far plane (z=1 in 0..1)
    // Reconstruct world ray.
    float4x4 inv_vp = transpose(g_view_proj); // we will pass full inverse via cb in renderer
    // Simpler: sample direction from view-space ray.
    float4 view_dir4 = mul(ndc, transpose(g_proj));
    float3 view_dir = view_dir4.xyz / view_dir4.w;
    float3 world_dir = mul(view_dir, (float3x3)transpose(g_view));
    VSOut o;
    o.pos_cs = ndc;
    o.dir = world_dir;
    return o;
}

float4 SkyboxPS(VSOut pin) : SV_TARGET {
    float3 dir = normalize(pin.dir);
    float3 col = t_skybox.SampleLevel(s_linear, dir, 0).rgb;
    return float4(col, 1.0);
}
