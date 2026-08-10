#include "CubusCore/Actors/CubusWorldVegetationActor.h"
#include "CubusCore/Vegetation/CubusVegetationAssetResolver.h"
#include "CubusCore/Vegetation/CubusVegetationWindUtilities.h"
#include "CubusCore/Vegetation/CubusVegetationChunkFilter.h"
#include "CubusCore/Vegetation/CubusVegetationRepresentationSelector.h"

#include "CubusCore/Actors/CubusBlockWorldActor.h"
#include "CubusCore/Actors/CubusVoxelVolumeActor.h"
#include "CubusCore/Chunks/CubusBlockChunkData.h"
#include "CubusCore/Chunks/CubusChunkConstants.h"
#include "CubusCore/Generation/CubusBlockVegetationGenerator.h"
#include "CubusCore/Generation/CubusTerrainDensityField.h"
#include "CubusCore/Data/CubusVegetationInstance.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/InstancedSkinnedMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SceneComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Animation/TransformProviderData.h"
#include "DynamicWindData.h"
#include "DynamicWindSkeletalData.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Materials/MaterialInterface.h"
#include "UObject/SoftObjectPtr.h"

#if WITH_EDITOR
#include "FileHelpers.h"
#include "AnimToTextureBPLibrary.h"
#include "UObject/Package.h"
#endif

namespace
{
    constexpr int32 WorldGrassType = 1;
    constexpr int32 WorldShrubType = 2;
    constexpr int32 WorldBroadleafType = 3;
    constexpr int32 WorldReedsType = 4;
    constexpr int32 WorldAlpineType = 5;
    constexpr int32 WorldConiferType = 6;

    float ResolveTypeScaleMultiplier(
        const int32 TypeId,
        const bool bEnablePerTypeScaleOverrides,
        const float BroadleafScaleMultiplier,
        const float ConiferScaleMultiplier,
        const float ShrubScaleMultiplier,
        const float GrassScaleMultiplier,
        const float ReedsScaleMultiplier,
        const float AlpineScaleMultiplier
    )
    {
        if (!bEnablePerTypeScaleOverrides)
        {
            return 1.0f;
        }

        switch (TypeId)
        {
            case WorldBroadleafType:
                return BroadleafScaleMultiplier;
            case WorldConiferType:
                return ConiferScaleMultiplier;
            case WorldShrubType:
                return ShrubScaleMultiplier;
            case WorldGrassType:
                return GrassScaleMultiplier;
            case WorldReedsType:
                return ReedsScaleMultiplier;
            case WorldAlpineType:
                return AlpineScaleMultiplier;
            default:
                return 1.0f;
        }
    }
}

uint32 CalculateChunkVegetationSignature(
    const FCubusBlockChunkData& ChunkData
)
{
    uint32 Hash =
        GetTypeHash(
            ChunkData.GetVegetationInstances().Num()
        );

    Hash = HashCombineFast(
        Hash,
        GetTypeHash(
            ChunkData.GetVegetationRevision()
        )
    );

    return Hash;
}

uint32 CalculateVegetationSignatureMapHash(
    const TMap<FIntVector, uint32>& Signatures
)
{
    uint32 UnorderedHash = 0;

    for (
        const TPair<FIntVector, uint32>& Pair
        : Signatures
    )
    {
        UnorderedHash ^=
            HashCombineFast(
                GetTypeHash(Pair.Key),
                Pair.Value
            );
    }

    return HashCombineFast(
        GetTypeHash(Signatures.Num()),
        UnorderedHash
    );
}

ACubusWorldVegetationActor::ACubusWorldVegetationActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);
}

void ACubusWorldVegetationActor::ConfigureForWorld(
    ACubusBlockWorldActor* InBlockWorld
)
{
    BlockWorld = InBlockWorld;
    PublishedPlacementHash = 0;
    TimeUntilRefresh = 0.0f;
    VegetationPlacement.Reset();

    if (HasActorBegunPlay())
    {
        RebuildWorldVegetation();
    }
}

void ACubusWorldVegetationActor::BeginPlay()
{
    Super::BeginPlay();

    VegetationPlacement.Reset();

    ResolveBlockWorld();
    RefreshVegetationBatches();
    RefreshFarVegetationBatches();

    TimeUntilRefresh = 0.0f;
    TimeUntilFarVegetationPublish = 0.0f;
}

void ACubusWorldVegetationActor::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    UpdateDynamicWindBridge();

    ResolveBlockWorld();

    UpdateFarVegetationStreaming(
        DeltaSeconds
    );

    TimeUntilRefresh -= DeltaSeconds;
    if (TimeUntilRefresh > 0.0f)
    {
        return;
    }

    TimeUntilRefresh = FMath::Max(0.1f, RefreshInterval);

    int32 CurrentLoadedChunkCount = 0;
    const uint32 CurrentHash =
        CalculateLoadedPlacementHash(CurrentLoadedChunkCount);
    const uint32 CurrentSettingsHash =
        CalculateVegetationSettingsHash();

    bool bStreamingRecenterRequired = false;

    if (
        bPublishedVegetationBudgetSaturated &&
        bHasLastFullVegetationBuildCameraLocation
    )
    {
        const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
        if (IsValid(PlayerPawn))
        {
            const FVector StreamingLocation = PlayerPawn->GetActorLocation();
            const double RecenterDistance = static_cast<double>(
                FMath::Max(100.0f, VegetationRecenterDistance)
            );
            bStreamingRecenterRequired =
                FVector::DistSquared(
                    StreamingLocation,
                    LastFullVegetationBuildCameraLocation
                ) >= RecenterDistance * RecenterDistance;
        }
    }

    if (
        CurrentHash !=
            static_cast<uint32>(
                PublishedPlacementHash
            ) ||
        CurrentLoadedChunkCount !=
            LoadedChunkCount ||
        CurrentSettingsHash !=
            PublishedVegetationSettingsHash ||
        bStreamingRecenterRequired
    )
    {
        RebuildWorldVegetation();
    }
}

void ACubusWorldVegetationActor::EndPlay(
    const EEndPlayReason::Type EndPlayReason
)
{
    ClearFarVegetation();
    ClearWorldVegetation();
    VegetationPlacement.Reset();
    Super::EndPlay(EndPlayReason);
}

void ACubusWorldVegetationActor::UpdateDynamicWindBridge()
{
    if (!bBridgeUdwToDynamicWind)
    {
        return;
    }

    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return;
    }

    if (!IsValid(CachedUltraDynamicWeatherActor))
    {
        CachedUltraDynamicWeatherActor = FCubusVegetationWindUtilities::ResolveUltraDynamicWeatherActor(World);
    }

    if (!IsValid(CachedUltraDynamicWeatherActor))
    {
        return;
    }

    if (!IsValid(CachedDynamicWindCollection))
    {
        if (!DynamicWindCollectionOverride.IsNull())
        {
            CachedDynamicWindCollection =
                DynamicWindCollectionOverride.LoadSynchronous();
        }

        if (!IsValid(CachedDynamicWindCollection))
        {
            UMaterialParameterCollection* BestCandidate = nullptr;
            int32 BestScore = MIN_int32;

            for (TObjectIterator<UMaterialParameterCollection> It; It; ++It)
            {
                UMaterialParameterCollection* Candidate = *It;

                if (!IsValid(Candidate))
                {
                    continue;
                }

                const FString CandidateName =
                    Candidate->GetName();

                int32 Score = 0;

                if (CandidateName.Equals(TEXT("gUdw"), ESearchCase::IgnoreCase))
                {
                    Score += 100;
                }

                if (
                    CandidateName.Contains(TEXT("DynamicWind"), ESearchCase::IgnoreCase) ||
                    CandidateName.Contains(TEXT("Dynamic_Wind"), ESearchCase::IgnoreCase)
                )
                {
                    Score += 80;
                }

                if (CandidateName.Contains(TEXT("UDW_Wind"), ESearchCase::IgnoreCase))
                {
                    Score += 70;
                }

                if (CandidateName.Contains(TEXT("UltraDynamicWeather"), ESearchCase::IgnoreCase))
                {
                    Score += 50;
                }

                bool bHasWindVectorParam = false;
                for (const FCollectionVectorParameter& Param : Candidate->VectorParameters)
                {
                    const FString Name = Param.ParameterName.ToString();
                    if (
                        Name.Contains(TEXT("Wind"), ESearchCase::IgnoreCase) &&
                        (
                            Name.Contains(TEXT("Direction"), ESearchCase::IgnoreCase) ||
                            Name.Contains(TEXT("Dir"), ESearchCase::IgnoreCase)
                        )
                    )
                    {
                        bHasWindVectorParam = true;
                        break;
                    }
                }

                bool bHasWindScalarParam = false;
                for (const FCollectionScalarParameter& Param : Candidate->ScalarParameters)
                {
                    const FString Name = Param.ParameterName.ToString();
                    if (
                        Name.Contains(TEXT("Wind"), ESearchCase::IgnoreCase) &&
                        (
                            Name.Contains(TEXT("Intensity"), ESearchCase::IgnoreCase) ||
                            Name.Contains(TEXT("Speed"), ESearchCase::IgnoreCase) ||
                            Name.Contains(TEXT("Strength"), ESearchCase::IgnoreCase)
                        )
                    )
                    {
                        bHasWindScalarParam = true;
                        break;
                    }
                }

                if (bHasWindVectorParam)
                {
                    Score += 20;
                }

                if (bHasWindScalarParam)
                {
                    Score += 20;
                }

                if (Score > BestScore)
                {
                    BestScore = Score;
                    BestCandidate = Candidate;
                }
            }

            if (BestScore > 0)
            {
                CachedDynamicWindCollection = BestCandidate;
            }
        }
    }

    FVector WindDirection =
        CachedUltraDynamicWeatherActor->GetActorForwardVector();

    const FName DirectionCandidates[] =
    {
        TEXT("WindDirection"),
        TEXT("Wind_Direction"),
        TEXT("GlobalWindDirection"),
        TEXT("WindDir")
    };

    for (const FName PropertyName : DirectionCandidates)
    {
        FVector CandidateDirection = FVector::ZeroVector;

        if (
            FCubusVegetationWindUtilities::TryReadVectorLikeProperty(
                CachedUltraDynamicWeatherActor,
                PropertyName,
                CandidateDirection
            )
        )
        {
            WindDirection = CandidateDirection;
            break;
        }
    }

    // Some UDW setups store wind direction as a yaw angle in degrees.
    float DirectionDegrees = 0.0f;

    if (
        FCubusVegetationWindUtilities::TryReadFloatProperty(
            CachedUltraDynamicWeatherActor,
            TEXT("WindDirection"),
            DirectionDegrees
        )
    )
    {
        WindDirection =
            FRotator(0.0f, DirectionDegrees, 0.0f).Vector();
    }

    float WindIntensity = 0.0f;
    bool bFoundWindIntensity = false;

    const FName IntensityCandidates[] =
    {
        TEXT("WindIntensity"),
        TEXT("Wind_Intensity"),
        TEXT("ManualWindIntensity"),
        TEXT("WindSpeed"),
        TEXT("GlobalWindSpeed")
    };

    for (const FName PropertyName : IntensityCandidates)
    {
        float CandidateIntensity = 0.0f;

        if (
            FCubusVegetationWindUtilities::TryReadFloatProperty(
                CachedUltraDynamicWeatherActor,
                PropertyName,
                CandidateIntensity
            )
        )
        {
            WindIntensity = CandidateIntensity;
            bFoundWindIntensity = true;
            break;
        }
    }

    if (!bFoundWindIntensity)
    {
        for (TFieldIterator<FProperty> FieldIt(CachedUltraDynamicWeatherActor->GetClass()); FieldIt; ++FieldIt)
        {
            const FProperty* Property = *FieldIt;

            if (Property == nullptr)
            {
                continue;
            }

            const FString PropertyName = Property->GetName();

            const bool bLooksLikeWindStrength =
                PropertyName.Contains(TEXT("Wind"), ESearchCase::IgnoreCase) &&
                (
                    PropertyName.Contains(TEXT("Intensity"), ESearchCase::IgnoreCase) ||
                    PropertyName.Contains(TEXT("Speed"), ESearchCase::IgnoreCase) ||
                    PropertyName.Contains(TEXT("Strength"), ESearchCase::IgnoreCase)
                );

            if (!bLooksLikeWindStrength)
            {
                continue;
            }

            float CandidateIntensity = 0.0f;

            if (FCubusVegetationWindUtilities::TryReadFloatProperty(CachedUltraDynamicWeatherActor, Property->GetFName(), CandidateIntensity))
            {
                WindIntensity = CandidateIntensity;
                bFoundWindIntensity = true;
                break;
            }
        }
    }

    if (!bFoundWindIntensity)
    {
        // Failsafe: avoid dead-still foliage when property names differ.
        WindIntensity = 1.0f;
    }

    WindIntensity = FMath::Max(0.0f, WindIntensity);

    if (!WindDirection.Normalize())
    {
        WindDirection = FVector(1.0f, 0.0f, 0.0f);
    }

    if (
        WindDirection.Equals(LastBridgedWindDirection, 0.001f) &&
        FMath::IsNearlyEqual(
            WindIntensity,
            LastBridgedWindIntensity,
            0.001f
        )
    )
    {
        return;
    }

    const FLinearColor DirectionColor(
        WindDirection.X,
        WindDirection.Y,
        WindDirection.Z,
        1.0f
    );

    TArray<UMaterialParameterCollection*> TargetCollections;

    if (IsValid(CachedDynamicWindCollection))
    {
        TargetCollections.Add(CachedDynamicWindCollection);
    }

    for (TObjectIterator<UMaterialParameterCollection> It; It; ++It)
    {
        UMaterialParameterCollection* Candidate = *It;

        if (!IsValid(Candidate) || TargetCollections.Contains(Candidate))
        {
            continue;
        }

        const FString CandidateName = Candidate->GetName();

        bool bHasWindVectorParam = false;
        for (const FCollectionVectorParameter& Param : Candidate->VectorParameters)
        {
            const FString Name = Param.ParameterName.ToString();
            if (
                Name.Contains(TEXT("Wind"), ESearchCase::IgnoreCase) &&
                (
                    Name.Contains(TEXT("Direction"), ESearchCase::IgnoreCase) ||
                    Name.Contains(TEXT("Dir"), ESearchCase::IgnoreCase)
                )
            )
            {
                bHasWindVectorParam = true;
                break;
            }
        }

        bool bHasWindScalarParam = false;
        for (const FCollectionScalarParameter& Param : Candidate->ScalarParameters)
        {
            const FString Name = Param.ParameterName.ToString();
            if (
                Name.Contains(TEXT("Wind"), ESearchCase::IgnoreCase) &&
                (
                    Name.Contains(TEXT("Intensity"), ESearchCase::IgnoreCase) ||
                    Name.Contains(TEXT("Speed"), ESearchCase::IgnoreCase) ||
                    Name.Contains(TEXT("Strength"), ESearchCase::IgnoreCase)
                )
            )
            {
                bHasWindScalarParam = true;
                break;
            }
        }

        const bool bLooksLikeFoliageWindCollection =
            CandidateName.Contains(TEXT("Foliage"), ESearchCase::IgnoreCase) ||
            CandidateName.Contains(TEXT("Megascans"), ESearchCase::IgnoreCase) ||
            CandidateName.Contains(TEXT("MS_"), ESearchCase::IgnoreCase) ||
            CandidateName.Contains(TEXT("Pivot"), ESearchCase::IgnoreCase) ||
            CandidateName.Contains(TEXT("Wind"), ESearchCase::IgnoreCase);

        if ((bHasWindVectorParam || bHasWindScalarParam) && bLooksLikeFoliageWindCollection)
        {
            TargetCollections.Add(Candidate);
        }
    }

    int32 UpdatedCollectionCount = 0;

    for (UMaterialParameterCollection* Collection : TargetCollections)
    {
        if (!IsValid(Collection))
        {
            continue;
        }

        UMaterialParameterCollectionInstance* Instance =
            World->GetParameterCollectionInstance(Collection);

        if (!IsValid(Instance))
        {
            continue;
        }

        Instance->SetVectorParameterValue(
            TEXT("WindDirection"),
            DirectionColor
        );
        Instance->SetVectorParameterValue(
            TEXT("Wind Direction"),
            DirectionColor
        );
        Instance->SetVectorParameterValue(
            TEXT("Wind_Direction"),
            DirectionColor
        );
        Instance->SetVectorParameterValue(
            TEXT("DynamicWindDirection"),
            DirectionColor
        );
        Instance->SetVectorParameterValue(
            TEXT("GlobalWindDirection"),
            DirectionColor
        );
        Instance->SetVectorParameterValue(
            TEXT("UDW_WindDirection"),
            DirectionColor
        );

        Instance->SetScalarParameterValue(
            TEXT("WindIntensity"),
            WindIntensity
        );
        Instance->SetScalarParameterValue(
            TEXT("Wind Intensity"),
            WindIntensity
        );
        Instance->SetScalarParameterValue(
            TEXT("Wind_Intensity"),
            WindIntensity
        );
        Instance->SetScalarParameterValue(
            TEXT("DynamicWindIntensity"),
            WindIntensity
        );
        Instance->SetScalarParameterValue(
            TEXT("WindSpeed"),
            WindIntensity
        );
        Instance->SetScalarParameterValue(
            TEXT("GlobalWindSpeed"),
            WindIntensity
        );
        Instance->SetScalarParameterValue(
            TEXT("GlobalWindIntensity"),
            WindIntensity
        );
        Instance->SetScalarParameterValue(
            TEXT("UDW_WindIntensity"),
            WindIntensity
        );

        for (const FCollectionVectorParameter& VectorParameter
             : Collection->VectorParameters)
        {
            const FString ParameterName =
                VectorParameter.ParameterName.ToString();

            const bool bLooksLikeWindDirection =
                ParameterName.Contains(TEXT("Wind"), ESearchCase::IgnoreCase) &&
                (
                    ParameterName.Contains(TEXT("Direction"), ESearchCase::IgnoreCase) ||
                    ParameterName.Contains(TEXT("Dir"), ESearchCase::IgnoreCase)
                );

            if (bLooksLikeWindDirection)
            {
                Instance->SetVectorParameterValue(
                    VectorParameter.ParameterName,
                    DirectionColor
                );
            }
        }

        for (const FCollectionScalarParameter& ScalarParameter
             : Collection->ScalarParameters)
        {
            const FString ParameterName =
                ScalarParameter.ParameterName.ToString();

            const bool bLooksLikeWindScalar =
                ParameterName.Contains(TEXT("Wind"), ESearchCase::IgnoreCase) &&
                (
                    ParameterName.Contains(TEXT("Intensity"), ESearchCase::IgnoreCase) ||
                    ParameterName.Contains(TEXT("Speed"), ESearchCase::IgnoreCase) ||
                    ParameterName.Contains(TEXT("Strength"), ESearchCase::IgnoreCase)
                );

            if (bLooksLikeWindScalar)
            {
                Instance->SetScalarParameterValue(
                    ScalarParameter.ParameterName,
                    WindIntensity
                );
            }
        }

        ++UpdatedCollectionCount;
    }

    const float SafeUdwWindMax = FMath::Max(0.01f, UdwWindIntensityMax);
    const float NormalizedUdwWind = FMath::Clamp(
        WindIntensity / SafeUdwWindMax,
        0.0f,
        1.0f
    );
    const float GlobalFoliageWindSpeed =
        FMath::Max(0.0f, GlobalFoliageWindSpeedMax) *
        FMath::Pow(
            NormalizedUdwWind,
            FMath::Max(0.1f, GlobalFoliageWindResponseExponent)
        );
    const FVector GlobalFoliageWindDirection =
        WindDirection.RotateAngleAxis(
            FMath::Clamp(
                GlobalFoliageWindDirectionYawOffset,
                -180.0f,
                180.0f
            ),
            FVector::UpVector
        ).GetSafeNormal();

    const auto ApplyGlobalFoliageDirectionFlip =
        [this](UObject* Target) -> int32
    {
        int32 UpdatedCount = 0;

        static const FName FlipPropertyCandidates[] =
        {
            TEXT("FlipWindDirection"),
            TEXT("Flip Wind Direction"),
            TEXT("Flip_Wind_Direction"),
            TEXT("bFlipWindDirection")
        };

        for (const FName PropertyName : FlipPropertyCandidates)
        {
            if (
                FCubusVegetationWindUtilities::TryWriteBoolProperty(
                    Target,
                    PropertyName,
                    bGlobalFoliageFlipWindDirection
                )
            )
            {
                ++UpdatedCount;
            }
        }

        return UpdatedCount;
    };

    int32 UpdatedGlobalFoliagePropertyCount = 0;
    int32 InvokedGlobalFoliageWindFunctionCount = 0;
    int32 UpdatedSpawnedSkeletalPropertyCount = 0;
    int32 BoundSpawnedTransformProviderCount = 0;
    UObject* SharedWindTransformProvider = nullptr;

    if (bBridgeUdwToGlobalFoliageActor)
    {
        if (!IsValid(CachedGlobalFoliageActor))
        {
            CachedGlobalFoliageActor = FCubusVegetationWindUtilities::ResolveGlobalFoliageActor(World);
        }

        if (IsValid(CachedGlobalFoliageActor))
        {
            UpdatedGlobalFoliagePropertyCount +=
                ApplyGlobalFoliageDirectionFlip(CachedGlobalFoliageActor);

            FCubusVegetationWindUtilities::AssignLikelyWindProviderActor(
                CachedGlobalFoliageActor,
                CachedUltraDynamicWeatherActor
            );

            UpdatedGlobalFoliagePropertyCount +=
                FCubusVegetationWindUtilities::ApplyWindToObject(
                    CachedGlobalFoliageActor,
                    GlobalFoliageWindDirection,
                    GlobalFoliageWindSpeed
                );

            TInlineComponentArray<UActorComponent*> Components(CachedGlobalFoliageActor);
            for (UActorComponent* ComponentObject : Components)
            {
                UpdatedGlobalFoliagePropertyCount +=
                    ApplyGlobalFoliageDirectionFlip(ComponentObject);

                FCubusVegetationWindUtilities::AssignLikelyWindProviderActor(
                    ComponentObject,
                    CachedUltraDynamicWeatherActor
                );

                UpdatedGlobalFoliagePropertyCount +=
                    FCubusVegetationWindUtilities::ApplyWindToObject(
                        ComponentObject,
                        GlobalFoliageWindDirection,
                        GlobalFoliageWindSpeed
                    );
            }

            InvokedGlobalFoliageWindFunctionCount +=
                FCubusVegetationWindUtilities::InvokeLikelyWindRefreshFunctions(CachedGlobalFoliageActor);

            for (UActorComponent* ComponentObject : Components)
            {
                InvokedGlobalFoliageWindFunctionCount +=
                    FCubusVegetationWindUtilities::InvokeLikelyWindRefreshFunctions(ComponentObject);
            }

            SharedWindTransformProvider =
                FCubusVegetationWindUtilities::ResolveWindTransformProviderFromActor(CachedGlobalFoliageActor);
        }
    }

    for (USkeletalMeshComponent* HeroComponent : HeroSkeletalWindComponents)
    {
        if (!IsValid(HeroComponent))
        {
            continue;
        }

        UpdatedSpawnedSkeletalPropertyCount +=
            FCubusVegetationWindUtilities::ApplyWindToObject(
                HeroComponent,
                WindDirection,
                WindIntensity
            );
    }

    for (const TPair<int64, TObjectPtr<UInstancedSkinnedMeshComponent>>& Pair
         : CatalogSkeletalBatchComponents)
    {
        UInstancedSkinnedMeshComponent* SkinnedBatch = Pair.Value;

        if (!IsValid(SkinnedBatch))
        {
            continue;
        }

        UpdatedSpawnedSkeletalPropertyCount +=
            FCubusVegetationWindUtilities::ApplyWindToObject(
                SkinnedBatch,
                WindDirection,
                WindIntensity
            );

        if (
            IsValid(SharedWindTransformProvider) &&
            Cast<UDynamicWindData>(SkinnedBatch->GetTransformProvider()) == nullptr
        )
        {
            UObject* ExistingProvider = SkinnedBatch->GetTransformProvider();
            UTransformProviderData* TargetProvider =
                Cast<UTransformProviderData>(SharedWindTransformProvider);

            if (IsValid(TargetProvider) && ExistingProvider != TargetProvider)
            {
                SkinnedBatch->SetTransformProvider(TargetProvider);
                SkinnedBatch->MarkRenderStateDirty();
                SkinnedBatch->MarkRenderDynamicDataDirty();
                ++BoundSpawnedTransformProviderCount;
            }
        }
    }

    if (
        UpdatedCollectionCount <= 0 &&
        UpdatedGlobalFoliagePropertyCount <= 0 &&
        UpdatedSpawnedSkeletalPropertyCount <= 0
    )
    {
        return;
    }

    static double LastBridgeDebugLogTime = 0.0;
    const double Now = FPlatformTime::Seconds();

    if (Now - LastBridgeDebugLogTime >= 2.0)
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Cubus wind bridge: collections=%d primary=%s intensity=%.3f direction=(%.2f, %.2f, %.2f)"),
            UpdatedCollectionCount,
            IsValid(CachedDynamicWindCollection)
                ? *CachedDynamicWindCollection->GetName()
                : TEXT("None"),
            WindIntensity,
            WindDirection.X,
            WindDirection.Y,
            WindDirection.Z
        );

        if (bBridgeUdwToGlobalFoliageActor)
        {
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Cubus global foliage wind sync: actor=%s updatedProps=%d uds=%.3f mappedSpeed=%.3f exponent=%.2f udsYaw=%.1f foliageYaw=%.1f flip=%s"),
                IsValid(CachedGlobalFoliageActor)
                    ? *CachedGlobalFoliageActor->GetName()
                    : TEXT("None"),
                UpdatedGlobalFoliagePropertyCount,
                WindIntensity,
                GlobalFoliageWindSpeed,
                GlobalFoliageWindResponseExponent,
                FRotator::NormalizeAxis(WindDirection.Rotation().Yaw),
                FRotator::NormalizeAxis(GlobalFoliageWindDirection.Rotation().Yaw),
                bGlobalFoliageFlipWindDirection ? TEXT("yes") : TEXT("no")
            );

            UE_LOG(
                LogTemp,
                Display,
                TEXT("Cubus global foliage wind refresh: invokedFunctions=%d"),
                InvokedGlobalFoliageWindFunctionCount
            );
        }

        UE_LOG(
            LogTemp,
            Display,
            TEXT("Cubus spawned skeletal wind sync: updatedProps=%d"),
            UpdatedSpawnedSkeletalPropertyCount
        );

        if (BoundSpawnedTransformProviderCount > 0)
        {
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Cubus spawned skeletal transform provider sync: bound=%d provider=%s"),
                BoundSpawnedTransformProviderCount,
                IsValid(SharedWindTransformProvider)
                    ? *SharedWindTransformProvider->GetName()
                    : TEXT("None")
            );
        }

        LastBridgeDebugLogTime = Now;
    }

    LastBridgedWindDirection = WindDirection;
    LastBridgedWindIntensity = WindIntensity;
}

bool ACubusWorldVegetationActor::FindInteractiveTreeAlongRay(
    const FVector& TraceStart,
    const FVector& TraceEnd,
    const float SelectionRadius,
    FIntVector& OutWorldVoxel
)
{
    ResolveBlockWorld();
    OutWorldVoxel = FIntVector::ZeroValue;

    const FVector Segment =
        TraceEnd - TraceStart;

    const double SegmentLengthSquared =
        Segment.SizeSquared();

    if (
        !IsValid(BlockWorld) ||
        SegmentLengthSquared <=
            static_cast<double>(SMALL_NUMBER)
    )
    {
        return false;
    }

    const FCubusVegetationRandomizationSettings RandomizationSettings
    {
        bEnableRuntimeRandomization,
        RuntimeRandomizationSeed,
        RandomPruneProbability,
        RandomScaleJitterMin,
        RandomScaleJitterMax,
        RandomPositionJitterVoxelFraction,
        RandomYawJitterDegrees
    };

    const double SafeRadiusSquared =
        FMath::Square(FMath::Max(1.0f, SelectionRadius));
    double BestAlongSegment = TNumericLimits<double>::Max();
    bool bFound = false;

    const auto& RegisteredChunks =
        BlockWorld->GetRegisteredChunks();

    for (const auto& Pair : RegisteredChunks)
    {
        const ACubusVoxelVolumeActor* Chunk =
            Pair.Value.Get();

        if (
            !IsValid(Chunk) ||
            Chunk->GetChunkData() == nullptr
        )
        {
            continue;
        }

        const float SafeVoxelSize = FMath::Max(1.0f, Chunk->GetVoxelSize());
        const double ChunkHalfWorldExtent =
            static_cast<double>(Cubus::ChunkSize) * SafeVoxelSize * 0.5;

        for (const FCubusVegetationInstance& Instance
             : Chunk->GetChunkData()->GetVegetationInstances())
        {
            if (
                Instance.TypeId != WorldBroadleafType &&
                Instance.TypeId != WorldConiferType
            )
            {
                continue;
            }

            const FVector BaseWorldLocation(
                (static_cast<double>(Instance.WorldVoxel.X) + 0.5) * SafeVoxelSize -
                    ChunkHalfWorldExtent,
                (static_cast<double>(Instance.WorldVoxel.Y) + 0.5) * SafeVoxelSize -
                    ChunkHalfWorldExtent,
                static_cast<double>(Instance.WorldVoxel.Z) * SafeVoxelSize -
                    ChunkHalfWorldExtent
            );

            const float TypeScaleMultiplier = ResolveTypeScaleMultiplier(
                Instance.TypeId,
                bEnablePerTypeScaleOverrides,
                BroadleafScaleMultiplier,
                ConiferScaleMultiplier,
                ShrubScaleMultiplier,
                GrassScaleMultiplier,
                ReedsScaleMultiplier,
                AlpineScaleMultiplier
            );

            const float CombinedScale = FMath::Max(
                0.01f,
                Instance.Scale *
                    FMath::Max(0.01f, GlobalPlantScaleMultiplier) *
                    FMath::Max(0.01f, TypeScaleMultiplier)
            );

            const FCubusResolvedVegetationPlacement Resolved =
                VegetationPlacement.Resolve(
                    Instance,
                    BaseWorldLocation,
                    SafeVoxelSize,
                    CombinedScale,
                    RandomizationSettings
                );

            if (Resolved.bPruned)
            {
                continue;
            }

            // Aim at the lower trunk rather than the ground placement point.
            const FVector Target =
                Resolved.Location +
                FVector::UpVector * SafeVoxelSize * 3.0f * Resolved.Scale;

            const double Along = FVector::DotProduct(
                Target - TraceStart,
                Segment
            ) / SegmentLengthSquared;

            if (Along < 0.0 || Along > 1.0)
            {
                continue;
            }

            const FVector ClosestPoint = TraceStart + Segment * Along;
            if (FVector::DistSquared(Target, ClosestPoint) > SafeRadiusSquared)
            {
                continue;
            }

            if (Along < BestAlongSegment)
            {
                BestAlongSegment = Along;
                OutWorldVoxel = Instance.WorldVoxel;
                bFound = true;
            }
        }
    }

    return bFound;
}

bool ACubusWorldVegetationActor::EnsureFarVegetationProxyAssets(
    const bool bSaveGeneratedAssets
)
{
#if WITH_EDITOR
    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return false;
    }

    const FString SafeRoot = FarProxyPackageRoot.IsEmpty()
        ? TEXT("/Game/OrakaiGenerated/Vegetation/Far")
        : FarProxyPackageRoot;

    TArray<UPackage*> PackagesToSave;
    int32 BakedProxyCount = 0;
    int32 ReusedProxyCount = 0;
    int32 FailedProxyCount = 0;
    bool bCatalogChanged = false;

    for (int32 SpeciesIndex = 0; SpeciesIndex < SpeciesCatalog.Num(); ++SpeciesIndex)
    {
        FCubusVegetationSpeciesCatalogEntry& Entry = SpeciesCatalog[SpeciesIndex];
        const int32 StageCount = Entry.GrowthStageMeshes.Num();

        if (StageCount <= 0)
        {
            continue;
        }

        if (Entry.StaticGrowthStageAssets.Num() != StageCount)
        {
            Entry.StaticGrowthStageAssets.SetNum(StageCount);
            bCatalogChanged = true;
        }

        FString SpeciesToken = Entry.SpeciesId.IsNone()
            ? FString::Printf(TEXT("Species_%d"), SpeciesIndex)
            : Entry.SpeciesId.ToString();

        SpeciesToken.ReplaceInline(TEXT(" "), TEXT("_"));
        SpeciesToken.ReplaceInline(TEXT("/"), TEXT("_"));
        SpeciesToken.ReplaceInline(TEXT("\\"), TEXT("_"));
        SpeciesToken.ReplaceInline(TEXT("."), TEXT("_"));

        for (int32 StageIndex = 0; StageIndex < StageCount; ++StageIndex)
        {
            const TSoftObjectPtr<UObject>& SourceReference =
                Entry.GrowthStageMeshes[StageIndex];

            if (SourceReference.IsNull())
            {
                continue;
            }

            UObject* SourceAsset = SourceReference.LoadSynchronous();

            if (!IsValid(SourceAsset))
            {
                ++FailedProxyCount;
                continue;
            }

            if (UStaticMesh* SourceStaticMesh = Cast<UStaticMesh>(SourceAsset))
            {
                const TSoftObjectPtr<UObject> StaticReference(SourceStaticMesh);
                if (
                    Entry.StaticGrowthStageAssets[StageIndex].ToSoftObjectPath() !=
                    StaticReference.ToSoftObjectPath()
                )
                {
                    Entry.StaticGrowthStageAssets[StageIndex] = StaticReference;
                    bCatalogChanged = true;
                }
                continue;
            }

            USkeletalMesh* SourceSkeletalMesh = Cast<USkeletalMesh>(SourceAsset);

            if (!IsValid(SourceSkeletalMesh))
            {
                ++FailedProxyCount;
                continue;
            }

            const FString AssetName = FString::Printf(
                TEXT("%s_Stage_%d_FarProxy"),
                *SpeciesToken,
                StageIndex
            );
            const FString PackageName = SafeRoot / AssetName;
            const FString ObjectPath = PackageName + TEXT(".") + AssetName;

            UStaticMesh* GeneratedMesh = LoadObject<UStaticMesh>(nullptr, *ObjectPath);

            if (!IsValid(GeneratedMesh))
            {
                // Epic's dedicated editor utility converts the skeletal mesh
                // asset directly into a static mesh. Unlike the generic component
                // merge path, this is explicitly designed for USkeletalMesh input.
                GeneratedMesh =
                    UAnimToTextureBPLibrary::ConvertSkeletalMeshToStaticMesh(
                        SourceSkeletalMesh,
                        PackageName,
                        0
                    );

                if (IsValid(GeneratedMesh))
                {
                    GeneratedMesh->MarkPackageDirty();
                    PackagesToSave.AddUnique(GeneratedMesh->GetPackage());
                    ++BakedProxyCount;

                    UE_LOG(
                        LogTemp,
                        Display,
                        TEXT(
                            "Cubus far proxy baked: species=%s stage=%d source=%s proxy=%s"
                        ),
                        *SpeciesToken,
                        StageIndex,
                        *SourceSkeletalMesh->GetName(),
                        *GeneratedMesh->GetName()
                    );
                }
            }
            else
            {
                ++ReusedProxyCount;
            }

            if (!IsValid(GeneratedMesh))
            {
                ++FailedProxyCount;
                UE_LOG(
                    LogTemp,
                    Error,
                    TEXT("Cubus far proxy bake failed: species=%s stage=%d source=%s"),
                    *SpeciesToken,
                    StageIndex,
                    *SourceSkeletalMesh->GetName()
                );
                continue;
            }

            const TSoftObjectPtr<UObject> GeneratedReference(GeneratedMesh);
            if (
                Entry.StaticGrowthStageAssets[StageIndex].ToSoftObjectPath() !=
                GeneratedReference.ToSoftObjectPath()
            )
            {
                Entry.StaticGrowthStageAssets[StageIndex] = GeneratedReference;
                bCatalogChanged = true;
            }
        }
    }

    if (bCatalogChanged)
    {
        Modify();
        MarkPackageDirty();
    }

    if (bSaveGeneratedAssets && !PackagesToSave.IsEmpty())
    {
        UEditorLoadingAndSavingUtils::SavePackages(PackagesToSave, true);
    }

    if (BakedProxyCount > 0 || ReusedProxyCount > 0 || FailedProxyCount > 0)
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Cubus far proxy assets: baked=%d reused=%d failed=%d root=%s"),
            BakedProxyCount,
            ReusedProxyCount,
            FailedProxyCount,
            *SafeRoot
        );
    }

    return bCatalogChanged || BakedProxyCount > 0;
#else
    return false;
#endif
}

void ACubusWorldVegetationActor::BakeFarVegetationProxies()
{
#if WITH_EDITOR
    const bool bChanged = EnsureFarVegetationProxyAssets(true);
    if (bChanged)
    {
        RefreshVegetationBatches();
        RefreshFarVegetationBatches();
    }
#endif
}

void
ACubusWorldVegetationActor::RefreshFarVegetationBatches()
{
    for (
        const TPair<
            int64,
            TObjectPtr<
                UHierarchicalInstancedStaticMeshComponent
            >
        >& Pair
        : FarCatalogStaticBatchComponents
    )
    {
        if (IsValid(Pair.Value))
        {
            Pair.Value->ClearInstances();
            Pair.Value->DestroyComponent();
        }
    }

    FarCatalogStaticBatchComponents.Reset();

    if (!bEnableFarVegetation)
    {
        return;
    }

    for (
        const TPair<
            int64,
            TObjectPtr<
                UHierarchicalInstancedStaticMeshComponent
            >
        >& Pair
        : CatalogStaticBatchComponents
    )
    {
        UHierarchicalInstancedStaticMeshComponent*
            SourceComponent =
                Pair.Value;

        if (
            !IsValid(SourceComponent) ||
            !IsValid(
                SourceComponent->GetStaticMesh()
            )
        )
        {
            continue;
        }

        const FName ComponentName(
            *FString::Printf(
                TEXT("CubusFarVegetation_%llu"),
                static_cast<uint64>(
                    Pair.Key
                )
            )
        );

        UHierarchicalInstancedStaticMeshComponent*
            FarComponent =
                VegetationRenderer.CreateStaticBatch(
                    this,
                    Root,
                    ComponentName,
                    bCastFarVegetationShadows,
                    0,
                    0
                );

        if (!IsValid(FarComponent))
        {
            continue;
        }

        FarComponent->SetStaticMesh(
            SourceComponent->GetStaticMesh()
        );

        FarComponent->SetCastShadow(
            bCastFarVegetationShadows
        );

        // Far vegetation is the horizon representation. Do not apply a
        // per-instance distance cull here; streaming decides which cells exist.
        // Zero/zero disables ISM/HISM distance culling and avoids materials that
        // consume PerInstanceFadeAmount fading the trees out before the streamer
        // evicts their cells.
        FarComponent->SetCullDistances(0, 0);

        // This component is the explicit long-range vegetation representation.
        // Do not allow a Cull Distance Volume or inherited max draw distance to
        // silently cull the whole far forest before our per-instance HISM range.
        FarComponent->bAllowCullDistanceVolume = false;
        FarComponent->SetCullDistance(0.0f);
        FarComponent->SetVisibility(true, true);
        FarComponent->SetHiddenInGame(false, true);

        const int32 MaterialCount =
            SourceComponent->GetNumMaterials();

        for (
            int32 MaterialIndex = 0;
            MaterialIndex < MaterialCount;
            ++MaterialIndex
        )
        {
            UMaterialInterface* Material =
                SourceComponent->GetMaterial(
                    MaterialIndex
                );

            if (IsValid(Material))
            {
                FarComponent->SetMaterial(
                    MaterialIndex,
                    Material
                );
            }
        }

        FarCatalogStaticBatchComponents.Add(
            Pair.Key,
            FarComponent
        );
    }

    bFarVegetationRenderDirty = true;

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "Cubus far vegetation batches: "
            "sourceStatic=%d farStatic=%d enabled=%s"
        ),
        CatalogStaticBatchComponents.Num(),
        FarCatalogStaticBatchComponents.Num(),
        bEnableFarVegetation
            ? TEXT("true")
            : TEXT("false")
    );
}

void ACubusWorldVegetationActor::RebuildWorldVegetation()
{
    ResolveBlockWorld();

    const FCubusVegetationRandomizationSettings
    RandomizationSettings
    {
        bEnableRuntimeRandomization,
        RuntimeRandomizationSeed,
        RandomPruneProbability,
        RandomScaleJitterMin,
        RandomScaleJitterMax,
        RandomPositionJitterVoxelFraction,
        RandomYawJitterDegrees
    };

    if (
        !GetWorld() ||
        !GetWorld()->IsGameWorld() ||
        (
            CatalogGrassBatchComponents.IsEmpty() &&
            CatalogStaticBatchComponents.IsEmpty() &&
            CatalogSkeletalBatchComponents.IsEmpty()
        )
    )
    {
        RefreshVegetationBatches();
        RefreshFarVegetationBatches();
    }

    if (!IsValid(BlockWorld))
    {
        return;
    }

    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return;
    }

    const int32 PlantLimit = MaximumRenderedPlants > 0
        ? MaximumRenderedPlants
        : MAX_int32;

    const uint32 CurrentVegetationSettingsHash =
        CalculateVegetationSettingsHash();

    const APlayerController* PlayerController =
        UGameplayStatics::GetPlayerController(this, 0);

    const bool bHasCamera =
        IsValid(PlayerController) &&
        IsValid(PlayerController->PlayerCameraManager);

    const FVector CameraLocation = bHasCamera
        ? PlayerController->PlayerCameraManager->GetCameraLocation()
        : FVector::ZeroVector;

    const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
    const bool bHasStreamingOrigin = IsValid(PlayerPawn);
    const FVector StreamingLocation = bHasStreamingOrigin
        ? PlayerPawn->GetActorLocation()
        : FVector::ZeroVector;

    const bool bUseCameraChunkCulling =
        bCullByCameraChunkRadius &&
        bHasStreamingOrigin;

    TMap<FIntVector, uint32> CurrentChunkVegetationSignatures;

    const auto& RegisteredChunks =
        BlockWorld->GetRegisteredChunks();

    ApplyVegetationDistancePolicy(
        FMath::Max(
            1.0f,
            BlockWorld->GetGeneratedVoxelSize()
        )
    );

    for (const auto& Pair : RegisteredChunks)
    {
        const ACubusVoxelVolumeActor* Chunk =
            Pair.Value.Get();

        if (
            !IsValid(Chunk) ||
            !FCubusVegetationChunkFilter::IsWithinCameraRadius(
                Chunk,
                StreamingLocation,
                bUseCameraChunkCulling,
                CameraChunkHorizontalRadius,
                CameraChunkVerticalRadius
            )
        )
        {
            continue;
        }

        const FCubusBlockChunkData* ChunkData =
            Chunk->GetChunkData();

        if (ChunkData == nullptr)
        {
            continue;
        }

        CurrentChunkVegetationSignatures.Add(
            Pair.Key,
            CalculateChunkVegetationSignature(
                *ChunkData
            )
        );
    }

    bool bAppendOnly =
        !PublishedChunkVegetationSignatures.IsEmpty() &&
        PublishedVegetationSettingsHash == CurrentVegetationSettingsHash &&
        CurrentChunkVegetationSignatures.Num() >=
            PublishedChunkVegetationSignatures.Num();

    if (bAppendOnly)
    {
        for (const TPair<FIntVector, uint32>& PublishedPair
             : PublishedChunkVegetationSignatures)
        {
            const uint32* CurrentSignature =
                CurrentChunkVegetationSignatures.Find(PublishedPair.Key);

            if (
                CurrentSignature == nullptr ||
                *CurrentSignature != PublishedPair.Value
            )
            {
                bAppendOnly = false;
                break;
            }
        }
    }

    /*
    * REPLACEMENT STARTS HERE.
    */
    if (bEnableHeroSkeletalWindMode)
    {
        bAppendOnly = false;
    }

    bool bCameraMovedEnoughForRecenter =
        false;

    if (
        bHasStreamingOrigin &&
        bHasLastFullVegetationBuildCameraLocation
    )
    {
        const double RecenterDistance =
            static_cast<double>(
                FMath::Max(
                    100.0f,
                    VegetationRecenterDistance
                )
            );

        bCameraMovedEnoughForRecenter =
            FVector::DistSquared(
                StreamingLocation,
                LastFullVegetationBuildCameraLocation
            ) >=
            RecenterDistance *
            RecenterDistance;
    }

    /*
    * A saturated population remains untouched while the camera stays in the
    * same local vegetation area.
    *
    * New chunk signatures are still published here so their arrival does not
    * repeatedly trigger a full vegetation rebuild.
    */
    if (
        bAppendOnly &&
        bPublishedVegetationBudgetSaturated &&
        !bCameraMovedEnoughForRecenter
    )
    {
        LoadedChunkCount =
            CurrentChunkVegetationSignatures.Num();

        PublishedPlacementHash =
            static_cast<int64>(
                CalculateVegetationSignatureMapHash(
                    CurrentChunkVegetationSignatures
                )
            );

        PublishedVegetationSettingsHash =
            CurrentVegetationSettingsHash;

        PublishedChunkVegetationSignatures =
            MoveTemp(
                CurrentChunkVegetationSignatures
            );

        return;
    }

    /*
    * The camera has moved far enough that the fixed vegetation population
    * needs to follow it.
    *
    * Force the normal full nearest-first rebuild.
    */
    if (
        bAppendOnly &&
        bPublishedVegetationBudgetSaturated &&
        bCameraMovedEnoughForRecenter
    )
    {
        bAppendOnly = false;
    }

    const bool bFullVegetationRebuild =
        !bAppendOnly;

    if (!bAppendOnly)
    {
        ClearWorldVegetation();
    }
    /*
    * REPLACEMENT ENDS HERE.
    */

    TArray<FCubusVegetationRepresentationCandidate>
        HeroTreeCandidates;

    TMap<int64, TArray<FTransform>>
        CatalogTransformsByBatchKey;

    TMap<int64, TArray<FTransform>>
        HeroTransformsByBatchKey;

    int32 StaticBatchTransformCount = 0;
    int32 SkeletalBatchTransformCount = 0;

    if (bAppendOnly)
    {
        for (const TPair<int64, TObjectPtr<UInstancedStaticMeshComponent>>& Pair
            : CatalogGrassBatchComponents)
        {
            if (IsValid(Pair.Value))
            {
                StaticBatchTransformCount +=
                    Pair.Value->GetInstanceCount();
            }
        }
        for (const TPair<int64, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>>& Pair
             : CatalogStaticBatchComponents)
        {
            if (IsValid(Pair.Value))
            {
                StaticBatchTransformCount += Pair.Value->GetInstanceCount();
            }
        }

        for (const TPair<int64, TObjectPtr<UInstancedSkinnedMeshComponent>>& Pair
             : CatalogSkeletalBatchComponents)
        {
            if (IsValid(Pair.Value))
            {
                SkeletalBatchTransformCount += Pair.Value->GetInstanceCount();
            }
        }
    }

    /*
    * MaximumRenderedPlants is enforced against actual rendered instances,
    * not source vegetation placements.
    *
    * This matters especially for grass because one logical placement can expand
    * into GrassInstancesPerPlacement rendered ISM instances.
    */
    int32 RenderedInstanceBudgetCount =
        StaticBatchTransformCount +
        SkeletalBatchTransformCount;

    int32 InstancedSkeletalFallbackCount = 0;
    int32 FoliageMaterialOverrideComponentCount = 0;
    int32 BoundSpeciesTransformProviderCount = 0;
    int32 RejectedTreeSurfaceCount = 0;

    int32 RandomizedPlantCount = 0;
    int32 RandomPrunedPlantCount = 0;
    float ObservedRandomScaleMin = MAX_flt;
    float ObservedRandomScaleMax = 0.0f;

    TArray<FIntVector> VegetationChunkCoordinates;

    VegetationChunkCoordinates.Reserve(
        RegisteredChunks.Num()
    );

    for (const auto& Pair : RegisteredChunks)
    {
        ACubusVoxelVolumeActor* Chunk =
            Pair.Value.Get();

        if (
            !IsValid(Chunk) ||
            Chunk->GetChunkData() == nullptr
        )
        {
            continue;
        }

        if (
            !FCubusVegetationChunkFilter::IsWithinCameraRadius(
                Chunk,
                StreamingLocation,
                bUseCameraChunkCulling,
                CameraChunkHorizontalRadius,
                CameraChunkVerticalRadius
            )
        )
        {
            continue;
        }

        VegetationChunkCoordinates.Add(
            Pair.Key
        );
    }

    VegetationChunkCoordinates.Sort(
        [&RegisteredChunks, &StreamingLocation](
            const FIntVector& A,
            const FIntVector& B
        )
        {
            const ACubusVoxelVolumeActor* ChunkA =
                RegisteredChunks.FindRef(A).Get();

            const ACubusVoxelVolumeActor* ChunkB =
                RegisteredChunks.FindRef(B).Get();

            if (!IsValid(ChunkA))
            {
                return false;
            }

            if (!IsValid(ChunkB))
            {
                return true;
            }

            return FVector::DistSquared(
                ChunkA->GetActorLocation(),
                StreamingLocation
            ) <
            FVector::DistSquared(
                ChunkB->GetActorLocation(),
                StreamingLocation
            );
        }
    );

    for (
        const FIntVector& ChunkCoordinate :
        VegetationChunkCoordinates
    )
    {
        ACubusVoxelVolumeActor* Chunk =
            RegisteredChunks.FindRef(
                ChunkCoordinate
            ).Get();

        if (!IsValid(Chunk))
        {
            continue;
        }

        const FCubusBlockChunkData* ChunkData =
            Chunk->GetChunkData();

        if (ChunkData == nullptr)
        {
            continue;
        }

        if (
            bAppendOnly &&
            PublishedChunkVegetationSignatures.Contains(
                ChunkCoordinate
            )
        )
        {
            continue;
        }

        if (
            RenderedInstanceBudgetCount >=
            PlantLimit
        )
        {
            break;
        }

        const float SafeVoxelSize =
            FMath::Max(1.0f, Chunk->GetVoxelSize());

        const double ChunkHalfWorldExtent =
            static_cast<double>(Cubus::ChunkSize) *
            static_cast<double>(SafeVoxelSize) *
            0.5;

        for (
            const FCubusVegetationInstance& Instance :
            ChunkData->GetVegetationInstances()
        )
        {
            if (
                RenderedInstanceBudgetCount >=
                PlantLimit
            )
            {
                break;
            }

            const FVector WorldLocation(
                (static_cast<double>(Instance.WorldVoxel.X) + 0.5) *
                    SafeVoxelSize -
                    ChunkHalfWorldExtent,
                (static_cast<double>(Instance.WorldVoxel.Y) + 0.5) *
                    SafeVoxelSize -
                    ChunkHalfWorldExtent,
                static_cast<double>(Instance.WorldVoxel.Z) *
                    SafeVoxelSize -
                    ChunkHalfWorldExtent
        );

        if (
            bEnableHeightPruneFilter &&
            (
                WorldLocation.Z < PruneMinWorldZ ||
                        WorldLocation.Z > PruneMaxWorldZ
                    )
        )
        {
            continue;
        }

        const float TypeScaleMultiplier =
            ResolveTypeScaleMultiplier(
                Instance.TypeId,
                bEnablePerTypeScaleOverrides,
                BroadleafScaleMultiplier,
                ConiferScaleMultiplier,
                ShrubScaleMultiplier,
                GrassScaleMultiplier,
                ReedsScaleMultiplier,
                AlpineScaleMultiplier
            );

        const float CombinedScale = FMath::Max(
            0.01f,
            Instance.Scale *
                FMath::Max(
                    0.01f,
                    GlobalPlantScaleMultiplier
                ) *
                FMath::Max(
                    0.01f,
                    TypeScaleMultiplier
                )
        );

        const FCubusResolvedVegetationPlacement ResolvedPlacement =
            VegetationPlacement.Resolve(
                Instance,
                WorldLocation,
                SafeVoxelSize,
                CombinedScale,
                RandomizationSettings
            );

        if (ResolvedPlacement.bRandomized)
        {
            ++RandomizedPlantCount;

            ObservedRandomScaleMin = FMath::Min(
                ObservedRandomScaleMin,
                ResolvedPlacement.AppliedRandomScale
            );

            ObservedRandomScaleMax = FMath::Max(
                ObservedRandomScaleMax,
                ResolvedPlacement.AppliedRandomScale
            );
        }
            if (ResolvedPlacement.bPruned)
            {
                ++RandomPrunedPlantCount;
                continue;
            }

            FVector FinalLocation =
                ResolvedPlacement.Location;

            FVector SurfaceNormal =
                FVector::UpVector;

            bool bFoundTerrainSurface = false;

            const float FinalScale =
                ResolvedPlacement.Scale;

            const float FinalYaw =
                ResolvedPlacement.Yaw;

            UProceduralMeshComponent* TerrainMesh =
                Chunk->GetTerrainMeshComponent();

            if (
                IsValid(TerrainMesh) &&
                Chunk->HasBuiltTerrainCollision()
            )
            {
                const float SurfaceSearchDistance =
                    SafeVoxelSize * 3.0f;

                const FVector TraceStart =
                    FinalLocation +
                    FVector::UpVector * SurfaceSearchDistance;

                const FVector TraceEnd =
                    FinalLocation -
                    FVector::UpVector * SurfaceSearchDistance;

                FHitResult SurfaceHit;

                FCollisionQueryParams QueryParams(
                    SCENE_QUERY_STAT(CubusVegetationSurfaceSnap),
                    false,
                    this
                );

                if (
                    TerrainMesh->LineTraceComponent(
                        SurfaceHit,
                        TraceStart,
                        TraceEnd,
                        QueryParams
                    )
                )
                {
                    FinalLocation =
                        SurfaceHit.ImpactPoint;

                    SurfaceNormal =
                        SurfaceHit.ImpactNormal.GetSafeNormal();

                    bFoundTerrainSurface = true;
                }
            }

            const float SurfaceSlopeDegrees =
                FMath::RadiansToDegrees(
                    FMath::Acos(
                        FMath::Clamp(
                            FVector::DotProduct(
                                SurfaceNormal,
                                FVector::UpVector
                            ),
                            -1.0f,
                            1.0f
                        )
                    )
                );

            const bool bTreeType =
                Instance.TypeId == CubusVegetationType::BroadleafTree ||
                Instance.TypeId == CubusVegetationType::ConiferTree;

            const bool bGrassType =
                Instance.TypeId == CubusVegetationType::Grass;

            if (
                (bGrassType || bTreeType) &&
                !bFoundTerrainSurface
            )
            {
                continue;
            }

            if (
                bTreeType &&
                SurfaceSlopeDegrees > MaximumTreeSlopeDegrees
            )
            {
                continue;
            }

            if (
                bGrassType &&
                SurfaceSlopeDegrees > MaximumGrassSlopeDegrees
            )
            {
                continue;
            }

            const FTransform WorldTransform(
                FRotator(0.0f, FinalYaw, 0.0f),
                FinalLocation,
                FVector(FinalScale)
            );

            if (
                !bRenderWorldPlantBatches ||
                RenderedInstanceBudgetCount >= PlantLimit
            )
            {
                continue;
            }

            const FTransform LocalTransform =
                WorldTransform.GetRelativeTransform(GetActorTransform());

            const int32 SpeciesIndex =
                VegetationCatalog.SelectSpeciesIndex(
                    Instance,
                    SpeciesCatalog,
                    bClusterTreeFamilies,
                    TreeFamilyCellSizeVoxels,
                    RuntimeRandomizationSeed
                );

            if (SpeciesIndex == INDEX_NONE)
            {
                continue;
            }

            const FCubusVegetationSpeciesCatalogEntry& Entry =
                SpeciesCatalog[SpeciesIndex];

            const int32 StageCount =
                Entry.GrowthStageMeshes.Num();

            if (StageCount <= 0)
            {
                continue;
            }

            const int32 GrowthStage =
                VegetationCatalog.ResolveGrowthStageIndex(
                    Instance,
                    StageCount,
                    bClusterTreeFamilies,
                    TreeFamilyCellSizeVoxels,
                    TreeFamilyCenterJitterFraction,
                    MatureTreeCoreRadius,
                    YoungTreeRingRadius,
                    SaplingTreeRingRadius,
                    TreeFamilyGrowthNoise,
                    RuntimeRandomizationSeed
                );

            const int64 PrimaryBatchKey =
                FCubusVegetationRenderer::MakePrimaryBatchKey(
                    SpeciesIndex,
                    GrowthStage
                );

            const bool bPrimaryBatchIsSkeletal =
                CatalogSkeletalBatchComponents.Contains(
                    PrimaryBatchKey
                );

            int64 TargetBatchKey = PrimaryBatchKey;

            /*
            * Near/detail vegetation keeps its authored primary representation.
            *
            * StaticGrowthStageAssets are reserved for the independent far
            * vegetation renderer and must not replace close skeletal trees.
            */
            if (
                bPrimaryBatchIsSkeletal &&
                bTreeType &&
                bEnableHeroSkeletalWindMode &&
                bHasCamera
            )
            {
                if (RenderedInstanceBudgetCount >= PlantLimit)
                {
                    continue;
                }

                FCubusVegetationRepresentationCandidate Candidate;

                Candidate.PrimaryBatchKey =
                    PrimaryBatchKey;

                Candidate.StaticFallbackBatchKey =
                    INDEX_NONE;

                Candidate.LocalTransform =
                    LocalTransform;

                Candidate.DistanceSquared =
                    FVector::DistSquared(
                        FinalLocation,
                        CameraLocation
                    );

                Candidate.bHasStaticFallback =
                    false;

                HeroTreeCandidates.Add(
                    MoveTemp(Candidate)
                );

                ++RenderedInstanceBudgetCount;
                ++RenderedPlantCount;

                continue;
            }
            
            if (
                !CatalogGrassBatchComponents.Contains(
                    TargetBatchKey
                ) &&
                !CatalogStaticBatchComponents.Contains(
                    TargetBatchKey
                ) &&
                !CatalogSkeletalBatchComponents.Contains(
                    TargetBatchKey
                )
            )
            {
                continue;
            }

            TArray<FTransform>& BatchTransforms =
                CatalogTransformsByBatchKey.FindOrAdd(
                    TargetBatchKey
                );

            if (
                Instance.TypeId != CubusVegetationType::Grass ||
                GrassInstancesPerPlacement <= 1
            )
            {
                
                if (RenderedInstanceBudgetCount >= PlantLimit)
                {
                    continue;
                }

                BatchTransforms.Add(
                    LocalTransform
                );

                ++RenderedInstanceBudgetCount;
                ++RenderedPlantCount;
            }
            else
            {
                const int32 RemainingInstanceBudget =
                    PlantLimit == MAX_int32
                        ? MAX_int32
                        : FMath::Max(
                            0,
                            PlantLimit -
                                RenderedInstanceBudgetCount
                        );

                if (RemainingInstanceBudget <= 0)
                {
                    continue;
                }

                const int32 GrassCount =
                    FMath::Min(
                        FMath::Max(
                            1,
                            GrassInstancesPerPlacement
                        ),
                        RemainingInstanceBudget
                    );

                for (
                    int32 GrassIndex = 0;
                    GrassIndex < GrassCount;
                    ++GrassIndex
                )
                {
                    const uint32 ScatterHash =
                        HashCombineFast(
                            GetTypeHash(
                                Instance.WorldVoxel
                            ),
                            HashCombineFast(
                                GetTypeHash(
                                    TargetBatchKey
                                ),
                                GetTypeHash(
                                    GrassIndex
                                )
                            )
                        );

                    FRandomStream ScatterRandom(
                        static_cast<int32>(
                            ScatterHash
                        )
                    );

                    const float Angle =
                        ScatterRandom.FRandRange(
                            0.0f,
                            2.0f * PI
                        );

                    const float SafeGrassScatterRadius =
                        FMath::Min(
                            GrassScatterRadius,
                            SafeVoxelSize * 0.42f
                        );

                    const float Radius =
                        FMath::Sqrt(
                            ScatterRandom.FRand()
                        ) *
                        SafeGrassScatterRadius;

                    const FVector ScatterOffset(
                        FMath::Cos(Angle) * Radius,
                        FMath::Sin(Angle) * Radius,
                        0.0f
                    );

                    FVector GrassWorldLocation =
                        FinalLocation +
                        ScatterOffset;

                    if (
                        FMath::Abs(
                            SurfaceNormal.Z
                        ) > 0.01f
                    )
                    {
                        GrassWorldLocation.Z =
                            FinalLocation.Z -
                            (
                                SurfaceNormal.X *
                                    ScatterOffset.X +
                                SurfaceNormal.Y *
                                    ScatterOffset.Y
                            ) /
                            SurfaceNormal.Z;
                    }

                    const float GrassYaw =
                        FinalYaw +
                        ScatterRandom.FRandRange(
                            0.0f,
                            360.0f
                        );

                    const float GrassScale =
                        FinalScale *
                        ScatterRandom.FRandRange(
                            0.82f,
                            1.18f
                        );

                    const FTransform GrassWorldTransform(
                        FRotator(
                            0.0f,
                            GrassYaw,
                            0.0f
                        ),
                        GrassWorldLocation,
                        FVector(GrassScale)
                    );

                    BatchTransforms.Add(
                        GrassWorldTransform
                            .GetRelativeTransform(
                                GetActorTransform()
                            )
                    );
                }

                RenderedInstanceBudgetCount +=
                    GrassCount;

                ++RenderedPlantCount;
            }
        }
    }

    const int32 HeroComponentLimit =
        bEnableHeroSkeletalWindMode &&
        MaxHeroSkeletalWindComponents > 0
            ? FMath::Clamp(
                MaxHeroSkeletalWindComponents,
                0,
                64
            )
            : 0;

    FCubusVegetationRepresentationSelector::RouteCandidates(
        HeroTreeCandidates,
        HeroComponentLimit,
        HeroSkeletalWindMaxDistance,
        CatalogTransformsByBatchKey,
        HeroTransformsByBatchKey
    );

    for (
        const TPair<int64, TObjectPtr<UInstancedStaticMeshComponent>>& Pair
         : CatalogGrassBatchComponents)
    {
        UInstancedStaticMeshComponent* GrassBatch =
            Pair.Value;

        if (!IsValid(GrassBatch))
        {
            continue;
        }

        const TArray<FTransform>* Transforms =
            CatalogTransformsByBatchKey.Find(Pair.Key);

        if (
            Transforms == nullptr ||
            Transforms->IsEmpty()
        )
        {
            continue;
        }

        GrassBatch->AddInstances(
            *Transforms,
            false,
            false
        );

        StaticBatchTransformCount +=
            Transforms->Num();
    }

    for (const TPair<int64, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>>& Pair
     : CatalogStaticBatchComponents)
    {
        UHierarchicalInstancedStaticMeshComponent* Component =
            Pair.Value;

        if (!IsValid(Component))
        {
            continue;
        }

        const TArray<FTransform>* Transforms =
            CatalogTransformsByBatchKey.Find(Pair.Key);

        if (Transforms == nullptr || Transforms->IsEmpty())
        {
            continue;
        }

        if (!bForceMegaplantFoliageMaterialOverride)
        {
            Component->EmptyOverrideMaterials();
        }

        Component->AddInstances(*Transforms, false, false, false);
        Component->BuildTreeIfOutdated(false, false);
        StaticBatchTransformCount += Transforms->Num();
    }

    int32 ActiveHeroComponentCount = 0;
    int32 ActiveHeroPveActorCount = 0;

    if (bEnableHeroSkeletalWindMode)
    {
        for (USkeletalMeshComponent* HeroComponent : HeroSkeletalWindComponents)
        {
            if (!IsValid(HeroComponent))
            {
                continue;
            }

            HeroComponent->SetVisibility(false, true);
            HeroComponent->SetHiddenInGame(true, true);
        }

        for (AActor* HeroActor : HeroPveWindActors)
        {
            if (!IsValid(HeroActor))
            {
                continue;
            }

            HeroActor->SetActorHiddenInGame(true);
        }
    }

    int32 RemainingInstancedSkeletalFallbackBudget = 0;

    if (bUseInstancedSkeletalFallbackBeyondHeroDistance)
    {
        const int32 ConfiguredFallbackBudget =
            FMath::Max(0, MaxInstancedSkeletalFallbackInstances);

        // Keep fallback capacity aligned with the current render plant limit so
        // non-hero trees do not vanish as hero wind selection updates.
        RemainingInstancedSkeletalFallbackBudget =
            FMath::Max(ConfiguredFallbackBudget, PlantLimit);
    }

    for (const TPair<int64, TObjectPtr<UInstancedSkinnedMeshComponent>>& Pair
     : CatalogSkeletalBatchComponents)
    {
    UInstancedSkinnedMeshComponent* Component = Pair.Value;

    if (!IsValid(Component))
    {
        continue;
    }

    const TArray<FTransform>* RegularTransforms =
        CatalogTransformsByBatchKey.Find(Pair.Key);

    const TArray<FTransform>* HeroTransforms =
        HeroTransformsByBatchKey.Find(Pair.Key);

    const bool bHasRegularTransforms =
        RegularTransforms != nullptr &&
        !RegularTransforms->IsEmpty();

    const bool bHasHeroTransforms =
        HeroTransforms != nullptr &&
        !HeroTransforms->IsEmpty();

    if (!bHasRegularTransforms && !bHasHeroTransforms)
    {
        continue;
    }

    const int32 SpeciesIndex =
        static_cast<int32>(Pair.Key >> 32);

    UMaterialInterface* FoliageOverrideMaterial = nullptr;
    UClass* HeroPveActorClass = nullptr;
    UTransformProviderData* SpeciesTransformProvider = nullptr;

    if (SpeciesCatalog.IsValidIndex(SpeciesIndex))
    {
        const FCubusVegetationSpeciesCatalogEntry& SpeciesEntry =
            SpeciesCatalog[SpeciesIndex];

        if (!IsValid(Component->GetTransformProvider()))
        {
            SpeciesTransformProvider =
                FCubusVegetationAssetResolver::ResolveTransformProvider(
                    SpeciesEntry.SpeciesId
                );
        }

        if (
            IsValid(SpeciesTransformProvider) &&
            Component->GetTransformProvider() != SpeciesTransformProvider
        )
        {
            Component->SetTransformProvider(SpeciesTransformProvider);
            Component->MarkRenderStateDirty();
            Component->MarkRenderDynamicDataDirty();
            ++BoundSpeciesTransformProviderCount;
        }

        const bool bSpeciesHasNativeDynamicWindProvider =
            Cast<UDynamicWindData>(
                Component->GetTransformProvider()
            ) != nullptr;

        if (
            bUseHeroPveActorWindMode &&
            !bSpeciesHasNativeDynamicWindProvider
        )
        {
            HeroPveActorClass =
                FCubusVegetationAssetResolver::ResolveHeroPveActorClass(
                    SpeciesEntry
                );
        }

        if (bForceMegaplantFoliageMaterialOverride)
        {
            const TSoftObjectPtr<UMaterialInterface> FoliageMaterialRef =
                FCubusVegetationAssetResolver::ResolveFoliageMaterial(
                    SpeciesEntry.SpeciesId
                );

            if (!FoliageMaterialRef.IsNull())
            {
                FoliageOverrideMaterial =
                    FoliageMaterialRef.LoadSynchronous();

                if (IsValid(FoliageOverrideMaterial))
                {
                    ++FoliageMaterialOverrideComponentCount;
                }

                VegetationRenderer.ApplyFoliageMaterialOverride(
                    Component,
                    FoliageOverrideMaterial
                );
            }
        }
    }

    const bool bHasNativeDynamicWindProvider =
        Cast<UDynamicWindData>(
            Component->GetTransformProvider()
        ) != nullptr;

    FCubusHeroVegetationRenderSettings RenderSettings;
    RenderSettings.bUsePveActors = bUseHeroPveActorWindMode;
    RenderSettings.bUseInstancedSkeletalFallback =
        bUseInstancedSkeletalFallbackBeyondHeroDistance;
    RenderSettings.bForceFoliageMaterialOverride =
        bForceMegaplantFoliageMaterialOverride;
    RenderSettings.bCastShadow = bCastWorldPlantShadows;
    RenderSettings.bAppendOnly = bAppendOnly;

    if (bHasRegularTransforms)
    {
        RenderSettings.bEnabled = false;

        const FCubusHeroVegetationRenderResult RegularResult =
            VegetationRenderer.RenderSkeletalBatch(
                this,
                World,
                Root,
                Component,
                *RegularTransforms,
                HeroPveActorClass,
                FoliageOverrideMaterial,
                CachedUltraDynamicWeatherActor,
                RenderSettings,
                ActiveHeroComponentCount,
                ActiveHeroPveActorCount,
                RemainingInstancedSkeletalFallbackBudget,
                HeroSkeletalWindComponents,
                HeroPveWindActors
            );

        SkeletalBatchTransformCount +=
            RegularResult.SkeletalInstanceCount;

        InstancedSkeletalFallbackCount +=
            RegularResult.InstancedFallbackCount;
    }

    if (bHasHeroTransforms)
    {
        RenderSettings.bEnabled =
            bEnableHeroSkeletalWindMode &&
            !bHasNativeDynamicWindProvider;

        const FCubusHeroVegetationRenderResult HeroResult =
            VegetationRenderer.RenderSkeletalBatch(
                this,
                World,
                Root,
                Component,
                *HeroTransforms,
                HeroPveActorClass,
                FoliageOverrideMaterial,
                CachedUltraDynamicWeatherActor,
                RenderSettings,
                ActiveHeroComponentCount,
                ActiveHeroPveActorCount,
                RemainingInstancedSkeletalFallbackBudget,
                HeroSkeletalWindComponents,
                HeroPveWindActors
            );

        SkeletalBatchTransformCount +=
            HeroResult.SkeletalInstanceCount;

        InstancedSkeletalFallbackCount +=
            HeroResult.InstancedFallbackCount;
    }
}

    LoadedChunkCount =
        CurrentChunkVegetationSignatures.Num();

    PublishedPlacementHash =
        static_cast<int64>(
            CalculateVegetationSignatureMapHash(
                CurrentChunkVegetationSignatures
            )
        );

    PublishedVegetationSettingsHash =
        CurrentVegetationSettingsHash;

    PublishedChunkVegetationSignatures =
        MoveTemp(CurrentChunkVegetationSignatures);

    const int32 TotalRenderedInstanceCount =
        StaticBatchTransformCount +
        SkeletalBatchTransformCount +
        ActiveHeroComponentCount +
        ActiveHeroPveActorCount;

    bPublishedVegetationBudgetSaturated =
    PlantLimit != MAX_int32 &&
    TotalRenderedInstanceCount >=
        PlantLimit;

    if (
        bFullVegetationRebuild &&
        bHasStreamingOrigin
    )
    {
        LastFullVegetationBuildCameraLocation =
            StreamingLocation;

        bHasLastFullVegetationBuildCameraLocation =
            true;
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "Cubus world vegetation: %d chunks, "
            "%d placements, %d rendered instances "
            "(static=%d, skeletal-instanced=%d, hero=%d, heroPveActors=%d, "
            "fallback=%d, foliage-overrides=%d, heroPve=%s, fallbackInstanced=%s)"
        ),
        LoadedChunkCount,
        RenderedPlantCount,
        TotalRenderedInstanceCount,
        StaticBatchTransformCount,
        SkeletalBatchTransformCount,
        ActiveHeroComponentCount,
        ActiveHeroPveActorCount,
        InstancedSkeletalFallbackCount,
        FoliageMaterialOverrideComponentCount,
        bUseHeroPveActorWindMode ? TEXT("on") : TEXT("off"),
        bUseInstancedSkeletalFallbackBeyondHeroDistance ? TEXT("on") : TEXT("off")
    );

    if (bEnableRuntimeRandomization && RandomizedPlantCount > 0)
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "Cubus per-tree randomization: "
                "sampled=%d pruned=%d "
                "scaleRange=(%.3f, %.3f)"
            ),
            RandomizedPlantCount,
            RandomPrunedPlantCount,
            ObservedRandomScaleMin,
            ObservedRandomScaleMax
        );
    }

    if (BoundSpeciesTransformProviderCount > 0)
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Cubus species transform provider binds: %d"),
            BoundSpeciesTransformProviderCount
        );
    }

    if (
        bBridgeUdwToDynamicWind &&
        bEnableHeroSkeletalWindMode &&
        SkeletalBatchTransformCount <= 0 &&
        ActiveHeroComponentCount <= 0
    )
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT(
                "Cubus wind diagnostics: no skeletal vegetation instances are active; world batches are static-only. Wind bridge values are updating, but visible motion requires wind-enabled foliage materials on static meshes or skeletal growth-stage assets near camera."
            )
        );
    }
}

void ACubusWorldVegetationActor::ClearWorldVegetation()
{
    LoadedChunkCount = 0;
    RenderedPlantCount = 0;
    PublishedPlacementHash = 0;

    PublishedChunkVegetationSignatures.Reset();
    PublishedVegetationSettingsHash = 0;

    bPublishedVegetationBudgetSaturated =
        false;

    VegetationRenderer.ClearBatches(
        CatalogGrassBatchComponents,
        CatalogStaticBatchComponents,
        CatalogSkeletalBatchComponents
    );

    for (USkeletalMeshComponent* HeroComponent : HeroSkeletalWindComponents)
    {
        if (!IsValid(HeroComponent))
        {
            continue;
        }

        HeroComponent->SetVisibility(false, true);
        HeroComponent->SetHiddenInGame(true, true);
    }

    for (AActor* HeroActor : HeroPveWindActors)
    {
        if (!IsValid(HeroActor))
        {
            continue;
        }

        HeroActor->SetActorHiddenInGame(true);
    }
}

void ACubusWorldVegetationActor::ResolveBlockWorld()
{
    if (IsValid(BlockWorld))
    {
        return;
    }

    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return;
    }

    for (
        TActorIterator<ACubusBlockWorldActor> Iterator(World);
        Iterator;
        ++Iterator
    )
    {
        BlockWorld = *Iterator;
        break;
    }
}

uint32 ACubusWorldVegetationActor::CalculateVegetationSettingsHash() const
{
    const FCubusVegetationRandomizationSettings
        RandomizationSettings
        {
            bEnableRuntimeRandomization,
            RuntimeRandomizationSeed,
            RandomPruneProbability,
            RandomScaleJitterMin,
            RandomScaleJitterMax,
            RandomPositionJitterVoxelFraction,
            RandomYawJitterDegrees
        };

    uint32 Hash =
        VegetationPlacement
            .CalculateRandomizationSettingsHash(
                RandomizationSettings
            );
    
    Hash = HashCombineFast(Hash, GetTypeHash(bRenderWorldPlantBatches));
    Hash = HashCombineFast(Hash, GetTypeHash(GlobalPlantScaleMultiplier));
    Hash = HashCombineFast(Hash, GetTypeHash(bEnablePerTypeScaleOverrides));
    Hash = HashCombineFast(Hash, GetTypeHash(BroadleafScaleMultiplier));
    Hash = HashCombineFast(Hash, GetTypeHash(ConiferScaleMultiplier));
    Hash = HashCombineFast(Hash, GetTypeHash(ShrubScaleMultiplier));
    Hash = HashCombineFast(Hash, GetTypeHash(GrassScaleMultiplier));
    Hash = HashCombineFast(Hash, GetTypeHash(ReedsScaleMultiplier));
    Hash = HashCombineFast(Hash, GetTypeHash(AlpineScaleMultiplier));
    Hash = HashCombineFast(Hash, GetTypeHash(bEnableHeightPruneFilter));
    Hash = HashCombineFast(Hash, GetTypeHash(PruneMinWorldZ));
    Hash = HashCombineFast(Hash, GetTypeHash(PruneMaxWorldZ));
    Hash = HashCombineFast(Hash, GetTypeHash(MaximumRenderedPlants));
    Hash = HashCombineFast(Hash, GetTypeHash(PlantStartCullDistance));
    Hash = HashCombineFast(Hash, GetTypeHash(PlantEndCullDistance));
    Hash = HashCombineFast(Hash, GetTypeHash(bCullByCameraChunkRadius));
    Hash = HashCombineFast(Hash, GetTypeHash(CameraChunkHorizontalRadius));
    Hash = HashCombineFast(Hash, GetTypeHash(CameraChunkVerticalRadius));
    Hash = HashCombineFast(Hash, GetTypeHash(VegetationRecenterDistance));
    Hash = HashCombineFast(Hash, GetTypeHash(FarVegetationInnerRadius));
    Hash = HashCombineFast(Hash, GetTypeHash(FarVegetationEndCullDistance));
    Hash = HashCombineFast(Hash, GetTypeHash(bClusterTreeFamilies));
    Hash = HashCombineFast(Hash, GetTypeHash(TreeFamilyCellSizeVoxels));
    Hash = HashCombineFast(Hash, GetTypeHash(TreeFamilyCenterJitterFraction));
    Hash = HashCombineFast(Hash, GetTypeHash(MatureTreeCoreRadius));
    Hash = HashCombineFast(Hash, GetTypeHash(YoungTreeRingRadius));
    Hash = HashCombineFast(Hash, GetTypeHash(SaplingTreeRingRadius));
    Hash = HashCombineFast(Hash, GetTypeHash(TreeFamilyGrowthNoise));

    Hash = HashCombineFast(
        Hash,
        GetTypeHash(bEnableHeroSkeletalWindMode)
    );

    Hash = HashCombineFast(
        Hash,
        GetTypeHash(bUseHeroPveActorWindMode)
    );

    Hash = HashCombineFast(
        Hash,
        GetTypeHash(
            bUseInstancedSkeletalFallbackBeyondHeroDistance
        )
    );

    Hash = HashCombineFast(
        Hash,
        GetTypeHash(HeroSkeletalWindMaxDistance)
    );

    Hash = HashCombineFast(
        Hash,
        GetTypeHash(MaxHeroSkeletalWindComponents)
    );

    Hash = HashCombineFast(
        Hash,
        GetTypeHash(MaxInstancedSkeletalFallbackInstances)
    );

    Hash = HashCombineFast(
        Hash,
        GetTypeHash(HeroRepresentationRefreshDistance)
    );

    if (bEnableHeroSkeletalWindMode)
    {
        const APlayerController* PlayerController =
            UGameplayStatics::GetPlayerController(
                this,
                0
            );

        if (
            IsValid(PlayerController) &&
            IsValid(
                PlayerController->PlayerCameraManager
            )
        )
        {
            const FVector CameraLocation =
                PlayerController
                    ->PlayerCameraManager
                    ->GetCameraLocation();

            const double RefreshDistance =
                static_cast<double>(
                    FMath::Max(
                        100.0f,
                        HeroRepresentationRefreshDistance
                    )
                );

            const FIntVector CameraRepresentationCell(
                FMath::FloorToInt(
                    CameraLocation.X /
                    RefreshDistance
                ),
                FMath::FloorToInt(
                    CameraLocation.Y /
                    RefreshDistance
                ),
                FMath::FloorToInt(
                    CameraLocation.Z /
                    RefreshDistance
                )
            );

            Hash = HashCombineFast(
                Hash,
                GetTypeHash(
                    CameraRepresentationCell
                )
            );
        }
    }

    for (const FCubusVegetationSpeciesCatalogEntry& Entry : SpeciesCatalog)
    {
        Hash = HashCombineFast(Hash, GetTypeHash(Entry.SpeciesId));
        Hash = HashCombineFast(Hash, GetTypeHash(Entry.TypeId));
        Hash = HashCombineFast(Hash, GetTypeHash(Entry.Weight));
        Hash = HashCombineFast(Hash, GetTypeHash(Entry.BiomeMask));

        for (
            const TSoftObjectPtr<UObject>& MeshReference :
            Entry.GrowthStageMeshes)
        {
            Hash = HashCombineFast(
                Hash,
                GetTypeHash(MeshReference.ToSoftObjectPath().ToString())
            );
        }

        for (
            const TSoftObjectPtr<UObject>& MeshReference :
            Entry.StaticGrowthStageAssets
        )
        {
            Hash = HashCombineFast(
                Hash,
                GetTypeHash(
                    MeshReference.ToSoftObjectPath().ToString()
                )
            );
        }
    }

    return Hash;
}

uint32 ACubusWorldVegetationActor::CalculateLoadedPlacementHash(
    int32& OutLoadedChunkCount
) const
{
    OutLoadedChunkCount = 0;

    if (!IsValid(BlockWorld))
    {
        return 0;
    }

    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return 0;
    }

    const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
    const bool bHasStreamingOrigin = IsValid(PlayerPawn);
    const bool bUseCameraChunkCulling =
        bCullByCameraChunkRadius && bHasStreamingOrigin;
    const FVector StreamingLocation = bHasStreamingOrigin
        ? PlayerPawn->GetActorLocation()
        : FVector::ZeroVector;

    TMap<FIntVector, uint32>
        CurrentChunkVegetationSignatures;

    const auto& RegisteredChunks =
        BlockWorld->GetRegisteredChunks();

    for (const auto& Pair : RegisteredChunks)
    {
        const ACubusVoxelVolumeActor* Chunk =
            Pair.Value.Get();

        if (!IsValid(Chunk))
        {
            continue;
        }

        const FCubusBlockChunkData* ChunkData =
            Chunk->GetChunkData();

        if (ChunkData == nullptr)
        {
            continue;
        }

        if (
            !FCubusVegetationChunkFilter::IsWithinCameraRadius(
                Chunk,
                StreamingLocation,
                bUseCameraChunkCulling,
                CameraChunkHorizontalRadius,
                CameraChunkVerticalRadius
            )
        )
        {
            continue;
        }

        ++OutLoadedChunkCount;

        CurrentChunkVegetationSignatures.Add(
          Pair.Key,
            CalculateChunkVegetationSignature(
                *ChunkData
            )
        );
    }

    return CalculateVegetationSignatureMapHash(
        CurrentChunkVegetationSignatures
    );
}

ACubusWorldVegetationActor::FCubusVegetationDistancePolicy
ACubusWorldVegetationActor::ResolveVegetationDistancePolicy(
    const float VoxelSize
) const
{
    FCubusVegetationDistancePolicy Policy;

    Policy.NearStartCullDistance =
        FMath::Max(0, PlantStartCullDistance);

    Policy.NearEndCullDistance =
        FMath::Max(
  Policy.NearStartCullDistance,
  PlantEndCullDistance
        );

    Policy.FarInnerRadius =
        FMath::Max(0.0f, FarVegetationInnerRadius);

    if (!bCullByCameraChunkRadius)
    {
        Policy.NearResidentRadius =
  static_cast<float>(Policy.NearEndCullDistance);
        return Policy;
    }

    const float SafeVoxelSize =
        FMath::Max(1.0f, VoxelSize);

    const float ChunkWorldSize =
        static_cast<float>(Cubus::ChunkSize) *
        SafeVoxelSize;

    /*
     * Chunk culling is inclusive. Radius 8 therefore owns chunk centres out to
     * 8 chunks, plus half a chunk of physical coverage at the edge.
     */
    Policy.NearResidentRadius =
        (
  static_cast<float>(
      FMath::Max(0, CameraChunkHorizontalRadius)
  ) +
  0.5f
        ) *
        ChunkWorldSize;

    /*
     * Keep at least two ordinary chunks of overlap between the detailed and
     * far representations. This prevents a residency/cull gap even when old
     * Blueprint values still contain kilometre-scale near cull distances.
     */
    const float OverlapDistance =
        FMath::Min(
  Policy.NearResidentRadius,
  FMath::Max(
      ChunkWorldSize * 2.0f,
      Policy.NearResidentRadius * 0.20f
  )
        );

    const float LatestSafeFarStart =
        FMath::Max(
  0.0f,
  Policy.NearResidentRadius -
  OverlapDistance
        );

    Policy.FarInnerRadius =
        FMath::Min(
  Policy.FarInnerRadius,
  LatestSafeFarStart
        );

    Policy.NearEndCullDistance =
        FMath::Max(
  1,
  FMath::RoundToInt(
      FMath::Min(
          static_cast<float>(
              Policy.NearEndCullDistance
          ),
          Policy.NearResidentRadius
      )
  )
        );

    Policy.NearStartCullDistance =
        FMath::Clamp(
  FMath::Min(
      Policy.NearStartCullDistance,
      FMath::RoundToInt(
          Policy.FarInnerRadius
      )
  ),
  0,
  Policy.NearEndCullDistance
        );

    return Policy;
}

void ACubusWorldVegetationActor::ApplyVegetationDistancePolicy(
    const float VoxelSize
)
{
    const FCubusVegetationDistancePolicy Policy =
        ResolveVegetationDistancePolicy(VoxelSize);

    for (const auto& Pair : CatalogGrassBatchComponents)
    {
        if (IsValid(Pair.Value))
        {
  Pair.Value->SetCullDistances(
      Policy.NearStartCullDistance,
      Policy.NearEndCullDistance
  );
        }
    }

    for (const auto& Pair : CatalogStaticBatchComponents)
    {
        if (IsValid(Pair.Value))
        {
  Pair.Value->SetCullDistances(
      Policy.NearStartCullDistance,
      Policy.NearEndCullDistance
  );
        }
    }

    static bool bLoggedVegetationDistancePolicy = false;
    if (!bLoggedVegetationDistancePolicy)
    {
        UE_LOG(
  LogTemp,
  Display,
  TEXT(
      "Cubus vegetation distance policy: nearResident=%.0fcm "
      "nearCull=%d..%dcm farInner=%.0fcm farEnd=%.0fcm "
      "chunkRadius=(%d,%d) recenter=%.0fcm"
  ),
  Policy.NearResidentRadius,
  Policy.NearStartCullDistance,
  Policy.NearEndCullDistance,
  Policy.FarInnerRadius,
  FarVegetationEndCullDistance,
  CameraChunkHorizontalRadius,
  CameraChunkVerticalRadius,
  VegetationRecenterDistance
        );

        bLoggedVegetationDistancePolicy = true;
    }
}

void ACubusWorldVegetationActor::RefreshVegetationBatches()
{
    if (!bRenderWorldPlantBatches)
    {
        return;
    }

    VegetationCatalog.BuildDefaultsIfNeeded(
        SpeciesCatalog,
        bAutoSeedCatalogDefaults
    );

    VegetationCatalog.Rebuild(
        SpeciesCatalog
    );

    VegetationRenderer.EnsureBatches(
        this,
        Root,
        SpeciesCatalog,
        bCastWorldPlantShadows,
        PlantStartCullDistance,
        PlantEndCullDistance,
        CatalogGrassBatchComponents,
        CatalogStaticBatchComponents,
        CatalogSkeletalBatchComponents
    );
}

void
ACubusWorldVegetationActor::UpdateFarVegetationStreaming(
    const float DeltaSeconds
)
{
    static bool bLoggedFarStreamingActivation =
        false;

    if (!bLoggedFarStreamingActivation)
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "Cubus far streaming entered: "
                "enabled=%s blockWorld=%s"
            ),
            bEnableFarVegetation
                ? TEXT("true")
                : TEXT("false"),
            IsValid(BlockWorld)
                ? TEXT("valid")
                : TEXT("null")
        );

        bLoggedFarStreamingActivation =
            true;
    }

    TimeUntilFarVegetationPublish -=
        DeltaSeconds;

    if (
        !bEnableFarVegetation ||
        !IsValid(BlockWorld)
    )
    {
        return;
    }

    const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);

    if (!IsValid(PlayerPawn))
    {
        return;
    }

    const FVector StreamingLocation = PlayerPawn->GetActorLocation();

    ACubusVoxelVolumeActor* SnapshotChunk =
        nullptr;

    const auto& RegisteredChunks =
        BlockWorld->GetRegisteredChunks();

    for (
        const TPair<
            FIntVector,
            TWeakObjectPtr<ACubusVoxelVolumeActor>
        >& Pair
        : RegisteredChunks
    )
    {
        ACubusVoxelVolumeActor* Candidate =
            Pair.Value.Get();

        if (
            IsValid(Candidate) &&
            Candidate->GetChunkData() != nullptr
        )
        {
            SnapshotChunk = Candidate;
            break;
        }
    }

    if (!IsValid(SnapshotChunk))
    {
        return;
    }

    // Far-cell ownership and world transforms must use the canonical
    // LOD0 voxel scale. RegisteredChunks may contain authored or transitional
    // chunks with a different VoxelSize, so choosing the first map entry makes
    // the entire far forest scale nondeterministically.
    const float SafeVoxelSize =
        FMath::Max(
            1.0f,
            BlockWorld->GetGeneratedVoxelSize()
        );

    const int32 CellSizeVoxels =
        FMath::Max(
            1,
            FarVegetationCellSizeChunks
        ) *
        Cubus::ChunkSize;

    const double HalfChunkWorldExtent =
        static_cast<double>(
            Cubus::ChunkSize
        ) *
        static_cast<double>(
            SafeVoxelSize
        ) *
        0.5;

    /*
     * Inverse of the same world-voxel transform used by the existing
     * vegetation renderer.
     */
    const int32 StreamingWorldVoxelX =
        FMath::FloorToInt(
            (
                static_cast<double>(
                    StreamingLocation.X
                ) +
                HalfChunkWorldExtent
            ) /
            static_cast<double>(
                SafeVoxelSize
            )
        );

    const int32 StreamingWorldVoxelY =
        FMath::FloorToInt(
            (
                static_cast<double>(
                    StreamingLocation.Y
                ) +
                HalfChunkWorldExtent
            ) /
            static_cast<double>(
                SafeVoxelSize
            )
        );

    const FIntPoint CentreCell(
        FMath::FloorToInt(
            static_cast<double>(
                StreamingWorldVoxelX
            ) /
            static_cast<double>(
                CellSizeVoxels
            )
        ),
        FMath::FloorToInt(
            static_cast<double>(
                StreamingWorldVoxelY
            ) /
            static_cast<double>(
                CellSizeVoxels
            )
        )
    );

    if (
        CentreCell !=
        LastFarVegetationCentreCell
    )
    {
        LastFarVegetationCentreCell =
            CentreCell;

        // Every recenter begins by filling any newly visible cells with the
        // sparse horizon pass. Existing cached cells already at a higher pass
        // are retained and simply satisfy this pass immediately.
        FarVegetationRefinementPass = 0;

        RequiredFarVegetationCells.Reset();
        PendingFarVegetationCells.Reset();

        const int32 SafeRadius =
            FMath::Clamp(
                FarVegetationRadiusCells,
                1,
                32
            );

        const double RadiusExtent =
            static_cast<double>(
                SafeRadius
            ) +
            0.5;

        const double RadiusSquared =
            RadiusExtent *
            RadiusExtent;

        for (
            int32 Y = -SafeRadius;
            Y <= SafeRadius;
            ++Y
        )
        {
            for (
                int32 X = -SafeRadius;
                X <= SafeRadius;
                ++X
            )
            {
                const double DistanceSquared =
                    static_cast<double>(
                        X * X +
                        Y * Y
                    );

                if (
                    DistanceSquared >
                    RadiusSquared
                )
                {
                    continue;
                }

                RequiredFarVegetationCells.Add(
                    CentreCell +
                    FIntPoint(
                        X,
                        Y
                    )
                );
            }
        }

        /*
         * Drop cached cells outside the new fixed window.
         */
        TArray<FIntPoint> CachedCoordinates;

        FarVegetationCellCache.GetKeys(
            CachedCoordinates
        );

        for (
            const FIntPoint& Cell
            : CachedCoordinates
        )
        {
            if (
                !RequiredFarVegetationCells
                    .Contains(Cell)
            )
            {
                FarVegetationCellCache.Remove(
                    Cell
                );
                FarVegetationCellRefinementPasses.Remove(
                    Cell
                );

                bFarVegetationRenderDirty =
                    true;
            }
        }

        for (
            const FIntPoint& Cell
            : RequiredFarVegetationCells
        )
        {
            const int32 CompletedPass =
                FarVegetationCellRefinementPasses.FindRef(
                    Cell
                );

            const bool bHasCachedCell =
                FarVegetationCellCache.Contains(Cell);

            if (
                FarVegetationCellsBuilding.Contains(Cell) ||
                (
                    bHasCachedCell &&
                    CompletedPass >= FarVegetationRefinementPass
                )
            )
            {
                continue;
            }

            PendingFarVegetationCells.Add(Cell);
        }

        PendingFarVegetationCells.Sort(
            [CentreCell](
                const FIntPoint& A,
                const FIntPoint& B
            )
            {
                const int32 DistanceA =
                    FMath::Abs(
                        A.X -
                        CentreCell.X
                    ) +
                    FMath::Abs(
                        A.Y -
                        CentreCell.Y
                    );

                const int32 DistanceB =
                    FMath::Abs(
                        B.X -
                        CentreCell.X
                    ) +
                    FMath::Abs(
                        B.Y -
                        CentreCell.Y
                    );

                /*
                 * Pop() then returns nearest-first.
                 */
                return
                    DistanceA >
                    DistanceB;
            }
        );

        bFarVegetationRenderDirty =
            true;
    }

    /*
     * Collect completed worker cells.
     */
    for (
        int32 BuildIndex =
            FarVegetationBuilds.Num() - 1;
        BuildIndex >= 0;
        --BuildIndex
    )
    {
        FCubusFarVegetationCellBuild&
            Build =
                FarVegetationBuilds[
                    BuildIndex
                ];

        if (!Build.Task.IsCompleted())
        {
            continue;
        }

        FCubusFarVegetationCellBuildResult
            Result =
                MoveTemp(
                    Build.Task.GetResult()
                );

        FarVegetationCellsBuilding.Remove(
            Build.CellCoordinate
        );

        if (
            RequiredFarVegetationCells.Contains(
                Result.CellCoordinate
            )
        )
        {
            FarVegetationCellCache.Add(
                Result.CellCoordinate,
                MoveTemp(
                    Result.Trees
                )
            );

            FarVegetationCellRefinementPasses.Add(
                Result.CellCoordinate,
                Result.RefinementPass
            );

            bFarVegetationRenderDirty =
                true;
        }

        FarVegetationBuilds.RemoveAtSwap(
            BuildIndex,
            1,
            EAllowShrinking::No
        );
    }

    // Do not spend full-density work on the nearby part of the
    // window while most of the horizon is still empty. Once every required
    // cell has the current pass, advance the entire window together. Because
    // pass strides are exact multiples (12 -> 6 -> configured 3 by default),
    // existing tree sample positions are a deterministic subset of later
    // passes, so refinement adds detail rather than relocating the forest.
    if (
        PendingFarVegetationCells.IsEmpty() &&
        FarVegetationBuilds.IsEmpty() &&
        FarVegetationRefinementPass < 2
    )
    {
        ++FarVegetationRefinementPass;

        for (
            const FIntPoint& Cell
            : RequiredFarVegetationCells
        )
        {
            const int32* CompletedPass =
                FarVegetationCellRefinementPasses.Find(Cell);

            if (
                CompletedPass == nullptr ||
                *CompletedPass < FarVegetationRefinementPass
            )
            {
                PendingFarVegetationCells.Add(Cell);
            }
        }

        PendingFarVegetationCells.Sort(
            [CentreCell](
                const FIntPoint& A,
                const FIntPoint& B
            )
            {
                const int32 DistanceA =
                    FMath::Abs(A.X - CentreCell.X) +
                    FMath::Abs(A.Y - CentreCell.Y);
                const int32 DistanceB =
                    FMath::Abs(B.X - CentreCell.X) +
                    FMath::Abs(B.Y - CentreCell.Y);

                return DistanceA > DistanceB;
            }
        );

        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "Cubus far vegetation refinement: pass=%d pending=%d"
            ),
            FarVegetationRefinementPass,
            PendingFarVegetationCells.Num()
        );
    }

    /*
     * Start a bounded number of new worker jobs.
     */
    const int32 SafeConcurrentBuilds =
        FMath::Clamp(
            MaxConcurrentFarVegetationBuilds,
            1,
            16
        );

    const int32 SafeStartsPerTick =
        FMath::Clamp(
            MaxFarVegetationBuildStartsPerTick,
            1,
            16
        );

    int32 StartedThisTick = 0;

    while (
        FarVegetationBuilds.Num() <
            SafeConcurrentBuilds &&
        StartedThisTick <
            SafeStartsPerTick &&
        !PendingFarVegetationCells.IsEmpty()
    )
    {
        const FIntPoint Cell =
            PendingFarVegetationCells.Pop(
                EAllowShrinking::No
            );

        if (
            !RequiredFarVegetationCells.Contains(
                Cell
            ) ||
            (
                FarVegetationCellCache.Contains(Cell) &&
                FarVegetationCellRefinementPasses.FindRef(Cell) >=
                    FarVegetationRefinementPass
            ) ||
            FarVegetationCellsBuilding.Contains(
                Cell
            )
        )
        {
            continue;
        }

        const FCubusGenerationSeeds
            GenerationSeeds =
                BlockWorld->GetGenerationSeeds();

        const FCubusVegetationGenerationSettings
            GenerationSettings =
                FCubusBlockVegetationGenerator::
                    CaptureGenerationSettings(
                        SnapshotChunk
                            ->GetGeologyProfile(),
                        GenerationSeeds
                    );

        const FCubusTerrainDensitySettings
            DensitySettings =
                SnapshotChunk
                    ->CaptureTerrainDensitySettings();

        FCubusVegetationRegion Region;

        Region.Minimum =
            FIntPoint(
                Cell.X *
                    CellSizeVoxels,
                Cell.Y *
                    CellSizeVoxels
            );

        Region.Maximum =
            Region.Minimum +
            FIntPoint(
                CellSizeVoxels,
                CellSizeVoxels
            );

        const int32 FullSampleStride =
            FMath::Clamp(
                FarTreeSampleStrideVoxels,
                2,
                64
            );

        const int32 RefinementPass =
            FMath::Clamp(
                FarVegetationRefinementPass,
                0,
                2
            );

        int32 SampleStride = FullSampleStride;

        if (RefinementPass == 0)
        {
            SampleStride =
                FMath::Clamp(
                    FullSampleStride * 4,
                    FullSampleStride,
                    64
                );
        }
        else if (RefinementPass == 1)
        {
            SampleStride =
                FMath::Clamp(
                    FullSampleStride * 2,
                    FullSampleStride,
                    64
                );
        }

        const float DensityScale =
            FMath::Clamp(
                FarTreeDensityScale,
                0.0f,
                1.0f
            );

        FCubusFarVegetationCellBuild Build;

        Build.CellCoordinate =
            Cell;
        Build.RefinementPass =
            RefinementPass;

        Build.Task =
            UE::Tasks::Launch(
                TEXT(
                    "CubusFarVegetationCell"
                ),
                [
                    Cell,
                    Region,
                    GenerationSeeds,
                    GenerationSettings,
                    DensitySettings,
                    SampleStride,
                    DensityScale,
                    RefinementPass
                ]()
                {
                    FCubusFarVegetationCellBuildResult
                        Result;

                    Result.CellCoordinate =
                        Cell;
                    Result.RefinementPass =
                        RefinementPass;

                    const FCubusTerrainDensityField
                        DensityField(
                            DensitySettings
                        );

                    FCubusBlockVegetationGenerator::
                        GenerateFarTreesForRegion(
                            Region,
                            GenerationSeeds,
                            GenerationSettings,
                            DensityField,
                            SampleStride,
                            DensityScale,
                            Result.Trees
                        );

                    return Result;
                }
            );

        FarVegetationCellsBuilding.Add(
            Cell
        );

        FarVegetationBuilds.Add(
            MoveTemp(Build)
        );

        ++StartedThisTick;
    }

    LoadedFarVegetationCellCount =
        FarVegetationCellCache.Num();

    if (
        bFarVegetationRenderDirty &&
        TimeUntilFarVegetationPublish <=
            0.0f
    )
    {
        PublishFarVegetation(
            StreamingLocation,
            SafeVoxelSize
        );

        TimeUntilFarVegetationPublish =
            FMath::Max(
                0.1f,
                FarVegetationPublishInterval
            );
    }
}

void
ACubusWorldVegetationActor::PublishFarVegetation(
    const FVector& CameraLocation,
    const float VoxelSize
)
{
    if (!bEnableFarVegetation)
    {
        return;
    }

    if (
        FarCatalogStaticBatchComponents.IsEmpty()
    )
    {
        static bool
            bLoggedMissingFarStaticBatches =
                false;

        if (
            !bLoggedMissingFarStaticBatches
        )
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT(
                    "Cubus far vegetation cannot publish: "
                    "no static far-tree batches exist. "
                    "CatalogStaticBatchComponents=%d"
                ),
                CatalogStaticBatchComponents.Num()
            );

            bLoggedMissingFarStaticBatches =
                true;
        }

        return;
    }

    for (
        const TPair<
            int64,
            TObjectPtr<
                UHierarchicalInstancedStaticMeshComponent
            >
        >& Pair
        : FarCatalogStaticBatchComponents
    )
    {
        if (IsValid(Pair.Value))
        {
            Pair.Value->ClearInstances();
        }
    }

    const float SafeVoxelSize =
        FMath::Max(
            1.0f,
            VoxelSize
        );

    const double HalfChunkWorldExtent =
        static_cast<double>(
            Cubus::ChunkSize
        ) *
        static_cast<double>(
            SafeVoxelSize
        ) *
        0.5;

    const FCubusVegetationDistancePolicy DistancePolicy =
        ResolveVegetationDistancePolicy(
            SafeVoxelSize
        );

    const double InnerRadiusSquared =
        FMath::Square(
            static_cast<double>(
                DistancePolicy.FarInnerRadius
            )
        );

    const double OuterRadiusSquared =
        FMath::Square(
            static_cast<double>(
                FMath::Max(
                    DistancePolicy.FarInnerRadius,
                    FarVegetationEndCullDistance
                )
            )
        );

    const FCubusVegetationRandomizationSettings
        RandomizationSettings
    {
        bEnableRuntimeRandomization,
        RuntimeRandomizationSeed,
        RandomPruneProbability,
        RandomScaleJitterMin,
        RandomScaleJitterMax,
        RandomPositionJitterVoxelFraction,
        RandomYawJitterDegrees
    };

    TMap<
        int64,
        TArray<FTransform>
    > TransformsByBatchKey;

    TArray<FIntPoint> Cells;

    FarVegetationCellCache.GetKeys(
        Cells
    );

    const int32 CellSizeVoxels =
        FMath::Max(
            1,
            FarVegetationCellSizeChunks
        ) *
        Cubus::ChunkSize;

    Cells.Sort(
        [
            CameraLocation,
            SafeVoxelSize,
            CellSizeVoxels,
            HalfChunkWorldExtent
        ](
            const FIntPoint& A,
            const FIntPoint& B
        )
        {
            const FVector LocationA(
                (
                    static_cast<double>(
                        A.X *
                            CellSizeVoxels
                    ) +
                    static_cast<double>(
                        CellSizeVoxels
                    ) *
                        0.5
                ) *
                    SafeVoxelSize -
                    HalfChunkWorldExtent,
                (
                    static_cast<double>(
                        A.Y *
                            CellSizeVoxels
                    ) +
                    static_cast<double>(
                        CellSizeVoxels
                    ) *
                        0.5
                ) *
                    SafeVoxelSize -
                    HalfChunkWorldExtent,
                CameraLocation.Z
            );

            const FVector LocationB(
                (
                    static_cast<double>(
                        B.X *
                            CellSizeVoxels
                    ) +
                    static_cast<double>(
                        CellSizeVoxels
                    ) *
                        0.5
                ) *
                    SafeVoxelSize -
                    HalfChunkWorldExtent,
                (
                    static_cast<double>(
                        B.Y *
                            CellSizeVoxels
                    ) +
                    static_cast<double>(
                        CellSizeVoxels
                    ) *
                        0.5
                ) *
                    SafeVoxelSize -
                    HalfChunkWorldExtent,
                CameraLocation.Z
            );

            return
                FVector::DistSquared(
                    LocationA,
                    CameraLocation
                ) <
                FVector::DistSquared(
                    LocationB,
                    CameraLocation
                );
        }
    );

    const int32 TreeLimit =
        FMath::Max(
            1000,
            MaximumFarRenderedTrees
        );

    int32 FarTreeCount = 0;

    for (
        const FIntPoint& Cell
        : Cells
    )
    {
        if (FarTreeCount >= TreeLimit)
        {
            break;
        }

        if (
            !RequiredFarVegetationCells.Contains(
                Cell
            )
        )
        {
            continue;
        }

        const TArray<FCubusVegetationInstance>*
            Trees =
                FarVegetationCellCache.Find(
                    Cell
                );

        if (Trees == nullptr)
        {
            continue;
        }

        for (
            const FCubusVegetationInstance&
                Instance
            : *Trees
        )
        {
            if (FarTreeCount >= TreeLimit)
            {
                break;
            }

            const FVector BaseWorldLocation(
                (
                    static_cast<double>(
                        Instance.WorldVoxel.X
                    ) +
                    0.5
                ) *
                    SafeVoxelSize -
                    HalfChunkWorldExtent,
                (
                    static_cast<double>(
                        Instance.WorldVoxel.Y
                    ) +
                    0.5
                ) *
                    SafeVoxelSize -
                    HalfChunkWorldExtent,
                static_cast<double>(
                    Instance.WorldVoxel.Z
                ) *
                    SafeVoxelSize -
                    HalfChunkWorldExtent
            );

            const double DistanceSquared =
                FVector::DistSquared(
                    BaseWorldLocation,
                    CameraLocation
                );

            if (
                DistanceSquared <
                    InnerRadiusSquared ||
                DistanceSquared >
                    OuterRadiusSquared
            )
            {
                continue;
            }

            const float TypeScaleMultiplier =
                ResolveTypeScaleMultiplier(
                    Instance.TypeId,
                    bEnablePerTypeScaleOverrides,
                    BroadleafScaleMultiplier,
                    ConiferScaleMultiplier,
                    ShrubScaleMultiplier,
                    GrassScaleMultiplier,
                    ReedsScaleMultiplier,
                    AlpineScaleMultiplier
                );

            const float CombinedScale =
                FMath::Max(
                    0.01f,
                    Instance.Scale *
                        FMath::Max(
                            0.01f,
                            GlobalPlantScaleMultiplier
                        ) *
                        FMath::Max(
                            0.01f,
                            TypeScaleMultiplier
                        )
                );

            const FCubusResolvedVegetationPlacement
                ResolvedPlacement =
                    VegetationPlacement.Resolve(
                        Instance,
                        BaseWorldLocation,
                        SafeVoxelSize,
                        CombinedScale,
                        RandomizationSettings
                    );

            if (ResolvedPlacement.bPruned)
            {
                continue;
            }

            const int32 SpeciesIndex =
                VegetationCatalog
                    .SelectSpeciesIndex(
                        Instance,
                        SpeciesCatalog,
                        bClusterTreeFamilies,
                        TreeFamilyCellSizeVoxels,
                        RuntimeRandomizationSeed
                    );

            if (SpeciesIndex == INDEX_NONE)
            {
                continue;
            }

            const FCubusVegetationSpeciesCatalogEntry&
                Entry =
                    SpeciesCatalog[
                        SpeciesIndex
                    ];

            const int32 StageCount =
                Entry.GrowthStageMeshes.Num();

            if (StageCount <= 0)
            {
                continue;
            }

            const int32 GrowthStage =
                VegetationCatalog
                    .ResolveGrowthStageIndex(
                        Instance,
                        StageCount,
                        bClusterTreeFamilies,
                        TreeFamilyCellSizeVoxels,
                        TreeFamilyCenterJitterFraction,
                        MatureTreeCoreRadius,
                        YoungTreeRingRadius,
                        SaplingTreeRingRadius,
                        TreeFamilyGrowthNoise,
                        RuntimeRandomizationSeed
                    );

            const int64 PrimaryBatchKey =
                FCubusVegetationRenderer::
                    MakePrimaryBatchKey(
                        SpeciesIndex,
                        GrowthStage
                    );

            const int64 StaticFallbackBatchKey =
                FCubusVegetationRenderer::
                    MakeStaticFallbackBatchKey(
                        SpeciesIndex,
                        GrowthStage
                    );

            int64 TargetBatchKey =
                PrimaryBatchKey;

            if (
                !FarCatalogStaticBatchComponents
                    .Contains(
                        TargetBatchKey
                    )
            )
            {
                TargetBatchKey =
                    StaticFallbackBatchKey;
            }

            if (
                !FarCatalogStaticBatchComponents
                    .Contains(
                        TargetBatchKey
                    )
            )
            {
                continue;
            }

            const FTransform WorldTransform(
                FRotator(
                    0.0f,
                    ResolvedPlacement.Yaw,
                    0.0f
                ),
                ResolvedPlacement.Location,
                FVector(
                    ResolvedPlacement.Scale
                )
            );

            TransformsByBatchKey
                .FindOrAdd(
                    TargetBatchKey
                )
                .Add(
                    WorldTransform
                        .GetRelativeTransform(
                            GetActorTransform()
                        )
                );

            ++FarTreeCount;
        }
    }

    for (
        const TPair<
            int64,
            TObjectPtr<
                UHierarchicalInstancedStaticMeshComponent
            >
        >& Pair
        : FarCatalogStaticBatchComponents
    )
    {
        UHierarchicalInstancedStaticMeshComponent*
            Component =
                Pair.Value;

        if (!IsValid(Component))
        {
            continue;
        }

        const TArray<FTransform>* Transforms =
            TransformsByBatchKey.Find(
                Pair.Key
            );

        if (
            Transforms == nullptr ||
            Transforms->IsEmpty()
        )
        {
            continue;
        }

        Component->AddInstances(
            *Transforms,
            false,
            false,
            false
        );

        // HISM instance storage can be populated while the component's cached
        // scene bounds remain stale. A zero-sized Bounds causes primitive
        // frustum/distance culling to reject the entire far forest before
        // per-instance HISM culling is even considered. Force the cluster tree
        // and primitive bounds to match the newly published instances.
        Component->BuildTreeIfOutdated(
            false,
            true
        );
        Component->UpdateBounds();
        Component->MarkRenderTransformDirty();
        Component->MarkRenderStateDirty();
    }

    int32 PublishedFarInstanceCount = 0;

    for (
        const TPair<
            int64,
            TObjectPtr<
                UHierarchicalInstancedStaticMeshComponent
            >
        >& Pair
        : FarCatalogStaticBatchComponents
    )
    {
        if (IsValid(Pair.Value))
        {
            PublishedFarInstanceCount +=
                Pair.Value->GetInstanceCount();
        }
    }

    RenderedFarTreeCount =
        PublishedFarInstanceCount;

    bFarVegetationRenderDirty =
        false;

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "Cubus far vegetation: cells=%d "
            "trees=%d pending=%d building=%d voxel=%.2fcm pass=%d"
        ),
        FarVegetationCellCache.Num(),
        RenderedFarTreeCount,
        PendingFarVegetationCells.Num(),
        FarVegetationBuilds.Num(),
        SafeVoxelSize,
        FarVegetationRefinementPass
    );
}

void
ACubusWorldVegetationActor::ClearFarVegetation()
{
    for (
        const TPair<
            int64,
            TObjectPtr<
                UHierarchicalInstancedStaticMeshComponent
            >
        >& Pair
        : FarCatalogStaticBatchComponents
    )
    {
        if (!IsValid(Pair.Value))
        {
            continue;
        }

        Pair.Value->ClearInstances();
        Pair.Value->DestroyComponent();
    }

    FarCatalogStaticBatchComponents.Reset();

    RequiredFarVegetationCells.Reset();
    FarVegetationCellsBuilding.Reset();
    PendingFarVegetationCells.Reset();
    FarVegetationBuilds.Reset();
    FarVegetationCellCache.Reset();
    FarVegetationCellRefinementPasses.Reset();
    FarVegetationRefinementPass = 0;

    LastFarVegetationCentreCell =
        FIntPoint(
            MAX_int32,
            MAX_int32
        );

    LoadedFarVegetationCellCount = 0;
    RenderedFarTreeCount = 0;

    TimeUntilFarVegetationPublish =
        0.0f;

    bFarVegetationRenderDirty =
        false;
}