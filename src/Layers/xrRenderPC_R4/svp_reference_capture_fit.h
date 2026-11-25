#pragma once
#include <cmath>
namespace SvpReferenceCapture
{
// Screen height divided by the on-screen height of the round aperture. The lens
// is drawn with the HUD projection, so its _22 term is used. Returns 1 when
// the lens is not drawn this frame or the inputs are unusable.
inline float CurrentFit(float radius, float depth, float hudProjection22, bool fresh)
{
    if (!fresh || !std::isfinite(radius) || !std::isfinite(depth)
        || !std::isfinite(hudProjection22) || radius <= 0.00001f
        || depth <= 0.00001f || hudProjection22 <= 0.f)
        return 1.f;
    const float ndcHeight = 2.f * radius * hudProjection22 / depth;
    if (!std::isfinite(ndcHeight) || ndcHeight <= 0.0001f)
        return 1.f;
    return 2.f / ndcHeight;
}
// The lens matrix carries the scope model's scale (a MAS scope's root bone takes the weapon's
// attach_scale, player_hud.cpp:743-745) while the lens radius is the visual's own. Returns that
// uniform scale, so radius * scale is the world radius, or 0 when the basis is not a uniform scale.
inline float LensScale(const Fmatrix& lens)
{
    const float s = lens.i.magnitude();
    if (!(s > 0.00001f) || std::abs(lens.j.magnitude() - s) > 0.001f * s || std::abs(lens.k.magnitude() - s) > 0.001f * s)
        return 0.f;
    const float ss = s * s;
    if (std::abs(lens.i.dotproduct(lens.j)) > 0.001f * ss || std::abs(lens.i.dotproduct(lens.k)) > 0.001f * ss
        || std::abs(lens.j.dotproduct(lens.k)) > 0.001f * ss)
        return 0.f;
    return s;
}
}
