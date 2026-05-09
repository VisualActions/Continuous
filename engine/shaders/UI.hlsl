// UI.hlsl - 2D textured quad pipeline.

cbuffer CBUI : register(b0) {
    float4 g_screen;   // xy = screen size
};

Texture2D    t_atlas : register(t0);
SamplerState s_lin   : register(s0);

struct VSIn  { float2 pos : POSITION; float2 uv : TEXCOORD0; float4 col : COLOR0; };
struct VSOut { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; float4 col : COLOR0; };

VSOut UI_VS(VSIn vin) {
    VSOut o;
    float2 ndc = (vin.pos / g_screen.xy) * float2(2, -2) + float2(-1, 1);
    o.pos = float4(ndc, 0, 1);
    o.uv  = vin.uv;
    o.col = vin.col;
    return o;
}

float4 UI_PS(VSOut pin) : SV_TARGET {
    float4 tex = t_atlas.Sample(s_lin, pin.uv);
    return tex * pin.col;
}
