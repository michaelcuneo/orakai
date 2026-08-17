#include "CubusCore/Generation/CubusLandscapeEvolution.h"

#include "Algo/Sort.h"
#include "Containers/Queue.h"
#include "HAL/PlatformTime.h"

namespace CubusLandscapeEvolution
{
namespace
{
constexpr int32 D8X[8] = {1, 1, 0, -1, -1, -1, 0, 1};
constexpr int32 D8Y[8] = {0, 1, 1, 1, 0, -1, -1, -1};

float Hash01(uint32 V)
{
	V ^= V >> 16;
	V *= 0x7feb352du;
	V ^= V >> 15;
	V *= 0x846ca68bu;
	V ^= V >> 16;
	return static_cast<float>(V & 0x00ffffffu) / static_cast<float>(0x01000000u);
}

float ValueNoise2D(const FVector2D& P, const int32 Seed)
{
	const int32 X0 = FMath::FloorToInt(P.X);
	const int32 Y0 = FMath::FloorToInt(P.Y);
	const float Fx = static_cast<float>(P.X - X0);
	const float Fy = static_cast<float>(P.Y - Y0);
	const float Sx = Fx * Fx * (3.0f - 2.0f * Fx);
	const float Sy = Fy * Fy * (3.0f - 2.0f * Fy);

	auto H = [Seed](const int32 X, const int32 Y)
	{
		return Hash01(static_cast<uint32>(X * 73856093) ^ static_cast<uint32>(Y * 19349663) ^ static_cast<uint32>(Seed * 83492791));
	};

	const float A = FMath::Lerp(H(X0, Y0), H(X0 + 1, Y0), Sx);
	const float B = FMath::Lerp(H(X0, Y0 + 1), H(X0 + 1, Y0 + 1), Sx);
	return FMath::Lerp(A, B, Sy) * 2.0f - 1.0f;
}

float LongWaveField(const FVector2D& WorldMeters, const FSettings& Settings)
{
	const double MidWavelength = 0.5 * (Settings.LongWaveMinM + Settings.LongWaveMaxM);
	const double Scale = FMath::Max(1.0, MidWavelength);
	const FVector2D P = WorldMeters / Scale;
	return 0.65f * ValueNoise2D(P, Settings.Seed + 911) + 0.35f * ValueNoise2D(P * 0.47, Settings.Seed + 2719);
}

EBoundaryType ClassifyBoundary(const FPlate& A, const FPlate& B, const FVector2D& FromAToB, float& OutNormal, float& OutTangent)
{
	const FVector2D N = FromAToB.GetSafeNormal();
	const FVector2D T(-N.Y, N.X);
	const FVector2D Relative = B.Velocity - A.Velocity;
	OutNormal = FVector2D::DotProduct(Relative, N);
	OutTangent = FMath::Abs(FVector2D::DotProduct(Relative, T));

	if (OutTangent > FMath::Abs(OutNormal) * 1.35f && OutTangent > 0.08f)
	{
		return EBoundaryType::Transform;
	}
	if (OutNormal < -0.05f)
	{
		return EBoundaryType::Convergent;
	}
	if (OutNormal > 0.05f)
	{
		return EBoundaryType::Divergent;
	}
	return EBoundaryType::Stable;
}

float BoundaryContribution(const EBoundaryType Type, const float DistanceM, const FSettings& Settings)
{
	const float Width = FMath::Max(1000.0f, Settings.BoundaryWidthM);
	const float X = DistanceM / Width;
	const float W = FMath::Exp(-X * X * 2.0f);
	switch (Type)
	{
	case EBoundaryType::Convergent:
		return Settings.ConvergentUpliftM * W;
	case EBoundaryType::Divergent:
		return -Settings.DivergentSubsidenceM * W;
	case EBoundaryType::Transform:
		return Settings.TransformReliefM * W;
	default:
		return 0.0f;
	}
}

float EdgeOceanMask(const FVector2D& WorldMeters, const FSettings& Settings)
{
	const double Half = Settings.WorldSizeMeters * 0.5;
	const double Dx = Half - FMath::Abs(WorldMeters.X);
	const double Dy = Half - FMath::Abs(WorldMeters.Y);
	const double EdgeDistance = FMath::Min(Dx, Dy);
	return 1.0f - FMath::Clamp(static_cast<float>(EdgeDistance / FMath::Max(1.0f, Settings.CoastalMarginM)), 0.0f, 1.0f);
}

uint64 PairKey(const uint8 A, const uint8 B)
{
	const uint8 Lo = FMath::Min(A, B);
	const uint8 Hi = FMath::Max(A, B);
	return (static_cast<uint64>(Lo) << 32) | static_cast<uint64>(Hi);
}
}

void FGlobalDem::Reset()
{
	*this = FGlobalDem();
}

bool FGlobalDem::IsValid() const
{
	return Resolution > 1 && ElevationM.Num() == Resolution * Resolution;
}

bool FGlobalDem::HasHydrology() const
{
	return IsValid() && Receiver.Num() == ElevationM.Num() && DrainageAreaKm2.Num() == ElevationM.Num();
}

int32 FGlobalDem::NumCells() const
{
	return Resolution * Resolution;
}

int32 FGlobalDem::Index(const int32 X, const int32 Y) const
{
	return Y * Resolution + X;
}

FIntPoint FGlobalDem::Coordinates(const int32 CellIndex) const
{
	return FIntPoint(CellIndex % Resolution, CellIndex / Resolution);
}

bool FGlobalDem::IsBoundaryCell(const int32 X, const int32 Y) const
{
	return X == 0 || Y == 0 || X == Resolution - 1 || Y == Resolution - 1;
}

float FGlobalDem::SampleHeightBilinearM(const FVector2D& WorldMeters) const
{
	if (!IsValid())
	{
		return 0.0f;
	}
	const double Half = WorldSizeMeters * 0.5;
	const double U = FMath::Clamp((WorldMeters.X + Half) / WorldSizeMeters, 0.0, 1.0) * (Resolution - 1);
	const double V = FMath::Clamp((WorldMeters.Y + Half) / WorldSizeMeters, 0.0, 1.0) * (Resolution - 1);
	const int32 X0 = FMath::Clamp(FMath::FloorToInt(U), 0, Resolution - 1);
	const int32 Y0 = FMath::Clamp(FMath::FloorToInt(V), 0, Resolution - 1);
	const int32 X1 = FMath::Min(X0 + 1, Resolution - 1);
	const int32 Y1 = FMath::Min(Y0 + 1, Resolution - 1);
	const float Fx = static_cast<float>(U - X0);
	const float Fy = static_cast<float>(V - Y0);
	const float A = FMath::Lerp(ElevationM[Index(X0, Y0)], ElevationM[Index(X1, Y0)], Fx);
	const float B = FMath::Lerp(ElevationM[Index(X0, Y1)], ElevationM[Index(X1, Y1)], Fx);
	return FMath::Lerp(A, B, Fy);
}

FProvinceParameters FGenerator::GetDefaultProvinceParameters(const EProvinceType Province)
{
	FProvinceParameters Result;
	switch (Province)
	{
	case EProvinceType::StablePlain:
		Result = {0.8f, 1.5f, 0.45f, 0.85f};
		break;
	case EProvinceType::SedimentaryBasin:
		Result = {1.5f, 1.8f, 0.38f, 0.72f};
		break;
	case EProvinceType::UpliftedPlateau:
		Result = {0.75f, 0.8f, 0.65f, 0.48f};
		break;
	case EProvinceType::FoldMountainBelt:
		Result = {0.6f, 0.45f, 0.9f, 0.22f};
		break;
	case EProvinceType::RiftValley:
		Result = {1.2f, 1.0f, 0.6f, 0.35f};
		break;
	case EProvinceType::VolcanicProvince:
		Result = {0.7f, 0.55f, 0.85f, 0.18f};
		break;
	case EProvinceType::CoastalShelf:
		Result = {1.4f, 2.2f, 0.25f, 0.9f};
		break;
	}
	return Result;
}

bool FGenerator::GenerateSkeleton(const FSettings& Settings, FGlobalDem& OutDem, FGenerationStats* OutStats, FString* OutError)
{
	if (Settings.Resolution < 17 || Settings.PlateCount < 2 || Settings.PlateCount > 255 || Settings.WorldSizeMeters <= 0.0)
	{
		if (OutError)
		{
			*OutError = TEXT("Invalid landscape-evolution settings.");
		}
		return false;
	}

	const double Start = FPlatformTime::Seconds();
	OutDem.Reset();
	OutDem.Resolution = Settings.Resolution;
	OutDem.WorldSizeMeters = Settings.WorldSizeMeters;
	OutDem.CellSizeMeters = Settings.WorldSizeMeters / static_cast<double>(Settings.Resolution - 1);
	OutDem.OceanLevelM = Settings.OceanLevelM;

	FRandomStream Random(Settings.Seed);
	OutDem.Plates.Reserve(Settings.PlateCount);
	for (int32 I = 0; I < Settings.PlateCount; ++I)
	{
		FPlate Plate;
		Plate.Center01 = FVector2D(Random.FRand(), Random.FRand());
		const float Angle = Random.FRandRange(-PI, PI);
		const float Speed = Random.FRandRange(0.06f, 0.28f);
		Plate.Velocity = FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Speed;
		Plate.BaseElevationM = Random.FRandRange(120.0f, 650.0f);
		Plate.CrustStrength = Random.FRandRange(0.65f, 1.4f);
		Plate.Age01 = Random.FRand();
		Plate.bVolcanic = Random.FRand() < 0.12f;
		const float ProvinceRoll = Random.FRand();
		Plate.InteriorProvince = ProvinceRoll < 0.30f ? EProvinceType::StablePlain :
			ProvinceRoll < 0.48f ? EProvinceType::SedimentaryBasin :
			ProvinceRoll < 0.68f ? EProvinceType::UpliftedPlateau :
			ProvinceRoll < 0.80f ? EProvinceType::RiftValley :
			Plate.bVolcanic ? EProvinceType::VolcanicProvince : EProvinceType::StablePlain;
		OutDem.Plates.Add(Plate);
	}

	const int32 CellCount = OutDem.NumCells();
	OutDem.ElevationM.SetNumUninitialized(CellCount);
	OutDem.UpliftM.SetNumZeroed(CellCount);
	OutDem.PlateId.SetNumUninitialized(CellCount);
	OutDem.ProvinceId.SetNumUninitialized(CellCount);
	OutDem.BoundaryType.SetNumZeroed(CellCount);

	TMap<uint64, FPlateBoundary> BoundaryMap;
	float MinElevation = TNumericLimits<float>::Max();
	float MaxElevation = TNumericLimits<float>::Lowest();
	const double Half = Settings.WorldSizeMeters * 0.5;

	for (int32 Y = 0; Y < Settings.Resolution; ++Y)
	{
		const double WorldY = -Half + Y * OutDem.CellSizeMeters;
		for (int32 X = 0; X < Settings.Resolution; ++X)
		{
			const double WorldX = -Half + X * OutDem.CellSizeMeters;
			const FVector2D WorldMeters(WorldX, WorldY);
			const FVector2D P01((WorldX + Half) / Settings.WorldSizeMeters, (WorldY + Half) / Settings.WorldSizeMeters);

			int32 Closest = 0;
			int32 Second = 1;
			double ClosestD2 = TNumericLimits<double>::Max();
			double SecondD2 = TNumericLimits<double>::Max();
			for (int32 P = 0; P < OutDem.Plates.Num(); ++P)
			{
				const double D2 = FVector2D::DistSquared(P01, OutDem.Plates[P].Center01);
				if (D2 < ClosestD2)
				{
					SecondD2 = ClosestD2;
					Second = Closest;
					ClosestD2 = D2;
					Closest = P;
				}
				else if (D2 < SecondD2)
				{
					SecondD2 = D2;
					Second = P;
				}
			}

			const FPlate& Plate = OutDem.Plates[Closest];
			const FPlate& Other = OutDem.Plates[Second];
			const FVector2D PairAxis = (Other.Center01 - Plate.Center01) * Settings.WorldSizeMeters;
			float NormalSpeed = 0.0f;
			float TangentSpeed = 0.0f;
			const EBoundaryType Type = ClassifyBoundary(Plate, Other, PairAxis, NormalSpeed, TangentSpeed);

			const double ClosestD = FMath::Sqrt(ClosestD2) * Settings.WorldSizeMeters;
			const double SecondD = FMath::Sqrt(SecondD2) * Settings.WorldSizeMeters;
			float BoundaryDistanceM = static_cast<float>(0.5 * FMath::Abs(SecondD - ClosestD));
			const float Warp = ValueNoise2D(WorldMeters / 35000.0, Settings.Seed + Closest * 97 + Second * 211);
			BoundaryDistanceM = FMath::Max(0.0f, BoundaryDistanceM + Warp * Settings.BoundaryWarpAmplitudeM);

			float Uplift = BoundaryContribution(Type, BoundaryDistanceM, Settings);
			if (Type == EBoundaryType::Convergent)
			{
				Uplift *= FMath::Clamp((FMath::Abs(NormalSpeed) + 0.05f) * 3.0f, 0.45f, 1.5f);
			}

			EProvinceType Province = Plate.InteriorProvince;
			if (BoundaryDistanceM < Settings.BoundaryWidthM * 0.9f)
			{
				Province = Type == EBoundaryType::Convergent ? EProvinceType::FoldMountainBelt :
					Type == EBoundaryType::Divergent ? EProvinceType::RiftValley : Province;
			}
			if (Plate.bVolcanic && BoundaryDistanceM < Settings.BoundaryWidthM * 0.55f)
			{
				Province = EProvinceType::VolcanicProvince;
			}

			const float OceanMask = EdgeOceanMask(WorldMeters, Settings);
			if (OceanMask > 0.3f)
			{
				Province = EProvinceType::CoastalShelf;
			}

			const float WeakRelief = LongWaveField(WorldMeters, Settings) * Settings.LongWaveAmplitudeM;
			float Elevation = Plate.BaseElevationM + Uplift + WeakRelief;
			Elevation = FMath::Lerp(Elevation, Settings.OceanFloorM, OceanMask * OceanMask);

			const int32 Cell = OutDem.Index(X, Y);
			OutDem.ElevationM[Cell] = Elevation;
			OutDem.UpliftM[Cell] = Uplift;
			OutDem.PlateId[Cell] = static_cast<uint8>(Closest);
			OutDem.ProvinceId[Cell] = static_cast<uint8>(Province);
			OutDem.BoundaryType[Cell] = static_cast<uint8>(Type);
			MinElevation = FMath::Min(MinElevation, Elevation);
			MaxElevation = FMath::Max(MaxElevation, Elevation);

			if (BoundaryDistanceM < OutDem.CellSizeMeters * 1.75)
			{
				const uint64 Key = PairKey(static_cast<uint8>(Closest), static_cast<uint8>(Second));
				if (!BoundaryMap.Contains(Key))
				{
					FPlateBoundary Boundary;
					Boundary.PlateA = static_cast<uint8>(Closest);
					Boundary.PlateB = static_cast<uint8>(Second);
					Boundary.Type = Type;
					Boundary.RelativeNormalSpeed = NormalSpeed;
					Boundary.RelativeTangentialSpeed = TangentSpeed;
					BoundaryMap.Add(Key, Boundary);
				}
			}
		}
	}

	BoundaryMap.GenerateValueArray(OutDem.PlateBoundaries);
	if (OutStats)
	{
		OutStats->CellCount = CellCount;
		OutStats->MinimumElevationM = MinElevation;
		OutStats->MaximumElevationM = MaxElevation;
		OutStats->SkeletonSeconds = FPlatformTime::Seconds() - Start;
	}
	return true;
}

bool FGenerator::SolveHydrology(const FSettings& Settings, FGlobalDem& InOutDem, FGenerationStats* OutStats, FString* OutError)
{
	if (!InOutDem.IsValid() || InOutDem.Resolution != Settings.Resolution)
	{
		if (OutError)
		{
			*OutError = TEXT("GenerateSkeleton must succeed before SolveHydrology.");
		}
		return false;
	}

	const double Start = FPlatformTime::Seconds();
	const int32 N = InOutDem.NumCells();
	InOutDem.HydrologyElevationM = InOutDem.ElevationM;
	InOutDem.Receiver.Init(INDEX_NONE, N);
	InOutDem.DrainageAreaKm2.Init(static_cast<float>((InOutDem.CellSizeMeters * InOutDem.CellSizeMeters) / 1000000.0), N);
	InOutDem.DistanceToOutletKm.Init(0.0f, N);
	InOutDem.BasinId.Init(INDEX_NONE, N);
	InOutDem.RiverMask.Init(0, N);

	struct FHeapNode
	{
		float Elevation = 0.0f;
		int32 Cell = INDEX_NONE;
		bool operator<(const FHeapNode& Other) const { return Elevation > Other.Elevation; }
	};

	TArray<FHeapNode> Heap;
	Heap.Reserve(N / 8);
	TBitArray<> Visited(false, N);
	for (int32 Y = 0; Y < InOutDem.Resolution; ++Y)
	{
		for (int32 X = 0; X < InOutDem.Resolution; ++X)
		{
			if (!InOutDem.IsBoundaryCell(X, Y))
			{
				continue;
			}
			const int32 Cell = InOutDem.Index(X, Y);
			Visited[Cell] = true;
			Heap.HeapPush({InOutDem.HydrologyElevationM[Cell], Cell});
		}
	}

	while (Heap.Num() > 0)
	{
		FHeapNode Node;
		Heap.HeapPop(Node);
		const FIntPoint C = InOutDem.Coordinates(Node.Cell);
		for (int32 Dir = 0; Dir < 8; ++Dir)
		{
			const int32 NX = C.X + D8X[Dir];
			const int32 NY = C.Y + D8Y[Dir];
			if (NX < 0 || NY < 0 || NX >= InOutDem.Resolution || NY >= InOutDem.Resolution)
			{
				continue;
			}
			const int32 Neighbor = InOutDem.Index(NX, NY);
			if (Visited[Neighbor])
			{
				continue;
			}
			Visited[Neighbor] = true;
			const float Raised = FMath::Max(InOutDem.HydrologyElevationM[Neighbor], Node.Elevation + Settings.PriorityFloodEpsilonM);
			InOutDem.HydrologyElevationM[Neighbor] = Raised;
			Heap.HeapPush({Raised, Neighbor});
		}
	}

	for (int32 Cell = 0; Cell < N; ++Cell)
	{
		const FIntPoint C = InOutDem.Coordinates(Cell);
		if (InOutDem.IsBoundaryCell(C.X, C.Y))
		{
			continue;
		}
		float BestElevation = InOutDem.HydrologyElevationM[Cell];
		int32 Best = INDEX_NONE;
		for (int32 Dir = 0; Dir < 8; ++Dir)
		{
			const int32 NX = C.X + D8X[Dir];
			const int32 NY = C.Y + D8Y[Dir];
			const int32 Neighbor = InOutDem.Index(NX, NY);
			const float E = InOutDem.HydrologyElevationM[Neighbor];
			if (E < BestElevation)
			{
				BestElevation = E;
				Best = Neighbor;
			}
		}
		InOutDem.Receiver[Cell] = Best;
	}

	TArray<int32> Order;
	Order.SetNumUninitialized(N);
	for (int32 I = 0; I < N; ++I)
	{
		Order[I] = I;
	}
	Algo::Sort(Order, [&InOutDem](const int32 A, const int32 B)
	{
		return InOutDem.HydrologyElevationM[A] > InOutDem.HydrologyElevationM[B];
	});

	for (const int32 Cell : Order)
	{
		const int32 Receiver = InOutDem.Receiver[Cell];
		if (Receiver != INDEX_NONE)
		{
			InOutDem.DrainageAreaKm2[Receiver] += InOutDem.DrainageAreaKm2[Cell];
		}
	}

	int32 BasinCounter = 0;
	TMap<int32, int32> OutletToBasin;
	float MaxArea = 0.0f;
	int32 RiverCells = 0;
	for (int32 Cell = 0; Cell < N; ++Cell)
	{
		MaxArea = FMath::Max(MaxArea, InOutDem.DrainageAreaKm2[Cell]);
		if (InOutDem.DrainageAreaKm2[Cell] >= Settings.RiverSourceAreaKm2 && InOutDem.ElevationM[Cell] > Settings.OceanLevelM)
		{
			InOutDem.RiverMask[Cell] = 1;
			++RiverCells;
		}

		int32 Current = Cell;
		double DistanceM = 0.0;
		int32 Guard = 0;
		while (InOutDem.Receiver[Current] != INDEX_NONE && Guard++ < InOutDem.Resolution * 4)
		{
			const int32 Next = InOutDem.Receiver[Current];
			const FIntPoint A = InOutDem.Coordinates(Current);
			const FIntPoint B = InOutDem.Coordinates(Next);
			DistanceM += InOutDem.CellSizeMeters * ((A.X != B.X && A.Y != B.Y) ? UE_SQRT_2 : 1.0);
			Current = Next;
		}
		int32* Basin = OutletToBasin.Find(Current);
		if (!Basin)
		{
			const int32 NewId = BasinCounter++;
			OutletToBasin.Add(Current, NewId);
			Basin = OutletToBasin.Find(Current);
		}
		InOutDem.BasinId[Cell] = *Basin;
		InOutDem.DistanceToOutletKm[Cell] = static_cast<float>(DistanceM / 1000.0);
	}

	if (OutStats)
	{
		OutStats->BasinCount = BasinCounter;
		OutStats->RiverCellCount = RiverCells;
		OutStats->MaximumDrainageAreaKm2 = MaxArea;
		OutStats->HydrologySeconds = FPlatformTime::Seconds() - Start;
	}
	return true;
}
} // namespace CubusLandscapeEvolution
