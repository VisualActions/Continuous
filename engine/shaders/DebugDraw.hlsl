cbuffer CBDebug : register(b0) {
    float4x4 g_view_proj;
};

struct VSIn {
    float3 pos : POSITION;
    float4 col : COLOR0;
};

struct VSOut {
    float4 pos_cs : SV_POSITION;
    float4 col    : COLOR0;
};

VSOut DebugVS(VSIn vin) {
    VSOut o;
    o.pos_cs = mul(float4(vin.pos, 1.0), g_view_proj);
    o.col = vin.col;
    return o;
}

float4 DebugPS(VSOut pin) : SV_TARGET {
    return pin.col;
}
