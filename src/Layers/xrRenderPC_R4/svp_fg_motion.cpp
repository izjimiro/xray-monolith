#include "stdafx.h"
#include "svp_optics.h"
#ifdef XR_DX12_BRIDGE
#include "Dlss/FrameGeneration.h"
#include "../xrRenderDX10/D3D12PresentBridge.h"
#endif

extern int ps_r__svp_fg_lens_mv;

// pip frame generation moves the lens pixels by the main view's vectors, which follow the glass, not the PiP
// image it shows; the Scope Effects image pass then also writes the capture point of each lens pixel
bool svp_fg_lens_motion_wanted()
{
#ifdef XR_DX12_BRIDGE
	// frame generation also needs an upscaler (fg::Eligible); without one the pass is wasted
	auto* bridge = HW.PresentationBridge();
	return ps_r__svp_fg_lens_mv && dlss::Configured() && bridge && bridge->NeedsFGInputs();
#else
	return false;
#endif
}

#ifdef XR_DX12_BRIDGE
// The scene's render-size vectors with the image's own motion over the lens (svp_fg_lens_motion.ps), handed
// to frame generation in place of the captured ones; the upscaler keeps the scene's.
void CRenderTarget::svp_fg_lens_motion()
{
	if (this != RImplementation.TargetMain || svp_fg_capture_frame != Device.dwFrame || !RImplementation.TargetSVP
		|| !rt_scope_reference_capture_tc || !rt_ssfx_motion_vectors || !svp_fg_lens_motion_wanted())
		return;
	const u32 w = rt_ssfx_motion_vectors->dwWidth, h = rt_ssfx_motion_vectors->dwHeight;
	if (rt_fg_lens_motion && (rt_fg_lens_motion->dwWidth != w || rt_fg_lens_motion->dwHeight != h))
		rt_fg_lens_motion.destroy();
	if (!rt_fg_lens_motion)
		rt_fg_lens_motion.create("$user$fg_lens_motion", w, h, D3DFMT_A16B16G16R16F);
	if (!s_svp_fg_lens_motion)
		s_svp_fg_lens_motion.create("svp_fg_lens_motion");
	if (!rt_fg_lens_motion || !rt_fg_lens_motion->pRT || !s_svp_fg_lens_motion)
		return;

	// the 2D HUD draws next onto the target combine left bound, and the next frame's LOD reads its size
	const u32 target_w = dwWidth, target_h = dwHeight;
	ID3DRenderTargetView* targets[3]{RCache.get_RT(0), RCache.get_RT(1), RCache.get_RT(2)};
	ID3DDepthStencilView* depth = RCache.get_ZB();
	D3D11_VIEWPORT viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
	UINT viewport_count = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
	HW.pContext->RSGetViewports(&viewport_count, viewports);

	u_setrt(w, h, rt_fg_lens_motion->pRT, nullptr, nullptr, nullptr);
	set_viewport_size(HW.pContext, float(w), float(h));
	RCache.set_CullMode(CULL_NONE);
	RCache.set_Stencil(FALSE);
	RCache.set_ColorWriteEnable();
	const float x = float(w), y = float(h);
	u32 offset = 0;
	FVF::TL* pv = (FVF::TL*)RCache.Vertex.Lock(3, g_combine->vb_stride, offset);
	pv->set(0, y * 2, EPS_S, 1.f, 0xffffffff, 0.f, 2.f); pv++;
	pv->set(0, 0, EPS_S, 1.f, 0xffffffff, 0.f, 0.f); pv++;
	pv->set(x * 2, 0, EPS_S, 1.f, 0xffffffff, 2.f, 0.f); pv++;
	RCache.Vertex.Unlock(3, g_combine->vb_stride);
	RCache.set_Geometry(g_combine);
	RCache.set_Element(s_svp_fg_lens_motion->E[1]);
	RCache.set_c("screen_res", x, y, 1.f / x, 1.f / y);
	const auto& capture = rt_scope_reference_capture_tc;
	RCache.set_c("svp_fg_lens", 1.f / float(capture->dwWidth), 1.f / float(capture->dwHeight),
		scope_glass_deferred_current() ? 1.f : 0.f, 0.f);
	RCache.Render(D3DPT_TRIANGLELIST, offset, 0, 3, 0, 1);

	for (int i = 0; i < 3; ++i)
		RCache.set_RT(targets[i], i);
	RCache.set_ZB(depth);
	HW.pContext->RSSetViewports(viewport_count, viewports);
	dwWidth = target_w;
	dwHeight = target_h;
	fg::CaptureLensMotion(rt_fg_lens_motion->pSurface);
}
#endif
