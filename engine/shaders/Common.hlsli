// Common.hlsli - shared types and constants for the renderer.

#ifndef CONTINUOUS_COMMON_HLSLI
#define CONTINUOUS_COMMON_HLSLI

#define SHADOW_CASCADES 4
#define MAX_LIGHTS 16

struct LightGPU {
    float3 position;     // for point/spot
    float  range;
    float3 direction;    // for dir/spot
    float  intensity;
    float3 color;
    uint   type;         // 0=Dir, 1=Point, 2=Spot
    float  spot_inner_cos;
    float  spot_outer_cos;
    uint   casts_shadow;
    float  _pad;
};

struct CascadeGPU {
    float4x4 view_proj;
    float    split_depth;
    float3   _pad;
};

cbuffer CBFrame : register(b0) {
    float4x4 g_view;
    float4x4 g_proj;
    float4x4 g_view_proj;
    float4   g_camera_pos;     // xyz, w=time
    float4   g_screen_size;    // xy=size, zw=inv
    uint     g_light_count;
    uint     g_use_ibl;
    uint     g_use_csm;
    uint     g_csm_count;
    LightGPU g_lights[MAX_LIGHTS];
    CascadeGPU g_cascades[SHADOW_CASCADES];
};

cbuffer CBObject : register(b1) {
    float4x4 g_model;
    float4x4 g_model_inv_t;
    float4   g_object_color;
};

cbuffer CBMaterial : register(b2) {
    float4 g_base_color;
    float4 g_emissive;       // xyz emissive, w unused
    float  g_metallic;
    float  g_roughness;
    float  g_ao;
    float  g_normal_strength;
    uint   g_has_basecolor_tex;
    uint   g_has_normal_tex;
    uint   g_has_mr_tex;
    uint   g_has_emissive_tex;
};

cbuffer CBShadow : register(b3) {
    float4x4 g_shadow_view_proj;
    uint     g_cascade_index;
    float3   _pad_shadow;
};

// PBR constants.
static const float PI = 3.14159265358979323846;

// Schlick Fresnel.
float3 fresnel_schlick(float cosTheta, float3 F0) {
    return F0 + (1.0 - F0) * pow(saturate(1.0 - cosTheta), 5.0);
}
float3 fresnel_schlick_roughness(float cosTheta, float3 F0, float roughness) {
    return F0 + (max(float3(1,1,1) * (1.0 - roughness), F0) - F0) *
                pow(saturate(1.0 - cosTheta), 5.0);
}

// GGX normal distribution.
float distribution_ggx(float3 N, float3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = saturate(dot(N, H));
    float d = (NdotH * a2 - NdotH) * NdotH + 1.0;
    return a2 / max(PI * d * d, 1e-7);
}

float geometry_schlick_ggx(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float geometry_smith(float3 N, float3 V, float3 L, float roughness) {
    float NdotV = saturate(dot(N, V));
    float NdotL = saturate(dot(N, L));
    return geometry_schlick_ggx(NdotV, roughness) * geometry_schlick_ggx(NdotL, roughness);
}

#endif // CONTINUOUS_COMMON_HLSLI
