from pathlib import Path
import runpy

runpy.run_path('.github/agent/finalize_watertight_density_v2.py', run_name='__main__')

path = Path('Source/Orakai/CubusCore/Meshing/CubusDensityMesher.cpp')
text = path.read_text(encoding='utf-8')
path.write_text('\n'.join(line.rstrip() for line in text.splitlines()) + '\n', encoding='utf-8')
