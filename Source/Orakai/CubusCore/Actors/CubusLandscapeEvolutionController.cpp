#include "CubusCore/Actors/CubusLandscapeEvolutionController.h"

#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"

#if WITH_EDITOR
#include "UObject/UnrealType.h"
#endif

ACubusLandscapeEvolutionController::ACubusLandscapeEvolutionController()
{
	PrimaryActorTick.bCanEverTick = false;
	PreviewMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("PreviewMesh"));
	SetRootComponent(PreviewMesh);
	PreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewMesh->bUseAsyncCooking = true;
}

void ACubusLandscapeEvolutionController::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (bGenerateOnConstruction)
	{
		GenerateAndSolve();
	}
	else if (GlobalDem.IsValid())
	{
		RebuildPreview();
	}
}

#if WITH_EDITOR
void ACubusLandscapeEvolutionController::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName ChangedPropertyName = PropertyChangedEvent.Property != nullptr
		? PropertyChangedEvent.Property->GetFName()
		: NAME_None;

	if (ChangedPropertyName == GET_MEMBER_NAME_CHECKED(ACubusLandscapeEvolutionController, EditorAction))
	{
		const ECubusLandscapeEditorAction Action = EditorAction;
		EditorAction = ECubusLandscapeEditorAction::None;

		switch (Action)
		{
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

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

CubusLandscapeEvolution::FSettings ACubusLandscapeEvolutionController::MakeSettings() const
{
	CubusLandscapeEvolution::FSettings Settings;
	Settings.Seed = Seed;
	Settings.Resolution = GlobalResolution;
	Settings.WorldSizeMeters = WorldSizeMeters;
	Settings.PlateCount = PlateCount;
	Settings.RiverSourceAreaKm2 = RiverSourceAreaKm2;
	Settings.EvolutionIterations = EvolutionIterations;
	Settings.EvolutionStepYears = EvolutionStepYears;
	Settings.StreamPowerK = StreamPowerK;
	Settings.StreamPowerM = StreamPowerM;
	Settings.BaseUpliftRateMPerYear = BaseUpliftRateMPerYear;
	Settings.HillslopeDiffusivityM2PerYear = HillslopeDiffusivityM2PerYear;
	Settings.HydrologyRefreshInterval = HydrologyRefreshInterval;
	return Settings;
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
	if (!CubusLandscapeEvolution::FGenerator::GenerateSkeleton(Settings, GlobalDem, &Stats, &Error))
	{
		UE_LOG(LogTemp, Error, TEXT("Cubus landscape skeleton generation failed: %s"), *Error);
		return;
	}
	if (!CubusLandscapeEvolution::FGenerator::SolveHydrology(Settings, GlobalDem, &Stats, &Error))
	{
		UE_LOG(LogTemp, Error, TEXT("Cubus landscape hydrology solve failed: %s"), *Error);
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
	if (!CubusLandscapeEvolution::FGenerator::GenerateSkeleton(Settings, GlobalDem, &Stats, &Error))
	{
		UE_LOG(LogTemp, Error, TEXT("Cubus landscape skeleton generation failed: %s"), *Error);
		return;
	}
	if (!CubusLandscapeEvolution::FGenerator::SolveHydrology(Settings, GlobalDem, &Stats, &Error))
	{
		UE_LOG(LogTemp, Error, TEXT("Cubus landscape hydrology solve failed: %s"), *Error);
		return;
	}
	if (!CubusLandscapeEvolution::FGenerator::EvolveLandscape(Settings, GlobalDem, &Stats, &Error))
	{
		UE_LOG(LogTemp, Error, TEXT("Cubus landscape evolution failed: %s"), *Error);
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
	if (Stats.RiverCellCount > 0 || GlobalDem.HasHydrology())
	{
		GeneratedRiverCellCount = Stats.RiverCellCount;
		GeneratedBasinCount = Stats.BasinCount;
	}
	if (Stats.EvolutionIterations > 0)
	{
		MaximumStreamIncisionM = Stats.MaximumStreamIncisionM;
		MeanStreamIncisionM = Stats.MeanStreamIncisionM;
	}
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
			FLinearColor(0.40f, 0.72f, 0.35f),
			FLinearColor(0.65f, 0.52f, 0.30f),
			FLinearColor(0.58f, 0.52f, 0.38f),
			FLinearColor(0.42f, 0.40f, 0.38f),
			FLinearColor(0.55f, 0.38f, 0.30f),
			FLinearColor(0.35f, 0.25f, 0.22f),
			FLinearColor(0.30f, 0.48f, 0.52f)};
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
		const float T = FMath::Clamp(FMath::LogX(10.0f, FMath::Max(1.0f, A)) / 4.0f, 0.0f, 1.0f);
		return FLinearColor(T, T, 1.0f);
	}
	case ECubusLandscapeDebugView::RiverNetwork:
	{
		const bool bRiver = GlobalDem.RiverMask.IsValidIndex(Cell) && GlobalDem.RiverMask[Cell] != 0;
		if (bRiver)
		{
			return FLinearColor(0.05f, 0.25f, 1.0f);
		}
		const float E = GlobalDem.ElevationM[Cell];
		return E <= GlobalDem.OceanLevelM ? FLinearColor(0.03f, 0.08f, 0.18f) : FLinearColor(0.28f, 0.30f, 0.26f);
	}
	case ECubusLandscapeDebugView::StreamIncision:
	{
		const float Incision = GlobalDem.StreamIncisionM.IsValidIndex(Cell) ? GlobalDem.StreamIncisionM[Cell] : 0.0f;
		const float Scale = FMath::Max(1.0f, MaximumStreamIncisionM);
		const float T = FMath::Clamp(FMath::Sqrt(Incision / Scale), 0.0f, 1.0f);
		return FLinearColor(T, 0.08f, 1.0f - T);
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

	const int32 R = FMath::Clamp(PreviewResolution, 17, 513);
	const int32 SourceR = GlobalDem.Resolution;
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
	const double Half = GlobalDem.WorldSizeMeters * 0.5;

	for (int32 Y = 0; Y < R; ++Y)
	{
		const double V = static_cast<double>(Y) / static_cast<double>(R - 1);
		const int32 SourceY = FMath::Clamp(FMath::RoundToInt(V * (SourceR - 1)), 0, SourceR - 1);
		for (int32 X = 0; X < R; ++X)
		{
			const double U = static_cast<double>(X) / static_cast<double>(R - 1);
			const int32 SourceX = FMath::Clamp(FMath::RoundToInt(U * (SourceR - 1)), 0, SourceR - 1);
			const int32 SourceCell = GlobalDem.Index(SourceX, SourceY);
			const int32 I = Y * R + X;
			const double WorldX = -Half + U * GlobalDem.WorldSizeMeters;
			const double WorldY = -Half + V * GlobalDem.WorldSizeMeters;
			Vertices[I] = FVector(
				WorldX * PreviewWorldScale,
				WorldY * PreviewWorldScale,
				GlobalDem.ElevationM[SourceCell] * PreviewHeightScale);
			UV0[I] = FVector2D(U, V);
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
}
