from pathlib import Path

# One-shot cleanup for the generated v29 geology function.
path = Path('Source/Orakai/CubusCore/Generation/CubusTerrainDensityField.cpp')
text = path.read_text()
start = text.index('float FCubusTerrainDensityField::SampleGeologicalDensity(')
end = text.index('float FCubusTerrainDensityField::SampleCaveDensity(', start)
section = text[start:end]
if '\\t' not in section:
    raise RuntimeError('Expected literal \\t sequences in generated geology section')
section = section.replace('\\t', '\t')
path.write_text(text[:start] + section + text[end:])
print('Fixed literal tab escapes in v29 geology function')
