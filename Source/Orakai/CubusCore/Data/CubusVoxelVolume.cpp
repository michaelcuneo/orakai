#include "CubusCore/Data/CubusVoxelVolume.h"

FCubusVoxelVolume::FCubusVoxelVolume()
{
}

FCubusVoxelVolume::FCubusVoxelVolume(
    const FIntVector& InDimensions
)
{
    Initialize(InDimensions);
}

void FCubusVoxelVolume::Initialize(
    const FIntVector& InDimensions
)
{
    Dimensions = SanitizeDimensions(InDimensions);

    const int64 RequiredVoxelCount =
        static_cast<int64>(Dimensions.X) *
        static_cast<int64>(Dimensions.Y) *
        static_cast<int64>(Dimensions.Z);

    check(
        RequiredVoxelCount >= 0 &&
        RequiredVoxelCount <= MAX_int32
    );

    Voxels.SetNum(
        static_cast<int32>(RequiredVoxelCount)
    );

    Clear();
}

int32 FCubusVoxelVolume::GetSolidVoxelCount() const
{
    int32 OccupiedCount = 0;

    for (const FCubusVoxel& Voxel : Voxels)
    {
        if (!Voxel.IsEmpty())
        {
            ++OccupiedCount;
        }
    }

    return OccupiedCount;
}

bool FCubusVoxelVolume::IsInside(
    const int32 X,
    const int32 Y,
    const int32 Z
) const
{
    return
        X >= 0 &&
        Y >= 0 &&
        Z >= 0 &&
        X < Dimensions.X &&
        Y < Dimensions.Y &&
        Z < Dimensions.Z;
}

bool FCubusVoxelVolume::IsInside(
    const FIntVector& Position
) const
{
    return IsInside(
        Position.X,
        Position.Y,
        Position.Z
    );
}

bool FCubusVoxelVolume::IsEmpty(
    const int32 X,
    const int32 Y,
    const int32 Z
) const
{
    const FCubusVoxel* Voxel =
        GetVoxel(X, Y, Z);

    return Voxel == nullptr || Voxel->IsEmpty();
}

bool FCubusVoxelVolume::IsEmpty(
    const FIntVector& Position
) const
{
    return IsEmpty(
        Position.X,
        Position.Y,
        Position.Z
    );
}

FCubusVoxel* FCubusVoxelVolume::GetVoxel(
    const int32 X,
    const int32 Y,
    const int32 Z
)
{
    if (!IsInside(X, Y, Z))
    {
        return nullptr;
    }

    return &Voxels[ToIndex(X, Y, Z)];
}

const FCubusVoxel* FCubusVoxelVolume::GetVoxel(
    const int32 X,
    const int32 Y,
    const int32 Z
) const
{
    if (!IsInside(X, Y, Z))
    {
        return nullptr;
    }

    return &Voxels[ToIndex(X, Y, Z)];
}

FCubusVoxel* FCubusVoxelVolume::GetVoxel(
    const FIntVector& Position
)
{
    return GetVoxel(
        Position.X,
        Position.Y,
        Position.Z
    );
}

const FCubusVoxel* FCubusVoxelVolume::GetVoxel(
    const FIntVector& Position
) const
{
    return GetVoxel(
        Position.X,
        Position.Y,
        Position.Z
    );
}

bool FCubusVoxelVolume::SetVoxel(
    const int32 X,
    const int32 Y,
    const int32 Z,
    const FCubusVoxel& Voxel
)
{
    FCubusVoxel* ExistingVoxel =
        GetVoxel(X, Y, Z);

    if (ExistingVoxel == nullptr)
    {
        return false;
    }

    *ExistingVoxel = Voxel;

    return true;
}

bool FCubusVoxelVolume::SetVoxel(
    const FIntVector& Position,
    const FCubusVoxel& Voxel
)
{
    return SetVoxel(
        Position.X,
        Position.Y,
        Position.Z,
        Voxel
    );
}

bool FCubusVoxelVolume::SetMaterialId(
    const int32 X,
    const int32 Y,
    const int32 Z,
    const int32 MaterialId
)
{
    FCubusVoxel* Voxel =
        GetVoxel(X, Y, Z);

    if (Voxel == nullptr)
    {
        return false;
    }

    Voxel->MaterialId =
        FMath::Clamp(MaterialId, 0, 65535);

    return true;
}

bool FCubusVoxelVolume::SetMaterialId(
    const FIntVector& Position,
    const int32 MaterialId
)
{
    return SetMaterialId(
        Position.X,
        Position.Y,
        Position.Z,
        MaterialId
    );
}

void FCubusVoxelVolume::Fill(
    const FCubusVoxel& Voxel
)
{
    for (FCubusVoxel& ExistingVoxel : Voxels)
    {
        ExistingVoxel = Voxel;
    }
}

void FCubusVoxelVolume::Clear()
{
    FCubusVoxel AirVoxel;
    AirVoxel.MaterialId = 0;
    AirVoxel.Flags = 0;

    Fill(AirVoxel);
}

int32 FCubusVoxelVolume::ToIndex(
    const int32 X,
    const int32 Y,
    const int32 Z
) const
{
    return X +
        Dimensions.X *
        (
            Y +
            Dimensions.Y * Z
        );
}

FIntVector FCubusVoxelVolume::SanitizeDimensions(
    const FIntVector& InDimensions
)
{
    return FIntVector(
        FMath::Max(1, InDimensions.X),
        FMath::Max(1, InDimensions.Y),
        FMath::Max(1, InDimensions.Z)
    );
}