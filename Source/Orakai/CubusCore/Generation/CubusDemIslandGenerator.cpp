#include "CubusCore/Generation/CubusDemIslandGenerator.h"

#include "Async/ParallelFor.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace CubusDemIsland
{
namespace
{
constexpr uint32 DemMagic = 0x4d454443u; // 'CDEM' in little endian.
constexpr uint32 DemVersion = 1u;

struct FHeader
{
	uint32 Magic = DemMagic;
	uint32 Version = DemVersion;
	int32 Width = 0;
	int32 Height = 0;
	float CellSizeM = 1.0f;
	float MinimumElevationM = 0.0f;
	float MaximumElevationM = 0.0f;
	float MeanElevationM = 0.0f;
};

uint32 Hash(uint32 V)
{
	V ^= V >> 16;
	V *= 0x7feb352du;
	V ^= V >> 15;
	V *= 0x846ca68bu;
	V ^= V >> 16;
	return V;
}

float HashSigned(const int32 X, const int32 Y, const int32 Seed)
{
	const uint32 H = Hash(static_cast<uint32>(X) * 73856093u ^ static_cast<uint32>(Y) * 19349663u ^ static_cast<uint32>(Seed) * 83492791u);
	return static_cast<float>(H & 0x00ffffffu) / static_cast<float>(0x00ffffffu) * 2.0f - 1.0f;
}

float Smooth01(const float T)
{
	const float X = FMath::Clamp(T, 0.0f, 1.0f);
	return X * X * (3.0f - 2.0f * X);
}

float ValueNoise(const double X, const double Y, const int32 Seed)
{
	const int32 X0 = FMath::FloorToInt(X);
	const int32 Y0 = FMath::FloorToInt(Y);
	const float Fx = static_cast<float>(X - X0);
	const float Fy = static_cast<float>(Y - Y0);
	const float Sx = Smooth01(Fx);
	const float Sy = Smooth01(Fy);
	const float A = FMath::Lerp(HashSigned(X0, Y0, Seed), HashSigned(X0 + 1, Y0, Seed), Sx);
	const float B = FMath::Lerp(HashSigned(X0, Y0 + 1, Seed), HashSigned(X0 + 1, Y0 + 1, Seed), Sx);
	return FMath::Lerp(A, B, Sy);
}

float Fbm(double X, double Y, const int32 Seed)
{
	float Sum = 0.0f;
	float Weight = 0.0f;
	float Amp = 1.0f;
	for (int32 Octave = 0; Octave < 4; ++Octave)
	{
		Sum += ValueNoise(X, Y, Seed + Octave * 1013) * Amp;
		Weight += Amp;
		X = X * 2.03 + 17.1;
		Y = Y * 2.01 - 11.7;
		Amp *= 0.5f;
	}
	return Weight > 0.0f ? Sum / Weight : 0.0f;
}

FVector2D TransformUv(FVector2D UV, const int32 Variant)
{
{
	UV -= FVector2D(0.5, 0.5);
	if ((Variant & 1) != 0)
	{
		UV.X = -UV.X;
	}
	if ((Variant & 2) != 0)
	{
		UV.Y = -UV.Y;
	}
	switch ((Variant >> 2) & 3)
	{
	case 1: UV = FVector2D(-UV.Y, UV.X); break;
	case 2: UV = FVector2D(-UV.X, -UV.Y); break;
	case 3: UV = FVector2D(UV.Y, -UV.X); break;
	default: break;
	}
	return UV + FVector2D(0.5, 0.5);
}
}

float IslandEnvelope(const double WorldX, const double WorldY, const FSettings& Settings, const int32 Seed)
{
	const double Half = Settings.WorldSizeMeters * 0.5;
	const double CoastBand = FMath::Clamp<double>(Settings.CoastBandM, 8.0, Half * 0.45);

	// Start with a rounded rectangular/superellipse footprint so the entire 500 m
	// domain remains useful, then disturb the shoreline with broad seeded fields.
	const float WarpX = Fbm(WorldX / 150.0, WorldY / 150.0, Seed ^ 0x51d7348d);
	const float WarpY = Fbm(WorldX / 170.0, WorldY / 170.0, Seed ^ 0x94d049bb);
	const double X = WorldX + WarpX * 28.0;
	const double Y = WorldY + WarpY * 28.0;
	const double Nx = FMath::Abs(X) / FMath::Max(1.0, Half - CoastBand * 0.15);
	const double Ny = FMath::Abs(Y) / FMath::Max(1.0, Half - CoastBand * 0.15);
	const double Super = FMath::Pow(FMath::Pow(Nx, 3.3) + FMath::Pow(Ny, 3.3), 1.0 / 3.3);
	const double ApproxDistanceToCoast = (1.0 - Super) * (Half - CoastBand * 0.15);
	return Smooth01(static_cast<float>(ApproxDistanceToCoast / CoastBand));
}

void Measure(const CubusLandscapeEvolution::FGlobalDem& Dem, CubusLandscapeEvolution::FGenerationStats* Stats)
{
	if (!Stats)
	{
		return;
	}
	Stats->CellCount = Dem.NumCells();
	Stats->MinimumElevationM = TNumericLimits<float>::Max();
	Stats->MaximumElevationM = TNumericLimits<float>::Lowest();
	for (const float E : Dem.ElevationM)
	{
		Stats->MinimumElevationM = FMath::Min(Stats->MinimumElevationM, E);
		Stats->MaximumElevationM = FMath::Max(Stats->MaximumElevationM, E);
	}
	CubusLandscapeEvolution::FGenerator::MeasureQuality(Dem, *Stats);
}
}

bool FPatch::IsValid() const
{
	return Width > 1 && Height > 1 && ElevationM.Num() == Width * Height && CellSizeM > 0.0f;
}

float FPatch::SampleBilinear(const float InU, const float InV) const
{
	if (!IsValid())
	{
		return 0.0f;
	}
	const float U = FMath::Clamp(InU, 0.0f, 1.0f) * static_cast<float>(Width - 1);
	const float V = FMath::Clamp(InV, 0.0f, 1.0f) * static_cast<float>(Height - 1);
	const int32 X0 = FMath::Clamp(FMath::FloorToInt(U), 0, Width - 1);
	const int32 Y0 = FMath::Clamp(FMath::FloorToInt(V), 0, Height - 1);
	const int32 X1 = FMath::Min(X0 + 1, Width - 1);
	const int32 Y1 = FMath::Min(Y0 + 1, Height - 1);
	const float Fx = U - X0;
	const float Fy = V - Y0;
	const float A = FMath::Lerp(ElevationM[Y0 * Width + X0], ElevationM[Y0 * Width + X1], Fx);
	const float B = FMath::Lerp(ElevationM[Y1 * Width + X0], ElevationM[Y1 * Width + X1], Fx);
	return FMath::Lerp(A, B, Fy);
}

FString FGenerator::ResolveSourceDirectory(const FSettings& Settings)
{
	return FPaths::IsRelative(Settings.SourceDirectory)
		? FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / Settings.SourceDirectory)
		: FPaths::ConvertRelativePathToFull(Settings.SourceDirectory);
}

bool FGenerator::DiscoverPatches(const FSettings& Settings, TArray<FString>& OutPatchPaths, FString* OutError)
{
	OutPatchPaths.Reset();
	const FString Directory = ResolveSourceDirectory(Settings);
	IFileManager::Get().FindFilesRecursive(OutPatchPaths, *Directory, TEXT("*.cdem"), true, false, false);
	OutPatchPaths.Sort();
	if (OutPatchPaths.IsEmpty())
	{
		if (OutError)
		{
			*OutError = FString::Printf(TEXT("No prepared DEM patches found in %s. Run Tools/DemLibrary/prepare_linz_coastal.py first."), *Directory);
		}
		return false;
	}
	return true;
}

bool FGenerator::LoadPatch(const FString& Path, FPatch& OutPatch, FString* OutError)
{
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *Path))
	{
		if (OutError) *OutError = FString::Printf(TEXT("Could not read DEM patch: %s"), *Path);
		return false;
	}
	if (Bytes.Num() < static_cast<int32>(sizeof(FHeader)))
	{
		if (OutError) *OutError = FString::Printf(TEXT("DEM patch header is truncated: %s"), *Path);
		return false;
	}

	FHeader Header;
	FMemory::Memcpy(&Header, Bytes.GetData(), sizeof(FHeader));
	if (Header.Magic != DemMagic || Header.Version != DemVersion || Header.Width < 2 || Header.Height < 2 || Header.CellSizeM <= 0.0f)
	{
		if (OutError) *OutError = FString::Printf(TEXT("Unsupported/corrupt DEM patch header: %s"), *Path);
		return false;
	}

	const int64 SampleCount = static_cast<int64>(Header.Width) * static_cast<int64>(Header.Height);
	const int64 ExpectedBytes = static_cast<int64>(sizeof(FHeader)) + SampleCount * static_cast<int64>(sizeof(float));
	if (ExpectedBytes != Bytes.Num())
	{
		if (OutError) *OutError = FString::Printf(TEXT("DEM patch has wrong byte count: %s"), *Path);
		return false;
	}

	OutPatch.SourcePath = Path;
	OutPatch.Width = Header.Width;
	OutPatch.Height = Header.Height;
	OutPatch.CellSizeM = Header.CellSizeM;
	OutPatch.MinimumElevationM = Header.MinimumElevationM;
	OutPatch.MaximumElevationM = Header.MaximumElevationM;
	OutPatch.MeanElevationM = Header.MeanElevationM;
	OutPatch.ElevationM.SetNumUninitialized(static_cast<int32>(SampleCount));
	FMemory::Memcpy(OutPatch.ElevationM.GetData(), Bytes.GetData() + sizeof(FHeader), SampleCount * sizeof(float));
	return true;
}

bool FGenerator::Generate(const FSettings& Settings, CubusLandscapeEvolution::FGlobalDem& OutDem,
	CubusLandscapeEvolution::FGenerationStats* OutStats, FString* OutError)
{
	if (Settings.Resolution < 33 || Settings.WorldSizeMeters <= 100.0 || Settings.OceanFloorM >= Settings.OceanLevelM)
	{
		if (OutError) *OutError = TEXT("Invalid DEM island settings.");
		return false;
	}

	TArray<FString> Paths;
	if (!DiscoverPatches(Settings, Paths, OutError))
	{
		return false;
	}

	FRandomStream Random(Settings.Seed);
	const int32 PrimaryIndex = Random.RandRange(0, Paths.Num() - 1);
	int32 SecondaryIndex = PrimaryIndex;
	if (Paths.Num() > 1)
	{
		while (SecondaryIndex == PrimaryIndex)
		{
			SecondaryIndex = Random.RandRange(0, Paths.Num() - 1);
		}
	}

	FPatch Primary;
	FPatch Secondary;
	if (!LoadPatch(Paths[PrimaryIndex], Primary, OutError) || !LoadPatch(Paths[SecondaryIndex], Secondary, OutError))
	{
		return false;
	}

	const int32 PrimaryVariant = Random.RandRange(0, 15);
	const int32 SecondaryVariant = Random.RandRange(0, 15);
	const FVector2D PrimaryOffset(Random.FRandRange(-0.10f, 0.10f), Random.FRandRange(-0.10f, 0.10f));
	const FVector2D SecondaryOffset(Random.FRandRange(-0.15f, 0.15f), Random.FRandRange(-0.15f, 0.15f));

	OutDem.Reset();
	OutDem.Resolution = Settings.Resolution;
	OutDem.WorldSizeMeters = Settings.WorldSizeMeters;
	OutDem.CellSizeMeters = Settings.WorldSizeMeters / static_cast<double>(Settings.Resolution - 1);
	OutDem.OceanLevelM = Settings.OceanLevelM;
	OutDem.ElevationM.SetNumUninitialized(OutDem.NumCells());
	OutDem.UpliftM.Init(0.0f, OutDem.NumCells());
	OutDem.PlateId.Init(0, OutDem.NumCells());
	OutDem.ProvinceId.Init(static_cast<uint8>(CubusLandscapeEvolution::EProvinceType::StablePlain), OutDem.NumCells());
	OutDem.BoundaryType.Init(static_cast<uint8>(CubusLandscapeEvolution::EBoundaryType::Stable), OutDem.NumCells());

	const int32 R = OutDem.Resolution;
	const double Half = Settings.WorldSizeMeters * 0.5;
	ParallelFor(OutDem.NumCells(), [&OutDem, &Settings, &Primary, &Secondary, PrimaryVariant, SecondaryVariant, PrimaryOffset, SecondaryOffset, R, Half](const int32 Cell)
	{
		const int32 X = Cell % R;
		const int32 Y = Cell / R;
		const double U = static_cast<double>(X) / static_cast<double>(R - 1);
		const double V = static_cast<double>(Y) / static_cast<double>(R - 1);
		const double WorldX = -Half + U * Settings.WorldSizeMeters;
		const double WorldY = -Half + V * Settings.WorldSizeMeters;

		const float WarpU = Fbm(WorldX / 115.0, WorldY / 115.0, Settings.Seed ^ 0x6d2b79f5) * Settings.WarpMeters / static_cast<float>(Settings.WorldSizeMeters);
		const float WarpV = Fbm(WorldX / 131.0, WorldY / 131.0, Settings.Seed ^ 0x1b873593) * Settings.WarpMeters / static_cast<float>(Settings.WorldSizeMeters);
		FVector2D SourceUv(U + WarpU, V + WarpV);

		const FVector2D PUv = TransformUv(SourceUv + PrimaryOffset, PrimaryVariant);
		const FVector2D SUv = TransformUv(SourceUv + SecondaryOffset, SecondaryVariant);
		const float P = (Primary.SampleBilinear(PUv.X, PUv.Y) - Primary.MeanElevationM) * Settings.ReliefScale;
		const float S = (Secondary.SampleBilinear(SUv.X, SUv.Y) - Secondary.MeanElevationM) * Settings.ReliefScale;
		const float BlendNoise = Smooth01(0.5f + 0.5f * Fbm(WorldX / 210.0, WorldY / 210.0, Settings.Seed ^ 0xa5a5a5a5));
		const float SecondaryWeight = FMath::Clamp(Settings.SecondaryBlend * FMath::Lerp(0.35f, 1.0f, BlendNoise), 0.0f, 0.48f);
		const float RealRelief = FMath::Lerp(P, S, SecondaryWeight);

		const float Envelope = IslandEnvelope(WorldX, WorldY, Settings, Settings.Seed);
		const float LandHeight = Settings.BaseLandElevationM + RealRelief;
		const float Elevation = FMath::Lerp(Settings.OceanFloorM, LandHeight, Envelope);
		OutDem.ElevationM[Cell] = Elevation;

		if (Elevation <= Settings.OceanLevelM)
		{
			OutDem.ProvinceId[Cell] = static_cast<uint8>(CubusLandscapeEvolution::EProvinceType::OceanicBasin);
		}
		else if (Envelope < 0.40f)
		{
			OutDem.ProvinceId[Cell] = static_cast<uint8>(CubusLandscapeEvolution::EProvinceType::CoastalShelf);
		}
	});

	Measure(OutDem, OutStats);
	UE_LOG(LogTemp, Display, TEXT("Cubus DEM island: primary %s, secondary %s, seed %d, %.2f m/cell"),
		*FPaths::GetCleanFilename(Primary.SourcePath), *FPaths::GetCleanFilename(Secondary.SourcePath), Settings.Seed, OutDem.CellSizeMeters);
	return true;
}
} // namespace CubusDemIsland
