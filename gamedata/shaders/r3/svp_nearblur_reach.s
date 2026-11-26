function normal(shader, t_base, t_second, t_detail)
	shader:begin("stub_notransform_t", "svp_nearblur_reach")
	: zb(false, false)
	shader:dx10texture("s_nb_reach_src", "$user$svp_nb_reach_src")
end
