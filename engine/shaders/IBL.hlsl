// IBL.hlsl - generates irradiance, specular prefilter, BRDF LUT, and a
// procedural sky into a cubemap.

#include "Common.hlsli"

cbuffer CBIBL : register(b0) {
    float4x4 g_face_view;        // view matrix per face
    float4x4 g_face_proj;
    float    g_roughness;
    uint     g_face;
    uint     g_size;
    float    g_pad_;
};

struct VSOut {
    float4 pos_cs : SV_POSITION;
    float3 dir    : TEXCOORD0;
};

VSOut FaceVS(uint vid : SV_VERTEXID) {
    // Fullscreen triangle, reconstruct world dir.
    VSOut o;
    float2 uv = float2((vid << 1) & 2, vid & 2);
    o.pos_cs = float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);

    // ndc to view to world.
    float4 ndc = float4(o.pos_cs.xy, 1, 1);
    float4 v   = mul(ndc, transpose(g_face_proj));
    float3 vd  = normalize(v.xyz / v.w);
    float3 wd  = mul(vd, (float3x3)transpose(g_face_view));
    o.dir = wd;
    return o;
}

// Procedural sky (Hosek-style approximation - simple).
float3 sky_color(float3 dir) {
    float t = saturate(dir.y * 0.5 + 0.5);
    float3 horizon = float3(0.85, 0.92, 1.0);
    float3 zenith  = float3(0.30, 0.55, 1.0);
    float3 ground  = float3(0.25, 0.20, 0.18);
    float3 sky = lerp(horizon, zenith, pow(t, 0.5));
    sky = lerp(ground, sky, smoothstep(-0.05, 0.10, dir.y));
    // Sun.
    float3 sundir = normalize(float3(0.4, 0.7, 0.5));
    float sun = pow(saturate(dot(dir, sundir)), 256.0);
    return sky + sun * float3(2.0, 1.6, 1.2);
}

float4 ProceduralSkyPS(VSOut pin) : SV_TARGET {
    float3 d = normalize(pin.dir);
    return float4(sky_color(d), 1);
}

// ---- Irradiance: cosine-weighted hemisphere integration ----
TextureCube  t_env    : register(t0);
SamplerState s_linear : register(s0);

float4 IrradiancePS(VSOut pin) : SV_TARGET {
    float3 N = normalize(pin.dir);
    float3 up = float3(0, 1, 0);
    float3 right = normalize(cross(up, N));
    up = normalize(cross(N, right));
    float3 irr = 0;
    float sample_count = 0;
    const float dphi = 0.0436;   // ~2.5 deg
    const float dtheta = 0.0436;
    for (float phi = 0; phi < 6.2831853; phi += dphi) {
        for (float theta = 0; theta < 1.5707963; theta += dtheta) {
            float3 ts = float3(sin(theta) * cos(phi),
                               sin(theta) * sin(phi),
                               cos(theta));
            float3 ws = ts.x * right + ts.y * up + ts.z * N;
            irr += t_env.SampleLevel(s_linear, ws, 0).rgb * cos(theta) * sin(theta);
            sample_count += 1;
        }
    }
    irr = 3.14159265 * irr / sample_count;
    return float4(irr, 1);
}

// ---- Specular prefilter (importance-sampled GGX) ----
float radical_inverse_vdc(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}
float2 hammersley(uint i, uint N) {
    return float2(float(i) / float(N), radical_inverse_vdc(i));
}
float3 importance_sample_ggx(float2 Xi, float3 N, float roughness) {
    float a = roughness * roughness;
    float phi = 2.0 * 3.14159265 * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    float3 H = float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
    float3 up = abs(N.z) < 0.999 ? float3(0, 0, 1) : float3(1, 0, 0);
    float3 tan = normalize(cross(up, N));
    float3 bit = cross(N, tan);
    return normalize(tan * H.x + bit * H.y + N * H.z);
}

float4 PrefilterPS(VSOut pin) : SV_TARGET {
    float3 N = normalize(pin.dir);
    float3 R = N;
    float3 V = R;
    float3 prefiltered = 0;
    float total = 0;
    const uint kSamples = 1024;
    for (uint i = 0; i < kSamples; ++i) {
        float2 Xi = hammersley(i, kSamples);
        float3 H = importance_sample_ggx(Xi, N, g_roughness);
        float3 L = normalize(2.0 * dot(V, H) * H - V);
        float NdotL = max(dot(N, L), 0.0);
        if (NdotL > 0) {
            prefiltered += t_env.SampleLevel(s_linear, L, 0).rgb * NdotL;
            total += NdotL;
        }
    }
    return float4(prefiltered / max(total, 1e-4), 1);
}

// ---- BRDF LUT ----
float G_smith_ibl(float NdotV, float NdotL, float roughness) {
    float a = roughness;
    float k = (a * a) / 2.0;
    float gv = NdotV / (NdotV * (1.0 - k) + k);
    float gl = NdotL / (NdotL * (1.0 - k) + k);
    return gv * gl;
}

float4 BRDFLutPS(VSOut pin) : SV_TARGET {
    float NdotV = pin.pos_cs.x / float(g_size);
    float roughness = pin.pos_cs.y / float(g_size);
    NdotV = max(NdotV, 0.001);
    float3 V = float3(sqrt(1.0 - NdotV * NdotV), 0, NdotV);
    float A = 0; float B = 0;
    float3 N = float3(0, 0, 1);
    const uint kSamples = 1024;
    for (uint i = 0; i < kSamples; ++i) {
        float2 Xi = hammersley(i, kSamples);
        float3 H = importance_sample_ggx(Xi, N, roughness);
        float3 L = normalize(2.0 * dot(V, H) * H - V);
        float NdotL = max(L.z, 0.0);
        float NdotH = max(H.z, 0.0);
        float VdotH = max(dot(V, H), 0.0);
        if (NdotL > 0) {
            float G = G_smith_ibl(NdotV, NdotL, roughness);
            float G_vis = (G * VdotH) / (NdotH * NdotV);
            float Fc = pow(1.0 - VdotH, 5.0);
            A += (1.0 - Fc) * G_vis;
            B += Fc * G_vis;
        }
    }
    return float4(A / kSamples, B / kSamples, 0, 1);
}
