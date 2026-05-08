// PBR.hlsl - the main forward PBR shader.
//
// Vertex:  StandardVS
// Pixel:   StandardPS
//
// Inputs (per-vertex): position, normal, tangent (xyz, w=sign), uv, color.

#include "Common.hlsli"

struct VSIn {
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float4 tangent  : TANGENT;
    float2 uv       : TEXCOORD0;
    float4 color    : COLOR0;
};

struct VSOut {
    float4 pos_cs   : SV_POSITION;
    float3 pos_ws   : POSITION0;
    float3 normal_ws: NORMAL0;
    float3 tangent_ws: TANGENT0;
    float3 bitangent_ws: TANGENT1;
    float2 uv       : TEXCOORD0;
    float4 color    : COLOR0;
    float  view_z   : TEXCOORD1; // for cascade selection
};

// Resources.
Texture2D    t_basecolor       : register(t0);
Texture2D    t_normal          : register(t1);
Texture2D    t_metallic_rough  : register(t2);
Texture2D    t_emissive        : register(t3);
Texture2D    t_ao              : register(t4);
TextureCube  t_irradiance      : register(t5);
TextureCube  t_prefilter       : register(t6);
Texture2D    t_brdf_lut        : register(t7);
Texture2DArray t_shadow_atlas  : register(t8);
TextureCube  t_skybox          : register(t9);

SamplerState           s_linear : register(s0);
SamplerState           s_clamp  : register(s1);
SamplerComparisonState s_shadow : register(s2);

VSOut StandardVS(VSIn vin) {
    VSOut o;
    float4 pos_ws = mul(float4(vin.position, 1.0), g_model);
    o.pos_ws = pos_ws.xyz;
    o.pos_cs = mul(pos_ws, g_view_proj);
    o.normal_ws    = normalize(mul(float4(vin.normal,    0.0), g_model_inv_t).xyz);
    o.tangent_ws   = normalize(mul(float4(vin.tangent.xyz, 0.0), g_model).xyz);
    o.bitangent_ws = cross(o.normal_ws, o.tangent_ws) * vin.tangent.w;
    o.uv     = vin.uv;
    o.color  = vin.color;
    float4 pv = mul(pos_ws, g_view);
    o.view_z = pv.z;
    return o;
}

float sample_shadow_cascade(uint idx, float3 pos_ws, float NdotL) {
    float4 sh = mul(float4(pos_ws, 1.0), g_cascades[idx].view_proj);
    sh.xyz /= sh.w;
    float2 uv = sh.xy * 0.5 + 0.5;
    uv.y = 1.0 - uv.y;
    if (any(uv < 0) || any(uv > 1) || sh.z < 0 || sh.z > 1) return 1.0;
    // Slope-scaled bias.
    float bias = max(0.005 * (1.0 - NdotL), 0.0008);
    float3 sample_uv = float3(uv, (float)idx);
    // 3x3 PCF.
    float result = 0;
    float2 texel = 1.0 / float2(g_screen_size.xy); // we will repurpose ratio later
    [unroll]
    for (int x = -1; x <= 1; ++x) {
        [unroll]
        for (int y = -1; y <= 1; ++y) {
            float2 off = float2(x, y) / 1024.0;
            result += t_shadow_atlas.SampleCmpLevelZero(s_shadow,
                float3(sample_uv.xy + off, sample_uv.z), sh.z - bias);
        }
    }
    return result / 9.0;
}

float compute_shadow(float3 pos_ws, float view_z, float NdotL) {
    if (g_use_csm == 0) return 1.0;
    uint idx = g_csm_count - 1;
    [unroll]
    for (uint i = 0; i < SHADOW_CASCADES; ++i) {
        if (view_z < g_cascades[i].split_depth) { idx = i; break; }
    }
    return sample_shadow_cascade(idx, pos_ws, NdotL);
}

float3 unpack_normal(float3 n) {
    return normalize(n * 2.0 - 1.0);
}

float4 StandardPS(VSOut pin) : SV_TARGET {
    // Sample material.
    float4 base = g_base_color * pin.color;
    if (g_has_basecolor_tex) base *= t_basecolor.Sample(s_linear, pin.uv);
    if (base.a < 0.04) discard;

    float3 N = normalize(pin.normal_ws);
    if (g_has_normal_tex) {
        float3x3 TBN = float3x3(normalize(pin.tangent_ws),
                                normalize(pin.bitangent_ws),
                                N);
        float3 nm = unpack_normal(t_normal.Sample(s_linear, pin.uv).xyz);
        nm.xy *= g_normal_strength;
        N = normalize(mul(nm, TBN));
    }

    float metallic  = g_metallic;
    float roughness = g_roughness;
    if (g_has_mr_tex) {
        // glTF convention: G = roughness, B = metallic.
        float4 mr = t_metallic_rough.Sample(s_linear, pin.uv);
        roughness *= mr.g;
        metallic  *= mr.b;
    }
    roughness = clamp(roughness, 0.04, 1.0);

    float3 V = normalize(g_camera_pos.xyz - pin.pos_ws);
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), base.rgb, metallic);

    float3 Lo = 0;
    for (uint i = 0; i < g_light_count; ++i) {
        LightGPU lt = g_lights[i];
        float3 L;
        float  attenuation = 1.0;
        if (lt.type == 0) {
            L = normalize(-lt.direction);
        } else {
            float3 to_light = lt.position - pin.pos_ws;
            float  dist = length(to_light);
            L = to_light / max(dist, 1e-4);
            float r = lt.range;
            attenuation = saturate(1.0 - (dist * dist) / (r * r));
            attenuation *= attenuation;
            if (lt.type == 2) {
                float cosA = dot(-L, normalize(lt.direction));
                float t = saturate((cosA - lt.spot_outer_cos) /
                                   max(lt.spot_inner_cos - lt.spot_outer_cos, 1e-4));
                attenuation *= t;
            }
        }
        float3 H = normalize(V + L);
        float NdotL = saturate(dot(N, L));
        float NdotV = saturate(dot(N, V));

        float D = distribution_ggx(N, H, roughness);
        float G = geometry_smith(N, V, L, roughness);
        float3 F = fresnel_schlick(saturate(dot(H, V)), F0);

        float3 kS = F;
        float3 kD = (1.0 - kS) * (1.0 - metallic);

        float3 num   = D * G * F;
        float  denom = 4.0 * NdotV * NdotL + 1e-4;
        float3 spec  = num / denom;

        float3 radiance = lt.color * lt.intensity * attenuation;
        float shadow = 1.0;
        if (lt.casts_shadow != 0 && lt.type == 0) {
            shadow = compute_shadow(pin.pos_ws, pin.view_z, NdotL);
        }
        Lo += (kD * base.rgb / PI + spec) * radiance * NdotL * shadow;
    }

    // Indirect IBL.
    float3 ambient;
    if (g_use_ibl) {
        float3 R = reflect(-V, N);
        float3 F = fresnel_schlick_roughness(saturate(dot(N, V)), F0, roughness);
        float3 kS = F;
        float3 kD = (1.0 - kS) * (1.0 - metallic);
        float3 irr = t_irradiance.Sample(s_linear, N).rgb;
        float3 diffuse = irr * base.rgb;
        float lod = roughness * 4.0;
        float3 prefiltered = t_prefilter.SampleLevel(s_linear, R, lod).rgb;
        float2 brdf = t_brdf_lut.SampleLevel(s_clamp, float2(saturate(dot(N, V)), roughness), 0).rg;
        float3 specular = prefiltered * (F * brdf.x + brdf.y);
        ambient = (kD * diffuse + specular);
    } else {
        ambient = float3(0.03, 0.03, 0.04) * base.rgb;
    }

    float aoFactor = g_ao;
    ambient *= aoFactor;

    float3 emissive = g_emissive.rgb;
    if (g_has_emissive_tex) emissive *= t_emissive.Sample(s_linear, pin.uv).rgb;

    float3 col = ambient + Lo + emissive;
    return float4(col, base.a);
}
