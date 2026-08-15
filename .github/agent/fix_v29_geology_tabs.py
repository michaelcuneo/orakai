from pathlib import Path

# One-shot v29 correctness cleanup.
path = Path('Source/Orakai/CubusCore/Generation/CubusTerrainDensityField.cpp')
text = path.read_text()
old = '\tif (FMath::Abs(MacroTerrainDensity) < 2.5f)'
new = '\tif (Settings.bUseHeightTerrain && FMath::Abs(MacroTerrainDensity) < 2.5f)'
if text.count(old) != 1:
    raise RuntimeError(f'Expected one fine-relief guard, found {text.count(old)}')
path.write_text(text.replace(old, new, 1))
print('Restricted v29 sub-voxel relief to generated height terrain')
