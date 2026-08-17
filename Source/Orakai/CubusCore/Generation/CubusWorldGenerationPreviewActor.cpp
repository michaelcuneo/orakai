#include "CubusCore/Generation/CubusWorldGenerationPreviewActor.h"

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
    PreviewMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    PreviewMesh->SetCollisionObjectType(ECC_WorldDynamic);
    PreviewMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
    PreviewMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    PreviewMesh->bUseAsyncCooking = true;
    PreviewMesh->CastShadow = true;

    PreviewCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("PreviewCapture"));
    PreviewCapture->SetupAttachment(Root);
    PreviewCapture->ProjectionType = ECameraProjectionMode::Perspective;
    PreviewCapture->FOVAngle = 38.0f;
    PreviewCapture->bCaptureEveryFrame = false;
    PreviewCapture->bCaptureOnMovement = false;
    PreviewCapture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;

    PreviewLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("PreviewLight"));
    PreviewLight->SetupAttachment(Root);
    PreviewLight->SetRelativeRotation(FRotator(-52.0, -36.0, 0.0));
    PreviewLight->Intensity = 5.0f;

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
    if (!FMath::IsNearlyEqual(CurrentProgress, LastObservedOverallProgress, KINDA_SMALL_NUMBER) || CurrentStage != LastObservedStage)
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
    PreviewPitchDegrees = FMath::Clamp(
        PreviewPitchDegrees - static_cast<float>(PointerDelta.Y) * OrbitDegreesPerPixel,
        -24.0f,
        34.0f);
    ApplyOrbitTransform();
    CapturePreview();
}

void ACubusWorldGenerationPreviewActor::ResetOrbit()
{
    PreviewYawDegrees = -28.0f;
    PreviewPitchDegrees = 0.0f;
    ApplyOrbitTransform();
    CapturePreview();
}

void ACubusWorldGenerationPreviewActor::ApplyOrbitTransform()
{
    if (IsValid(MeshPivot))
    {
        MeshPivot->SetRelativeRotation(FRotator(PreviewPitchDegrees, PreviewYawDegrees, 0.0f));
    }

    if (IsValid(PreviewCapture))
    {
        const float Distance = FMath::Max(PreviewHorizontalSize * 1.55f, 600.0f);
        const FVector CameraLocation(Distance, 0.0f, Distance * 0.83f);
        PreviewCapture->SetRelativeLocation(CameraLocation);
        PreviewCapture->SetRelativeRotation(UKismetMathLibrary::FindLookAtRotation(CameraLocation, FVector::ZeroVector));
    }
}

void ACubusWorldGenerationPreviewActor::RefreshPreviewNow()
{
    ResolveLoader();
    EnsureRenderTarget();
    if (!IsValid(TargetLoader))
    {
        return;
    }

    RebuildPreviewMesh();
    LastObservedOverallProgress = TargetLoader->GetOverallProgress();
    LastObservedStage = static_cast<uint8>(TargetLoader->GetGenerationStage());
    CapturePreview();
}

void ACubusWorldGenerationPreviewActor::RebuildPreviewMesh()
{
    if (!IsValid(TargetLoader) || !IsValid(PreviewMesh))
    {
        return;
    }

    const int32 Resolution = FMath::Clamp(PreviewMeshResolution, 32, 256);
    const FVector2D WorldSizeMeters(
        FMath::Max(1.0, TargetLoader->GenerationSizeKm.X * 1000.0),
        FMath::Max(1.0, TargetLoader->GenerationSizeKm.Y * 1000.0));
    const FVector2D HalfWorld = WorldSizeMeters * 0.5;

    const float MaxDimensionMeters = static_cast<float>(FMath::Max(WorldSizeMeters.X, WorldSizeMeters.Y));
    const float HorizontalScale = PreviewHorizontalSize / FMath::Max(1.0f, MaxDimensionMeters);
    BuiltMeshDimensions = FVector2D(
        static_cast<double>(WorldSizeMeters.X) * HorizontalScale,
        static_cast<double>(WorldSizeMeters.Y) * HorizontalScale);

    TArray<float> Heights;
    Heights.Init(MAX_flt, Resolution * Resolution);

    float MinHeight = MAX_flt;
    float MaxHeight = -MAX_flt;
    for (int32 Y = 0; Y < Resolution; ++Y)
    {
        const double V = static_cast<double>(Y) / static_cast<double>(Resolution - 1);
        const double WorldY = FMath::Lerp(-HalfWorld.Y, HalfWorld.Y, V);
        for (int32 X = 0; X < Resolution; ++X)
        {
            const double U = static_cast<double>(X) / static_cast<double>(Resolution - 1);
            const double WorldX = FMath::Lerp(-HalfWorld.X, HalfWorld.X, U);
            float HeightMeters = 0.0f;
            if (!TargetLoader->GetGeneratedHeightMeters(WorldX, WorldY, HeightMeters))
            {
                continue;
            }

            Heights[Y * Resolution + X] = HeightMeters;
            MinHeight = FMath::Min(MinHeight, HeightMeters);
            MaxHeight = FMath::Max(MaxHeight, HeightMeters);
        }
    }

    if (MinHeight == MAX_flt)
    {
        PreviewMesh->ClearAllMeshSections();
        bHasRenderableTerrain = false;
        bPickingCollisionBuilt = false;
        return;
    }

    const float HeightSpan = FMath::Max(1.0f, MaxHeight - MinHeight);
    const float MidHeight = (MinHeight + MaxHeight) * 0.5f;
    const float VerticalScale = PreviewVerticalRelief / HeightSpan;

    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FLinearColor> Colors;
    TArray<FProcMeshTangent> Tangents;
    TArray<int32> VertexMap;
    VertexMap.Init(INDEX_NONE, Resolution * Resolution);

    Vertices.Reserve(Resolution * Resolution);
    Normals.Reserve(Resolution * Resolution);
    UVs.Reserve(Resolution * Resolution);
    Colors.Reserve(Resolution * Resolution);

    for (int32 Y = 0; Y < Resolution; ++Y)
    {
        const float V = static_cast<float>(Y) / static_cast<float>(Resolution - 1);
        for (int32 X = 0; X < Resolution; ++X)
        {
            const int32 SourceIndex = Y * Resolution + X;
            const float Height = Heights[SourceIndex];
            if (Height == MAX_flt)
            {
                continue;
            }

            const float U = static_cast<float>(X) / static_cast<float>(Resolution - 1);
            const float LocalX = (U - 0.5f) * static_cast<float>(BuiltMeshDimensions.X);
            const float LocalY = (V - 0.5f) * static_cast<float>(BuiltMeshDimensions.Y);
            const float LocalZ = (Height - MidHeight) * VerticalScale;

            VertexMap[SourceIndex] = Vertices.Add(FVector(LocalX, LocalY, LocalZ));
            UVs.Add(FVector2D(U, 1.0f - V));
            Colors.Add(HeightColor(FMath::Clamp((Height - MinHeight) / HeightSpan, 0.0f, 1.0f)));
            Normals.Add(FVector::UpVector);
        }
    }

    for (int32 Y = 0; Y < Resolution - 1; ++Y)
    {
        for (int32 X = 0; X < Resolution - 1; ++X)
        {
            const int32 I00 = VertexMap[Y * Resolution + X];
            const int32 I10 = VertexMap[Y * Resolution + X + 1];
            const int32 I01 = VertexMap[(Y + 1) * Resolution + X];
            const int32 I11 = VertexMap[(Y + 1) * Resolution + X + 1];
            if (I00 == INDEX_NONE || I10 == INDEX_NONE || I01 == INDEX_NONE || I11 == INDEX_NONE)
            {
                continue;
            }

            Triangles.Add(I00);
            Triangles.Add(I11);
            Triangles.Add(I10);
            Triangles.Add(I00);
            Triangles.Add(I01);
            Triangles.Add(I11);
        }
    }

    if (Triangles.IsEmpty())
    {
        PreviewMesh->ClearAllMeshSections();
        bHasRenderableTerrain = false;
        bPickingCollisionBuilt = false;
        return;
    }

    Normals.Init(FVector::ZeroVector, Vertices.Num());
    for (int32 Index = 0; Index + 2 < Triangles.Num(); Index += 3)
    {
        const int32 A = Triangles[Index];
        const int32 B = Triangles[Index + 1];
        const int32 C = Triangles[Index + 2];
        const FVector FaceNormal = FVector::CrossProduct(Vertices[B] - Vertices[A], Vertices[C] - Vertices[A]).GetSafeNormal();
        Normals[A] += FaceNormal;
        Normals[B] += FaceNormal;
        Normals[C] += FaceNormal;
    }
    for (FVector& Normal : Normals)
    {
        Normal = Normal.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
    }

    PreviewMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, Colors, Tangents, true);
    if (IsValid(PreviewMaterial))
    {
        PreviewMesh->SetMaterial(0, PreviewMaterial);
    }

    bHasRenderableTerrain = true;
    bPickingCollisionBuilt = true;
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

    const float TanHalfFov = FMath::Tan(FMath::DegreesToRadians(PreviewCapture->FOVAngle * 0.5f));
    const float NdcX = static_cast<float>(PreviewUV.X * 2.0 - 1.0);
    const float NdcY = static_cast<float>(1.0 - PreviewUV.Y * 2.0);

    const FTransform CaptureTransform = PreviewCapture->GetComponentTransform();
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
    if (!IsValid(TargetLoader) || !bHasRenderableTerrain || !bPickingCollisionBuilt ||
        PreviewWidgetSize.X <= UE_SMALL_NUMBER || PreviewWidgetSize.Y <= UE_SMALL_NUMBER)
    {
        return false;
    }

    const FVector2D ScreenUV(
        FMath::Clamp(LocalPreviewPosition.X / PreviewWidgetSize.X, 0.0, 1.0),
        FMath::Clamp(LocalPreviewPosition.Y / PreviewWidgetSize.Y, 0.0, 1.0));

    FVector RayOrigin;
    FVector RayDirection;
    if (!BuildCaptureRay(ScreenUV, RayOrigin, RayDirection))
    {
        return false;
    }

    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(CubusGeneratedPreviewPick), true);
    if (!PreviewMesh->LineTraceComponent(Hit, RayOrigin, RayOrigin + RayDirection * 100000.0f, Params))
    {
        return false;
    }

    const FVector LocalHit = MeshPivot->GetComponentTransform().InverseTransformPosition(Hit.ImpactPoint);
    const double U = FMath::Clamp(LocalHit.X / FMath::Max(1.0, BuiltMeshDimensions.X) + 0.5, 0.0, 1.0);
    const double VBottomUp = FMath::Clamp(LocalHit.Y / FMath::Max(1.0, BuiltMeshDimensions.Y) + 0.5, 0.0, 1.0);

    OutPreviewUV = FVector2D(U, 1.0 - VBottomUp);
    return TargetLoader->SetGeneratedSpawnFromPreviewUV(OutPreviewUV, OutWorldMeters);
}
