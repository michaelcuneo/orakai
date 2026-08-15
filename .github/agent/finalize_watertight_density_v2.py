from pathlib import Path
from urllib.request import urlopen

root = Path('.')
world = root / 'Source/Orakai/CubusCore/Actors/CubusBlockWorldActor.cpp'
mesher = root / 'Source/Orakai/CubusCore/Meshing/CubusDensityMesher.cpp'
tables = root / 'Source/Orakai/CubusCore/Meshing/CubusTransvoxelTables.h'

# Correct 2:1 ratio guard.
s = world.read_text()
old = 'NeighbourSubdivisions <= SelfSubdivisions * 2 || SelfSubdivisions <= NeighbourSubdivisions * 2,'
new = 'NeighbourSubdivisions <= SelfSubdivisions * 2 && SelfSubdivisions <= NeighbourSubdivisions * 2,'
if s.count(old) != 1: raise RuntimeError('2:1 guard anchor missing')
world.write_text(s.replace(old, new, 1))

# Pull pinned MIT Transvoxel regular tables.
commit = '51a494f03c5b024cd153b596bcc7152eb3cc93a6'
src = urlopen(f'https://raw.githubusercontent.com/EricLengyel/Transvoxel/{commit}/Transvoxel.cpp').read().decode()
def arr(decl):
    a = src.index(decl); b = src.index('\n};', a) + 3
    return src[a:b]
rc = arr('const unsigned char regularCellClass[256]').replace('const unsigned char regularCellClass[256]', 'inline constexpr uint8 RegularCellClass[256]')
rd = arr('const RegularCellData regularCellData[16]').replace('const RegularCellData regularCellData[16]', 'inline constexpr FRegularCellData RegularCellData[16]')
rv = arr('const unsigned short regularVertexData[256][12]').replace('const unsigned short regularVertexData[256][12]', 'inline constexpr uint16 RegularVertexData[256][12]')
block = '''    struct FRegularCellData
    {
        uint8 GeometryCounts = 0;
        uint8 VertexIndex[15] = {};
        constexpr int32 GetVertexCount() const { return GeometryCounts >> 4; }
        constexpr int32 GetTriangleCount() const { return GeometryCounts & 0x0F; }
    };

''' + rc + '\n\n' + rd + '\n\n' + rv + '''

    static_assert(UE_ARRAY_COUNT(RegularCellClass) == 256);
    static_assert(UE_ARRAY_COUNT(RegularCellData) == 16);
    static_assert(UE_ARRAY_COUNT(RegularVertexData) == 256);

'''
t = tables.read_text()
anchor = '    struct FTransitionCellData\n'
if t.count(anchor) != 1: raise RuntimeError('transition table insertion anchor missing')
tables.write_text(t.replace(anchor, block + anchor, 1))

# Add regular topology adapter before material helper.
m = mesher.read_text()
anchor = '''    int32 ClampDensityMaterialId(const int32 MaterialId)
    {
'''
helper = '''    constexpr int32 TransvoxelPointToOrakaiCorner[8] = { 0, 1, 3, 2, 4, 5, 7, 6 };

    int32 ToTransvoxelRegularCase(const int32 OrakaiCaseIndex)
    {
        int32 Result = 0;
        for (int32 Point = 0; Point < 8; ++Point)
        {
            const int32 Corner = TransvoxelPointToOrakaiCorner[Point];
            if ((OrakaiCaseIndex & (1 << Corner)) != 0) Result |= 1 << Point;
        }
        return Result;
    }

    int32 FindOrakaiEdge(const int32 A, const int32 B)
    {
        for (int32 Edge = 0; Edge < 12; ++Edge)
        {
            const int32 EA = EdgeCornerIndices[Edge][0];
            const int32 EB = EdgeCornerIndices[Edge][1];
            if ((EA == A && EB == B) || (EA == B && EB == A)) return Edge;
        }
        return INDEX_NONE;
    }

    int32 GetRegularTriangleEdge(const int32 OrakaiCaseIndex, const int32 Entry)
    {
        using namespace CubusTransvoxelTables;
        if (Entry < 0) return -1;
        const int32 C = ToTransvoxelRegularCase(OrakaiCaseIndex);
        const FRegularCellData& Data = RegularCellData[RegularCellClass[C]];
        if (Entry >= Data.GetTriangleCount() * 3) return -1;
        const int32 V = Data.VertexIndex[Entry];
        if (V < 0 || V >= Data.GetVertexCount()) return -1;
        const uint8 Code = static_cast<uint8>(RegularVertexData[C][V] & 0xFFu);
        const int32 PA = (Code >> 4) & 0xF;
        const int32 PB = Code & 0xF;
        if (PA >= 8 || PB >= 8) return -1;
        return FindOrakaiEdge(TransvoxelPointToOrakaiCorner[PA], TransvoxelPointToOrakaiCorner[PB]);
    }

'''
if m.count(anchor) != 1: raise RuntimeError('regular helper anchor missing')
m = m.replace(anchor, helper + anchor, 1)
# Normalize every classic topology call regardless of line wrapping.
m = m.replace('CubusMarchingCubesTables::', '')
m = m.replace('GetTriangleEdge(', 'GetRegularTriangleEdge(')
m = m.replace('#include "CubusCore/Meshing/CubusMarchingCubesTables.h"\n', '')
mesher.write_text(m)

if 'GetTriangleEdge(' in m or 'CubusMarchingCubesTables::' in m: raise RuntimeError('classic topology remains')
if 'GetRegularTriangleEdge(' not in m: raise RuntimeError('regular adapter not wired')
