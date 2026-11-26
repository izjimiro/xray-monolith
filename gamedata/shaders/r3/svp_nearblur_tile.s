function normal(shader, t_base, t_second, t_detail)
	shader:begin("stub_notransform_t", "svp_nearblur_tile")
	: zb(false, false)
	shader:dx10texture("s_nb_pos", "$user$svp_nearblur_pos")
end
