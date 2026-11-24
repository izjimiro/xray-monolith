#ifndef ATMOS_HUD_MTL_H
#define ATMOS_HUD_MTL_H

#include "atmos_shared.h"
#include "dlss_contract.h"

#define ATMOS_HUD_MS_ENCODED 0.064516f
#define ATMOS_HUD_MS_PBR     0.021505f
#define ATMOS_FLORA_MS_PBR   1.225806f
#define ATMOS_WORLD_MS_PBR   1.268817f
#define ATMOS_TERRAIN_MS_PBR 1.311828f

#ifdef ATMOS_FORCE_PBR
bool  atmos_hud_pbr()        { return true; }
#else
bool  atmos_hud_pbr()        { return xmaterial * 4.0f - 0.5f > 4.25f; }
#endif
bool  atmos_mat_sss()        { return xmaterial * 4.0f - 0.5f > 4.75f; }
bool  atmos_terrain_pbr()    { return xmaterial * 4.0f - 0.5f > 4.25f; }

float atmos_pbr_wet_gloss(float gloss_dry, float wet)
{
    float r = 1.0f - saturate(gloss_dry);
    return 1.0f - lerp(r, min(r, 0.15f), saturate(wet));
}

float atmos_flora_wet_rough(float r)
{
    return lerp(r, min(r, FLORA_ROUGH_WET), saturate(rain_params.y));
}

float atmos_grass_wet_rough(float r)
{
    return lerp(r, min(r, 0.4f), saturate(rain_params.y));
}

float atmos_lsb2(float c, uint bits)
{
    float x  = saturate(c) * 255.0f;
    int   lo = (((int)floor(x)) & ~3) | (int)bits;
    if ((float)lo > x)
        lo -= 4;
    int   k  = ((x - (float)lo) * 0.25f > 0.5f) ? lo + 4 : lo;
    if (k < 0)   k += 4;
    if (k > 255) k -= 4;
    return (float)k / 255.0f;
}

void atmos_hud_hs_tag(inout float gloss, float2 pos2d)
{
    float hs = 1.0f / (m_P[1][1] * abs(pos_decompression_params.y));
    uint  c  = (uint)round(saturate((log2(hs) - ATMOS_HS_LOG_LO) / (ATMOS_HS_LOG_HI - ATMOS_HS_LOG_LO)) * 4095.0f);
    int2  p  = (int2)floor(pos2d);
    gloss = atmos_lsb2(gloss, (c >> (2u * ((uint)(p.x % 3) + 3u * (uint)(p.y & 1)))) & 3u);
}

void atmos_encode_code(inout float3 base, uint code)
{
    base.r = atmos_lsb2(base.r,  code       & 3u);
    base.g = atmos_lsb2(base.g, (code >> 2) & 3u);
    base.b = atmos_lsb2(base.b, (code >> 4) & 3u);
}

uint atmos_flora_code(float sss, bool grass)
{
    return (grass ? 32u : 0u) | (uint)round(saturate(sss) * 31.0f);
}

float2 atmos_px(float3 P)
{
    float4 c = mul(m_P, float4(P, 1.0f));
    return (c.xy / c.w * float2(0.5f, -0.5f) + 0.5f) * screen_res.xy;
}

float atmos_code_noise(float2 pix)
{
    float2 q = floor(pix) + 5.588238f * floor(frac(timers.x * 0.25f) * 240.0f);
    return frac(52.9829189f * frac(dot(q, float2(0.06711056f, 0.00583715f))));
}

// DLSS / FSR (Native Upscaling Binaries): a material code dithered with noise
// that changes every frame reaches the upscaler as noise it cannot tell from
// detail, and shows as grain on hands, weapons and foliage. Round the codes
// while the upscaler is on. Blood decals keep atmos_code_noise.
bool atmos_code_upscaled()
{
    return dlss_params.x > 0.5f;
}

float atmos_code_dither(float2 pix)
{
    return atmos_code_upscaled() ? 0.5f : atmos_code_noise(pix);
}

void atmos_grass_ao_gloss(inout float gloss, float ao, float2 pix)
{
    gloss = atmos_lsb2(gloss, (uint)min(floor(saturate(ao) * 3.0f + atmos_code_dither(pix)), 3.0f));
}

float atmos_cloth_w(float a)
{
    return saturate((0.5f - a) * 4.0f);
}

uint atmos_pbr_code_make(float metal, float mask, float transl, float cloth, float2 pix)
{
    [branch]
    if (mask >= 0.5f && mask >= metal)
    {
        return 32u | (uint)min(floor(saturate(transl) * 31.0f + atmos_code_dither(pix)), 31.0f);
    }
    float cloth_cut = atmos_code_upscaled() ? 0.5f : frac(atmos_code_noise(pix) + 0.618034f);
    if (metal < 0.5f && cloth > cloth_cut)
        return 31u;
    return (uint)min(floor(saturate(metal) * 30.0f + atmos_code_dither(pix)), 30.0f);
}

void atmos_hud_encode(inout float3 base, inout float ms)
{
    float t = clamp(xmaterial * 4.0f - 0.5f, 0.0f, 3.99f);
    int   r = (int)floor(ms / 1.333333f * 31.0f) - 28;
    if (r == 1 && t >= 2.0f)
        t = 1.0f;
    atmos_encode_code(base, (uint)round(t * 0.25f * 63.0f));
    ms = ATMOS_HUD_MS_ENCODED;
}

void atmos_hud_encode_pbr(inout float3 base, float2 pix, float metal, float mask, float transl, float cloth, inout float ms)
{
    atmos_encode_code(base, atmos_pbr_code_make(metal, mask, transl, cloth, pix));
    ms = ATMOS_HUD_MS_PBR;
}

void atmos_world_encode_pbr(inout float3 base, float2 pix, float metal, float mask, float transl, float cloth, inout float ms)
{
    atmos_encode_code(base, atmos_pbr_code_make(metal, mask, transl, cloth, pix));
    ms = ATMOS_WORLD_MS_PBR;
}

#endif
