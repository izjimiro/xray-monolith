# Upstream merge of 2026-11-09 and the upscaler

Notes from merging `upstream/all-in-one-vs2022-wpo-mt` (tag `2026.11.9`, 152
commits, 278 files) on branch `merge-upstream-2026-11-09`. The merge brought in
upstream's scripted attachments, 3D inventory icons, arm IK and camera-attached
FOV work.

## Conflicts

Three files conflicted, each with both sides adding code at the same spot.
All were resolved by keeping both sides.

- `r__dsgraph_render.cpp`: our motion history helpers and upstream's
  `CCustomFOVInitializer`.
- `r__dsgraph_types.h`: our after-upscale lists and upstream's camera-attached
  scope lists.
- `r4_rendertarget.h`: our `phase_combine_volumetric(bool high = true)` and
  upstream's `phase_ui_3d_icons_postprocess()`.

## What did not need changes

- No upstream shader touches a DLSS shader. The only new ones are `ui_icons*`.
- Jitter is a vertex shader constant, so upstream's new projection code keeps it.
- 3D icons draw during the UI, after `dlss::EndScene`. Both the direct path and
  the optional atlas (`r4_atlas 1`) therefore see display resolution, native
  depth and zero jitter.
- Upstream moved camera-attached sorted geometry into
  `r_dsgraph_render_sorted_hud`. `render_forward` already calls that, so it
  still draws.

## Changes made for the upscaler

### Script attachment transforms

Upstream's `script_attachment::RenderAt` now submits a transform from its stack,
and `r_dsgraph_insert_dynamic` copied it into `owned_transforms`. That caused two
problems:

- `set_dynamic_world_prev` tracks previous transforms by address. A new address
  every frame looked like a new object, so rigid attachments reported no motion
  of their own. Skinned ones use `Matrix_Prev` and were not affected.
- `owned_transforms` was only cleared on level unload, so it grew every frame.

`CDSGraphManager::store_instance_transform` replaces that for attachments. It
keeps one slot per owner, visual and submission order, so an attachment's
transform stays at one address while it keeps drawing. The passes after the
upscaler can also still read it. Slots unused for 600 frames are freed.
`owned_transforms` now only serves 3D icons, and is freed once they are drawn.

If an attachment changes how many instances of one visual it draws, the slots
shift and those instances get wrong motion for one frame.

### Custom FOV attachments

`set_custom_fov` is a script call and is off by default. Upstream's
`CCustomFOVInitializer` swapped only the current projection, so a still object
reported the difference between the two FOVs as motion. It now swaps the previous
projection too, in DX11.

These attachments also go into `mapCustomFOVSorted`, which our passes did not
read. Lasers in it now move to `mapDlssEffects`, and reticles are copied to
`mapFgReticles`. The reticle pass draws custom FOV items without the HUD setup,
matching how the scene drew them.

### Camera-attached scopes

Scope lenses on camera-attached objects go into upstream's new
`mapScopeCamAttachedSorted`.

- The after-upscale scope pass only ran when `mapScopeHUDSorted` had entries, so
  a camera-attached lens alone never drew and its list kept growing. The pass now
  runs for either list.
- `r_dsgraph_render_fg_mask` now also draws reticles from the camera-attached list.

## Release packaging

`CRenderTarget` always loads the `ui_icons` shader, but `package_release.ps1` and
`package_compatibility.py` only ship `extras/dlss_gamma/gamedata`. These files are
copied there from `gamedata/shaders/r3`, and need refreshing if upstream changes
them:

- `ui_icons.s`, `ui_icons.vs`, `ui_icons.ps`
- `ui_icons_pp.s`, `ui_icons_pp.ps`

## Worth testing in game

- Weapon attachments that move with the hands, looking for ghosting and frame
  generation afterimages.
- 3D inventory icons, with `r4_atlas` at 0 and 1.
- A camera-attached scope, if a mod provides one.

# Upstream merge of 2026-11-22

Branch `merge-upstream-2026-11-22`: our `all-in-one-vs2022-wpo-mt` (0.72 +
compatibility patch update), then `merge-upstream-2026-11-09` above, then
`upstream/all-in-one-vs2022-wpo-mt` at tag `2026.11.22` (129 commits since
`2026.11.9`, 154 files). Both merges were clean, with no conflicts. DX11 and
DX12 (`/t:xrEngine`) build with no errors and no warnings in upscaler code.

## Still needed from the 11-09 merge

All of it. Upstream's attachment commits only fix leaks (motion end and bone
callbacks, physics contact functor) and add a `set_scale(x, y, z)` overload.
`RenderAt`, `r_dsgraph_insert_dynamic`, `CCustomFOVInitializer` and the
camera-attached scope lists are unchanged, so the stable instance transforms,
the custom FOV previous projection and the camera-attached scope pass still
apply. No upstream shader changed, so the `ui_icons*` copies in
`extras/dlss_gamma` are current.

## New upstream changes checked

- Texture eviction (`28c76573`): adds `bNoEvict` for env cubes and colour
  maps. Eviction already skips `$user$` and `bUser` textures, so our render
  targets are unaffected.
- `seqFrameMT` now runs before the parallel Lua GC (`3470feeb`). Only sound
  and network register there; nothing of ours does.
- Binocular vision is now a UI class, and its target frames are limited to the
  shader-based 2D scope circle. It works from projected positions in the UI
  pass, after `dlss::EndScene`, so it sees display resolution and no jitter.
- `lock_to_camera` for script hand motions blends the arms toward the camera
  each frame. HUD motion vectors still come from the previous transforms.
- `$level$` is no longer re-pointed for an unchanged level (`e280ff81`), file
  system locking (`ee3bab13`), weapon end-of-animation timing, and the rest are
  outside the renderer.

The reflex reticle change was ported by hand onto this branch: reticles are
held from `mapHUDSorted` and `mapCustomFOVSorted`, and the laser mask pass
matches both prefixes.

## Not done

- Not run in game.
