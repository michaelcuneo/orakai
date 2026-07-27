# UDS 9.6 Foliage Wind Implementation (Orakai)

This project now uses the correct architecture:
- UDS/UDW drive wind.
- Foliage movement is material-graph based.
- Cubus C++ does not push wind parameters into foliage anymore.

## What Is Already Implemented In Code

- Removed Cubus runtime wind sync logic from `ACubusWorldVegetationActor`.
- Removed UDS/UDW property polling and material parameter forcing from C++.
- This prevents Cubus from interfering with UDS wind state, wind audio, and weather timing.

## Required Material Implementation (UDS 9.6)

These steps must be done in Unreal material graphs (cannot be reliably automated via C++ text edits).

## A) For Foliage Materials That Need New Wind Motion

1. Open the foliage master material (or a new parent for foliage instances).
2. Add material function: `Foliage Wind Movement`.
3. Add material function: `Sample UDW Wind`.
4. Connect `Sample UDW Wind` outputs to matching wind inputs on `Foliage Wind Movement`.
5. Add `Foliage Wind Movement` output into `World Position Offset`.

Recommended defaults:
- Keep Small/Medium/Large movement enabled initially.
- Expose layer strengths as scalar parameters.
- Expose Apply Small/Medium/Large as static switches if you want per-instance cost control.

## B) Masking The Three Movement Layers

Use vertex color channels for masks. There is no universal standard in third-party assets.

Practical starting mapping:
- Small mask: VertexColor.R
- Medium mask: VertexColor.G
- Large mask: VertexColor.B

Notes:
- Small mask is required.
- Medium/Large can auto-mask from pivot if not supplied, but custom vertex masks usually look better.

## C) For Foliage Assets That Already Have Wind Logic

Prefer replacing their existing wind source inputs rather than stacking another wind function.

1. Keep existing wind deformation logic.
2. Add `Sample UDW Wind`.
3. Replace old wind direction/intensity parameters in the existing graph with `Sample UDW Wind` outputs.

This usually preserves authored look while syncing to UDW weather and gusts.

## D) Pairing With Weather Material Effects (Optional But Recommended)

For leaves/grass/needles:
1. Add `Foliage Weather Effects` near the end of the graph.
2. Enable Apply Wetness and Apply Snow/Dust as needed.

For trunks/opaque bark:
1. Prefer `Surface Weather Effects` instead of foliage weather function.

## E) UDW Settings That Affect Wind Behavior

In Ultra Dynamic Weather:
- Basic Controls -> Wind Direction
- Weather State -> Wind Intensity
- Wind Gusts settings (variation amount/speed)

If using SpeedTree/cloth/physics reactions:
- Wind -> Wind Directional Source section on UDW (UDW can manage this internally)

## F) Validation Checklist

1. In PIE, increase Wind Intensity override on UDW.
2. Confirm weather wind audio responds.
3. Confirm foliage WPO motion responds.
4. Rotate wind direction and confirm directional sway changes.
5. Check close foliage for over-bending and tune layer strengths.

## G) Performance Notes

- Wind WPO on large foliage counts can be expensive.
- Use static switches to disable unused movement layers on heavy assets.
- Keep world-batched non-nanite shadow casting conservative (already defaulted off in project C++ for plant batches).

## H) If Movement Still Looks Dead

1. Ensure the material is actually used by the spawned foliage mesh.
2. Confirm WPO is connected to Material Output.
3. Check Nanite/WPO compatibility for those assets.
4. Verify vertex color masks are non-zero where movement is expected.
5. If existing wind logic exists, do not double-apply; replace old wind source with `Sample UDW Wind`.
