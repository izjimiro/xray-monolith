#include "stdafx.h"
#include "../xrRender/FBasicVisual.h" // pip dxRender_Visual (GetTexture/Render) for draw_scope
#include "../xrRender/SkeletonX.h" // pip lens bone latch compensation for the skinned lens draws
#include "../../xrEngine/igame_persistent.h" // pip env-driven eye pupil for the exit-pupil twilight dimming
#include "../../xrEngine/environment.h"
#if defined(USE_DX11)
#include "../../../gamedata/shaders/r3/scope_defines.h" // SCOPE_PHASE_* (kept in sync with the shader)
#include "svp_optics.h" // pip svp_optic_eye_coupled, the data driven eyepiece gate
#endif

#if defined(USE_DX11)

// per-scope objective diameter in mm, resolved by the optics bus (spec cvar then authored w), 0 = none
static float svp_objective_mm()
{
	return Device.m_SecondViewport.svp_opt_obj_mm;
}

void CRenderTarget::svp_nearblur_mips(u32 width, u32 height, DXGI_FORMAT format, ID3DTexture2D*& surf, ref_texture& tex,
	LPCSTR name)
{
	D3D_TEXTURE2D_DESC md = {};
	md.Width = width;
	md.Height = height;
	md.ArraySize = 1;
	md.Format = format;
	md.SampleDesc.Count = 1;
	md.Usage = D3D_USAGE_DEFAULT;
	md.BindFlags = D3D_BIND_SHADER_RESOURCE | D3D_BIND_RENDER_TARGET;
	md.MiscFlags = D3D_RESOURCE_MISC_GENERATE_MIPS; // MipLevels 0 = the full chain
	if (FAILED(HW.pDevice->CreateTexture2D(&md, 0, &surf)))
	{
		surf = nullptr;
		Msg("! [SVP-NB] no video memory for the %s mip chain, near blur goes without the prefilter", name);
		return;
	}
	tex.create(name);
	tex->surface_set(surf);
	surf->Release(); // the texture holds it
}

static constexpr u32 NB_TILE = 16; // near-blur tile side in px, svp_nearblur_tiles.x for the shaders

// pip near-field defocus pass, true when it dispatched else the caller copies
bool CRenderTarget::svp_nearblur_pass(ID3DTexture2D* source, ID3DTexture2D* mips, ref_texture& mips_tex,
	ID3DRenderTargetView* destination)
{
	// pip near-field defocus replaces the plain copy in the realism mode, an uncoupled display
	// and a live thermal sensor keep full depth of field so they take the plain copy
	if (ps_r__svp_near_blur > 0.01f && scope_svp_enabled >= 2 && svp_optic_eye_coupled()
		&& !svp_thermal_overlay_active()
		&& rt_Position && destination)
	{
		EnsureScopeShaders();
		// per tile, the largest CoC of the near field that can reach it, so the gather skips the rest
		const u32 gw = (Width + NB_TILE - 1) / NB_TILE, gh = (Height + NB_TILE - 1) / NB_TILE;
		auto tiles = [gw, gh](ref_rt& rt, LPCSTR name)
		{
			if (!rt) // the target's size is fixed, a new size comes with a new SVP target (svp_target.cpp)
				rt.create(name, gw, gh, D3DFMT_R32F);
			return rt && rt->pRT;
		};
		if (s_svp_nearblur && s_svp_nearblur_tile && s_svp_nearblur_reach && tiles(rt_svp_nb_tile, "$user$svp_nb_tile")
			&& tiles(rt_svp_nb_reach_x, "$user$svp_nb_reach_x") && tiles(rt_svp_nb_reach, "$user$svp_nb_reach"))
		{
			// pip prefilter the svp color into a mip chain so each sparse ring area-averages its cell
			if (mips)
			{
				HW.pContext->CopySubresourceRegion(mips, 0, 0, 0, 0, source, 0, nullptr);
				HW.pContext->GenerateMips(mips_tex->get_SRView());
			}
			{
				ref_texture t; t.create("$user$svp_nearblur_src");
				t->surface_set(mips ? mips : source);
			}
			{
				ref_texture t; t.create("$user$svp_nearblur_pos");
				t->surface_set(rt_Position->pSurface);
			}
			const float w = float(Width), h = float(Height);
			// near-field defocus by the real thin-lens CoC of the objective aperture
			// y = k in px*m (CoC px = k * (1/z - 1/focus))
			const float omm = svp_objective_mm();
			const float A = (omm > 0.01f) ? omm * 0.001f : 0.024f; // typical 24mm when no per-scope data
			const float vfov = (Device.m_SecondViewport.svp_fov > 0.01f) ? Device.m_SecondViewport.svp_fov : 0.35f;
			const float k = A * h / vfov * _min(ps_r__svp_near_blur, 3.f);
			const float focus = _max(ps_r__svp_focus_m, 1.f);
			// the near field ends at the real muzzle, per weapon, clamped to the old design bound
			float md = 1.5f;
			CSecondVPParams::WeaponPoseSnapshot pose;
			if (Device.m_SecondViewport.ReadWeaponPose(pose)
				&& Device.m_SecondViewport.SnapshotExact(pose.frame, pose.session, Device.dwFrame)
				&& pose.muzzle_pos.square_magnitude() > EPS)
				md = clampr(pose.muzzle_pos.distance_to(Device.vCameraPosition), 0.5f, 3.f);
			// max radius is the physical coc at the svp near plane capped at the inscribed disc
			const float z_near = Device.m_SecondViewport.svp_near;
			const float coc_near = k * _max(1.f / _max(z_near, 0.05f) - 1.f / focus, 0.f);
			const float maxR = _min(coc_near, 0.5f * _min(w, h));
			// the tiles a disc of maxR (+1 px of point sampling) can reach past its own
			const float reach_tiles = ceilf((maxR + 1.f) / NB_TILE) + 1.f;
			auto pass = [&](ID3DRenderTargetView* target, u32 tw, u32 th, ref_selement& element, float axis_x, float axis_y)
			{
				u_setrt(tw, th, target, nullptr, nullptr, nullptr);
				set_viewport_size(HW.pContext, float(tw), float(th));
				const float x = float(tw), y = float(th);
				u32 Offset = 0;
				const u32 C = color_rgba(255, 255, 255, 255);
				FVF::TL* pv = (FVF::TL*)RCache.Vertex.Lock(3, g_combine->vb_stride, Offset);
				pv->set(0, y * 2, EPS_S, 1.f, C, 0.f, 2.f); pv++;
				pv->set(0, 0, EPS_S, 1.f, C, 0.f, 0.f); pv++;
				pv->set(x * 2, 0, EPS_S, 1.f, C, 2.f, 0.f); pv++;
				RCache.Vertex.Unlock(3, g_combine->vb_stride);
				RCache.set_Geometry(g_combine);
				RCache.InvalidateTextures(); // the tile targets turn from target to source between passes
				RCache.set_Element(element);
				// set_c is stateful, screen_res must be this pass's dims for the SV_Position uv
				RCache.set_c("screen_res", x, y, 1.f / x, 1.f / y);
				RCache.set_c("svp_nearblur_params", focus, k, maxR, md);
				RCache.set_c("svp_nearblur_tiles", float(NB_TILE), w, h, reach_tiles);
				RCache.set_c("svp_nearblur_axis", axis_x, axis_y, 0.f, 0.f);
				RCache.Render(D3DPT_TRIANGLELIST, Offset, 0, 3, 0, 1);
			};
			auto reach_source = [](const ref_rt& rt)
			{
				ref_texture t; t.create("$user$svp_nb_reach_src");
				t->surface_set(rt->pSurface);
			};
			pass(rt_svp_nb_tile->pRT, gw, gh, s_svp_nearblur_tile->E[1], 0.f, 0.f);
			reach_source(rt_svp_nb_tile);
			pass(rt_svp_nb_reach_x->pRT, gw, gh, s_svp_nearblur_reach->E[1], 1.f, 0.f);
			reach_source(rt_svp_nb_reach_x);
			pass(rt_svp_nb_reach->pRT, gw, gh, s_svp_nearblur_reach->E[1], 0.f, 1.f);
			pass(destination, Width, Height, s_svp_nearblur->E[1], 0.f, 0.f);
			// pip one-shot proof that the pass ran, throttled while r__svp_diag is on
			static u32 s_nb_ms = 0;
			if (ps_r__svp_diag && Device.dwTimeGlobal - s_nb_ms > 1000)
			{
				s_nb_ms = Device.dwTimeGlobal;
				const float g_dbg = powf(_max(maxR, 1.f), 1.f / 150.f) - 1.f; // 150 mirrors the shader NB_TAP_BUDGET
				u32 levels = 0;
				if (mips) { D3D_TEXTURE2D_DESC d; mips->GetDesc(&d); levels = d.MipLevels; }
				PipMsg("[SVP-NB] maxR=%.0fpx g=%.4f lodk=%.4f scatter=%d mips=%u tiles=%ux%u", maxR, g_dbg, g_dbg,
					ps_r__svp_nearblur_scatter, levels, gw, gh);
			}
			return true;
		}
	}
	return false;
}
#endif
