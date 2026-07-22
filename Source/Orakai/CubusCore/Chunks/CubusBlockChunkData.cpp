#include "CubusCore/Chunks/CubusBlockChunkData.h"

#include "CubusCore/Chunks/CubusChunkConstants.h"

FCubusBlockChunkData::FCubusBlockChunkData(
    const FIntVector& InChunkCoordinate
)
    : ChunkCoordinate(InChunkCoordinate)
{
    Voxels.SetNum(Cubus::ChunkVolume);

    Clear();
}

int32 FCubusBlockChunkData::GetOccupiedVoxelCount() const
{
    int32 OccupiedCount = 0;

    for (const FCubusBlockVoxel& Voxel : Voxels)
    {
        if (!Voxel.IsEmpty())
        {
            ++OccupiedCount;
        }
    }

    return OccupiedCount;
}

bool FCubusBlockChunkData::HasAnyOccupiedVoxel() const
{
    if (bOccupiedStateKnown)
    {
        return bHasAnyOccupiedVoxel;
    }

    for (const FCubusBlockVoxel& Voxel : Voxels)
    {
        if (!Voxel.IsEmpty())
        {
            bHasAnyOccupiedVoxel = true;
            bOccupiedStateKnown = true;

            return true;
        }
    }

    bHasAnyOccupiedVoxel = false;
    bOccupiedStateKnown = true;

    return false;
}

bool FCubusBlockChunkData::IsEmpty(
    const int32 X,
    const int32 Y,
    const int32 Z
) const
{
    const FCubusBlockVoxel* Voxel =
        GetVoxel(X, Y, Z);

    return
        Voxel == nullptr ||
        Voxel->IsEmpty();
}

bool FCubusBlockChunkData::IsEmpty(
    const FIntVector& LocalCoordinate
) const
{
    return IsEmpty(
        LocalCoordinate.X,
        LocalCoordinate.Y,
        LocalCoordinate.Z
    );
}

FCubusBlockVoxel* FCubusBlockChunkData::GetVoxel(
    const int32 X,
    const int32 Y,
    const int32 Z
)
{
    if (!Cubus::IsValidLocalCoordinate(X, Y, Z))
    {
        return nullptr;
    }

    return &Voxels[
        Cubus::FlattenLocalCoordinate(X, Y, Z)
    ];
}

const FCubusBlockVoxel*
FCubusBlockChunkData::GetVoxel(
    const int32 X,
    const int32 Y,
    const int32 Z
) const
{
    if (!Cubus::IsValidLocalCoordinate(X, Y, Z))
    {
        return nullptr;
    }

    return &Voxels[
        Cubus::FlattenLocalCoordinate(X, Y, Z)
    ];
}

FCubusBlockVoxel* FCubusBlockChunkData::GetVoxel(
    const FIntVector& LocalCoordinate
)
{
    return GetVoxel(
        LocalCoordinate.X,
        LocalCoordinate.Y,
        LocalCoordinate.Z
    );
}

const FCubusBlockVoxel*
FCubusBlockChunkData::GetVoxel(
    const FIntVector& LocalCoordinate
) const
{
    return GetVoxel(
        LocalCoordinate.X,
        LocalCoordinate.Y,
        LocalCoordinate.Z
    );
}

FCubusBlockVoxel&
FCubusBlockChunkData::GetVoxelChecked(
    const int32 X,
    const int32 Y,
    const int32 Z
)
{
    check(Cubus::IsValidLocalCoordinate(X, Y, Z));

    return Voxels[
        Cubus::FlattenLocalCoordinate(X, Y, Z)
    ];
}

const FCubusBlockVoxel&
FCubusBlockChunkData::GetVoxelChecked(
    const int32 X,
    const int32 Y,
    const int32 Z
) const
{
    check(Cubus::IsValidLocalCoordinate(X, Y, Z));

    return Voxels[
        Cubus::FlattenLocalCoordinate(X, Y, Z)
    ];
}

bool FCubusBlockChunkData::SetVoxel(
    const int32 X,
    const int32 Y,
    const int32 Z,
    const FCubusBlockVoxel& Voxel
)
{
    FCubusBlockVoxel* ExistingVoxel =
        GetVoxel(X, Y, Z);

    if (ExistingVoxel == nullptr)
    {
        return false;
    }

    *ExistingVoxel = Voxel;

    InvalidateOccupiedState();

    return true;
}

bool FCubusBlockChunkData::SetVoxel(
    const FIntVector& LocalCoordinate,
    const FCubusBlockVoxel& Voxel
)
{
    return SetVoxel(
        LocalCoordinate.X,
        LocalCoordinate.Y,
        LocalCoordinate.Z,
        Voxel
    );
}

bool FCubusBlockChunkData::SetMaterialId(
    const int32 X,
    const int32 Y,
    const int32 Z,
    const int32 MaterialId
)
{
    FCubusBlockVoxel* Voxel =
        GetVoxel(X, Y, Z);

    if (Voxel == nullptr)
    {
        return false;
    }

    Voxel->MaterialId =
        FMath::Clamp(MaterialId, 0, 65535);

    InvalidateOccupiedState();

    return true;
}

bool FCubusBlockChunkData::SetMaterialId(
    const FIntVector& LocalCoordinate,
    const int32 MaterialId
)
{
    return SetMaterialId(
        LocalCoordinate.X,
        LocalCoordinate.Y,
        LocalCoordinate.Z,
        MaterialId
    );
}

FIntVector FCubusBlockChunkData::LocalToWorldVoxel(
    const int32 X,
    const int32 Y,
    const int32 Z
) const
{
    check(Cubus::IsValidLocalCoordinate(X, Y, Z));

    return Cubus::LocalToWorldVoxel(
        ChunkCoordinate,
        FIntVector(X, Y, Z)
    );
}

FIntVector FCubusBlockChunkData::LocalToWorldVoxel(
    const FIntVector& LocalCoordinate
) const
{
    check(
        Cubus::IsValidLocalCoordinate(
            LocalCoordinate
        )
    );

    return Cubus::LocalToWorldVoxel(
        ChunkCoordinate,
        LocalCoordinate
    );
}

void FCubusBlockChunkData::Fill(
    const FCubusBlockVoxel& Voxel
)
{
    for (FCubusBlockVoxel& ExistingVoxel : Voxels)
    {
        ExistingVoxel = Voxel;
    }

    bHasAnyOccupiedVoxel = !Voxel.IsEmpty();
    bOccupiedStateKnown = true;
}

void FCubusBlockChunkData::Clear()
{
    FCubusBlockVoxel EmptyVoxel;
    EmptyVoxel.MaterialId = 0;
    EmptyVoxel.Flags = 0;

    Fill(EmptyVoxel);
}