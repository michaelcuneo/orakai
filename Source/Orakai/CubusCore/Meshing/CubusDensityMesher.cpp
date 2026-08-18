#include "CubusCore/Meshing/CubusDensityMesher.h"

#include "CubusCore/Chunks/CubusChunkConstants.h"
#include "CubusCore/Chunks/CubusDensitySamplingBuffer.h"
#include "CubusCore/Generation/CubusDensityField.h"
#include "CubusCore/Meshing/CubusDensityLod.h"
#include "CubusCore/Meshing/CubusTransvoxelTables.h"

namespace CubusDensityMesher
{
struct FMaterialBlend
{
	int32 MaterialIds[4] = {1, 1, 1, 1};
	float Weights[4]	 = {1.0f, 0.0f, 0.0f, 0.0f};
};

struct FInterpolatedVertex
{
	FVector		   LocalPosition		= FVector::ZeroVector;
	FVector		   GlobalSamplePosition = FVector::ZeroVector;
	FVector		   Normal				= FVector::UpVector;
	int32		   MaterialId			= 1;
	FMaterialBlend MaterialBlend;
};

struct FTriangleMaterialPalette
{
	int32 MaterialIds[4] = {1, 1, 1, 1};
	int32 Count			 = 1;

	int32 FindSlot(const int32 MaterialId) const
	{
		for (int32 Slot = 0; Slot < Count; ++Slot)
		{
			if (MaterialIds[Slot] == MaterialId)
			{
				return Slot;
			}
		}

		return INDEX_NONE;
	}
};

struct FWeightedMaterial
{
	int32 MaterialId = 1;
	float Weight	 = 0.0f;
};

const FIntVector CornerOffsets[8] = {FIntVector(0, 0, 0), FIntVector(1, 0, 0), FIntVector(1, 1, 0), FIntVector(0, 1, 0),
									 FIntVector(0, 0, 1), FIntVector(1, 0, 1), FIntVector(1, 1, 1), FIntVector(0, 1, 1)};

const int32 EdgeCornerIndices[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6}, {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};

constexpr int32 DensitySampleRowStride = FCubusDensitySamplingBuffer::SampleDimension;

constexpr int32 DensitySampleSliceStride = FCubusDensitySamplingBuffer::SampleDimension * FCubusDensitySamplingBuffer::SampleDimension;

/*
 * Flat offsets matching CornerOffsets exactly:
 *
 * 0 = (0,0,0)
 * 1 = (1,0,0)
 * 2 = (1,1,0)
 * 3 = (0,1,0)
 * 4 = (0,0,1)
 * 5 = (1,0,1)
 * 6 = (1,1,1)
 * 7 = (0,1,1)
 */
constexpr int32 CornerFlatOffsets[8] = {0,
										1,
										1 + DensitySampleRowStride,
										DensitySampleRowStride,
										DensitySampleSliceStride,
										DensitySampleSliceStride + 1,
										DensitySampleSliceStride + DensitySampleRowStride + 1,
										DensitySampleSliceStride + DensitySampleRowStride};

FVector ToVector(const FIntVector& Value)
{
	return FVector(Value.X, Value.Y, Value.Z);
}

constexpr int32 TransvoxelPointToOrakaiCorner[8] = {0, 1, 3, 2, 4, 5, 7, 6};

int32 ToTransvoxelRegularCase(const int32 OrakaiCaseIndex)
{
	int32 Result = 0;
	for (int32 Point = 0; Point < 8; ++Point)
	{
		const int32 Corner = TransvoxelPointToOrakaiCorner[Point];
		if ((OrakaiCaseIndex & (1 << Corner)) != 0)
			Result |= 1 << Point;
	}
	return Result;
}

int32 FindOrakaiEdge(const int32 A, const int32 B)
{
	for (int32 Edge = 0; Edge < 12; ++Edge)
	{
		const int32 EA = EdgeCornerIndices[Edge][0];
		const int32 EB = EdgeCornerIndices[Edge][1];
		if ((EA == A && EB == B) || (EA == B && EB == A))
			return Edge;
	}
	return INDEX_NONE;
}

int32 GetRegularTriangleEdge(const int32 OrakaiCaseIndex, const int32 Entry)
{
	using namespace CubusTransvoxelTables;
	if (Entry < 0)
		return -1;
	const int32				C	 = ToTransvoxelRegularCase(OrakaiCaseIndex);
	const FRegularCellData& Data = RegularCellData[RegularCellClass[C]];
	if (Entry >= Data.GetTriangleCount() * 3)
		return -1;
	const int32 V = Data.VertexIndex[Entry];
	if (V < 0 || V >= Data.GetVertexCount())
		return -1;
	const uint8 Code = static_cast<uint8>(RegularVertexData[C][V] & 0xFFu);
	const int32 PA	 = (Code >> 4) & 0xF;
	const int32 PB	 = Code & 0xF;
	if (PA >= 8 || PB >= 8)
		return -1;
	return FindOrakaiEdge(TransvoxelPointToOrakaiCorner[PA], TransvoxelPointToOrakaiCorner[PB]);
}

int32 ClampDensityMaterialId(const int32 MaterialId)
{
	return FMath::Clamp(MaterialId, 1, FCubusDensityMesher::MaximumDensityMaterialId);
}

void AddWeightedMaterial(FWeightedMaterial (&Materials)[8], int32& MaterialCount, const int32 MaterialId, const float Weight)
{
	if (MaterialId <= 0 || Weight <= UE_SMALL_NUMBER)
	{
		return;
	}

	const int32 ClampedMaterialId = ClampDensityMaterialId(MaterialId);

	for (int32 Index = 0; Index < MaterialCount; ++Index)
	{
		if (Materials[Index].MaterialId == ClampedMaterialId)
		{
			Materials[Index].Weight += Weight;
			return;
		}
	}

	if (MaterialCount >= 8)
	{
		return;
	}

	Materials[MaterialCount].MaterialId = ClampedMaterialId;

	Materials[MaterialCount].Weight = Weight;

	++MaterialCount;
}

void SortWeightedMaterials(FWeightedMaterial (&Materials)[8], const int32 MaterialCount)
{
	for (int32 Index = 1; Index < MaterialCount; ++Index)
	{
		const FWeightedMaterial Value = Materials[Index];

		int32 InsertIndex = Index;

		while (InsertIndex > 0)
		{
			const FWeightedMaterial& Previous = Materials[InsertIndex - 1];

			const bool bValueComesFirst = !FMath::IsNearlyEqual(Value.Weight, Previous.Weight) ? Value.Weight > Previous.Weight
																							   : Value.MaterialId < Previous.MaterialId;

			if (!bValueComesFirst)
			{
				break;
			}

			Materials[InsertIndex] = Previous;

			--InsertIndex;
		}

		Materials[InsertIndex] = Value;
	}
}

void SetSingleMaterialBlend(FMaterialBlend& Blend, const int32 MaterialId)
{
	const int32 ClampedMaterialId = ClampDensityMaterialId(MaterialId);
	for (int32 Slot = 0; Slot < 4; ++Slot)
	{
		Blend.MaterialIds[Slot] = ClampedMaterialId;
		Blend.Weights[Slot]		= Slot == 0 ? 1.0f : 0.0f;
	}
}

FMaterialBlend BuildCellMaterialBlend(const FCubusDensitySample (&CornerSamples)[8], const FVector& GlobalSamplePosition,
									  const FVector& GlobalCellOrigin, const float CellSize, const float IsoLevel,
									  const int32 FallbackMaterialId)
{
	FMaterialBlend Blend;
	SetSingleMaterialBlend(Blend, FallbackMaterialId);

	if (CellSize <= UE_SMALL_NUMBER)
	{
		return Blend;
	}

	const FVector LocalAlpha = (GlobalSamplePosition - GlobalCellOrigin) / CellSize;
	const FVector Alpha(FMath::Clamp(LocalAlpha.X, 0.0, 1.0), FMath::Clamp(LocalAlpha.Y, 0.0, 1.0), FMath::Clamp(LocalAlpha.Z, 0.0, 1.0));

	FWeightedMaterial Accumulated[8];
	int32			  AccumulatedCount = 0;

	for (int32 CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
	{
		const FCubusDensitySample& Sample = CornerSamples[CornerIndex];
		if (!Sample.IsSolid(IsoLevel) || Sample.MaterialId <= 0)
		{
			continue;
		}

		const FIntVector& Offset  = CornerOffsets[CornerIndex];
		const float		  WeightX = Offset.X == 0 ? 1.0f - static_cast<float>(Alpha.X) : static_cast<float>(Alpha.X);
		const float		  WeightY = Offset.Y == 0 ? 1.0f - static_cast<float>(Alpha.Y) : static_cast<float>(Alpha.Y);
		const float		  WeightZ = Offset.Z == 0 ? 1.0f - static_cast<float>(Alpha.Z) : static_cast<float>(Alpha.Z);

		AddWeightedMaterial(Accumulated, AccumulatedCount, Sample.MaterialId, WeightX * WeightY * WeightZ);
	}

	if (AccumulatedCount <= 0)
	{
		return Blend;
	}

	SortWeightedMaterials(Accumulated, AccumulatedCount);

	float TotalWeight = 0.0f;

	// Keep blends stable and readable by limiting each vertex to the
	// two strongest terrain materials.
	const int32 BlendCount = FMath::Min(AccumulatedCount, 2);

	for (int32 Slot = 0; Slot < BlendCount; ++Slot)
	{
		Blend.MaterialIds[Slot] = Accumulated[Slot].MaterialId;
		Blend.Weights[Slot]		= Accumulated[Slot].Weight;
		TotalWeight += Accumulated[Slot].Weight;
	}

	for (int32 Slot = BlendCount; Slot < 4; ++Slot)
	{
		Blend.MaterialIds[Slot] = Blend.MaterialIds[0];
		Blend.Weights[Slot]		= 0.0f;
	}

	if (TotalWeight <= UE_SMALL_NUMBER)
	{
		SetSingleMaterialBlend(Blend, FallbackMaterialId);
		return Blend;
	}

	for (int32 Slot = 0; Slot < 4; ++Slot)
	{
		Blend.Weights[Slot] /= TotalWeight;
	}

	return Blend;
}

FTriangleMaterialPalette BuildPalette(const FInterpolatedVertex (&Vertices)[3])
{
	int32 UniqueMaterialIds[12] = {};
	int32 UniqueMaterialCount	= 0;

	for (const FInterpolatedVertex& Vertex : Vertices)
	{
		for (int32 Slot = 0; Slot < 4; ++Slot)
		{
			if (Vertex.MaterialBlend.Weights[Slot] <= UE_SMALL_NUMBER)
			{
				continue;
			}

			const int32 MaterialId = ClampDensityMaterialId(Vertex.MaterialBlend.MaterialIds[Slot]);

			bool bAlreadyPresent = false;

			for (int32 ExistingIndex = 0; ExistingIndex < UniqueMaterialCount; ++ExistingIndex)
			{
				if (UniqueMaterialIds[ExistingIndex] == MaterialId)
				{
					bAlreadyPresent = true;
					break;
				}
			}

			if (!bAlreadyPresent && UniqueMaterialCount < 12)
			{
				UniqueMaterialIds[UniqueMaterialCount] = MaterialId;

				++UniqueMaterialCount;
			}
		}
	}

	for (int32 Index = 1; Index < UniqueMaterialCount; ++Index)
	{
		const int32 Value = UniqueMaterialIds[Index];

		int32 InsertIndex = Index;

		while (InsertIndex > 0 && UniqueMaterialIds[InsertIndex - 1] > Value)
		{
			UniqueMaterialIds[InsertIndex] = UniqueMaterialIds[InsertIndex - 1];

			--InsertIndex;
		}

		UniqueMaterialIds[InsertIndex] = Value;
	}

	FTriangleMaterialPalette Palette;

	if (UniqueMaterialCount <= 0)
	{
		Palette.MaterialIds[0] = ClampDensityMaterialId(Vertices[0].MaterialId);

		Palette.Count = 1;
	}
	else
	{
		Palette.Count = FMath::Clamp(UniqueMaterialCount, 1, 4);

		for (int32 Slot = 0; Slot < Palette.Count; ++Slot)
		{
			Palette.MaterialIds[Slot] = UniqueMaterialIds[Slot];
		}
	}

	for (int32 Slot = Palette.Count; Slot < 4; ++Slot)
	{
		Palette.MaterialIds[Slot] = Palette.MaterialIds[0];
	}

	return Palette;
}

FVector2D PackPalette(const FTriangleMaterialPalette& Palette)
{
	const int32 Base = FCubusDensityMesher::MaterialIdPackingBase;
	return FVector2D(static_cast<double>(Palette.MaterialIds[0] + Palette.MaterialIds[1] * Base),
					 static_cast<double>(Palette.MaterialIds[2] + Palette.MaterialIds[3] * Base));
}

FLinearColor BuildWeights(const FMaterialBlend& Blend, const FTriangleMaterialPalette& Palette)
{
	float PaletteWeights[4] = {0.0f, 0.0f, 0.0f, 0.0f};

	for (int32 BlendSlot = 0; BlendSlot < 4; ++BlendSlot)
	{
		const float Weight = Blend.Weights[BlendSlot];
		if (Weight <= UE_SMALL_NUMBER)
		{
			continue;
		}

		const int32 PaletteSlot = Palette.FindSlot(Blend.MaterialIds[BlendSlot]);
		if (PaletteSlot != INDEX_NONE)
		{
			PaletteWeights[PaletteSlot] += Weight;
		}
	}

	const float TotalWeight = PaletteWeights[0] + PaletteWeights[1] + PaletteWeights[2] + PaletteWeights[3];

	if (TotalWeight <= UE_SMALL_NUMBER)
	{
		return FLinearColor(1.0f, 0.0f, 0.0f, 0.0f);
	}

	return FLinearColor(PaletteWeights[0] / TotalWeight, PaletteWeights[1] / TotalWeight, PaletteWeights[2] / TotalWeight,
						PaletteWeights[3] / TotalWeight);
}

FVector ResolveTangentBasis(const FVector& FaceNormal)
{
	const FVector AbsoluteNormal(FMath::Abs(FaceNormal.X), FMath::Abs(FaceNormal.Y), FMath::Abs(FaceNormal.Z));

	if (AbsoluteNormal.X >= AbsoluteNormal.Y && AbsoluteNormal.X >= AbsoluteNormal.Z)
	{
		return FVector::RightVector;
	}

	return FVector::ForwardVector;
}

FCubusDensitySample SampleConsistentField(const ICubusDensityField& DensityField, const FVector& GlobalCoordinate)
{
	const FIntVector Rounded(FMath::RoundToInt(GlobalCoordinate.X), FMath::RoundToInt(GlobalCoordinate.Y),
							 FMath::RoundToInt(GlobalCoordinate.Z));
	const bool		 bCanonicalLatticePoint = FMath::IsNearlyEqual(GlobalCoordinate.X, static_cast<double>(Rounded.X), 1.0e-6) &&
											  FMath::IsNearlyEqual(GlobalCoordinate.Y, static_cast<double>(Rounded.Y), 1.0e-6) &&
											  FMath::IsNearlyEqual(GlobalCoordinate.Z, static_cast<double>(Rounded.Z), 1.0e-6);

	return bCanonicalLatticePoint ? DensityField.Sample(Rounded) : DensityField.SampleContinuous(GlobalCoordinate);
}

struct FTransitionFaceBasis
{
	FVector BoundaryOrigin = FVector::ZeroVector;
	FVector Inward		   = FVector::ZeroVector;
	FVector U			   = FVector::ZeroVector;
	FVector V			   = FVector::ZeroVector;
};

FTransitionFaceBasis GetTransitionFaceBasis(const ECubusDensityFace Face)
{
	FTransitionFaceBasis Result;
	switch (Face)
	{
	case ECubusDensityFace::NegativeX:
		Result.BoundaryOrigin = FVector(0.0, 0.0, Cubus::ChunkSize);
		Result.Inward		  = FVector(1.0, 0.0, 0.0);
		Result.U			  = FVector(0.0, 1.0, 0.0);
		Result.V			  = FVector(0.0, 0.0, -1.0);
		break;
	case ECubusDensityFace::PositiveX:
		Result.BoundaryOrigin = FVector(Cubus::ChunkSize, 0.0, 0.0);
		Result.Inward		  = FVector(-1.0, 0.0, 0.0);
		Result.U			  = FVector(0.0, 1.0, 0.0);
		Result.V			  = FVector(0.0, 0.0, 1.0);
		break;
	case ECubusDensityFace::NegativeY:
		Result.BoundaryOrigin = FVector(0.0, 0.0, 0.0);
		Result.Inward		  = FVector(0.0, 1.0, 0.0);
		Result.U			  = FVector(1.0, 0.0, 0.0);
		Result.V			  = FVector(0.0, 0.0, 1.0);
		break;
	case ECubusDensityFace::PositiveY:
		Result.BoundaryOrigin = FVector(0.0, Cubus::ChunkSize, Cubus::ChunkSize);
		Result.Inward		  = FVector(0.0, -1.0, 0.0);
		Result.U			  = FVector(1.0, 0.0, 0.0);
		Result.V			  = FVector(0.0, 0.0, -1.0);
		break;
	case ECubusDensityFace::NegativeZ:
		Result.BoundaryOrigin = FVector(0.0, Cubus::ChunkSize, 0.0);
		Result.Inward		  = FVector(0.0, 0.0, 1.0);
		Result.U			  = FVector(1.0, 0.0, 0.0);
		Result.V			  = FVector(0.0, -1.0, 0.0);
		break;
	case ECubusDensityFace::PositiveZ:
		Result.BoundaryOrigin = FVector(0.0, 0.0, Cubus::ChunkSize);
		Result.Inward		  = FVector(0.0, 0.0, -1.0);
		Result.U			  = FVector(1.0, 0.0, 0.0);
		Result.V			  = FVector(0.0, 1.0, 0.0);
		break;
	default:
		break;
	}

	checkSlow(FVector::DotProduct(FVector::CrossProduct(Result.U, Result.V), -Result.Inward) > 0.99);
	return Result;
}

FVector ApplyTransitionTransform(const FVector& LocalSamplePosition, const int32 SelfSubdivisions,
								 const FCubusDensityTransitionFaces& TransitionFaces)
{
	FVector		 Result				 = LocalSamplePosition;
	const int32	 SafeSubdivisions	 = FCubusDensityLod::NormalizeSubdivisions(SelfSubdivisions);
	const double CoarseSpacing		 = 1.0 / static_cast<double>(SafeSubdivisions);
	const double TransitionThickness = CoarseSpacing * 0.5;

	for (int32 FaceIndex = 0; FaceIndex < static_cast<int32>(ECubusDensityFace::Count); ++FaceIndex)
	{
		const ECubusDensityFace Face = static_cast<ECubusDensityFace>(FaceIndex);
		if (!TransitionFaces.HasFinerNeighbour(Face, SafeSubdivisions))
		{
			continue;
		}

		const FTransitionFaceBasis Basis				= GetTransitionFaceBasis(Face);
		const double			   DistanceFromBoundary = FVector::DotProduct(LocalSamplePosition - Basis.BoundaryOrigin, Basis.Inward);
		if (DistanceFromBoundary < -UE_KINDA_SMALL_NUMBER || DistanceFromBoundary >= CoarseSpacing)
		{
			continue;
		}

		const double Weight = 1.0 - FMath::Clamp(DistanceFromBoundary / CoarseSpacing, 0.0, 1.0);
		Result += Basis.Inward * (TransitionThickness * Weight);
	}
	return Result;
}

FVector SampleFieldGradient(const ICubusDensityField& DensityField, const FVector& GlobalCoordinate, const float Step)
{
	const float SafeStep  = FMath::Max(Step, 0.0001f);
	const float NegativeX = SampleConsistentField(DensityField, GlobalCoordinate - FVector(SafeStep, 0.0, 0.0)).Density;
	const float PositiveX = SampleConsistentField(DensityField, GlobalCoordinate + FVector(SafeStep, 0.0, 0.0)).Density;
	const float NegativeY = SampleConsistentField(DensityField, GlobalCoordinate - FVector(0.0, SafeStep, 0.0)).Density;
	const float PositiveY = SampleConsistentField(DensityField, GlobalCoordinate + FVector(0.0, SafeStep, 0.0)).Density;
	const float NegativeZ = SampleConsistentField(DensityField, GlobalCoordinate - FVector(0.0, 0.0, SafeStep)).Density;
	const float PositiveZ = SampleConsistentField(DensityField, GlobalCoordinate + FVector(0.0, 0.0, SafeStep)).Density;
	return FVector(PositiveX - NegativeX, PositiveY - NegativeY, PositiveZ - NegativeZ) / (2.0f * SafeStep);
}

FInterpolatedVertex InterpolateEdge(const FCubusDensitySamplingBuffer& DensityBuffer, const FCubusDensitySample& SampleA,
									const FCubusDensitySample& SampleB, const FVector& GradientA, const FVector& GradientB,
									const FIntVector& LocalSampleA, const FIntVector& LocalSampleB, const FVector& ChunkMinimum,
									const float VoxelSize, const float IsoLevel, const int32 SelfSubdivisions,
									const FCubusDensityTransitionFaces& TransitionFaces)
{
	const float DensityDelta = SampleB.Density - SampleA.Density;

	const float Alpha = FMath::IsNearlyZero(DensityDelta) ? 0.5f : FMath::Clamp((IsoLevel - SampleA.Density) / DensityDelta, 0.0f, 1.0f);

	const FVector LocalSamplePosition =
		FMath::Lerp(ToVector(LocalSampleA), ToVector(LocalSampleB), Alpha) + DensityBuffer.GetSampleOffsetInVoxels();

	const FVector GlobalSampleOrigin = ToVector(DensityBuffer.GetChunkCoordinate() * Cubus::ChunkSize);

	const FVector InterpolatedGradient = FMath::Lerp(GradientA, GradientB, Alpha);

	const bool bSampleAIsSolid = SampleA.IsSolid(IsoLevel);

	FInterpolatedVertex Result;

	Result.LocalPosition = ChunkMinimum + ApplyTransitionTransform(LocalSamplePosition, SelfSubdivisions, TransitionFaces) * VoxelSize;

	Result.GlobalSamplePosition = GlobalSampleOrigin + LocalSamplePosition;

	Result.Normal = (-InterpolatedGradient).GetSafeNormal();

	Result.MaterialId = ClampDensityMaterialId(bSampleAIsSolid ? SampleA.MaterialId : SampleB.MaterialId);

	SetSingleMaterialBlend(Result.MaterialBlend, Result.MaterialId);

	if (Result.Normal.IsNearlyZero())
	{
		const FVector SolidToEmpty = bSampleAIsSolid ? ToVector(LocalSampleB - LocalSampleA) : ToVector(LocalSampleA - LocalSampleB);

		Result.Normal = SolidToEmpty.GetSafeNormal();
	}

	if (Result.Normal.IsNearlyZero())
	{
		Result.Normal = FVector::UpVector;
	}

	return Result;
}

class FAdaptiveSampleCache
{
public:
	FAdaptiveSampleCache(const ICubusDensityField& InDensityField, const FIntVector& InChunkCoordinate, const int32 InSubdivisions)
		: DensityField(InDensityField), GlobalSampleOrigin(ToVector(InChunkCoordinate * Cubus::ChunkSize)),
		  SampleSpacing(1.0f / static_cast<float>(InSubdivisions)), FineChunkSize(Cubus::ChunkSize * InSubdivisions),
		  PackedCoordinateExtent(FineChunkSize + 3)
	{
		/*
		 * Only a fraction of the full fine lattice is sampled because coarse
		 * cells are rejected before fine marching cubes runs.
		 *
		 * Reserve enough for the common surface band so the maps avoid repeated
		 * allocation/rehash without allocating a dense 3D volume.
		 */
		Samples.Reserve(8192);

		Gradients.Reserve(4096);
	}

	int32 PackCoordinate(const FIntVector& FineCoordinate) const
	{
		/*
		 * Gradient evaluation can request one fine sample beyond the nominal
		 * chunk range, so shift by +1 before packing:
		 *
		 *     -1 .. FineChunkSize + 1
		 *
		 * becomes:
		 *
		 *      0 .. FineChunkSize + 2
		 */
		const int32 X = FineCoordinate.X + 1;

		const int32 Y = FineCoordinate.Y + 1;

		const int32 Z = FineCoordinate.Z + 1;

		return X + Y * PackedCoordinateExtent + Z * PackedCoordinateExtent * PackedCoordinateExtent;
	}

	const FCubusDensitySample& GetSample(const FIntVector& FineCoordinate)
	{
		const int32 PackedCoordinate = PackCoordinate(FineCoordinate);

		if (const FCubusDensitySample* Existing = Samples.Find(PackedCoordinate))
		{
			return *Existing;
		}

		const FVector GlobalCoordinate = GetGlobalCoordinate(FineCoordinate);

		FCubusDensitySample& AddedSample = Samples.Add(PackedCoordinate, SampleConsistentField(DensityField, GlobalCoordinate));

		return AddedSample;
	}

	FVector GetGradient(const FIntVector& FineCoordinate)
	{
		const int32 PackedCoordinate = PackCoordinate(FineCoordinate);

		if (const FVector* Existing = Gradients.Find(PackedCoordinate))
		{
			return *Existing;
		}

		const float NegativeX = GetSample(FineCoordinate - FIntVector(1, 0, 0)).Density;
		const float PositiveX = GetSample(FineCoordinate + FIntVector(1, 0, 0)).Density;
		const float NegativeY = GetSample(FineCoordinate - FIntVector(0, 1, 0)).Density;
		const float PositiveY = GetSample(FineCoordinate + FIntVector(0, 1, 0)).Density;
		const float NegativeZ = GetSample(FineCoordinate - FIntVector(0, 0, 1)).Density;
		const float PositiveZ = GetSample(FineCoordinate + FIntVector(0, 0, 1)).Density;

		const FVector Gradient(PositiveX - NegativeX, PositiveY - NegativeY, PositiveZ - NegativeZ);

		FVector& AddedGradient = Gradients.Add(PackedCoordinate, Gradient / FMath::Max(2.0f * SampleSpacing, UE_SMALL_NUMBER));

		return AddedGradient;
	}

	FVector GetGlobalCoordinate(const FIntVector& FineCoordinate) const
	{
		return GlobalSampleOrigin + ToVector(FineCoordinate) * SampleSpacing;
	}

	FVector GetLocalCoordinate(const FIntVector& FineCoordinate) const
	{
		return ToVector(FineCoordinate) * SampleSpacing + DensityField.GetSampleOffsetInVoxels();
	}

	float GetSampleSpacing() const { return SampleSpacing; }

private:
	const ICubusDensityField& DensityField;

	FVector GlobalSampleOrigin = FVector::ZeroVector;

	float SampleSpacing = 1.0f;

	int32 FineChunkSize = Cubus::ChunkSize;

	int32 PackedCoordinateExtent = Cubus::ChunkSize + 3;

	TMap<int32, FCubusDensitySample> Samples;

	TMap<int32, FVector> Gradients;
};

bool CellMayContainFineSurface(FAdaptiveSampleCache& SampleCache, const FIntVector& CoarseCellOrigin, const int32 Subdivisions,
							   const float IsoLevel)
{
	const FIntVector FineCellOrigin = CoarseCellOrigin * Subdivisions;

	bool  bAnySolid				 = false;
	bool  bAnyEmpty				 = false;
	float MinimumDistanceFromIso = MAX_flt;

	for (int32 CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
	{
		const FIntVector		  FineCorner   = FineCellOrigin + CornerOffsets[CornerIndex] * Subdivisions;
		const FCubusDensitySample CornerSample = SampleCache.GetSample(FineCorner);
		bAnySolid |= CornerSample.IsSolid(IsoLevel);
		bAnyEmpty |= !CornerSample.IsSolid(IsoLevel);
		MinimumDistanceFromIso = FMath::Min(MinimumDistanceFromIso, FMath::Abs(CornerSample.Density - IsoLevel));
	}

	if (bAnySolid && bAnyEmpty)
	{
		return true;
	}

	/*
	 * Do not let the canonical 80 cm lattice decide whether 20 cm detail is
	 * allowed to exist. Terrain fine relief is capped at 0.34 voxel and
	 * geology at 0.72 voxel; a 2.0-voxel conservative band therefore
	 * guarantees that a bounded interior zero-crossing is refined rather
	 * than discarded before the fine lattice is sampled.
	 */
	if (MinimumDistanceFromIso <= 2.0f)
	{
		return true;
	}

	const int32		 Half			= Subdivisions / 2;
	const FIntVector ProbeOffsets[] = {
		FIntVector(Half, Half, Half),		  FIntVector(0, Half, Half), FIntVector(Subdivisions, Half, Half), FIntVector(Half, 0, Half),
		FIntVector(Half, Subdivisions, Half), FIntVector(Half, Half, 0), FIntVector(Half, Half, Subdivisions)};

	for (const FIntVector& ProbeOffset : ProbeOffsets)
	{
		const FCubusDensitySample Probe = SampleCache.GetSample(FineCellOrigin + ProbeOffset);
		if (Probe.IsSolid(IsoLevel) != bAnySolid || FMath::Abs(Probe.Density - IsoLevel) <= 1.0f)
		{
			return true;
		}
	}

	return false;
}

FInterpolatedVertex InterpolateAdaptiveEdge(FAdaptiveSampleCache& SampleCache, const FIntVector& FineSampleA, const FIntVector& FineSampleB,
											const FVector& ChunkMinimum, const float CanonicalVoxelSize, const float IsoLevel,
											const int32 SelfSubdivisions, const FCubusDensityTransitionFaces& TransitionFaces)
{
	const FCubusDensitySample SampleA = SampleCache.GetSample(FineSampleA);
	const FCubusDensitySample SampleB = SampleCache.GetSample(FineSampleB);

	const float DensityDelta = SampleB.Density - SampleA.Density;
	const float Alpha = FMath::IsNearlyZero(DensityDelta) ? 0.5f : FMath::Clamp((IsoLevel - SampleA.Density) / DensityDelta, 0.0f, 1.0f);

	const FVector LocalSamplePosition =
		FMath::Lerp(SampleCache.GetLocalCoordinate(FineSampleA), SampleCache.GetLocalCoordinate(FineSampleB), Alpha);

	const FVector GlobalSamplePosition =
		FMath::Lerp(SampleCache.GetGlobalCoordinate(FineSampleA), SampleCache.GetGlobalCoordinate(FineSampleB), Alpha);

	const FVector InterpolatedGradient = FMath::Lerp(SampleCache.GetGradient(FineSampleA), SampleCache.GetGradient(FineSampleB), Alpha);

	const bool bSampleAIsSolid = SampleA.IsSolid(IsoLevel);

	FInterpolatedVertex Result;
	Result.LocalPosition =
		ChunkMinimum + ApplyTransitionTransform(LocalSamplePosition, SelfSubdivisions, TransitionFaces) * CanonicalVoxelSize;
	Result.GlobalSamplePosition = GlobalSamplePosition;
	Result.Normal				= (-InterpolatedGradient).GetSafeNormal();
	Result.MaterialId			= ClampDensityMaterialId(bSampleAIsSolid ? SampleA.MaterialId : SampleB.MaterialId);
	SetSingleMaterialBlend(Result.MaterialBlend, Result.MaterialId);

	if (Result.Normal.IsNearlyZero())
	{
		const FVector SolidToEmpty = bSampleAIsSolid
										 ? SampleCache.GetLocalCoordinate(FineSampleB) - SampleCache.GetLocalCoordinate(FineSampleA)
										 : SampleCache.GetLocalCoordinate(FineSampleA) - SampleCache.GetLocalCoordinate(FineSampleB);
		Result.Normal			   = SolidToEmpty.GetSafeNormal();
	}

	if (Result.Normal.IsNearlyZero())
	{
		Result.Normal = FVector::UpVector;
	}

	return Result;
}

bool AddTriangle(FCubusMeshData& MeshData, FInterpolatedVertex VertexA, FInterpolatedVertex VertexB, FInterpolatedVertex VertexC)
{
	const FVector EdgeAB = VertexB.LocalPosition - VertexA.LocalPosition;

	const FVector EdgeAC = VertexC.LocalPosition - VertexA.LocalPosition;

	const FVector EdgeBC = VertexC.LocalPosition - VertexB.LocalPosition;

	/*
	 * Reject non-finite geometry before it can reach rendering or Chaos.
	 */
	if (!FMath::IsFinite(VertexA.LocalPosition.X) || !FMath::IsFinite(VertexA.LocalPosition.Y) ||
		!FMath::IsFinite(VertexA.LocalPosition.Z) || !FMath::IsFinite(VertexB.LocalPosition.X) ||
		!FMath::IsFinite(VertexB.LocalPosition.Y) || !FMath::IsFinite(VertexB.LocalPosition.Z) ||
		!FMath::IsFinite(VertexC.LocalPosition.X) || !FMath::IsFinite(VertexC.LocalPosition.Y) || !FMath::IsFinite(VertexC.LocalPosition.Z))
	{
		return false;
	}

	/*
	 * Reject collapsed edges.
	 *
	 * Density vertices are expressed in centimetres, so anything below one
	 * hundredth of a centimetre is not useful terrain geometry or collision.
	 */
	constexpr double MinimumEdgeLengthSquared = 0.01 * 0.01;

	if (EdgeAB.SizeSquared() <= MinimumEdgeLengthSquared || EdgeAC.SizeSquared() <= MinimumEdgeLengthSquared ||
		EdgeBC.SizeSquared() <= MinimumEdgeLengthSquared)
	{
		return false;
	}

	FVector WindingCrossNormal = FVector::CrossProduct(EdgeAB, EdgeAC);

	/*
	 * Cross-product magnitude is twice the triangle area.
	 *
	 * SMALL_NUMBER is much too small for centimetre-scale collision geometry
	 * and allows extremely thin sliver triangles through to Chaos.
	 */
	constexpr double MinimumDoubleAreaSquared = 0.01 * 0.01;

	if (!FMath::IsFinite(WindingCrossNormal.X) || !FMath::IsFinite(WindingCrossNormal.Y) || !FMath::IsFinite(WindingCrossNormal.Z) ||
		WindingCrossNormal.SizeSquared() <= MinimumDoubleAreaSquared)
	{
		return false;
	}

	WindingCrossNormal.Normalize();

	const FVector AverageNormal = (VertexA.Normal + VertexB.Normal + VertexC.Normal).GetSafeNormal();

	if (!AverageNormal.IsNearlyZero() && FVector::DotProduct(WindingCrossNormal, AverageNormal) > 0.0)
	{
		Swap(VertexB, VertexC);
		WindingCrossNormal *= -1.0;
	}

	const FInterpolatedVertex	   Vertices[3]		= {VertexA, VertexB, VertexC};
	const FTriangleMaterialPalette Palette			= BuildPalette(Vertices);
	const FVector2D				   PackedPalette	= PackPalette(Palette);
	const FVector				   TangentBasis		= ResolveTangentBasis(WindingCrossNormal);
	const int32					   FirstVertexIndex = MeshData.Vertices.Num();

	MeshData.Vertices.Append({VertexA.LocalPosition, VertexB.LocalPosition, VertexC.LocalPosition});
	MeshData.Triangles.Append({FirstVertexIndex, FirstVertexIndex + 1, FirstVertexIndex + 2});

	for (const FInterpolatedVertex& Vertex : Vertices)
	{
		FVector TangentDirection = (TangentBasis - Vertex.Normal * FVector::DotProduct(TangentBasis, Vertex.Normal)).GetSafeNormal();

		if (TangentDirection.IsNearlyZero())
		{
			TangentDirection = FVector::CrossProduct(FVector::UpVector, Vertex.Normal).GetSafeNormal();
		}

		if (TangentDirection.IsNearlyZero())
		{
			TangentDirection = FVector::ForwardVector;
		}

		MeshData.Normals.Add(Vertex.Normal);
		MeshData.UV0.Add(PackedPalette);
		MeshData.VertexColors.Add(BuildWeights(Vertex.MaterialBlend, Palette));
		MeshData.Tangents.Add(FProcMeshTangent(TangentDirection, false));
	}

	return true;
}

struct FTransitionPoint
{
	FCubusDensitySample Sample;
	FVector				LocalGeometryPosition = FVector::ZeroVector;
	FVector				GlobalSamplePosition  = FVector::ZeroVector;
	FVector				Gradient			  = FVector::ZeroVector;
};

FInterpolatedVertex InterpolateTransitionEdge(const FTransitionPoint& PointA, const FTransitionPoint& PointB, const FVector& ChunkMinimum,
											  const float CanonicalVoxelSize, const float IsoLevel)
{
	const float DensityDelta = PointB.Sample.Density - PointA.Sample.Density;
	const float Alpha =
		FMath::IsNearlyZero(DensityDelta) ? 0.5f : FMath::Clamp((IsoLevel - PointA.Sample.Density) / DensityDelta, 0.0f, 1.0f);

	const bool			bAIsSolid = PointA.Sample.IsSolid(IsoLevel);
	FInterpolatedVertex Result;
	Result.LocalPosition =
		ChunkMinimum + FMath::Lerp(PointA.LocalGeometryPosition, PointB.LocalGeometryPosition, Alpha) * CanonicalVoxelSize;
	Result.GlobalSamplePosition = FMath::Lerp(PointA.GlobalSamplePosition, PointB.GlobalSamplePosition, Alpha);
	Result.Normal				= (-FMath::Lerp(PointA.Gradient, PointB.Gradient, Alpha)).GetSafeNormal();
	Result.MaterialId			= ClampDensityMaterialId(bAIsSolid ? PointA.Sample.MaterialId : PointB.Sample.MaterialId);
	SetSingleMaterialBlend(Result.MaterialBlend, Result.MaterialId);

	if (Result.Normal.IsNearlyZero())
	{
		const FVector SolidToEmpty = bAIsSolid ? PointB.LocalGeometryPosition - PointA.LocalGeometryPosition
											   : PointA.LocalGeometryPosition - PointB.LocalGeometryPosition;
		Result.Normal			   = SolidToEmpty.GetSafeNormal();
	}
	if (Result.Normal.IsNearlyZero())
	{
		Result.Normal = FVector::UpVector;
	}
	return Result;
}

void BuildTransitionCells(const ICubusDensityField& DensityField, const FIntVector& ChunkCoordinate, const float CanonicalVoxelSize,
						  const int32 SelfSubdivisions, const float IsoLevel, const FCubusDensityTransitionFaces& TransitionFaces,
						  FCubusMeshData& UnifiedMesh, int32& InOutGeneratedTriangleCount)
{
	using namespace CubusTransvoxelTables;

	const int32	  Subdivisions		  = FCubusDensityLod::NormalizeSubdivisions(SelfSubdivisions);
	const float	  CoarseSpacing		  = 1.0f / static_cast<float>(Subdivisions);
	const float	  FineSpacing		  = CoarseSpacing * 0.5f;
	const float	  TransitionThickness = CoarseSpacing * 0.5f;
	const int32	  FaceCellCount		  = Cubus::ChunkSize * Subdivisions;
	const FVector ChunkGlobalOrigin	  = ToVector(ChunkCoordinate * Cubus::ChunkSize);
	const float	  ChunkWorldSize	  = static_cast<float>(Cubus::ChunkSize) * CanonicalVoxelSize;
	const FVector ChunkMinimum(-ChunkWorldSize * 0.5f, -ChunkWorldSize * 0.5f, -ChunkWorldSize * 0.5f);

	/*
	 * Official Transvoxel transition-point numbering around the high-res
	 * face is perimeter-first plus centre:
	 *
	 *     6 -- 5 -- 4
	 *     |    |    |
	 *     7 -- 8 -- 3
	 *     |    |    |
	 *     0 -- 1 -- 2
	 *
	 * Low-resolution duplicate points are 9=0, A=2, B=6, C=4 and are
	 * displaced inward geometrically while retaining the coarse-corner
	 * scalar values. This is what lets the transition topology match both
	 * the fine boundary contour and the transformed coarse regular mesh.
	 */
	static constexpr int32 HighPointGrid[9][2]	= {{0, 0}, {1, 0}, {2, 0}, {2, 1}, {2, 2}, {1, 2}, {0, 2}, {0, 1}, {1, 1}};
	static constexpr int32 LowPointHighIndex[4] = {0, 2, 6, 4};

	for (int32 FaceIndex = 0; FaceIndex < static_cast<int32>(ECubusDensityFace::Count); ++FaceIndex)
	{
		const ECubusDensityFace Face = static_cast<ECubusDensityFace>(FaceIndex);
		if (!TransitionFaces.HasFinerNeighbour(Face, Subdivisions))
		{
			continue;
		}

		const FTransitionFaceBasis Basis = GetTransitionFaceBasis(Face);
		for (int32 VCell = 0; VCell < FaceCellCount; ++VCell)
		{
			for (int32 UCell = 0; UCell < FaceCellCount; ++UCell)
			{
				const float		 U0 = static_cast<float>(UCell) * CoarseSpacing;
				const float		 V0 = static_cast<float>(VCell) * CoarseSpacing;
				FTransitionPoint Points[13];
				int32			 CaseIndex = 0;

				for (int32 PointIndex = 0; PointIndex < 9; ++PointIndex)
				{
					const float		  U			= U0 + static_cast<float>(HighPointGrid[PointIndex][0]) * FineSpacing;
					const float		  V			= V0 + static_cast<float>(HighPointGrid[PointIndex][1]) * FineSpacing;
					FTransitionPoint& Point		= Points[PointIndex];
					Point.LocalGeometryPosition = Basis.BoundaryOrigin + Basis.U * U + Basis.V * V;
					Point.GlobalSamplePosition	= ChunkGlobalOrigin + Point.LocalGeometryPosition;
					Point.Sample				= SampleConsistentField(DensityField, Point.GlobalSamplePosition);
					Point.Gradient				= SampleFieldGradient(DensityField, Point.GlobalSamplePosition, FineSpacing);
					if (Point.Sample.IsSolid(IsoLevel))
					{
						CaseIndex |= 1 << PointIndex;
					}
				}

				if (CaseIndex == 0 || CaseIndex == 511)
				{
					continue;
				}

				for (int32 LowIndex = 0; LowIndex < 4; ++LowIndex)
				{
					const int32 HighIndex = LowPointHighIndex[LowIndex];
					Points[9 + LowIndex]  = Points[HighIndex];
					Points[9 + LowIndex].LocalGeometryPosition += Basis.Inward * TransitionThickness;
				}

				const uint8				   RawClass		   = TransitionCellClass[CaseIndex];
				const bool				   bReverseWinding = (RawClass & 0x80u) != 0;
				const FTransitionCellData& CellData		   = TransitionCellData[RawClass & 0x7Fu];
				const int32				   VertexCount	   = CellData.GetVertexCount();
				FInterpolatedVertex		   CellVertices[12];

				for (int32 VertexIndex = 0; VertexIndex < VertexCount; ++VertexIndex)
				{
					const uint16 VertexData	  = TransitionVertexData[CaseIndex][VertexIndex];
					const uint8	 EndpointCode = static_cast<uint8>(VertexData & 0x00FFu);
					const int32	 PointA		  = (EndpointCode >> 4) & 0x0F;
					const int32	 PointB		  = EndpointCode & 0x0F;
					if (PointA >= 13 || PointB >= 13)
					{
						ensureMsgf(false, TEXT("Invalid Transvoxel endpoint %d-%d for case %d"), PointA, PointB, CaseIndex);
						continue;
					}
					CellVertices[VertexIndex] =
						InterpolateTransitionEdge(Points[PointA], Points[PointB], ChunkMinimum, CanonicalVoxelSize, IsoLevel);

					FInterpolatedVertex&	  SurfaceVertex		 = CellVertices[VertexIndex];
					constexpr float			  MaterialProbeDepth = 0.001f;
					const FCubusDensitySample SurfaceSample =
						DensityField.SampleContinuous(SurfaceVertex.GlobalSamplePosition - SurfaceVertex.Normal * MaterialProbeDepth);
					if (SurfaceSample.MaterialId > 0)
					{
						SurfaceVertex.MaterialId = ClampDensityMaterialId(SurfaceSample.MaterialId);
						SetSingleMaterialBlend(SurfaceVertex.MaterialBlend, SurfaceVertex.MaterialId);
					}
				}

				const int32 TriangleCount = CellData.GetTriangleCount();
				for (int32 TriangleIndex = 0; TriangleIndex < TriangleCount; ++TriangleIndex)
				{
					int32 A = CellData.VertexIndex[TriangleIndex * 3 + 0];
					int32 B = CellData.VertexIndex[TriangleIndex * 3 + 1];
					int32 C = CellData.VertexIndex[TriangleIndex * 3 + 2];
					if (A >= VertexCount || B >= VertexCount || C >= VertexCount)
					{
						ensureMsgf(false, TEXT("Invalid Transvoxel triangle indices for case %d"), CaseIndex);
						continue;
					}
					if (bReverseWinding)
					{
						Swap(B, C);
					}

					if (AddTriangle(UnifiedMesh, CellVertices[A], CellVertices[B], CellVertices[C]))
					{
						++InOutGeneratedTriangleCount;
					}
				}
			}
		}
	}
}

} // namespace CubusDensityMesher

void FCubusDensityMesher::BuildChunk(const FCubusDensitySamplingBuffer& DensityBuffer, const float VoxelSize, const float IsoLevel,
									 TMap<int32, FCubusMeshData>& OutMaterialMeshes, int32& OutGeneratedTriangleCount,
									 const ICubusDensityField* SurfaceMaterialField, const ICubusDensityField* TransitionField,
									 const FCubusDensityTransitionFaces& TransitionFaces)
{
	using namespace CubusDensityMesher;

	OutMaterialMeshes.Reset();
	OutGeneratedTriangleCount = 0;

	if (!DensityBuffer.IsBuilt() || VoxelSize <= 0.0f)
	{
		return;
	}

	FCubusMeshData& UnifiedMesh = OutMaterialMeshes.FindOrAdd(UnifiedDensityMaterialKey);

	const float ChunkWorldSize = static_cast<float>(Cubus::ChunkSize) * VoxelSize;

	const FVector ChunkMinimum(ChunkWorldSize * -0.5f, ChunkWorldSize * -0.5f, ChunkWorldSize * -0.5f);

	const FIntVector GlobalChunkOrigin = DensityBuffer.GetChunkCoordinate() * Cubus::ChunkSize;

	for (int32 LocalZ = 0; LocalZ < Cubus::ChunkSize; ++LocalZ)
	{
		for (int32 LocalY = 0; LocalY < Cubus::ChunkSize; ++LocalY)
		{
			for (int32 LocalX = 0; LocalX < Cubus::ChunkSize; ++LocalX)
			{
				const FIntVector CellOrigin(LocalX, LocalY, LocalZ);

				/*
				 * Buffered local coordinates begin at -1.
				 *
				 * Canonical local sample (0,0,0) therefore occupies buffered
				 * coordinate (1,1,1).
				 */
				const int32 BufferedX = LocalX - FCubusDensitySamplingBuffer::MinimumLocalSample;

				const int32 BufferedY = LocalY - FCubusDensitySamplingBuffer::MinimumLocalSample;

				const int32 BufferedZ = LocalZ - FCubusDensitySamplingBuffer::MinimumLocalSample;

				const int32 BaseSampleIndex = BufferedX + DensitySampleRowStride * (BufferedY + DensitySampleRowStride * BufferedZ);

				FCubusDensitySample CornerSamples[8];
				FIntVector			CornerCoordinates[8];
				int32				CornerFlatIndices[8];

				int32 CaseIndex = 0;

				for (int32 CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
				{
					CornerCoordinates[CornerIndex] = CellOrigin + CornerOffsets[CornerIndex];

					CornerFlatIndices[CornerIndex] = BaseSampleIndex + CornerFlatOffsets[CornerIndex];

					CornerSamples[CornerIndex] = DensityBuffer.GetSampleByFlatIndexChecked(CornerFlatIndices[CornerIndex]);

					if (CornerSamples[CornerIndex].IsSolid(IsoLevel))
					{
						CaseIndex |= 1 << CornerIndex;
					}
				}

				if (

					GetRegularTriangleEdge(CaseIndex, 0) < 0)
				{
					continue;
				}

				const FVector GlobalCellOrigin = ToVector(GlobalChunkOrigin + CellOrigin);

				FInterpolatedVertex EdgeVertices[12];
				bool				bEdgeVertexBuilt[12] = {};

				FVector CornerGradients[8];
				bool	bCornerGradientBuilt[8] = {};

				for (int32 TriangleEdgeIndex = 0; TriangleEdgeIndex < 16; TriangleEdgeIndex += 3)
				{
					if (

						GetRegularTriangleEdge(CaseIndex, TriangleEdgeIndex) < 0)
					{
						break;
					}

					FInterpolatedVertex TriangleVertices[3];

					bool bTriangleIsValid = true;

					for (int32 VertexIndex = 0; VertexIndex < 3; ++VertexIndex)
					{
						const int32 EdgeIndex =

							GetRegularTriangleEdge(CaseIndex, TriangleEdgeIndex + VertexIndex);

						if (EdgeIndex < 0 || EdgeIndex >= 12)
						{
							ensureMsgf(false,
									   TEXT("Invalid Marching Cubes edge %d "
											"for case %d at table index %d."),
									   EdgeIndex, CaseIndex, TriangleEdgeIndex + VertexIndex);

							bTriangleIsValid = false;
							break;
						}

						if (!bEdgeVertexBuilt[EdgeIndex])
						{
							const int32 CornerIndexA = EdgeCornerIndices[EdgeIndex][0];

							const int32 CornerIndexB = EdgeCornerIndices[EdgeIndex][1];

							if (!bCornerGradientBuilt[CornerIndexA])
							{
								CornerGradients[CornerIndexA] =
									DensityBuffer.GetGradientByFlatIndexChecked(CornerFlatIndices[CornerIndexA]);

								bCornerGradientBuilt[CornerIndexA] = true;
							}

							if (!bCornerGradientBuilt[CornerIndexB])
							{
								CornerGradients[CornerIndexB] =
									DensityBuffer.GetGradientByFlatIndexChecked(CornerFlatIndices[CornerIndexB]);

								bCornerGradientBuilt[CornerIndexB] = true;
							}

							EdgeVertices[EdgeIndex] = InterpolateEdge(
								DensityBuffer, CornerSamples[CornerIndexA], CornerSamples[CornerIndexB], CornerGradients[CornerIndexA],
								CornerGradients[CornerIndexB], CornerCoordinates[CornerIndexA], CornerCoordinates[CornerIndexB],
								ChunkMinimum, VoxelSize, IsoLevel, 1, TransitionFaces);

							if (SurfaceMaterialField != nullptr)
							{
								FInterpolatedVertex& SurfaceVertex = EdgeVertices[EdgeIndex];

								// Probe a tiny distance toward the solid side.
								// Coarse LOD passes its scaled canonical field,
								// so biome and slope are resolved at the actual
								// interpolated surface position instead of at
								// cell corners separated by an entire LOD stride.
								constexpr float MaterialProbeDepth = 0.001f;
								const FVector	MaterialProbePosition =
									SurfaceVertex.GlobalSamplePosition - SurfaceVertex.Normal * MaterialProbeDepth;

								const FCubusDensitySample SurfaceSample = SurfaceMaterialField->SampleContinuous(MaterialProbePosition);

								if (SurfaceSample.MaterialId > 0)
								{
									SurfaceVertex.MaterialId = ClampDensityMaterialId(SurfaceSample.MaterialId);

									SetSingleMaterialBlend(SurfaceVertex.MaterialBlend, SurfaceVertex.MaterialId);
								}
								else
								{
									SurfaceVertex.MaterialBlend =
										BuildCellMaterialBlend(CornerSamples, SurfaceVertex.GlobalSamplePosition, GlobalCellOrigin, 1.0f,
															   IsoLevel, SurfaceVertex.MaterialId);
								}
							}
							else
							{
								EdgeVertices[EdgeIndex].MaterialBlend =
									BuildCellMaterialBlend(CornerSamples, EdgeVertices[EdgeIndex].GlobalSamplePosition, GlobalCellOrigin,
														   1.0f, IsoLevel, EdgeVertices[EdgeIndex].MaterialId);
							}

							bEdgeVertexBuilt[EdgeIndex] = true;
						}

						TriangleVertices[VertexIndex] = EdgeVertices[EdgeIndex];
					}

					if (!bTriangleIsValid)
					{
						continue;
					}

					if (AddTriangle(UnifiedMesh, TriangleVertices[0], TriangleVertices[1], TriangleVertices[2]))
					{
						++OutGeneratedTriangleCount;
					}
				}
			}
		}
	}

	if (TransitionField != nullptr)
	{
		BuildTransitionCells(*TransitionField, DensityBuffer.GetChunkCoordinate(), VoxelSize, 1, IsoLevel, TransitionFaces, UnifiedMesh,
							 OutGeneratedTriangleCount);
	}

	if (UnifiedMesh.IsEmpty())
	{
		OutMaterialMeshes.Remove(UnifiedDensityMaterialKey);
	}
}

void FCubusDensityMesher::BuildAdaptiveChunk(const ICubusDensityField& DensityField, const FIntVector& ChunkCoordinate,
											 const float CanonicalVoxelSize, const int32 SubdivisionsPerVoxel, const float IsoLevel,
											 TMap<int32, FCubusMeshData>& OutMaterialMeshes, int32& OutGeneratedTriangleCount,
											 const FCubusDensityTransitionFaces& TransitionFaces,
											 const ICubusDensityField*			 SurfaceMaterialField)
{
	using namespace CubusDensityMesher;

	const int32 Subdivisions = FCubusDensityLod::NormalizeSubdivisions(SubdivisionsPerVoxel);

	if (Subdivisions <= 1)
	{
		FCubusDensitySamplingBuffer DensityBuffer;
		DensityBuffer.Build(ChunkCoordinate, DensityField);
		BuildChunk(DensityBuffer, CanonicalVoxelSize, IsoLevel, OutMaterialMeshes, OutGeneratedTriangleCount, SurfaceMaterialField,
				   &DensityField, TransitionFaces);
		return;
	}

	OutMaterialMeshes.Reset();
	OutGeneratedTriangleCount = 0;

	if (CanonicalVoxelSize <= 0.0f)
	{
		return;
	}

	FCubusMeshData& UnifiedMesh = OutMaterialMeshes.FindOrAdd(UnifiedDensityMaterialKey);

	const float ChunkWorldSize = static_cast<float>(Cubus::ChunkSize) * CanonicalVoxelSize;

	const FVector ChunkMinimum(ChunkWorldSize * -0.5f, ChunkWorldSize * -0.5f, ChunkWorldSize * -0.5f);

	FAdaptiveSampleCache SampleCache(DensityField, ChunkCoordinate, Subdivisions);

	for (int32 CoarseZ = 0; CoarseZ < Cubus::ChunkSize; ++CoarseZ)
	{
		for (int32 CoarseY = 0; CoarseY < Cubus::ChunkSize; ++CoarseY)
		{
			for (int32 CoarseX = 0; CoarseX < Cubus::ChunkSize; ++CoarseX)
			{
				const FIntVector CoarseCellOrigin(CoarseX, CoarseY, CoarseZ);

				if (!CellMayContainFineSurface(SampleCache, CoarseCellOrigin, Subdivisions, IsoLevel))
				{
					continue;
				}

				const FIntVector FineCoarseOrigin = CoarseCellOrigin * Subdivisions;

				for (int32 SubZ = 0; SubZ < Subdivisions; ++SubZ)
				{
					for (int32 SubY = 0; SubY < Subdivisions; ++SubY)
					{
						for (int32 SubX = 0; SubX < Subdivisions; ++SubX)
						{
							const FIntVector FineCellOrigin = FineCoarseOrigin + FIntVector(SubX, SubY, SubZ);

							const FVector GlobalCellOrigin = SampleCache.GetGlobalCoordinate(FineCellOrigin);

							FCubusDensitySample CornerSamples[8];
							FIntVector			CornerCoordinates[8];
							int32				CaseIndex = 0;

							for (int32 CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
							{
								CornerCoordinates[CornerIndex] = FineCellOrigin + CornerOffsets[CornerIndex];

								CornerSamples[CornerIndex] = SampleCache.GetSample(CornerCoordinates[CornerIndex]);

								if (CornerSamples[CornerIndex].IsSolid(IsoLevel))
								{
									CaseIndex |= 1 << CornerIndex;
								}
							}

							if (GetRegularTriangleEdge(CaseIndex, 0) < 0)
							{
								continue;
							}

							FInterpolatedVertex EdgeVertices[12];
							bool				bEdgeVertexBuilt[12] = {};

							for (int32 TriangleEdgeIndex = 0; TriangleEdgeIndex < 16; TriangleEdgeIndex += 3)
							{
								if (GetRegularTriangleEdge(CaseIndex, TriangleEdgeIndex) < 0)
								{
									break;
								}

								FInterpolatedVertex TriangleVertices[3];
								bool				bTriangleIsValid = true;

								for (int32 VertexIndex = 0; VertexIndex < 3; ++VertexIndex)
								{
									const int32 EdgeIndex = GetRegularTriangleEdge(CaseIndex, TriangleEdgeIndex + VertexIndex);

									if (EdgeIndex < 0 || EdgeIndex >= 12)
									{
										ensureMsgf(false, TEXT("Invalid adaptive Marching Cubes edge %d for case %d at table index %d."),
												   EdgeIndex, CaseIndex, TriangleEdgeIndex + VertexIndex);
										bTriangleIsValid = false;
										break;
									}

									if (!bEdgeVertexBuilt[EdgeIndex])
									{
										const int32 CornerIndexA = EdgeCornerIndices[EdgeIndex][0];
										const int32 CornerIndexB = EdgeCornerIndices[EdgeIndex][1];

										EdgeVertices[EdgeIndex] = InterpolateAdaptiveEdge(
											SampleCache, CornerCoordinates[CornerIndexA], CornerCoordinates[CornerIndexB], ChunkMinimum,
											CanonicalVoxelSize, IsoLevel, Subdivisions, TransitionFaces);

										FInterpolatedVertex& SurfaceVertex = EdgeVertices[EdgeIndex];
										if (SurfaceMaterialField != nullptr)
										{
											constexpr float			  MaterialProbeDepth = 0.001f;
											const FCubusDensitySample SurfaceSample		 = SurfaceMaterialField->SampleContinuous(
												SurfaceVertex.GlobalSamplePosition - SurfaceVertex.Normal * MaterialProbeDepth);
											if (SurfaceSample.MaterialId > 0)
											{
												SurfaceVertex.MaterialId = ClampDensityMaterialId(SurfaceSample.MaterialId);
												SetSingleMaterialBlend(SurfaceVertex.MaterialBlend, SurfaceVertex.MaterialId);
											}
											else
											{
												SurfaceVertex.MaterialBlend = BuildCellMaterialBlend(
													CornerSamples, SurfaceVertex.GlobalSamplePosition, GlobalCellOrigin,
													SampleCache.GetSampleSpacing(), IsoLevel, SurfaceVertex.MaterialId);
											}
										}
										else
										{
											SurfaceVertex.MaterialBlend =
												BuildCellMaterialBlend(CornerSamples, SurfaceVertex.GlobalSamplePosition, GlobalCellOrigin,
																	   SampleCache.GetSampleSpacing(), IsoLevel, SurfaceVertex.MaterialId);
										}

										bEdgeVertexBuilt[EdgeIndex] = true;
									}

									TriangleVertices[VertexIndex] = EdgeVertices[EdgeIndex];
								}

								if (!bTriangleIsValid)
								{
									continue;
								}

								if (AddTriangle(UnifiedMesh, TriangleVertices[0], TriangleVertices[1], TriangleVertices[2]))
								{
									++OutGeneratedTriangleCount;
								}
							}
						}
					}
				}
			}
		}
	}

	BuildTransitionCells(DensityField, ChunkCoordinate, CanonicalVoxelSize, Subdivisions, IsoLevel, TransitionFaces, UnifiedMesh,
						 OutGeneratedTriangleCount);

	if (UnifiedMesh.IsEmpty())
	{
		OutMaterialMeshes.Remove(UnifiedDensityMaterialKey);
	}
}
