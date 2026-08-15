from pathlib import Path

h = Path('Source/Orakai/CubusCore/Actors/CubusBlockWorldActor.h')
cpp = Path('Source/Orakai/CubusCore/Actors/CubusBlockWorldActor.cpp')

ht = h.read_text(encoding='utf-8')
old_h = '\tint32 DensityNearChunkRadius = 1;'
new_h = '\tint32 DensityNearChunkRadius = 2;'
if ht.count(old_h) != 1:
    raise RuntimeError(f'header anchor count={ht.count(old_h)}')
h.write_text(ht.replace(old_h, new_h, 1), encoding='utf-8')

ct = cpp.read_text(encoding='utf-8')
old_cpp = '\tDensityNearChunkRadius\t   = FMath::Max(0, DensityNearChunkRadius);'
new_cpp = '''\t// Keep the finest 20 cm density ring at least two chunks wide. This places\n\t// the 20->40 cm transition outside the immediate spawn/walking area and\n\t// also upgrades older saved Blueprint defaults that still carry radius 1.\n\tDensityNearChunkRadius\t   = FMath::Max(2, DensityNearChunkRadius);'''
if ct.count(old_cpp) != 1:
    raise RuntimeError(f'cpp anchor count={ct.count(old_cpp)}')
cpp.write_text(ct.replace(old_cpp, new_cpp, 1), encoding='utf-8')

# The middle tier must remain outside the near tier.
ct = cpp.read_text(encoding='utf-8')
if 'DensityMiddleChunkRadius   = FMath::Max(DensityNearChunkRadius, DensityMiddleChunkRadius);' not in ct:
    raise RuntimeError('middle-radius ordering guard missing')
if 'DensityNearChunkRadius\t   = FMath::Max(2, DensityNearChunkRadius);' not in ct:
    raise RuntimeError('near-radius runtime minimum missing')
if 'int32 DensityNearChunkRadius = 2;' not in h.read_text(encoding='utf-8'):
    raise RuntimeError('near-radius default not updated')
