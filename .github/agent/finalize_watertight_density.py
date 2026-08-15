from pathlib import Path
from urllib.request import urlopen

ROOT = Path('.')
UPSTREAM_COMMIT = '51a494f03c5b024cd153b596bcc7152eb3cc93a6'
UPSTREAM_URL = f'https://raw.githubusercontent.com/EricLengyel/Transvoxel/{UPSTREAM_COMMIT}/Transvoxel.cpp'


def read(path):
    return (ROOT / path).read_text(encoding='utf-8')


def write(path, text):
    (ROOT / path).write_text(text, encoding='utf-8')


def replace_once(path, old, new):
    text = read(path)
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f'{path}: expected one anchor, got {count}: {old[:180]!r}')
    write(path, text.replace(old, new, 1))


world_cpp = 'Source/Orakai/CubusCore/Actors/CubusBlockWorldActor.cpp'
replace_once(
    world_cpp,
    'NeighbourSubdivisions <= SelfSubdivisions * 2 || SelfSubdivisions <= NeighbourSubdivisions * 2,',
    'NeighbourSubdivisions <= SelfSubdivisions * 2 && SelfSubdivisions <= NeighbourSubdivisions * 2,'
)

source = urlopen(UPSTREAM_URL, timeout=30).read().decode('utf-8')

def extract_array(declaration):
    start = source.index(declaration)
    end = source.index('\n};', start) + len('\n};')
    return source[start:end]

regular_class = extract_array('const unsigned char regularCellClass[256]').replace(
    'const unsigned char regularCellClass[256]',
    'inline constexpr uint8 RegularCellClass[256]'
)
regular_data = extract_array('const RegularCellData regularCellData[16]').replace(
    'const RegularCellData regularCellData[16]',
    'inline constexpr FRegularCellData RegularCellData[16]'
)
regular_vertices = extract_array('const unsigned short regularVertexData[256][12]').replace(
    'const unsigned short regularVertexData[256][12]',
    'inline constexpr uint16 RegularVertexData[256][12]'
)

tables = 'Source/Orakai/CubusCore/Meshing/CubusTransvoxelTables.h'
insert = '''    struct FRegularCellData
    {
        uint8 GeometryCounts = 0;
        uint8 VertexIndex[15] = {};

        constexpr int32 GetVertexCount() const
        {
            return GeometryCounts >> 4;
        }

        constexpr int32 GetTriangleCount() const
        {
            return GeometryCounts & 0x0F;
        }
    };

''' + regular_class + '\n\n' + regular_data + '\n\n' + regular_vertices + '''

    static_assert(UE_ARRAY_COUNT(RegularCellClass) == 256, "Transvoxel regular class table must contain 256 cases.");
    static_assert(UE_ARRAY_COUNT(RegularCellData) == 16, "Transvoxel regular data must contain 16 triangulation classes.");
    static_assert(UE_ARRAY_COUNT(RegularVertexData) == 256, "Transvoxel regular vertex table must contain 256 cases.");

'''
replace_once(
    tables,
    '    struct FTransitionCellData\n',
    insert + '    struct FTransitionCellData\n'
)

mesher = 'Source/Orakai/CubusCore/Meshing/CubusDensityMesher.cpp'
helper_anchor = '''    int32 ClampDensityMaterialId(const int32 MaterialId)
    {
'''
helper = '''    /*
     * Transvoxel regular cells use binary cube-corner numbering:
     * 0=(0,0,0) 1=(1,0,0) 2=(0,1,0) 3=(1,1,0),
     * 4=(0,0,1) 5=(1,0,1) 6=(0,1,1) 7=(1,1,1).
     * Orakai's historic classic-MC order swaps 2<->3 and 6<->7.
     */
    constexpr int32 TransvoxelPointToOrakaiCorner[8] =
    {
        0, 1, 3, 2, 4, 5, 7, 6
    };

    int32 ToTransvoxelRegularCase(const int32 OrakaiCaseIndex)
    {
        int32 Result = 0;
        for (int32 PointIndex = 0; PointIndex < 8; ++PointIndex)
        {
            const int32 OrakaiCorner = TransvoxelPointToOrakaiCorner[PointIndex];
            if ((OrakaiCaseIndex & (1 << OrakaiCorner)) != 0)
            {
                Result |= 1 << PointIndex;
            }
        }
        return Result;
    }

    int32 FindOrakaiEdge(const int32 CornerA, const int32 CornerB)
    {
        for (int32 EdgeIndex = 0; EdgeIndex < 12; ++EdgeIndex)
        {
            const int32 EdgeA = EdgeCornerIndices[EdgeIndex][0];
            const int32 EdgeB = EdgeCornerIndices[EdgeIndex][1];
            if ((EdgeA == CornerA && EdgeB == CornerB) ||
                (EdgeA == CornerB && EdgeB == CornerA))
            {
                return EdgeIndex;
            }
        }
        return INDEX_NONE;
    }

    int32 GetRegularTriangleEdge(const int32 OrakaiCaseIndex, const int32 TriangleEntryIndex)
    {
        using namespace CubusTransvoxelTables;

        if (TriangleEntryIndex < 0)
        {
            return -1;
        }

        const int32 TransvoxelCase = ToTransvoxelRegularCase(OrakaiCaseIndex);
        const FRegularCellData& CellData = RegularCellData[RegularCellClass[TransvoxelCase]];
        if (TriangleEntryIndex >= CellData.GetTriangleCount() * 3)
        {
            return -1;
        }

        const int32 VertexIndex = CellData.VertexIndex[TriangleEntryIndex];
        if (VertexIndex < 0 || VertexIndex >= CellData.GetVertexCount())
        {
            return -1;
        }

        const uint8 EndpointCode = static_cast<uint8>(RegularVertexData[TransvoxelCase][VertexIndex] & 0x00FFu);
        const int32 PointA = (EndpointCode >> 4) & 0x0F;
        const int32 PointB = EndpointCode & 0x0F;
        if (PointA >= 8 || PointB >= 8)
        {
            return -1;
        }

        return FindOrakaiEdge(
            TransvoxelPointToOrakaiCorner[PointA],
            TransvoxelPointToOrakaiCorner[PointB]
        );
    }

'''
replace_once(mesher, helper_anchor, helper + helper_anchor)

text = read(mesher)
needle = 'CubusMarchingCubesTables::GetTriangleEdge'
count = text.count(needle)
if count < 1:
    raise RuntimeError('no classic density-mesher table calls found')
text = text.replace(needle, 'GetRegularTriangleEdge')
# The source also has line-wrapped namespace/function spellings.
text = text.replace('CubusMarchingCubesTables::\n                                GetTriangleEdge', 'GetRegularTriangleEdge')
text = text.replace('CubusMarchingCubesTables::\n                                            GetTriangleEdge', 'GetRegularTriangleEdge')
text = text.replace('CubusMarchingCubesTables::\n                        GetTriangleEdge', 'GetRegularTriangleEdge')
write(mesher, text)

replace_once(
    mesher,
    '#include "CubusCore/Meshing/CubusMarchingCubesTables.h"\n',
    ''
)

for path, token in [
    (world_cpp, 'NeighbourSubdivisions <= SelfSubdivisions * 2 && SelfSubdivisions <= NeighbourSubdivisions * 2'),
    (tables, 'RegularVertexData[256][12]'),
    (mesher, 'GetRegularTriangleEdge('),
    (mesher, 'TransvoxelPointToOrakaiCorner'),
]:
    if token not in read(path):
        raise RuntimeError(f'{path}: missing {token}')

if 'CubusMarchingCubesTables::' in read(mesher):
    raise RuntimeError('production density mesher still uses classic MC topology')
