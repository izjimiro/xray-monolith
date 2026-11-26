-- frame generation's motion over the PiP lens (svp_fg_motion.cpp, DX12 executable)
function normal(shader, t_base, t_second, t_detail)
	shader:begin("stub_notransform_t", "svp_fg_lens_motion")
	: zb(false, false)
	shader:dx10texture("s_fg_scene_motion", "$user$ssfx_motion_vectors")
	shader:dx10texture("s_fg_capture_tc", "$user$svp_reference_capture_tc")
	shader:dx10texture("s_fg_svp_motion", "$user$ssfx_motion_vectors$svp")
	shader:dx10sampler("smp_nofilter")
end
