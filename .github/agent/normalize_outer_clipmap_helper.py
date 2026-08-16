from pathlib import Path
p = Path('.github/agent/harden_density_outer_clipmaps.py')
s = p.read_text(encoding='utf-8')
old = """replace_once(CPP, '''\\tFCubusTerrainLodTierRuntime* Tiers[] = {&Lod6Runtime, &Lod5Runtime, &Lod4Runtime, &Lod3Runtime, &Lod2Runtime, &Lod1Runtime};\\n''', '''\\tFCubusTerrainLodTierRuntime* Tiers[] = {&Lod1Runtime, &Lod2Runtime, &Lod3Runtime, &Lod4Runtime, &Lod5Runtime, &Lod6Runtime};\n''')\n"""
new = """text = read(CPP)\nold_tiers = '\\tFCubusTerrainLodTierRuntime* Tiers[] = {&Lod6Runtime, &Lod5Runtime, &Lod4Runtime, &Lod3Runtime, &Lod2Runtime, &Lod1Runtime};\\n'\nnew_tiers = '\\tFCubusTerrainLodTierRuntime* Tiers[] = {&Lod1Runtime, &Lod2Runtime, &Lod3Runtime, &Lod4Runtime, &Lod5Runtime, &Lod6Runtime};\\n'\nif old_tiers not in text:\n    raise RuntimeError('no reverse tier arrays found to normalize')\nwrite(CPP, text.replace(old_tiers, new_tiers))\n"""
if old not in s:
    raise RuntimeError('target helper block not found')
p.write_text(s.replace(old, new, 1), encoding='utf-8')
