# Cubus weather-responsive materials

Cubus does not create or simulate weather. `ACubusBlockWorldActor` samples the
existing Ultra Dynamic Weather actor and its material parameter collection, then
converts the published rain or surface-wetness state into one accumulated Cubus
wetness value.

That value updates the shared block and density material instances. It does not
rebuild terrain, change chunk data, or interfere with the separate vegetation
wind bridge.

At full wetness, terrain uses the world actor's `Weather Wet Darkening` and
`Weather Wet Roughness` values. Rain increases wetness using `Weather Wetting
Rate`; dry weather reduces it using `Weather Drying Rate`.

## One-time material build

After compiling the code, open `/Game/Cubus/DataAssets/OrakaiMaterialLibrary`
and run **Build Weather Responsive Materials**. This regenerates
`M_CubusBlockPBR` and `M_CubusDensityPBR` with the weather inputs while retaining
the registry's authored texture data.

## Testing

Select `CubusBlockWorldActor` and inspect `Cubus > Weather > Materials`.

- Start rain through the existing UDW actor. `Current Weather Rain Intensity`
  and `Current Material Wetness` should rise, while terrain becomes darker and
  less rough.
- Stop rain. Rain intensity should return to zero and accumulated wetness should
  dry gradually.
- For a deterministic material-only test, enable `Override Weather Rain
  Intensity` and set the override to `1.0`.

The bridge currently changes Cubus terrain and player-placed block materials.
Third-party foliage materials continue to use their own UDW foliage weather
function and are not replaced or rewritten by Cubus.
