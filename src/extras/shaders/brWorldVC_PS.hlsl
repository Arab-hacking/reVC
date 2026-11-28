// Optional UV2 lightmap shader, not generic MatFX DUAL/detail textures.
// User ZIP lightmap loading is disabled; this shader alone imports nothing.
// BR scene formula: albedo is blended with 3x lightmap RGB by lightmap alpha.
sampler2D Diffuse : register(s0);
sampler2D Light : register(s1);
float4 fogColor : register(c0);
float4 lm : register(c1);

struct PS_INPUT
{
    float4 Color : COLOR0;
    float3 Tex0 : TEXCOORD0;
    float2 Tex1 : TEXCOORD1;
};

float4 main(PS_INPUT IN) : COLOR
{
    float4 albedo = tex2D(Diffuse, IN.Tex0.xy);
    float4 base = albedo * IN.Color;
    float4 lightmap = tex2D(Light, IN.Tex1);
    float3 lit = lerp(base.rgb,
                      albedo.rgb * lightmap.rgb * 3.0,
                      saturate(lightmap.a * lm.x));
    base.rgb = lerp(fogColor.rgb, lit, IN.Tex0.z);
    base.a *= lm.a;
    return base;
}
