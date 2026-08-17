#include "CubusCore/Generation/CubusWorldGenerationPreviewActor.h"

#include "CubusCore/Generation/CubusGeneratedTerrainRuntime.h"
#include "CubusCore/Generation/CubusWorldGenerationLoaderActor.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/Image.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/KismetMathLibrary.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "Styling/SlateBrush.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/SceneCaptureComponent2D.h"

namespace
{
bool RayTriangleIntersection(
    const FVector& RayOrigin,
    const FVector& RayDirection,
    const FVector& A,
    const FVector& B,
    const FVector& C,
    float& OutDistance)
{
    constexpr float Epsilon = 1.0e-6f;
    const FVector Edge1 = B - A;
    const FVector Edge2 = C - A;
    const FVector P = FVector::CrossProduct(RayDirection, Edge2);
    const float Determinant = FVector::DotProduct(Edge1, P);
    if (FMath::Abs(Determinant) < Epsilon)
    {
        return false;
    }

    const float InverseDeterminant = 1.0f / Determinant;
    const FVector T = RayOrigin - A;
    const float U = FVector::DotProduct(T, P) * InverseDeterminant;
    if (U < 0.0f || U > 1.0f)
    {
        return false;
    }

    const FVector Q = FVector::CrossProduct(T, Edge1);
    const float V = FVector::DotProduct(RayDirection, Q) * InverseDeterminant;
    if (V < 0.0f || U + V > 1.0f)
    {
        return false;
    }

    const float Distance = FVector::DotProduct(Edge2, Q) * InverseDeterminant;
    if (Distance <= Epsilon)
    {
        return false;
    }

    OutDistance = Distance;
    return true;
}
}

ACubusWorldGenerationPreviewActor::ACubusWorldGenerationPreviewActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    MeshPivot = CreateDefaultSubobject<USceneComponent>(TEXT("MeshPivot"));
    MeshPivot->SetupAttachment(Root);

    PreviewMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("PreviewMesh"));
    PreviewMesh->SetupAttachment(MeshPivot);
    PreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PreviewMesh->SetGenerateOverlapEvents(false);
    PreviewMesh->CastShadow = false;
    PreviewMesh->bCastDynamicShadow = false;
    PreviewMesh->bCastStaticShadow = false;

    PreviewCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("PreviewCapture"));
    PreviewCapture->SetupAttachment(Root);
    PreviewCapture->ProjectionType = ECameraProjectionMode::Orthographic;
    PreviewCapture->OrthoWidth = PreviewHorizontalSize * 2.0f;
    PreviewCapture->bCaptureEveryFrame = false;
    PreviewCapture->bCaptureOnMovement = false;
    PreviewCapture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
    PreviewCapture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
    PreviewCapture->ShowFlags.SetAtmosphere(false);
    PreviewCapture->ShowFlags.SetCloud(false);
    PreviewCapture->ShowFlags.SetFog(false);
    PreviewCapture->ShowFlags.SetMotionBlur(false);
    PreviewCapture->ShowFlags.SetBloom(false);
    PreviewCapture->ShowFlags.SetAmbientOcclusion(false);
    PreviewCapture->ShowFlags.SetAntiAliasing(true);

    PreviewLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("PreviewLight"));
    PreviewLight->SetupAttachment(Root);
    PreviewLight->SetRelativeRotation(FRotator(-52.0, -36.0, 0.0));
    PreviewLight->Intensity = 5.0f;
    PreviewLight->CastShadows = false;

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> VertexMaterial(
        TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial"));
    if (VertexMaterial.Succeeded())
    {
        PreviewMaterial = VertexMaterial.Object;
    }
}

void ACubusWorldGenerationPreviewActor::BeginPlay()
{
    Super::BeginPlay();
    ResolveLoader();
    EnsureRenderTarget();
    if (IsValid(PreviewCapture) && IsValid(PreviewMesh))
    {
        PreviewCapture->ShowOnlyComponent(PreviewMesh);
    }
    ResetOrbit();
    RefreshPreviewNow();
}

void ACubusWorldGenerationPreviewActor::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    ResolveLoader();
    if (!IsValid(TargetLoader))
    {
        return;
    }

    RefreshCountdown -= DeltaSeconds;
    if (RefreshCountdown > 0.0f)
    {
        return;
    }
    RefreshCountdown = FMath::Max(0.02f, PreviewRefreshInterval);

    const float CurrentProgress = TargetLoader->GetOverallProgress();
    const uint8 CurrentStage = static_cast<uint8>(TargetLoader->GetGenerationStage());
    if (!FMath::IsNearlyEqual(CurrentProgress, LastObservedOverallProgress, 0.0025f) || CurrentStage != LastObservedStage)
    {
        RefreshPreviewNow();
    }
}

void ACubusWorldGenerationPreviewActor::ResolveLoader()
{
    if (IsValid(TargetLoader) || !IsValid(GetWorld()))
    {
        return;
    }

    for (TActorIterator<ACubusWorldGenerationLoaderActor> It(GetWorld()); It; ++It)
    {
        if (IsValid(*It))
        {
            TargetLoader = *It;
            return;
        }
    }
}

void ACubusWorldGenerationPreviewActor::EnsureRenderTarget()
{
    const int32 Resolution = FMath::Clamp(PreviewRenderResolution, 256, 2048);
    if (IsValid(PreviewRenderTarget) && PreviewRenderTarget->SizeX == Resolution && PreviewRenderTarget->SizeY == Resolution)
    {
        return;
    }

    PreviewRenderTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("GeneratedTerrain3DPreviewRT"));
    PreviewRenderTarget->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8_SRGB;
    PreviewRenderTarget->ClearColor = FLinearColor(0.012f, 0.015f, 0.020f, 1.0f);
    PreviewRenderTarget->InitAutoFormat(Resolution, Resolution);
    PreviewRenderTarget->UpdateResourceImmediate(true);
    PreviewCapture->TextureTarget = PreviewRenderTarget;
}

bool ACubusWorldGenerationPreviewActor::ApplyPreviewToImage(UImage* TargetImage)
{
    if (!IsValid(TargetImage))
    {
        return false;
    }

    EnsureRenderTarget();
    if (!IsValid(PreviewRenderTarget))
    {
        return false;
    }

    FSlateBrush Brush;
    Brush.SetResourceObject(PreviewRenderTarget);
    Brush.ImageSize = FVector2D(PreviewRenderTarget->SizeX, PreviewRenderTarget->SizeY);
    TargetImage->SetBrush(Brush);
    return true;
}

void ACubusWorldGenerationPreviewActor::AddOrbitInput(const FVector2D PointerDelta)
{
    PreviewYawDegrees = FMath::Fmod(PreviewYawDegrees + static_cast<float>(PointerDelta.X) * OrbitDegreesPerPixel, 360.0f);
    if (PreviewYawDegrees < 0.0f)
    {
        PreviewYawDegrees += 360.0f;
    }

    const float PitchLimit = FMath::Max(0.0f, PreviewPitchLimitDegrees);
    PreviewPitchDegrees = FMath::Clamp(
        PreviewPitchDegrees - static_cast<float>(PointerDelta.Y) * OrbitDegreesPerPixel,
        -PitchLimit,
        PitchLimit);
    ApplyOrbitTransform();
    CapturePreview();
}

void ACubusWorldGenerationPreviewActor::ResetOrbit()
{
    PreviewYawDegrees = -45.0f;
    PreviewPitchDegrees = 0.0f;
    ApplyOrbitTransform();
    CapturePreview();
}

void ACubusWorldGenerationPreviewActor::ApplyOrbitTransform()
{
    if (!IsValid(PreviewCapture))
    {
        return;
    }

    const float HalfX = static_cast<float>(BuiltMeshDimensions.X) * 0.5f;
    const float HalfY = static_cast<float>(BuiltMeshDimensions.Y) * 0.5f;
    const float HalfZ = PreviewVerticalRelief * 0.5f;
    const float BoundingRadius = FMath::Sqrt(HalfX * HalfX + HalfY * HalfY + HalfZ * HalfZ);

    const float EffectiveMargin = FMath::Max(1.75f, PreviewFramingMargin);
    const float FullDiameter = FMath::Max(PreviewHorizontalSize, BoundingRadius * 2.0f);
    PreviewCapture->ProjectionType = ECameraProjectionMode::Orthographic;
    PreviewCapture->OrthoWidth = FullDiameter * EffectiveMargin;

    const float Distance = FMath::Max(FullDiameter * 1.6f, 1800.0f);
    const float ElevationDegrees = PreviewBaseElevationDegrees + PreviewPitchDegrees;
    const float ElevationRadians = FMath::DegreesToRadians(ElevationDegrees);
    const float YawRadians = FMath::DegreesToRadians(PreviewYawDegrees);
    const float HorizontalRadius = Distance * FMath::Cos(ElevationRadians);

    const FVector CameraLocation(
        HorizontalRadius * FMath::Cos(YawRadians),
        HorizontalRadius * FMath::Sin(YawRadians),
        Distance * FMath::Sin(ElevationRadians));

    PreviewCapture->SetRelativeLocation(CameraLocation);
    PreviewCapture->SetRelativeRotation(UKismetMathLibrary::FindLookAtRotation(CameraLocation, FVector::ZeroVector));
}

void ACubusWorldGenerationPreviewActor::RefreshPreviewNow()
{
    ResolveLoader();
    EnsureRenderTarget();
    if (!IsValid(TargetLoader) && !FCubusGeneratedTerrainRuntime::IsActive())
    {
        return;
    }

    RebuildPreviewMesh();
    ApplyOrbitTransform();
    if (IsValid(TargetLoader))
    {
        LastObservedOverallProgress = TargetLoader->GetOverallProgress();
        LastObservedStage = static_cast<uint8>(TargetLoader->GetGenerationStage());
    }
    CapturePreview();
}

void ACubusWorldGenerationPreviewActor::RebuildPreviewMesh()
{
    if (!IsValid(PreviewMesh))
    {
        return;
    }

    FBox2D RuntimeBounds;
    const bool bUseLoader = IsValid(TargetLoader);
    if (!bUseLoader && !FCubusGeneratedTerrainRuntime::GetPreviewBoundsMeters(RuntimeBounds))
    {
        PreviewMesh->ClearAllMeshSections();
        CachedPickVertices.Reset();
        CachedPickTriangles.Reset();
        bHasRenderableTerrain = false;
        return;
    }

    const FVector2D WorldSizeMeters = bUseLoader
        ? FVector2D(
            FMath::Max(1.0, TargetLoader->GenerationSizeKm.X * 1000.0),
            FMath::Max(1.0, TargetLoader->GenerationSizeKm.Y * 1000.0))
        : RuntimeBounds.GetSize();
    const FVector2D WorldMinimum = bUseLoader ? WorldSizeMeters * -0.5 : RuntimeBounds.Min;

    const int32 Resolution = FMath::Clamp(PreviewMeshResolution, 32, 256);
    const float MaxDimensionMeters = static_cast<float>(FMath::Max(WorldSizeMeters.X, WorldSizeMeters.Y));
    const float HorizontalScale = PreviewHorizontalSize / FMath::Max(1.0f, MaxDimensionMeters);
    BuiltMeshDimensions = FVector2D(
        static_cast<double>(WorldSizeMeters.X) * HorizontalScale,
        static_cast<double>(WorldSizeMeters.Y) * HorizontalScale);

    TArray<float> Heights;
    TArray<bool> HasHeight;
    Heights.Init(0.0f, Resolution * Resolution);
    HasHeight.Init(false, Resolution * Resolution);

    float MinHeight = MAX_flt;
    float MaxHeight = -MAX_flt;
    int32 KnownSampleCount = 0;
    for (int32 Y = 0; Y < Resolution; ++Y)
    {
        const double V = static_cast<double>(Y) / static_cast<double>(Resolution - 1);
        const double WorldY = WorldMinimum.Y + V * WorldSizeMeters.Y;
        for (int32 X = 0; X < Resolution; ++X)
        {
            const double U = static_cast<double>(X) / static_cast<double>(Resolution - 1);
            const double WorldX = WorldMinimum.X + U * WorldSizeMeters.X;
            float HeightMeters = 0.0f;
            const bool bSampled = bUseLoader
                ? TargetLoader->GetGeneratedHeightMeters(WorldX, WorldY, HeightMeters)
                : FCubusGeneratedTerrainRuntime::TrySampleHeightMeters(FVector2D(WorldX, WorldY), HeightMeters);
            if (!bSampled)
            {
                continue;
            }

            const int32 Index = Y * Resolution + X;
            Heights[Index] = HeightMeters;
            HasHeight[Index] = true;
            ++KnownSampleCount;
            MinHeight = FMath::Min(MinHeight, HeightMeters);
            MaxHeight = FMath::Max(MaxHeight, HeightMeters);
        }
    }

    for (int32 Pass = 0; Pass < FMath::Clamp(PreviewSmoothingPasses, 0, 3); ++Pass)
    {
        TArray<float> Smoothed = Heights;
        for (int32 Y = 1; Y < Resolution - 1; ++Y)
        {
            for (int32 X = 1; X < Resolution - 1; ++X)
            {
                const int32 Index = Y * Resolution + X;
                if (!HasHeight[Index])
                {
                    continue;
                }

                float Sum = Heights[Index] * 4.0f;
                float Weight = 4.0f;
                const int32 Neighbors[4] = {Index - 1, Index + 1, Index - Resolution, Index + Resolution};
                for (const int32 Neighbor : Neighbors)
                {
                    if (HasHeight[Neighbor])
                    {
                        Sum += Heights[Neighbor];
                        Weight += 1.0f;
                    }
                }
                Smoothed[Index] = Sum / Weight;
            }
        }
        Heights = MoveTemp(Smoothed);
    }

    if (KnownSampleCount > 0)
    {
        MinHeight = MAX_flt;
        MaxHeight = -MAX_flt;
        for (int32 Index = 0; Index < Heights.Num(); ++Index)
        {
            if (!HasHeight[Index])
            {
                continue;
            }
            MinHeight = FMath::Min(MinHeight, Heights[Index]);
            MaxHeight = FMath::Max(MaxHeight, Heights[Index]);
        }
    }

    const bool bHasKnownTerrain = KnownSampleCount > 0;
    const float HeightSpan = bHasKnownTerrain ? FMath::Max(1.0f, MaxHeight - MinHeight) : 1.0f;
    const float MidHeight = bHasKnownTerrain ? (MinHeight + MaxHeight) * 0.5f : 0.0f;
    const float VerticalScale = PreviewVerticalRelief / HeightSpan;

    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FLinearColor> Colors;
    TArray<FProcMeshTangent> Tangents;

    Vertices.SetNumUninitialized(Resolution * Resolution);
    Normals.Init(FVector::UpVector, Resolution * Resolution);
    UVs.SetNumUninitialized(Resolution * Resolution);
    Colors.SetNumUninitialized(Resolution * Resolution);

    for (int32 Y = 0; Y < Resolution; ++Y)
    {
        const float V = static_cast<float>(Y) / static_cast<float>(Resolution - 1);
        for (int32 X = 0; X < Resolution; ++X)
        {
            const int32 Index = Y * Resolution + X;
            const float U = static_cast<float>(X) / static_cast<float>(Resolution - 1);
            const float LocalX = (U - 0.5f) * static_cast<float>(BuiltMeshDimensions.X);
            const float LocalY = (V - 0.5f) * static_cast<float>(BuiltMeshDimensions.Y);
            const float LocalZ = HasHeight[Index]
                ? (Heights[Index] - MidHeight) * VerticalScale
                : 0.0f;

            Vertices[Index] = FVector(LocalX, LocalY, LocalZ);
            UVs[Index] = FVector2D(U, 1.0f - V);
            Colors[Index] = HasHeight[Index]
                ? HeightColor(FMath::Clamp((Heights[Index] - MinHeight) / HeightSpan, 0.0f, 1.0f))
                : FLinearColor(0.055f, 0.075f, 0.06f, 1.0f);
        }
    }

    // Smooth normals from adjacent height samples in O(vertices), rather than
    // accumulating every triangle face normal on every live refresh.
    const float StepX = static_cast<float>(BuiltMeshDimensions.X) / FMath::Max(1, Resolution - 1);
    const float StepY = static_cast<float>(BuiltMeshDimensions.Y) / FMath::Max(1, Resolution - 1);
    for (int32 Y = 0; Y < Resolution; ++Y)
    {
        for (int32 X = 0; X < Resolution; ++X)
        {
            const int32 XL = FMath::Max(0, X - 1);
            const int32 XR = FMath::Min(Resolution - 1, X + 1);
            const int32 YD = FMath::Max(0, Y - 1);
            const int32 YU = FMath::Min(Resolution - 1, Y + 1);
            const float ZL = Vertices[Y * Resolution + XL].Z;
            const float ZR = Vertices[Y * Resolution + XR].Z;
            const float ZD = Vertices[YD * Resolution + X].Z;
            const float ZU = Vertices[YU * Resolution + X].Z;
            const FVector DX(FMath::Max(StepX * static_cast<float>(XR - XL), UE_SMALL_NUMBER), 0.0f, ZR - ZL);
            const FVector DY(0.0f, FMath::Max(StepY * static_cast<float>(YU - YD), UE_SMALL_NUMBER), ZU - ZD);
            Normals[Y * Resolution + X] = FVector::CrossProduct(DX, DY).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
        }
    }

    const bool bTopologyMatches =
        PreviewMesh->GetNumSections() > 0 &&
        CachedPickVertices.Num() == Vertices.Num() &&
        CachedPickTriangles.Num() == (Resolution - 1) * (Resolution - 1) * 6;

    if (bTopologyMatches)
    {
        PreviewMesh->UpdateMeshSection_LinearColor(0, Vertices, Normals, UVs, Colors, Tangents);
    }
    else
    {
        Triangles.Reserve((Resolution - 1) * (Resolution - 1) * 6);
        for (int32 Y = 0; Y < Resolution - 1; ++Y)
        {
            for (int32 X = 0; X < Resolution - 1; ++X)
            {
                const int32 I00 = Y * Resolution + X;
                const int32 I10 = I00 + 1;
                const int32 I01 = (Y + 1) * Resolution + X;
                const int32 I11 = I01 + 1;
                Triangles.Add(I00);
                Triangles.Add(I11);
                Triangles.Add(I10);
                Triangles.Add(I00);
                Triangles.Add(I01);
                Triangles.Add(I11);
            }
        }

        PreviewMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, Colors, Tangents, false);
        PreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        if (IsValid(PreviewMaterial))
        {
            PreviewMesh->SetMaterial(0, PreviewMaterial);
        }
        CachedPickTriangles = MoveTemp(Triangles);
    }

    CachedPickVertices = MoveTemp(Vertices);
    bHasRenderableTerrain = true;
}

FLinearColor ACubusWorldGenerationPreviewActor::HeightColor(const float NormalizedHeight) const
{
    const float T = FMath::Clamp(NormalizedHeight, 0.0f, 1.0f);
    if (T < 0.28f)
    {
        return FMath::Lerp(FLinearColor(0.04f, 0.10f, 0.05f), FLinearColor(0.15f, 0.31f, 0.11f), T / 0.28f);
    }
    if (T < 0.58f)
    {
        return FMath::Lerp(FLinearColor(0.15f, 0.31f, 0.11f), FLinearColor(0.42f, 0.31f, 0.17f), (T - 0.28f) / 0.30f);
    }
    if (T < 0.82f)
    {
        return FMath::Lerp(FLinearColor(0.42f, 0.31f, 0.17f), FLinearColor(0.50f, 0.50f, 0.48f), (T - 0.58f) / 0.24f);
    }
    return FMath::Lerp(FLinearColor(0.50f, 0.50f, 0.48f), FLinearColor(0.94f, 0.96f, 0.98f), (T - 0.82f) / 0.18f);
}

void ACubusWorldGenerationPreviewActor::CapturePreview()
{
    if (IsValid(PreviewCapture) && IsValid(PreviewRenderTarget))
    {
        PreviewCapture->CaptureScene();
    }
}

bool ACubusWorldGenerationPreviewActor::BuildCaptureRay(
    const FVector2D PreviewUV,
    FVector& OutOrigin,
    FVector& OutDirection) const
{
    if (!IsValid(PreviewCapture))
    {
        return false;
    }

    const FTransform CaptureTransform = PreviewCapture->GetComponentTransform();
    if (PreviewCapture->ProjectionType == ECameraProjectionMode::Orthographic)
    {
        const float NdcX = static_cast<float>(PreviewUV.X * 2.0 - 1.0);
        const float NdcY = static_cast<float>(1.0 - PreviewUV.Y * 2.0);
        const float HalfWidth = PreviewCapture->OrthoWidth * 0.5f;
        const FVector LocalOrigin(0.0f, NdcX * HalfWidth, NdcY * HalfWidth);
        OutOrigin = CaptureTransform.TransformPosition(LocalOrigin);
        OutDirection = CaptureTransform.GetUnitAxis(EAxis::X);
        return true;
    }

    const float TanHalfFov = FMath::Tan(FMath::DegreesToRadians(PreviewCapture->FOVAngle * 0.5f));
    const float NdcX = static_cast<float>(PreviewUV.X * 2.0 - 1.0);
    const float NdcY = static_cast<float>(1.0 - PreviewUV.Y * 2.0);
    const FVector LocalDirection(1.0f, NdcX * TanHalfFov, NdcY * TanHalfFov);
    OutOrigin = CaptureTransform.GetLocation();
    OutDirection = CaptureTransform.TransformVectorNoScale(LocalDirection).GetSafeNormal();
    return true;
}

bool ACubusWorldGenerationPreviewActor::SetGeneratedSpawnFromPreviewPosition(
    const FVector2D LocalPreviewPosition,
    const FVector2D PreviewWidgetSize,
    FVector2D& OutPreviewUV,
    FVector2D& OutWorldMeters)
{
    OutPreviewUV = FVector2D::ZeroVector;
    OutWorldMeters = FVector2D::ZeroVector;
    if (!bHasRenderableTerrain || CachedPickVertices.IsEmpty() || CachedPickTriangles.IsEmpty() ||
        PreviewWidgetSize.X <= UE_SMALL_NUMBER || PreviewWidgetSize.Y <= UE_SMALL_NUMBER)
    {
        return false;
    }

    const FVector2D ScreenUV(
        FMath::Clamp(LocalPreviewPosition.X / PreviewWidgetSize.X, 0.0, 1.0),
        FMath::Clamp(LocalPreviewPosition.Y / PreviewWidgetSize.Y, 0.0, 1.0));

    FVector WorldRayOrigin;
    FVector WorldRayDirection;
    if (!BuildCaptureRay(ScreenUV, WorldRayOrigin, WorldRayDirection))
    {
        return false;
    }

    const FTransform PivotTransform = MeshPivot->GetComponentTransform();
    const FVector LocalRayOrigin = PivotTransform.InverseTransformPosition(WorldRayOrigin);
    const FVector LocalRayDirection = PivotTransform.InverseTransformVectorNoScale(WorldRayDirection).GetSafeNormal();

    float ClosestDistance = MAX_flt;
    FVector ClosestHit = FVector::ZeroVector;
    bool bHit = false;
    for (int32 Index = 0; Index + 2 < CachedPickTriangles.Num(); Index += 3)
    {
        const int32 IA = CachedPickTriangles[Index];
        const int32 IB = CachedPickTriangles[Index + 1];
        const int32 IC = CachedPickTriangles[Index + 2];
        if (!CachedPickVertices.IsValidIndex(IA) || !CachedPickVertices.IsValidIndex(IB) || !CachedPickVertices.IsValidIndex(IC))
        {
            continue;
        }

        float Distance = 0.0f;
        if (RayTriangleIntersection(
                LocalRayOrigin,
                LocalRayDirection,
                CachedPickVertices[IA],
                CachedPickVertices[IB],
                CachedPickVertices[IC],
                Distance) &&
            Distance < ClosestDistance)
        {
            ClosestDistance = Distance;
            ClosestHit = LocalRayOrigin + LocalRayDirection * Distance;
            bHit = true;
        }
    }

    if (!bHit)
    {
        return false;
    }

    const double U = FMath::Clamp(ClosestHit.X / FMath::Max(1.0, BuiltMeshDimensions.X) + 0.5, 0.0, 1.0);
    const double VBottomUp = FMath::Clamp(ClosestHit.Y / FMath::Max(1.0, BuiltMeshDimensions.Y) + 0.5, 0.0, 1.0);
    OutPreviewUV = FVector2D(U, 1.0 - VBottomUp);

    if (IsValid(TargetLoader))
    {
        return TargetLoader->SetGeneratedSpawnFromPreviewUV(OutPreviewUV, OutWorldMeters);
    }

    if (!FCubusGeneratedTerrainRuntime::IsActive())
    {
        return false;
    }

    FCubusGeneratedTerrainRuntime::SetProposedSpawnFromPreviewUV(OutPreviewUV);
    return FCubusGeneratedTerrainRuntime::GetProposedSpawnWorldMeters(OutWorldMeters);
}
