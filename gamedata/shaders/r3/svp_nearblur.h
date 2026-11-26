// near-field defocus of the SVP scope image: depth and thin-lens circle of confusion, shared by the
// gather (svp_nearblur.ps) and its tile passes (svp_nearblur_tile.ps, svp_nearblur_reach.ps)
#ifndef SVP_NEARBLUR_H
#define SVP_NEARBLUR_H
#include "common.h"

Texture2D s_nb_pos;

uniform float4 svp_nearblur_params; // x focus m, y thin-lens k px*m, z max radius px, w muzzle distance m
uniform float4 svp_nearblur_tiles; // x tile px, yz image px, w tiles a near-field disc can reach

float nb_far(float z)
{
	return (z < 0.02f) ? 10000.0f : z; // unwritten gbuffer (sky) counts as far
}

float nb_depth(float2 uv)
{
	return nb_far(s_nb_pos.SampleLevel(smp_nofilter, uv, 0).z);
}

float nb_size(float z) // near-field CoC radius in px from the objective aperture, fades out by 3m; falls with z
{
	float coc = svp_nearblur_params.y * max(1.0f / max(z, 0.05f) - 1.0f / svp_nearblur_params.x, 0.0f);
	// the near field ends at the weapon's own muzzle, fading over one octave past it
	coc *= smoothstep(svp_nearblur_params.w * 2.0f, svp_nearblur_params.w, z);
	return min(coc, svp_nearblur_params.z);
}
#endif
