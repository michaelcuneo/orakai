#include "CubusCore/Meshing/CubusBlockMesher.h"

#include "CubusCore/Chunks/CubusBlockChunkData.h"
#include "CubusCore/Chunks/CubusBlockChunkNeighborhood.h"
#include "CubusCore/Chunks/CubusChunkConstants.h"
#include "CubusCore/Chunks/CubusDensitySamplingBuffer.h"
#include "CubusCore/Data/CubusMaterialRegistry.h"
#include "CubusCore/Generation/CubusBlockDensityField.h"
#include "CubusCore/Meshing/CubusDensityMesher.h"

namespace CubusBlockMesher
{
FCubusBlockVoxel MakeEmptyVoxel()
{
	return FCubusBlockVoxel();
}

bool IsRenderableSolid(const FCubusBlockVoxel* Voxel, const UCubusMaterialRegistry* MaterialRegistry)
{
	if (Voxel == nullptr || Voxel->IsEmpty() || Voxel->IsWater())
	{
		return false;
	}

	return MaterialRegistry == nullptr || MaterialRegistry->IsRenderableSolid(Voxel->MaterialId);
}

FCubusBlockVoxel ResolveBlockSample(const FCubusBlockChunkNeighborhood& Neighborhood, const UCubusMaterialRegistry* MaterialRegistry,
									const FIntVector& LocalCoordinate)
{
	const FCubusBlockVoxel* DirectVoxel = Neighborhood.GetVoxel(LocalCoordinate.X, LocalCoordinate.Y, LocalCoordinate.Z);

	if (DirectVoxel != nullptr)
	{
		return IsRenderableSolid(DirectVoxel, MaterialRegistry) ? *DirectVoxel : MakeEmptyVoxel();
	}

	// Marching Cubes needs edge and corner halo samples that the legacy
	// six-face neighbourhood cannot address directly. Reconstruct those
	// samples from every available axis/plane projection. This is the
	// conservative version used before the material hardening pass and it
	// deliberately favours solid occupancy to prevent visible seam holes.
	TArray<const FCubusBlockVoxel*, TInlineAllocator<7>> Candidates;

	const int32 ClampedX = FMath::Clamp(LocalCoordinate.X, 0, Cubus::ChunkSize - 1);
	const int32 ClampedY = FMath::Clamp(LocalCoordinate.Y, 0, Cubus::ChunkSize - 1);
	const int32 ClampedZ = FMath::Clamp(LocalCoordinate.Z, 0, Cubus::ChunkSize - 1);

	Candidates.Add(Neighborhood.GetVoxel(LocalCoordinate.X, ClampedY, ClampedZ));
	Candidates.Add(Neighborhood.GetVoxel(ClampedX, LocalCoordinate.Y, ClampedZ));
	Candidates.Add(Neighborhood.GetVoxel(ClampedX, ClampedY, LocalCoordinate.Z));
	Candidates.Add(Neighborhood.GetVoxel(LocalCoordinate.X, LocalCoordinate.Y, ClampedZ));
	Candidates.Add(Neighborhood.GetVoxel(LocalCoordinate.X, ClampedY, LocalCoordinate.Z));
	Candidates.Add(Neighborhood.GetVoxel(ClampedX, LocalCoordinate.Y, LocalCoordinate.Z));
	Candidates.Add(Neighborhood.GetVoxel(ClampedX, ClampedY, ClampedZ));

	TMap<int32, int32> SolidVotesByMaterial;
	int32			   SolidVoteCount = 0;
	int32			   EmptyVoteCount = 0;

	for (const FCubusBlockVoxel* Candidate : Candidates)
	{
		if (IsRenderableSolid(Candidate, MaterialRegistry))
		{
			++SolidVoteCount;
			++SolidVotesByMaterial.FindOrAdd(Candidate->MaterialId);
		}
		else if (Candidate != nullptr)
		{
			++EmptyVoteCount;
		}
	}

	if (SolidVoteCount <= 0 || SolidVoteCount < EmptyVoteCount)
	{
		return MakeEmptyVoxel();
	}

	int32 DominantMaterialId = 1;
	int32 DominantVotes		 = -1;

	for (const TPair<int32, int32>& Pair : SolidVotesByMaterial)
	{
		if (Pair.Value > DominantVotes)
		{
			DominantMaterialId = Pair.Key;
			DominantVotes	   = Pair.Value;
		}
	}

	FCubusBlockVoxel Result;
	Result.MaterialId = DominantMaterialId;
	return Result;
}

int32 FindDominantPaletteSlot(const FCubusMeshData& Mesh, const int32 Index0, const int32 Index1, const int32 Index2)
{
	const int32 Indices[3] = {Index0, Index1, Index2};
	float		Totals[4]  = {0.0f, 0.0f, 0.0f, 0.0f};

	for (const int32 Index : Indices)
	{
		if (!Mesh.VertexColors.IsValidIndex(Index))
		{
			continue;
		}

		const FLinearColor& Weight = Mesh.VertexColors[Index];
		Totals[0] += Weight.R;
		Totals[1] += Weight.G;
		Totals[2] += Weight.B;
		Totals[3] += Weight.A;
	}

	int32 DominantSlot = 0;
	for (int32 Slot = 1; Slot < 4; ++Slot)
	{
		if (Totals[Slot] > Totals[DominantSlot])
		{
			DominantSlot = Slot;
		}
	}

	return DominantSlot;
}

FLinearColor MakeHardPaletteWeight(const int32 Slot)
{
	switch (Slot)
	{
	case 1:
		return FLinearColor(0.0f, 1.0f, 0.0f, 0.0f);
	case 2:
		return FLinearColor(0.0f, 0.0f, 1.0f, 0.0f);
	case 3:
		return FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);
	default:
		return FLinearColor(1.0f, 0.0f, 0.0f, 0.0f);
	}
}

void HardenGeologicalBlockMesh(FCubusMeshData& Mesh)
{
	for (int32 TriangleOffset = 0; TriangleOffset + 2 < Mesh.Triangles.Num(); TriangleOffset += 3)
	{
		const int32 Index0 = Mesh.Triangles[TriangleOffset + 0];
		const int32 Index1 = Mesh.Triangles[TriangleOffset + 1];
		const int32 Index2 = Mesh.Triangles[TriangleOffset + 2];

		if (!Mesh.Vertices.IsValidIndex(Index0) || !Mesh.Vertices.IsValidIndex(Index1) || !Mesh.Vertices.IsValidIndex(Index2))
		{
			continue;
		}

		const FVector& Vertex0 = Mesh.Vertices[Index0];
		const FVector& Vertex1 = Mesh.Vertices[Index1];
		const FVector& Vertex2 = Mesh.Vertices[Index2];

		FVector FaceNormal = FVector::CrossProduct(Vertex1 - Vertex0, Vertex2 - Vertex0).GetSafeNormal();

		if (FaceNormal.IsNearlyZero())
		{
			FaceNormal = FVector::UpVector;
		}

		const int32 DominantSlot = FindDominantPaletteSlot(Mesh, Index0, Index1, Index2);

		const FLinearColor HardWeight = MakeHardPaletteWeight(DominantSlot);

		const FVector		   TangentDirection = (Vertex1 - Vertex0).GetSafeNormal();
		const FProcMeshTangent Tangent(TangentDirection.IsNearlyZero() ? FVector::ForwardVector : TangentDirection, false);

		const int32 Indices[3] = {Index0, Index1, Index2};
		for (const int32 Index : Indices)
		{
			if (Mesh.Normals.IsValidIndex(Index))
			{
				Mesh.Normals[Index] = FaceNormal;
			}

			if (Mesh.VertexColors.IsValidIndex(Index))
			{
				Mesh.VertexColors[Index] = HardWeight;
			}

			if (Mesh.Tangents.IsValidIndex(Index))
			{
				Mesh.Tangents[Index] = Tangent;
			}
		}
	}
}
} // namespace CubusBlockMesher

const FCubusBlockVoxel* FCubusBlockChunkMeshSnapshot::GetVoxel(const int32 X, const int32 Y, const int32 Z) const
{
	if (!Cubus::IsValidLocalCoordinate(X, Y, Z))
	{
		return nullptr;
	}

	const int32 Index = Cubus::FlattenLocalCoordinate(X, Y, Z);
	return Voxels.IsValidIndex(Index) ? &Voxels[Index] : nullptr;
}

const FCubusBlockVoxel* FCubusBlockNeighborhoodMeshSnapshot::GetVoxel(const int32 X, const int32 Y, const int32 Z) const
{
	if (!Centre.IsValid())
	{
		return nullptr;
	}

	if (Cubus::IsValidLocalCoordinate(X, Y, Z))
	{
		return Centre->GetVoxel(X, Y, Z);
	}

	if (X == Cubus::ChunkSize && Y >= 0 && Y < Cubus::ChunkSize && Z >= 0 && Z < Cubus::ChunkSize)
	{
		return PositiveX.IsValid() ? PositiveX->GetVoxel(0, Y, Z) : nullptr;
	}
	if (X == -1 && Y >= 0 && Y < Cubus::ChunkSize && Z >= 0 && Z < Cubus::ChunkSize)
	{
		return NegativeX.IsValid() ? NegativeX->GetVoxel(Cubus::ChunkSize - 1, Y, Z) : nullptr;
	}
	if (Y == Cubus::ChunkSize && X >= 0 && X < Cubus::ChunkSize && Z >= 0 && Z < Cubus::ChunkSize)
	{
		return PositiveY.IsValid() ? PositiveY->GetVoxel(X, 0, Z) : nullptr;
	}
	if (Y == -1 && X >= 0 && X < Cubus::ChunkSize && Z >= 0 && Z < Cubus::ChunkSize)
	{
		return NegativeY.IsValid() ? NegativeY->GetVoxel(X, Cubus::ChunkSize - 1, Z) : nullptr;
	}
	if (Z == Cubus::ChunkSize && X >= 0 && X < Cubus::ChunkSize && Y >= 0 && Y < Cubus::ChunkSize)
	{
		return PositiveZ.IsValid() ? PositiveZ->GetVoxel(X, Y, 0) : nullptr;
	}
	if (Z == -1 && X >= 0 && X < Cubus::ChunkSize && Y >= 0 && Y < Cubus::ChunkSize)
	{
		return NegativeZ.IsValid() ? NegativeZ->GetVoxel(X, Y, Cubus::ChunkSize - 1) : nullptr;
	}

	return nullptr;
}

bool FCubusBlockMaterialMeshSnapshot::IsRenderableSolid(const FCubusBlockVoxel* Voxel) const
{
	return Voxel != nullptr && !Voxel->IsEmpty() && !Voxel->IsWater() && RenderableSolidMaterialIds.Contains(Voxel->MaterialId);
}

namespace CubusBlockMesher
{
FCubusBlockVoxel ResolveSnapshotSample(const FCubusBlockNeighborhoodMeshSnapshot& Neighborhood,
									   const FCubusBlockMaterialMeshSnapshot& Materials, const FIntVector& LocalCoordinate)
{
	const FCubusBlockVoxel* DirectVoxel = Neighborhood.GetVoxel(LocalCoordinate.X, LocalCoordinate.Y, LocalCoordinate.Z);
	if (DirectVoxel != nullptr)
	{
		return Materials.IsRenderableSolid(DirectVoxel) ? *DirectVoxel : MakeEmptyVoxel();
	}

	const int32		 ClampedX	  = FMath::Clamp(LocalCoordinate.X, 0, Cubus::ChunkSize - 1);
	const int32		 ClampedY	  = FMath::Clamp(LocalCoordinate.Y, 0, Cubus::ChunkSize - 1);
	const int32		 ClampedZ	  = FMath::Clamp(LocalCoordinate.Z, 0, Cubus::ChunkSize - 1);
	const FIntVector Candidates[] = {FIntVector(LocalCoordinate.X, ClampedY, ClampedZ),
									 FIntVector(ClampedX, LocalCoordinate.Y, ClampedZ),
									 FIntVector(ClampedX, ClampedY, LocalCoordinate.Z),
									 FIntVector(LocalCoordinate.X, LocalCoordinate.Y, ClampedZ),
									 FIntVector(LocalCoordinate.X, ClampedY, LocalCoordinate.Z),
									 FIntVector(ClampedX, LocalCoordinate.Y, LocalCoordinate.Z),
									 FIntVector(ClampedX, ClampedY, ClampedZ)};

	TMap<int32, int32> SolidVotesByMaterial;
	int32			   SolidVoteCount = 0;
	int32			   EmptyVoteCount = 0;
	for (const FIntVector& CandidateCoordinate : Candidates)
	{
		const FCubusBlockVoxel* Candidate = Neighborhood.GetVoxel(CandidateCoordinate.X, CandidateCoordinate.Y, CandidateCoordinate.Z);
		if (Materials.IsRenderableSolid(Candidate))
		{
			++SolidVoteCount;
			++SolidVotesByMaterial.FindOrAdd(Candidate->MaterialId);
		}
		else if (Candidate != nullptr)
		{
			++EmptyVoteCount;
		}
	}

	if (SolidVoteCount <= 0 || SolidVoteCount < EmptyVoteCount)
	{
		return MakeEmptyVoxel();
	}

	int32 DominantMaterialId = 1;
	int32 DominantVotes		 = -1;
	for (const TPair<int32, int32>& Pair : SolidVotesByMaterial)
	{
		if (Pair.Value > DominantVotes)
		{
			DominantMaterialId = Pair.Key;
			DominantVotes	   = Pair.Value;
		}
	}

	FCubusBlockVoxel Result;
	Result.MaterialId = DominantMaterialId;
	return Result;
}
} // namespace CubusBlockMesher

void FCubusBlockMesher::BuildChunk(const FCubusBlockNeighborhoodMeshSnapshot& Neighborhood,
								   const FCubusBlockMaterialMeshSnapshot& Materials, const float VoxelSize,
								   FCubusMaterialMeshMap& OutMaterialMeshes, int32& OutGeneratedFaceCount)
{
	OutMaterialMeshes.Reset();
	OutGeneratedFaceCount = 0;
	if (!Neighborhood.Centre.IsValid() || VoxelSize <= 0.0f)
	{
		return;
	}

	const FIntVector			  ChunkCoordinate	= Neighborhood.Centre->ChunkCoordinate;
	const FIntVector			  GlobalVoxelOrigin = ChunkCoordinate * Cubus::ChunkSize;
	const FCubusBlockDensityField BlockDensityField(
		[&Neighborhood, &Materials, GlobalVoxelOrigin](const FIntVector& GlobalVoxelCoordinate)
		{ return CubusBlockMesher::ResolveSnapshotSample(Neighborhood, Materials, GlobalVoxelCoordinate - GlobalVoxelOrigin); }, true,
		1.0f);

	FCubusDensitySamplingBuffer SamplingBuffer;
	SamplingBuffer.Build(ChunkCoordinate, BlockDensityField);
	int32 GeneratedTriangleCount = 0;
	FCubusDensityMesher::BuildChunk(SamplingBuffer, VoxelSize, 0.0f, OutMaterialMeshes, GeneratedTriangleCount);
	if (FCubusMeshData* GeologicalMesh = OutMaterialMeshes.Find(FCubusDensityMesher::UnifiedDensityMaterialKey))
	{
		CubusBlockMesher::HardenGeologicalBlockMesh(*GeologicalMesh);
	}
	OutGeneratedFaceCount = GeneratedTriangleCount;
}

void FCubusBlockMesher::BuildChunk(const FCubusBlockChunkNeighborhood& Neighborhood, const UCubusMaterialRegistry* MaterialRegistry,
								   const float VoxelSize, FCubusMaterialMeshMap& OutMaterialMeshes, int32& OutGeneratedFaceCount)
{
	OutMaterialMeshes.Reset();
	OutGeneratedFaceCount = 0;

	if (Neighborhood.Centre == nullptr || VoxelSize <= 0.0f)
	{
		return;
	}

	const FIntVector ChunkCoordinate = Neighborhood.Centre->GetChunkCoordinate();

	const FIntVector GlobalVoxelOrigin = ChunkCoordinate * Cubus::ChunkSize;

	const FCubusBlockDensityField BlockDensityField(
		[&Neighborhood, MaterialRegistry, GlobalVoxelOrigin](const FIntVector& GlobalVoxelCoordinate)
		{ return CubusBlockMesher::ResolveBlockSample(Neighborhood, MaterialRegistry, GlobalVoxelCoordinate - GlobalVoxelOrigin); }, true,
		1.0f);

	FCubusDensitySamplingBuffer SamplingBuffer;
	SamplingBuffer.Build(ChunkCoordinate, BlockDensityField);

	int32 GeneratedTriangleCount = 0;

	FCubusDensityMesher::BuildChunk(SamplingBuffer, VoxelSize, 0.0f, OutMaterialMeshes, GeneratedTriangleCount);

	if (FCubusMeshData* GeologicalMesh = OutMaterialMeshes.Find(FCubusDensityMesher::UnifiedDensityMaterialKey))
	{
		CubusBlockMesher::HardenGeologicalBlockMesh(*GeologicalMesh);
	}

	OutGeneratedFaceCount = GeneratedTriangleCount;
}
