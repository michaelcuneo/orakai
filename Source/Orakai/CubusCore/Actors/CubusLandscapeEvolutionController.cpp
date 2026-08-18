#include "CubusCore/Actors/CubusLandscapeEvolutionController.h"

#include "Engine/Engine.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"

#if WITH_EDITOR
#include "UObject/UnrealType.h"
#endif

namespace
{
constexpr uint64 LandscapeDiagnosticsMessageKey = 0xC0B055ULL;
}

ACubusLandscapeEvolutionController::ACubusLandscapeEvolutionController()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	PreviewMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("PreviewMesh"));
	SetRootComponent(PreviewMesh);
	PreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewMesh->bUseAsyncCooking = true;
}

void ACubusLandscapeEvolutionController::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bShowViewportDiagnostics || GEngine == nullptr)
	{
		return;
	}
	GEngine->AddOnScreenDebugMessage(LandscapeDiagnosticsMessageKey, 0.0f, FColor::Cyan, BuildViewportDiagnosticsText(), false,
		FVector2D(1.0f, 1.0f));
}

void ACubusLandscapeEvolutionController::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (bGenerateOnConstruction)
	{
		GenerateRandomDemIsland();
	}
	else if (GlobalDem.IsValid())
	{
		RebuildPreview();
	}
}

#if WITH_EDITOR
bool ACubusLandscapeEvolutionController::ShouldTickIfViewportsOnly() const
{
	return bShowViewportDiagnostics;
}

void ACubusLandscapeEvolutionController::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName ChangedPropertyName = PropertyChangedEvent.Property != nullptr ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	const ECubusLandscapeEditorAction PendingAction =
		ChangedPropertyName == GET_MEMBER_NAME_CHECKED(ACubusLandscapeEvolutionController, EditorAction)
			? EditorAction
			: ECubusLandscapeEditorAction::None;

	if (PendingAction != ECubusLandscapeEditorAction::None)
	{
		EditorAction = ECubusLandscapeEditorAction::None;
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);

	switch (PendingAction)
	{
	case ECubusLandscapeEditorAction::GenerateRandomDemIsland:
		GenerateRandomDemIsland();
		break;
	case ECubusLandscapeEditorAction::GenerateWorldSkeleton:
		GenerateWorldSkeleton();
		break;
	case ECubusLandscapeEditorAction::SolveGlobalHydrology:
		SolveGlobalHydrology();
		break;
	case ECubusLandscapeEditorAction::EvolveLandscape:
		EvolveLandscape();
		break;
	case ECubusLandscapeEditorAction::GenerateAndSolve:
		GenerateAndSolve();
		break;
	case ECubusLandscapeEditorAction::GenerateSolveAndEvolve:
		GenerateSolveAndEvolve();
		break;
	case ECubusLandscapeEditorAction::RebuildPreview:
		RebuildPreview();
		break;
	case ECubusLandscapeEditorAction::None:
	default:
		break;
	}
}
#endif

CubusLandscapeEvolution::FSettings ACubusLandscapeEvolutionController::MakeSettings() const
{
	CubusLandscapeEvolution::FSettings Settings;
	Settings.Seed = Seed;
	Settings.Resolution = GlobalResolution;
	Settings.WorldSizeMeters = WorldSizeMeters;
	Settings.PlateCount = PlateCount;
	Settings.OceanLevelM = OceanLevelM;
	Settings.OceanFloorM = OceanFloorM;
	Settings.CoastalMarginM = CoastalMarginM;
	Settings.RiverSourceAreaKm2 = RiverSourceAreaKm2;
	Settings.EvolutionIterations = EvolutionIterations;
	Settings.EvolutionStepYears = EvolutionStepYears;
	Settings.StreamPowerK = StreamPowerK;
	Settings.StreamPowerM = StreamPowerM;
	Settings.MaximumIncisionPerIterationM = MaximumIncisionPerIterationM;
	Settings.BaseUpliftRateMPerYear = BaseUpliftRateMPerYear;
	Settings.HillslopeDiffusivityM2PerYear = HillslopeDiffusivityM2PerYear;
	Settings.HydrologyRefreshInterval = HydrologyRefreshInterval;
	return Settings;
}

CubusDemIsland::FSettings ACubusLandscapeEvolutionController::MakeDemIslandSettings() const
{
	CubusDemIsland::FSettings Settings;
	Settings.Seed = Seed;
	Settings.Resolution = GlobalResolution;
	Settings.WorldSizeMeters = WorldSizeMeters;
	Settings.OceanLevelM = OceanLevelM;
	Settings.OceanFloorM = OceanFloorM;
	Settings.CoastBandM = DemCoastBandM;
	Settings.BaseLandElevationM = DemBaseLandElevationM;
	Settings.ReliefScale = DemReliefScale;
	Settings.SecondaryBlend = DemSecondaryBlend;
	Settings.WarpMeters = DemWarpMeters;
	Settings.SourceDirectory = DemSourceDirectory;
	return Settings;
}

void ACubusLandscapeEvolutionController::GenerateRandomDemIsland()
{
	CubusLandscapeEvolution::FGenerationStats Stats;
	FString Error;
	if (!CubusDemIsland::FGenerator::Generate(MakeDemIslandSettings(), GlobalDem, &Stats, &Error))
	{
		UE_LOG(LogTemp, Error, TEXT("Cubus real-DEM island generation failed: %s"), *Error);
		return;
	}

	const CubusLandscapeEvolution::FSettings EvolutionSettings = MakeSettings();
	if (!CubusLandscapeEvolution::FGenerator::SolveHydrology(EvolutionSettings, GlobalDem, &Stats, &Error))
	{
		UE_LOG(LogTemp, Error, TEXT("Cubus DEM island hydrology failed: %s"), *Error);
		return;
	}
	if (bEvolveDemIsland && !CubusLandscapeEvolution::FGenerator::EvolveLandscape(EvolutionSettings, GlobalDem, &Stats, &Error))
	{
		UE_LOG(LogTemp, Error, TEXT("Cubus DEM island evolution failed: %s"), *Error);
		return;
	}

	UpdateDiagnostics(Stats);
	UE_LOG(LogTemp, Display, TEXT("Cubus random real-DEM island generated: seed %d, %.0f x %.0f m, %.2f m/cell, land %.1f%%"),
		Seed, WorldSizeMeters, WorldSizeMeters, GlobalDem.CellSizeMeters, Stats.LandFraction * 100.0f);
	RebuildPreview();
}

void ACubusLandscapeEvolutionController::GenerateWorldSkeleton()
{
	CubusLandscapeEvolution::FGenerationStats Stats;
	FString Error;
	if (!CubusLandscapeEvolution::FGenerator::GenerateSkeleton(MakeSettings(), GlobalDem, &Stats, &Error))
	{
		UE_LOG(LogTemp, Error, TEXT("Cubus landscape skeleton generation failed: %s"), *Error);
		return;
	}
	UpdateDiagnostics(Stats);
	RebuildPreview();
}

void ACubusLandscapeEvolutionController::SolveGlobalHydrology()
{
	CubusLandscapeEvolution::FGenerationStats Stats;
	FString Error;
	if (!CubusLandscapeEvolution::FGenerator::SolveHydrology(MakeSettings(), GlobalDem, &Stats, &Error))
	{
		UE_LOG(LogTemp, Error, TEXT("Cubus landscape hydrology solve failed: %s"), *Error);
		return;
	}
	UpdateDiagnostics(Stats);
	RebuildPreview();
}

void ACubusLandscapeEvolutionController::EvolveLandscape()
{
	CubusLandscapeEvolution::FGenerationStats Stats;
	FString Error;
	if (!CubusLandscapeEvolution::FGenerator::EvolveLandscape(MakeSettings(), GlobalDem, &Stats, &Error))
	{
		UE_LOG(LogTemp, Error, TEXT("Cubus landscape evolution failed: %s"), *Error);
		return;
	}
	UpdateDiagnostics(Stats);
	RebuildPreview();
}

void ACubusLandscapeEvolutionController::GenerateAndSolve()
{
	CubusLandscapeEvolution::FGenerationStats Stats;
	FString Error;
	const CubusLandscapeEvolution::FSettings Settings = MakeSettings();
	if (!CubusLandscapeEvolution::FGenerator::GenerateSkeleton(Settings, GlobalDem, &Stats, &Error) ||
		!CubusLandscapeEvolution::FGenerator::SolveHydrology(Settings, GlobalDem, &Stats, &Error))
	{
		UE_LOG(LogTemp, Error, TEXT("Cubus legacy landscape generation failed: %s"), *Error);
		return;
	}
	UpdateDiagnostics(Stats);
	RebuildPreview();
}

void ACubusLandscapeEvolutionController::GenerateSolveAndEvolve()
{
	CubusLandscapeEvolution::FGenerationStats Stats;
	FString Error;
	const CubusLandscapeEvolution::FSettings Settings = MakeSettings();
	if (!CubusLandscapeEvolution::FGenerator::GenerateSkeleton(Settings, GlobalDem, &Stats, &Error) ||
		!CubusLandscapeEvolution::FGenerator::SolveHydrology(Settings, GlobalDem, &Stats, &Error) ||
		!CubusLandscapeEvolution::FGenerator::EvolveLandscape(Settings, GlobalDem, &Stats, &Error))
	{
		UE_LOG(LogTemp, Error, TEXT("Cubus legacy landscape evolution failed: %s"), *Error);
		return;
	}
	UpdateDiagnostics(Stats);
	RebuildPreview();
}

bool ACubusLandscapeEvolutionController::HasGeneratedDem() const
{
	return GlobalDem.IsValid();
}

float ACubusLandscapeEvolutionController::SampleGlobalHeightMeters(const FVector2D WorldMeters) const
{
	return GlobalDem.SampleHeightBilinearM(WorldMeters);
}

void ACubusLandscapeEvolutionController::UpdateDiagnostics(const CubusLandscapeEvolution::FGenerationStats& Stats)
{
	if (Stats.CellCount > 0)
	{
		GeneratedCellCount = Stats.CellCount;
		MinimumElevationM = Stats.MinimumElevationM;
		MaximumElevationM = Stats.MaximumElevationM;
	}
	if (GlobalDem.IsValid())
	{
		GlobalCellSizeM = static_cast<float>(GlobalDem.CellSizeMeters);
	}
	if (Stats.RiverCellCount > 0 || GlobalDem.HasHydrology())
	{
		GeneratedRiverCellCount = Stats.RiverCellCount;
		GeneratedBasinCount = Stats.BasinCount;
	}
	if (Stats.EvolutionIterations > 0)
	{
		MaximumStreamIncisionM = Stats.MaximumStreamIncisionM;
		MeanStreamIncisionM = Stats.MeanStreamIncisionM;
		MaximumAbsoluteElevationChangeM = Stats.MaximumAbsoluteElevationChangeM;
		MeanAbsoluteElevationChangeM = Stats.MeanAbsoluteElevationChangeM;
		MaximumTerrainLoweringM = Stats.MaximumTerrainLoweringM;
		MaximumTerrainRaisingM = Stats.MaximumTerrainRaisingM;
	}
	LandPercent = Stats.LandFraction * 100.0f;
	MeanLandSlopeDegrees = Stats.MeanLandSlopeDegrees;
	SteepLandPercent = Stats.SteepLandFraction * 100.0f;
	MaximumNeighbourStepM = Stats.MaximumNeighbourStepM;
}

FString ACubusLandscapeEvolutionController::BuildViewportDiagnosticsText() const
{
	const TCHAR* PreviewModeText = PreviewMode == ECubusLandscapePreviewMode::NativeDetail ? TEXT("Native Detail") : TEXT("Full Island");
	const TCHAR* DebugViewText = TEXT("Elevation");
	switch (DebugView)
	{
	case ECubusLandscapeDebugView::Province: DebugViewText = TEXT("Province"); break;
	case ECubusLandscapeDebugView::Plate: DebugViewText = TEXT("Plate"); break;
	case ECubusLandscapeDebugView::Uplift: DebugViewText = TEXT("Uplift"); break;
	case ECubusLandscapeDebugView::FlowDirection: DebugViewText = TEXT("Flow Direction"); break;
	case ECubusLandscapeDebugView::DrainageArea: DebugViewText = TEXT("Drainage Area"); break;
	case ECubusLandscapeDebugView::Watershed: DebugViewText = TEXT("Watershed"); break;
	case ECubusLandscapeDebugView::RiverNetwork: DebugViewText = TEXT("River Network"); break;
	case ECubusLandscapeDebugView::StreamIncision: DebugViewText = TEXT("Stream Incision"); break;
	case ECubusLandscapeDebugView::EvolutionDelta: DebugViewText = TEXT("Evolution Delta"); break;
	case ECubusLandscapeDebugView::Elevation:
	default: break;
	}

	return FString::Printf(
		TEXT("CUBUS REAL-DEM ISLAND\n")
		TEXT("World: %.0f x %.0f m\n")
		TEXT("DEM: %d x %d   Cell: %.2f m\n")
		TEXT("Preview: %s, %d x %d   Displayed cell: %.2f m\n")
		TEXT("Window: %.0f m   Center: (%.0f, %.0f) m\n")
		TEXT("Debug: %s\n")
		TEXT("Land: %.1f%%   Mean slope: %.1f deg   Steep 35+: %.1f%%\n")
		TEXT("Max neighbour step: %.2f m\n")
		TEXT("Rivers: %d   Basins: %d\n")
		TEXT("Max incision: %.2f m   Max |delta|: %.2f m"),
		WorldSizeMeters, WorldSizeMeters, GlobalResolution, GlobalResolution, GlobalCellSizeM,
		PreviewModeText, PreviewResolution, PreviewResolution, PreviewDisplayedCellSizeM,
		PreviewWindowSizeKm * 1000.0f, PreviewActualCenterWorldMeters.X, PreviewActualCenterWorldMeters.Y,
		DebugViewText, LandPercent, MeanLandSlopeDegrees, SteepLandPercent, MaximumNeighbourStepM,
		GeneratedRiverCellCount, GeneratedBasinCount, MaximumStreamIncisionM, MaximumAbsoluteElevationChangeM);
}

FLinearColor ACubusLandscapeEvolutionController::DebugColorForCell(const int32 Cell) const
{
	if (!GlobalDem.IsValid() || !GlobalDem.ElevationM.IsValidIndex(Cell))
	{
		return FLinearColor::Black;
	}

	switch (DebugView)
	{
	case ECubusLandscapeDebugView::Plate:
	{
		const uint8 Id = GlobalDem.PlateId.IsValidIndex(Cell) ? GlobalDem.PlateId[Cell] : 0;
		const float H = FMath::Frac(Id * 0.61803398875f);
		return FLinearColor::MakeFromHSV8(static_cast<uint8>(H * 255.0f), 180, 220);
	}
	case ECubusLandscapeDebugView::Province:
	{
		static const FLinearColor Colors[] = {
			FLinearColor(0.40f, 0.72f, 0.35f), FLinearColor(0.65f, 0.52f, 0.30f),
			FLinearColor(0.58f, 0.52f, 0.38f), FLinearColor(0.42f, 0.40f, 0.38f),
			FLinearColor(0.55f, 0.38f, 0.30f), FLinearColor(0.35f, 0.25f, 0.22f),
			FLinearColor(0.30f, 0.48f, 0.52f), FLinearColor(0.04f, 0.10f, 0.28f)};
		const uint8 Id = GlobalDem.ProvinceId.IsValidIndex(Cell) ? GlobalDem.ProvinceId[Cell] : 0;
		return Colors[FMath::Clamp<int32>(Id, 0, UE_ARRAY_COUNT(Colors) - 1)];
	}
	case ECubusLandscapeDebugView::Uplift:
	{
		const float V = GlobalDem.UpliftM.IsValidIndex(Cell) ? GlobalDem.UpliftM[Cell] : 0.0f;
		const float T = FMath::Clamp(0.5f + V / 6400.0f, 0.0f, 1.0f);
		return FLinearColor(T, 0.15f, 1.0f - T);
	}
	case ECubusLandscapeDebugView::DrainageArea:
	{
		const float A = GlobalDem.DrainageAreaKm2.IsValidIndex(Cell) ? GlobalDem.DrainageAreaKm2[Cell] : 0.0f;
		const float T = FMath::Clamp(FMath::LogX(10.0f, FMath::Max(0.0001f, A)) / 2.0f + 1.0f, 0.0f, 1.0f);
		return FLinearColor(T, T, 1.0f);
	}
	case ECubusLandscapeDebugView::RiverNetwork:
	{
		const bool bRiver = GlobalDem.RiverMask.IsValidIndex(Cell) && GlobalDem.RiverMask[Cell] != 0;
		if (bRiver) return FLinearColor(0.05f, 0.25f, 1.0f);
		return GlobalDem.ElevationM[Cell] <= GlobalDem.OceanLevelM
			? FLinearColor(0.03f, 0.08f, 0.18f)
			: FLinearColor(0.28f, 0.30f, 0.26f);
	}
	case ECubusLandscapeDebugView::StreamIncision:
	{
		const float Incision = GlobalDem.StreamIncisionM.IsValidIndex(Cell) ? GlobalDem.StreamIncisionM[Cell] : 0.0f;
		const float Scale = FMath::Max(1.0f, MaximumStreamIncisionM);
		const float T = FMath::Clamp(FMath::Sqrt(Incision / Scale), 0.0f, 1.0f);
		return FLinearColor(T, 0.08f, 1.0f - T);
	}
	case ECubusLandscapeDebugView::EvolutionDelta:
	{
		const float Delta = GlobalDem.EvolutionDeltaM.IsValidIndex(Cell) ? GlobalDem.EvolutionDeltaM[Cell] : 0.0f;
		const float Scale = FMath::Max(1.0f, MaximumAbsoluteElevationChangeM);
		const float Strength = FMath::Clamp(FMath::Sqrt(FMath::Abs(Delta) / Scale), 0.0f, 1.0f);
		if (Delta < 0.0f) return FLinearColor(0.05f, 0.15f + 0.35f * (1.0f - Strength), 0.35f + 0.65f * Strength);
		if (Delta > 0.0f) return FLinearColor(0.35f + 0.65f * Strength, 0.12f, 0.05f);
		return FLinearColor(0.08f, 0.08f, 0.08f);
	}
	case ECubusLandscapeDebugView::FlowDirection:
	{
		const int32 Receiver = GlobalDem.Receiver.IsValidIndex(Cell) ? GlobalDem.Receiver[Cell] : INDEX_NONE;
		if (Receiver == INDEX_NONE) return FLinearColor::Black;
		const FIntPoint From = GlobalDem.Coordinates(Cell);
		const FIntPoint To = GlobalDem.Coordinates(Receiver);
		const float Angle = FMath::Atan2(static_cast<float>(To.Y - From.Y), static_cast<float>(To.X - From.X));
		const float Hue = FMath::Fmod((Angle + PI) / (2.0f * PI), 1.0f);
		return FLinearColor::MakeFromHSV8(static_cast<uint8>(Hue * 255.0f), 220, 235);
	}
	case ECubusLandscapeDebugView::Watershed:
	{
		const int32 Basin = GlobalDem.BasinId.IsValidIndex(Cell) ? GlobalDem.BasinId[Cell] : INDEX_NONE;
		if (Basin == INDEX_NONE) return FLinearColor::Black;
		const float Hue = FMath::Frac(static_cast<float>(Basin) * 0.61803398875f);
		return FLinearColor::MakeFromHSV8(static_cast<uint8>(Hue * 255.0f), 170, 215);
	}
	case ECubusLandscapeDebugView::Elevation:
	default:
	{
		const float Range = FMath::Max(1.0f, MaximumElevationM - MinimumElevationM);
		const float T = FMath::Clamp((GlobalDem.ElevationM[Cell] - MinimumElevationM) / Range, 0.0f, 1.0f);
		if (GlobalDem.ElevationM[Cell] <= GlobalDem.OceanLevelM)
		{
			return FLinearColor(0.02f, 0.12f + 0.18f * T, 0.35f + 0.45f * T);
		}
		return FLinearColor(0.12f + 0.72f * T, 0.30f + 0.55f * T, 0.10f + 0.55f * T);
	}
	}
}

void ACubusLandscapeEvolutionController::RebuildPreview()
{
	if (!GlobalDem.IsValid() || PreviewMesh == nullptr)
	{
		return;
	}

	GlobalCellSizeM = static_cast<float>(GlobalDem.CellSizeMeters);
	const int32 SourceR = GlobalDem.Resolution;
	const int32 R = FMath::Clamp(PreviewResolution, 17, FMath::Min(513, SourceR));
	const int32 VertexCount = R * R;

	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UV0;
	TArray<FLinearColor> Colors;
	TArray<FProcMeshTangent> Tangents;
	Vertices.SetNumUninitialized(VertexCount);
	Normals.Init(FVector::UpVector, VertexCount);
	UV0.SetNumUninitialized(VertexCount);
	Colors.SetNumUninitialized(VertexCount);
	Tangents.Init(FProcMeshTangent(1.0f, 0.0f, 0.0f), VertexCount);
	Triangles.Reserve((R - 1) * (R - 1) * 6);

	constexpr double CentimetersPerMeter = 100.0;
	const double PreviewWorldScale = CentimetersPerMeter * PreviewHorizontalScale;
	const double PreviewHeightScale = PreviewWorldScale * PreviewVerticalScale;
	const double HalfWorld = GlobalDem.WorldSizeMeters * 0.5;

	int32 StartX = 0;
	int32 StartY = 0;
	bool bNativeDetail = PreviewMode == ECubusLandscapePreviewMode::NativeDetail && SourceR > R;

	if (bNativeDetail)
	{
		int32 CenterCell = INDEX_NONE;
		if (bAutoFocusErosion && GlobalDem.EvolutionDeltaM.Num() == GlobalDem.NumCells())
		{
			float BestChange = -1.0f;
			for (int32 Cell = 0; Cell < GlobalDem.NumCells(); ++Cell)
			{
				const float Change = FMath::Abs(GlobalDem.EvolutionDeltaM[Cell]);
				if (Change > BestChange)
				{
					BestChange = Change;
					CenterCell = Cell;
				}
			}
		}
		int32 CenterX = SourceR / 2;
		int32 CenterY = SourceR / 2;
		if (CenterCell != INDEX_NONE)
		{
			CenterX = CenterCell % SourceR;
			CenterY = CenterCell / SourceR;
		}
		else if (!bAutoFocusErosion)
		{
			const double U = FMath::Clamp((PreviewCenterWorldMeters.X + HalfWorld) / GlobalDem.WorldSizeMeters, 0.0, 1.0);
			const double V = FMath::Clamp((PreviewCenterWorldMeters.Y + HalfWorld) / GlobalDem.WorldSizeMeters, 0.0, 1.0);
			CenterX = FMath::RoundToInt(U * (SourceR - 1));
			CenterY = FMath::RoundToInt(V * (SourceR - 1));
		}
		const int32 HalfWindow = (R - 1) / 2;
		StartX = FMath::Clamp(CenterX - HalfWindow, 0, SourceR - R);
		StartY = FMath::Clamp(CenterY - HalfWindow, 0, SourceR - R);
		const int32 ActualCenterX = StartX + (R - 1) / 2;
		const int32 ActualCenterY = StartY + (R - 1) / 2;
		PreviewActualCenterWorldMeters = FVector2D(-HalfWorld + ActualCenterX * GlobalDem.CellSizeMeters,
			-HalfWorld + ActualCenterY * GlobalDem.CellSizeMeters);
		PreviewDisplayedCellSizeM = static_cast<float>(GlobalDem.CellSizeMeters);
		PreviewWindowSizeKm = static_cast<float>((R - 1) * GlobalDem.CellSizeMeters / 1000.0);
	}
	else
	{
		bNativeDetail = false;
		PreviewActualCenterWorldMeters = FVector2D::ZeroVector;
		PreviewDisplayedCellSizeM = static_cast<float>(GlobalDem.WorldSizeMeters / static_cast<double>(R - 1));
		PreviewWindowSizeKm = static_cast<float>(GlobalDem.WorldSizeMeters / 1000.0);
	}

	for (int32 Y = 0; Y < R; ++Y)
	{
		for (int32 X = 0; X < R; ++X)
		{
			int32 SourceX = 0;
			int32 SourceY = 0;
			double WorldX = 0.0;
			double WorldY = 0.0;
			if (bNativeDetail)
			{
				SourceX = StartX + X;
				SourceY = StartY + Y;
				WorldX = -HalfWorld + SourceX * GlobalDem.CellSizeMeters;
				WorldY = -HalfWorld + SourceY * GlobalDem.CellSizeMeters;
			}
			else
			{
				const double U = static_cast<double>(X) / static_cast<double>(R - 1);
				const double V = static_cast<double>(Y) / static_cast<double>(R - 1);
				SourceX = FMath::Clamp(FMath::RoundToInt(U * (SourceR - 1)), 0, SourceR - 1);
				SourceY = FMath::Clamp(FMath::RoundToInt(V * (SourceR - 1)), 0, SourceR - 1);
				WorldX = -HalfWorld + U * GlobalDem.WorldSizeMeters;
				WorldY = -HalfWorld + V * GlobalDem.WorldSizeMeters;
			}

			const int32 SourceCell = GlobalDem.Index(SourceX, SourceY);
			const int32 I = Y * R + X;
			const double LocalWorldX = bNativeDetail ? WorldX - PreviewActualCenterWorldMeters.X : WorldX;
			const double LocalWorldY = bNativeDetail ? WorldY - PreviewActualCenterWorldMeters.Y : WorldY;
			Vertices[I] = FVector(LocalWorldX * PreviewWorldScale, LocalWorldY * PreviewWorldScale,
				GlobalDem.ElevationM[SourceCell] * PreviewHeightScale);
			UV0[I] = FVector2D(static_cast<double>(X) / static_cast<double>(R - 1), static_cast<double>(Y) / static_cast<double>(R - 1));
			Colors[I] = DebugColorForCell(SourceCell);
		}
	}

	for (int32 Y = 0; Y < R - 1; ++Y)
	{
		for (int32 X = 0; X < R - 1; ++X)
		{
			const int32 A = Y * R + X;
			const int32 B = A + 1;
			const int32 C = A + R;
			const int32 D = C + 1;
			Triangles.Add(A); Triangles.Add(C); Triangles.Add(B);
			Triangles.Add(B); Triangles.Add(C); Triangles.Add(D);
		}
	}

	for (int32 Y = 0; Y < R; ++Y)
	{
		for (int32 X = 0; X < R; ++X)
		{
			const int32 Xm = FMath::Max(X - 1, 0);
			const int32 Xp = FMath::Min(X + 1, R - 1);
			const int32 Ym = FMath::Max(Y - 1, 0);
			const int32 Yp = FMath::Min(Y + 1, R - 1);
			const FVector Dx = Vertices[Y * R + Xp] - Vertices[Y * R + Xm];
			const FVector Dy = Vertices[Yp * R + X] - Vertices[Ym * R + X];
			Normals[Y * R + X] = FVector::CrossProduct(Dx, Dy).GetSafeNormal();
		}
	}

	PreviewMesh->ClearAllMeshSections();
	PreviewMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UV0, Colors, Tangents, false);
	if (PreviewMaterial)
	{
		PreviewMesh->SetMaterial(0, PreviewMaterial);
	}

	UE_LOG(LogTemp, Display, TEXT("Cubus island preview: %s, %.2f m/vertex, %.0f m window"),
		bNativeDetail ? TEXT("Native Detail") : TEXT("Full Island"), PreviewDisplayedCellSizeM, PreviewWindowSizeKm * 1000.0f);
}
