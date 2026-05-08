// PostProcess.hlsl - bloom (threshold/down/up), tonemap (ACES), FXAA.

cbuffer CBPost : register(b0) {
    float4 g_size_inv;       // xy = 1/size, zw = size
    float4 g_params;         // x = threshold, y = strength, z = exposure, w = gamma
};

Texture2D    t_input  : register(t0);
Texture2D    t_bloom  : register(t1);
SamplerState s_clamp  : register(s0);

struct VSOut {
    float4 pos_cs : SV_POSITION;
    float2 uv     : TEXCOORD0;
};

VSOut FullscreenVS(uint vid : SV_VERTEXID) {
    VSOut o;
    float2 uv = float2((vid << 1) & 2, vid & 2);
    o.pos_cs = float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);
    o.uv = uv;
    return o;
}

float4 ThresholdPS(VSOut pin) : SV_TARGET {
    float3 c = t_input.SampleLevel(s_clamp, pin.uv, 0).rgb;
    float bright = max(max(c.r, c.g), c.b);
    float soft = saturate(bright - g_params.x + 0.5);
    soft = soft * soft * 0.5;
    float contribution = max(soft, bright - g_params.x) / max(bright, 1e-4);
    return float4(c * contribution, 1);
}

// 13-tap downsample (Kawase / COD AW).
float4 DownsamplePS(VSOut pin) : SV_TARGET {
    float2 px = g_size_inv.xy;
    float2 uv = pin.uv;
    float3 a = t_input.SampleLevel(s_clamp, uv + float2(-2, -2) * px, 0).rgb;
    float3 b = t_input.SampleLevel(s_clamp, uv + float2( 0, -2) * px, 0).rgb;
    float3 c = t_input.SampleLevel(s_clamp, uv + float2( 2, -2) * px, 0).rgb;
    float3 d = t_input.SampleLevel(s_clamp, uv + float2(-2,  0) * px, 0).rgb;
    float3 e = t_input.SampleLevel(s_clamp, uv + float2( 0,  0) * px, 0).rgb;
    float3 f = t_input.SampleLevel(s_clamp, uv + float2( 2,  0) * px, 0).rgb;
    float3 g = t_input.SampleLevel(s_clamp, uv + float2(-2,  2) * px, 0).rgb;
    float3 h = t_input.SampleLevel(s_clamp, uv + float2( 0,  2) * px, 0).rgb;
    float3 i = t_input.SampleLevel(s_clamp, uv + float2( 2,  2) * px, 0).rgb;
    float3 j = t_input.SampleLevel(s_clamp, uv + float2(-1, -1) * px, 0).rgb;
    float3 k = t_input.SampleLevel(s_clamp, uv + float2( 1, -1) * px, 0).rgb;
    float3 l = t_input.SampleLevel(s_clamp, uv + float2(-1,  1) * px, 0).rgb;
    float3 m = t_input.SampleLevel(s_clamp, uv + float2( 1,  1) * px, 0).rgb;
    float3 col = e * 0.125;
    col += (a + c + g + i) * 0.03125;
    col += (b + d + f + h) * 0.0625;
    col += (j + k + l + m) * 0.125;
    return float4(col, 1);
}

// 9-tap upsample / blur / additive.
float4 UpsamplePS(VSOut pin) : SV_TARGET {
    float2 px = g_size_inv.xy;
    float2 uv = pin.uv;
    float3 col =
        t_input.SampleLevel(s_clamp, uv + float2(-1, -1) * px, 0).rgb +
        t_input.SampleLevel(s_clamp, uv + float2( 0, -1) * px, 0).rgb * 2 +
        t_input.SampleLevel(s_clamp, uv + float2( 1, -1) * px, 0).rgb +
        t_input.SampleLevel(s_clamp, uv + float2(-1,  0) * px, 0).rgb * 2 +
        t_input.SampleLevel(s_clamp, uv + float2( 0,  0) * px, 0).rgb * 4 +
        t_input.SampleLevel(s_clamp, uv + float2( 1,  0) * px, 0).rgb * 2 +
        t_input.SampleLevel(s_clamp, uv + float2(-1,  1) * px, 0).rgb +
        t_input.SampleLevel(s_clamp, uv + float2( 0,  1) * px, 0).rgb * 2 +
        t_input.SampleLevel(s_clamp, uv + float2( 1,  1) * px, 0).rgb;
    col *= 1.0 / 16.0;
    return float4(col, 1);
}

// ACES tonemap (Stephen Hill fit).
static const float3x3 ACESInputMat = float3x3(
    0.59719, 0.35458, 0.04823,
    0.07600, 0.90834, 0.01566,
    0.02840, 0.13383, 0.83777);
static const float3x3 ACESOutputMat = float3x3(
     1.60475, -0.53108, -0.07367,
    -0.10208,  1.10813, -0.00605,
    -0.00327, -0.07276,  1.07602);

float3 RRTAndODTFit(float3 v) {
    float3 a = v * (v + 0.0245786) - 0.000090537;
    float3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
    return a / b;
}

float3 ACESFitted(float3 c) {
    c = mul(ACESInputMat, c);
    c = RRTAndODTFit(c);
    c = mul(ACESOutputMat, c);
    return saturate(c);
}

float4 TonemapPS(VSOut pin) : SV_TARGET {
    float3 hdr = t_input.SampleLevel(s_clamp, pin.uv, 0).rgb * g_params.z;
    float3 bloom = t_bloom.SampleLevel(s_clamp, pin.uv, 0).rgb * g_params.y;
    hdr += bloom;
    float3 mapped = ACESFitted(hdr);
    mapped = pow(mapped, 1.0 / g_params.w);
    return float4(mapped, 1);
}

// FXAA 3.11 simplified.
float fxaa_luma(float3 c) { return dot(c, float3(0.299, 0.587, 0.114)); }

float4 FxaaPS(VSOut pin) : SV_TARGET {
    float2 px = g_size_inv.xy;
    float2 uv = pin.uv;
    float3 rgbM  = t_input.SampleLevel(s_clamp, uv, 0).rgb;
    float3 rgbNW = t_input.SampleLevel(s_clamp, uv + float2(-1,-1) * px, 0).rgb;
    float3 rgbNE = t_input.SampleLevel(s_clamp, uv + float2( 1,-1) * px, 0).rgb;
    float3 rgbSW = t_input.SampleLevel(s_clamp, uv + float2(-1, 1) * px, 0).rgb;
    float3 rgbSE = t_input.SampleLevel(s_clamp, uv + float2( 1, 1) * px, 0).rgb;
    float lM  = fxaa_luma(rgbM);
    float lNW = fxaa_luma(rgbNW);
    float lNE = fxaa_luma(rgbNE);
    float lSW = fxaa_luma(rgbSW);
    float lSE = fxaa_luma(rgbSE);
    float lMin = min(lM, min(min(lNW, lNE), min(lSW, lSE)));
    float lMax = max(lM, max(max(lNW, lNE), max(lSW, lSE)));
    float2 dir;
    dir.x = -((lNW + lNE) - (lSW + lSE));
    dir.y =  ((lNW + lSW) - (lNE + lSE));
    float dirRed = max((lNW + lNE + lSW + lSE) * 0.25 * 0.0625, 1e-5);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirRed);
    dir = clamp(dir * rcpDirMin, -8.0, 8.0) * px;
    float3 a = 0.5 * (
        t_input.SampleLevel(s_clamp, uv + dir * (1.0/3.0 - 0.5), 0).rgb +
        t_input.SampleLevel(s_clamp, uv + dir * (2.0/3.0 - 0.5), 0).rgb);
    float3 b = a * 0.5 + 0.25 * (
        t_input.SampleLevel(s_clamp, uv + dir * -0.5, 0).rgb +
        t_input.SampleLevel(s_clamp, uv + dir *  0.5, 0).rgb);
    float lB = fxaa_luma(b);
    float3 col = (lB < lMin || lB > lMax) ? a : b;
    return float4(col, 1);
}
