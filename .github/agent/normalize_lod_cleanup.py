from pathlib import Path

p = Path('.github/agent/lod_topology_cleanup.py')
s = p.read_text(encoding='utf-8')
old = """    new_text, count = re.subn(pattern, replacement, text, count=1, flags=flags)\n    if count != 1:\n        raise RuntimeError(f'{path}: regex expected one match, found {count}: {pattern[:140]!r}')\n    write(path, new_text)\n"""
new = """    new_text, count = re.subn(pattern, replacement, text, count=1, flags=flags)\n    if count == 0 and 'bResolutionStillCurrent' in pattern:\n        fallback = r'\\t\\tconst bool bResolutionStillCurrent =.*?\\n\\t\\tif \\(IsValid\\(Chunk\\) && FindChunk\\(Build\\.Coordinate\\) == Chunk'
        matched = re.search(fallback, text, flags=re.S)\n        if matched is not None:\n            prefix = matched.group(0)\n            suffix = '\\n\\t\\tif (IsValid(Chunk) && FindChunk(Build.Coordinate) == Chunk'\n            new_text = text[:matched.start()] + replacement + suffix + text[matched.end():]\n            count = 1\n    if count == 0 and replacement in text:\n        return\n    if count != 1:\n        raise RuntimeError(f'{path}: regex expected one match, found {count}: {pattern[:140]!r}')\n    write(path, new_text)\n"""
if old not in s:
    raise RuntimeError('regex_once implementation anchor not found')
p.write_text(s.replace(old, new, 1), encoding='utf-8')
