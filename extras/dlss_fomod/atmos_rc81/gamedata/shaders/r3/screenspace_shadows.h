/**
 * @ Version: SCREEN SPACE SHADERS - UPDATE 22
 * @ Description: SSS implementation
 * @ Modified time: 2025-02-07 17:04
 * @ Author: https://www.moddb.com/members/ascii1457
 * @ Mod: https://www.moddb.com/mods/stalker-anomaly/addons/screen-space-shaders
 */

uniform float4 ssfx_sss; // Dir Len, Omni Len, Grass Shadows, -

#ifndef SSFX_PT_LEN_SCALE
	#define SSFX_PT_LEN_SCALE ssfx_sss.y
#endif

#include "screenspace_common.h"
#include "settings_screenspace_SSS.h"
#include "screenspace_mvectors.h"
#include "dlss_contract.h"
#include "atmos_shared.h"

Texture2D s_motion_vectors;

float sss_hit_window(float2 uv, float2 uv_step, float scene_z, float ray_dz, uint iSample)
{
	float3 n   = gbuf_unpack_normal(SSFX_get_position(uv, iSample).xy);
	float2 dxy = scene_z * uv_step * screen_res.xy * pos_decompression_params.zw;
	return 0.04f + abs(ray_dz) + abs(dot(n.xy, dxy)) / max(abs(n.z), 0.25f);
}

float SSFX_ScreenSpaceShadows(float4 P, float3 N, float2 tc, float HudMask, uint iSample)
{
	// Weapon Mask
	bool IsHUD = HudMask > 0;

	float pLen = length(P.xyz);

	// Distance Factor
	float DFactor = saturate((pLen - 80) * 0.01f);

	// Adjust Settings for weapons and scene.
	float ray_detail	= lerp(G_SSS_DETAIL, 0.05f, pLen * 0.02f);
	float ray_thick		= lerp(0.2f, 10, DFactor);
	float ray_len		= lerp(1.0, 20, DFactor);

	ray_len *= saturate(P.z); // Limit the ray_len to avoid issues when surfaces are extremely close

	if (IsHUD)
	{
		ray_detail = 0;
		ray_len = G_SSS_WEAPON_LENGTH;
		ray_thick = 1.0f;
	}

	// DLSS / FSR (Native Upscaling Binaries): SSS keeps little history on the HUD
	// while it moves, so per-frame noise there reaches the upscaler as grain.
	// Hold the HUD's pattern still while the upscaler is on.
	float  ign_t      = (IsHUD && dlss_params.x > 0.5f) ? 0.0f : floor(frac(timers.x * 0.25f) * 240.0f);
	float2 ign_q      = floor(tc * screen_res.xy) + 5.588238f * ign_t;
	float step_jitter = frac(52.9829189f * frac(dot(ign_q, float2(0.06711056f, 0.00583715f))));

	RayTrace sss_ray = SSFX_ray_init(P, -L_sun_dir_e, ray_len * ssfx_sss.x, SSFX_SSS_DIR_QUALITY, step_jitter);

	[branch] if (IsHUD)
	{
		float  hs    = atmos_hud_view_scale();
		float3 E_hud = float3(P.xy * hs, P.z) + (-L_sun_dir_e) * (ray_len * ssfx_sss.x);
		E_hud.xy    /= hs;
		float3 d     = E_hud - P.xyz;
		float  dl    = length(d);
		sss_ray = SSFX_ray_init(P, d / max(dl, 1e-6f), dl, SSFX_SSS_DIR_QUALITY, step_jitter);
	}

	float hud_px = atmos_hud_view_scale() * 0.5f;
	float prev_z = P.z;

	bool  CanopyPass = !IsHUD && abs(P.w * (31.0f / 1.333333f) - 28.0f) < 0.25f;
	float canopy_len = CanopyPass ? ray_len * ssfx_sss.x * dot(-L_sun_dir_e, normalize(mul((float3x3)m_V, float3(0.0f, 1.0f, 0.0f)))) : 0.0f;

	[unroll (SSFX_SSS_DIR_QUALITY)]
	for (int i = 0; i < SSFX_SSS_DIR_QUALITY; i++)
	{
		// Break the march if ray go out of screen...
		if (!SSFX_is_valid_uv(sss_ray.r_pos))
			return 1;

		// Sample current ray pos ( x = difference | y = sample depth | z = current ray len )
		float3 depth_ray = SSFX_ray_intersect(sss_ray, iSample);

		// Check depth difference
		float diff = depth_ray.x;

		if (depth_ray.y <= SKY_EPS)
			return 1;

		// Disable weapons at some point to avoid wrong shadows on the ground
		if (!IsHUD)
			diff *= depth_ray.y >= 1.0f;

		// Negative: Ray is closer to the camera ( not occluded )
		// Positive: Ray is beyond the depth sample ( occluded )
		float ray_z  = depth_ray.x + depth_ray.y;
		float detail = ray_detail;

		bool canopy_own = CanopyPass && abs(gbuf_unpack_mtl(SSFX_get_position(sss_ray.r_pos, iSample).w) * (31.0f / 1.333333f) - 28.0f) < 0.25f
		               && depth_ray.z / max(sss_ray.r_length, 1e-6f) * ray_z / max(sss_ray.z_end, 1e-4f) * canopy_len < 0.125f;
		[branch] if (IsHUD)
		{
			float3 n  = gbuf_unpack_normal(SSFX_get_position(sss_ray.r_pos, iSample).xy);
			float2 px = depth_ray.y * hud_px * abs(pos_decompression_params.zw);
			detail = (abs(n.x) * px.x + abs(n.y) * px.y) / max(abs(n.z), 0.25f) + depth_ray.y * 0.001f;
		}
		if (diff > detail && !canopy_own)
		{
			float thick = ray_thick;
			if (!IsHUD)
				thick = lerp(min(ray_thick, sss_hit_window(sss_ray.r_pos, sss_ray.r_step, depth_ray.y, ray_z - prev_z, iSample)), ray_thick, DFactor);
			if (diff < thick)
				return saturate(1.0f - G_SSS_INTENSITY);
		}
		prev_z = ray_z;

		// Step the ray
		sss_ray.r_pos += sss_ray.r_step;
	}

	// None
	return 1;
}

float SSFX_ScreenSpaceShadows_Point(float4 P, float2 tc, float3 LightPos, float HudMask, uint iSample)
{
	// Light vector
	float3 L_dir = -normalize(P.xyz - LightPos);

	// Material conditions...
	bool mat_flora = abs(P.w - MAT_FLORA_SENTINEL) <= MAT_FLORA_SENTINEL_E;

	// Weapon Mask
	bool IsHUD = HudMask > 0;

	// Adjust Settings for weapons and scene.
	float ray_detail	= G_SSS_DETAIL;
	float ray_thick		= 0.2f;
	float ray_len		= 0.5f;

	// Grass overwrite.
	if (IsHUD)
	{
		ray_thick = 1.0f;
		ray_len = 0.06f;
	}
	else
	{
		if (mat_flora)
		{
			ray_thick = 0.5f;
			ray_len = 0.5f;
		}
	}

	float step_jitter = hash12(tc * 100 + timers.xx * 100);

	RayTrace sss_ray = SSFX_ray_init(P, L_dir, min(ray_len * SSFX_PT_LEN_SCALE, length(LightPos - P.xyz)), SSFX_SSS_OMNI_QUALITY, step_jitter);

	float2 taa_uv = ssfx_jitter.xy * float2(0.5f, -0.5f);

	[branch] if (IsHUD)
	{
		float  hs     = atmos_hud_view_scale();
		float3 P_true = float3(P.xy * hs, P.z);
		float3 E_true = P_true + normalize(LightPos - P_true) * min(ray_len * SSFX_PT_LEN_SCALE, length(LightPos - P_true));
		float3 d      = float3(E_true.xy / hs, E_true.z) - P.xyz;
		float  dl     = length(d);
		sss_ray = SSFX_ray_init(P.xyz, d / max(dl, 1e-6f), dl, SSFX_SSS_OMNI_QUALITY, step_jitter);
	}

	float prev_z = P.z;

	[unroll (SSFX_SSS_OMNI_QUALITY)]
	for (int i = 0; i < SSFX_SSS_OMNI_QUALITY; i++)
	{
		// Break the march if ray go out of screen...
		if (!SSFX_is_valid_uv(sss_ray.r_pos))
			return 1;

		// Sample current ray pos ( x = difference | y = sample depth | z = current ray len )
		RayTrace drawn = sss_ray;
		drawn.r_start += taa_uv;
		drawn.r_pos += taa_uv;
		float3 depth_ray = SSFX_ray_intersect(drawn, iSample);

		// Check depth difference
		float diff = depth_ray.x;

		// No Sky
		if (depth_ray.y <= SKY_EPS)
			return 1;

		// Disable weapons at some point to avoid wrong shadows on the ground
		if (!IsHUD)
			diff *= depth_ray.y >= 1.0f;

		// Negative: Ray is closer to the camera ( not occluded )
		// Positive: Ray is beyond the depth sample ( occluded )
		float ray_z = depth_ray.x + depth_ray.y;
		if (diff > ray_detail)
		{
			float thick = IsHUD ? ray_thick : min(ray_thick, sss_hit_window(drawn.r_pos, sss_ray.r_step, depth_ray.y, ray_z - prev_z, iSample));
			if (diff < thick)
				return 1.0f - saturate(G_SSS_INTENSITY - smoothstep( SSFX_SSS_OMNI_QUALITY * 0.5f, SSFX_SSS_OMNI_QUALITY + 1, i));
		}
		prev_z = ray_z;

		// Step the ray
		sss_ray.r_pos += sss_ray.r_step;
	}

	return 1;
}

float atmos_sss_point_hud(float4 P, float2 tc, float4 Light, float MaxLen, uint iSample)
{
	float  hs       = atmos_hud_view_scale();
	float3 P_true   = float3(P.xy * hs, P.z);
	float3 LightPos = float3(atmos_hud_light(Light.z, rsqrt(max(Light.w, 1e-8f))) ? Light.xy * hs : Light.xy, Light.z);
	float3 E_true   = P_true + normalize(LightPos - P_true) * min(MaxLen, max(length(LightPos - P_true) - 0.005f, 0.0f));
	float3 d        = float3(E_true.xy / hs, E_true.z) - P.xyz;
	float  dl       = length(d);
	// No history here: lit directly every frame. While the upscaler is on, use a
	// still interleaved gradient instead of per-frame white noise.
	float  pt_noise = dlss_params.x > 0.5f
		? frac(52.9829189f * frac(dot(floor(tc * screen_res.xy), float2(0.06711056f, 0.00583715f))))
		: hash12(tc * 100 + timers.xx * 100);
	RayTrace sss_ray = SSFX_ray_init(P.xyz, d / max(dl, 1e-6f), dl, SSFX_SSS_OMNI_QUALITY, pt_noise);
	float2 taa_uv = ssfx_jitter.xy * float2(0.5f, -0.5f);

	[unroll (SSFX_SSS_OMNI_QUALITY)]
	for (int i = 0; i < SSFX_SSS_OMNI_QUALITY; i++)
	{
		if (!SSFX_is_valid_uv(sss_ray.r_pos))
			return 1;
		RayTrace drawn = sss_ray;
		drawn.r_start += taa_uv;
		drawn.r_pos += taa_uv;
		float3 depth_ray = SSFX_ray_intersect(drawn, iSample);
		if (depth_ray.y <= SKY_EPS)
			return 1;
		if (depth_ray.x > G_SSS_DETAIL && depth_ray.x < 1.0f)
			return 1.0f - saturate(G_SSS_INTENSITY - smoothstep(SSFX_SSS_OMNI_QUALITY * 0.5f, SSFX_SSS_OMNI_QUALITY + 1, i));
		sss_ray.r_pos += sss_ray.r_step;
	}
	return 1;
}
