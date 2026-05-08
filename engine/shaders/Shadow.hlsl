// Shadow.hlsl - depth-only pass for shadow cascades.

#include "Common.hlsli"

struct VSIn {
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float4 tangent  : TANGENT;
    float2 uv       : TEXCOORD0;
    float4 color    : COLOR0;
};

struct VSOut {
    float4 pos_cs : SV_POSITION;
};

VSOut ShadowVS(VSIn vin) {
    VSOut o;
    float4 ws = mul(float4(vin.position, 1.0), g_model);
    o.pos_cs = mul(ws, g_shadow_view_proj);
    return o;
}

void ShadowPS(VSOut pin) {
    // Depth only.
}
