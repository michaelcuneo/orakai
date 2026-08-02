#include "CubusCore/Actors/CubusWorldVegetationActor.h"
#include "CubusCore/Vegetation/CubusVegetationAssetResolver.h"
#include "CubusCore/Vegetation/CubusVegetationWindUtilities.h"
#include "CubusCore/Vegetation/CubusVegetationChunkFilter.h"
#include "CubusCore/Vegetation/CubusVegetationRepresentationSelector.h"

#include "CubusCore/Actors/CubusBlockWorldActor.h"
#include "CubusCore/Actors/CubusVoxelVolumeActor.h"
#include "CubusCore/Chunks/CubusBlockChunkData.h"
#include "CubusCore/Chunks/CubusChunkConstants.h"
#include "CubusCore/Data/CubusVegetationInstance.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/InstancedSkinnedMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SceneComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Animation/TransformProviderData.h"
#include "DynamicWindData.h"
#include "DynamicWindSkeletalData.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Materials/MaterialInterface.h"
#include "UObject/SoftObjectPtr.h"

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

    uint32 CalculateChunkVegetationSignature(
        const FCubusBlockChunkData& ChunkData
    )
    {
        const auto Instances = ChunkData.GetVegetationInstances();
        uint32 Hash = GetTypeHash(Instances.Num());

        if (!Instances.IsEmpty())
        {
            const FCubusVegetationInstance& FirstInstance = Instances[0];
            const FCubusVegetationInstance& LastInstance =
                Instances[Instances.Num() - 1];

            Hash = HashCombineFast(Hash, GetTypeHash(FirstInstance.WorldVoxel));
            Hash = HashCombineFast(Hash, GetTypeHash(FirstInstance.TypeId));
            Hash = HashCombineFast(Hash, GetTypeHash(LastInstance.WorldVoxel));
            Hash = HashCombineFast(Hash, GetTypeHash(LastInstance.TypeId));
        }

        return Hash;
    }
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
    TimeUntilRefresh = 0.0f;
}

void ACubusWorldVegetationActor::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    UpdateDynamicWindBridge();

    TimeUntilRefresh -= DeltaSeconds;

    if (TimeUntilRefresh > 0.0f)
    {
        return;
    }

    TimeUntilRefresh = FMath::Max(0.1f, RefreshInterval);
    ResolveBlockWorld();

    VegetationRenderer.ApplyShadowSettings(
        bCastWorldPlantShadows,
        CatalogStaticBatchComponents,
        CatalogSkeletalBatchComponents,
        HeroSkeletalWindComponents
    );

    int32 CurrentLoadedChunkCount = 0;
    const uint32 CurrentHash =
        CalculateLoadedPlacementHash(CurrentLoadedChunkCount);
    const uint32 CurrentSettingsHash =
        CalculateVegetationSettingsHash();

    if (
        CurrentHash != static_cast<uint32>(PublishedPlacementHash) ||
        CurrentLoadedChunkCount != LoadedChunkCount ||
        CurrentSettingsHash != PublishedVegetationSettingsHash
    )
    {
        RebuildWorldVegetation();
    }
}

void ACubusWorldVegetationActor::EndPlay(
    const EEndPlayReason::Type EndPlayReason
)
{
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
            CatalogStaticBatchComponents.IsEmpty() &&
            CatalogSkeletalBatchComponents.IsEmpty()
        )
    )
    {
        RefreshVegetationBatches();
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

    const bool bUseCameraChunkCulling =
        bCullByCameraChunkRadius &&
        bHasCamera;

    const FVector CameraLocation = bHasCamera
        ? PlayerController->PlayerCameraManager->GetCameraLocation()
        : FVector::ZeroVector;

    TMap<FIntVector, uint32> CurrentChunkVegetationSignatures;

    for (
        TActorIterator<ACubusVoxelVolumeActor> Iterator(World);
        Iterator;
        ++Iterator
    )
    {
        const ACubusVoxelVolumeActor* Chunk = *Iterator;

        if (
            !IsValid(Chunk) ||
            Chunk->GetOwner() != BlockWorld ||
            !FCubusVegetationChunkFilter::IsWithinCameraRadius(
                Chunk,
                CameraLocation,
                bUseCameraChunkCulling,
                CameraChunkHorizontalRadius,
                CameraChunkVerticalRadius
            )
        )
        {
            continue;
        }

        const FCubusBlockChunkData* ChunkData = Chunk->GetChunkData();

        if (ChunkData == nullptr)
        {
            continue;
        }

        CurrentChunkVegetationSignatures.Add(
            Chunk->GetChunkCoordinate(),
            CalculateChunkVegetationSignature(*ChunkData)
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

    if (!bAppendOnly)
    {
        ClearWorldVegetation();
    }

    if (bEnableHeroSkeletalWindMode)
    {
        bAppendOnly = false;
    }

    TArray<FCubusVegetationRepresentationCandidate>
        HeroTreeCandidates;
    TMap<int64, TArray<FTransform>> CatalogTransformsByBatchKey;
    TMap<int64, TArray<FTransform>> HeroTransformsByBatchKey;
    int32 StaticBatchTransformCount = 0;
    int32 SkeletalBatchTransformCount = 0;

    if (bAppendOnly)
    {
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
    int32 InstancedSkeletalFallbackCount = 0;
    int32 FoliageMaterialOverrideComponentCount = 0;
    int32 BoundSpeciesTransformProviderCount = 0;

    int32 RandomizedPlantCount = 0;
    int32 RandomPrunedPlantCount = 0;
    float ObservedRandomScaleMin = MAX_flt;
    float ObservedRandomScaleMax = 0.0f;

    for (
        TActorIterator<ACubusVoxelVolumeActor> Iterator(World);
        Iterator;
        ++Iterator
    )
    {
        ACubusVoxelVolumeActor* Chunk = *Iterator;

        if (
            !IsValid(Chunk) ||
            Chunk->GetOwner() != BlockWorld
        )
        {
            continue;
        }

        const FCubusBlockChunkData* ChunkData = Chunk->GetChunkData();

        if (ChunkData == nullptr)
        {
            continue;
        }

        const FIntVector ChunkCoordinate = Chunk->GetChunkCoordinate();

        if (
            !FCubusVegetationChunkFilter::IsWithinCameraRadius(
                Chunk,
                CameraLocation,
                bUseCameraChunkCulling,
                CameraChunkHorizontalRadius,
                CameraChunkVerticalRadius
            ) ||
            (
                bAppendOnly &&
                PublishedChunkVegetationSignatures.Contains(ChunkCoordinate)
            )
        )
        {
            continue;
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

            const FVector FinalLocation =
                ResolvedPlacement.Location;

            const float FinalScale =
                ResolvedPlacement.Scale;

            const float FinalYaw =
                ResolvedPlacement.Yaw;

            const FTransform WorldTransform(
                FRotator(0.0f, FinalYaw, 0.0f),
                FinalLocation,
                FVector(FinalScale)
            );

            if (
                !bRenderWorldPlantBatches ||
                RenderedPlantCount >= PlantLimit
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

            const bool bTreeType =
                Instance.TypeId == WorldBroadleafType ||
                Instance.TypeId == WorldConiferType;

            int64 TargetBatchKey = PrimaryBatchKey;

            /*
            * Skeletal trees outside the hero animation radius use their matching
            * static HISM representation instead.
            */
            if (
                bPrimaryBatchIsSkeletal &&
                bTreeType &&
                bHasCamera
            )
            {
                const int64 StaticFallbackBatchKey =
                    FCubusVegetationRenderer::MakeStaticFallbackBatchKey(
                        SpeciesIndex,
                        GrowthStage
                    );

                FCubusVegetationRepresentationCandidate Candidate;
                Candidate.PrimaryBatchKey = PrimaryBatchKey;
                Candidate.StaticFallbackBatchKey =
                    StaticFallbackBatchKey;
                Candidate.LocalTransform = LocalTransform;
                Candidate.DistanceSquared =
                    FVector::DistSquared(
                        FinalLocation,
                        CameraLocation
                    );
                Candidate.bHasStaticFallback =
                    CatalogStaticBatchComponents.Contains(
                        StaticFallbackBatchKey
                    );

                HeroTreeCandidates.Add(
                    MoveTemp(Candidate)
                );

                ++RenderedPlantCount;
                continue;
            }

            if (
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

            CatalogTransformsByBatchKey
                .FindOrAdd(TargetBatchKey)
                .Add(LocalTransform);

            ++RenderedPlantCount;
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

    int32 SignatureLoadedChunkCount = 0;
    const uint32 SignatureHash =
        CalculateLoadedPlacementHash(SignatureLoadedChunkCount);

    LoadedChunkCount = SignatureLoadedChunkCount;
    PublishedPlacementHash = static_cast<int64>(SignatureHash);
    PublishedVegetationSettingsHash = CurrentVegetationSettingsHash;
    PublishedChunkVegetationSignatures =
        MoveTemp(CurrentChunkVegetationSignatures);

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "Cubus world vegetation: %d chunks, %d world-batched plants (static=%d, skeletal-instanced=%d, hero=%d, heroPveActors=%d, fallback=%d, foliage-overrides=%d, heroPve=%s, fallbackInstanced=%s)"
        ),
        LoadedChunkCount,
        RenderedPlantCount,
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

    VegetationRenderer.ClearBatches(
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
            const TSoftObjectPtr<UStaticMesh>& MeshReference :
            Entry.StaticGrowthStageMeshes
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

    uint32 Hash = 0;

    const APlayerController* PlayerController =
        UGameplayStatics::GetPlayerController(this, 0);

    const bool bHasCamera =
        IsValid(PlayerController) &&
        IsValid(PlayerController->PlayerCameraManager);

    const bool bUseCameraChunkCulling =
        bCullByCameraChunkRadius &&
        bHasCamera;

    const FVector CameraLocation = bHasCamera
        ? PlayerController->PlayerCameraManager->GetCameraLocation()
        : FVector::ZeroVector;

    for (
        TActorIterator<ACubusVoxelVolumeActor> Iterator(World);
        Iterator;
        ++Iterator
    )
    {
        const ACubusVoxelVolumeActor* Chunk = *Iterator;

        if (
            !IsValid(Chunk) ||
            Chunk->GetOwner() != BlockWorld
        )
        {
            continue;
        }

        const FCubusBlockChunkData* ChunkData = Chunk->GetChunkData();

        if (ChunkData == nullptr)
        {
            continue;
        }

        if (
            !FCubusVegetationChunkFilter::IsWithinCameraRadius(
                Chunk,
                CameraLocation,
                bUseCameraChunkCulling,
                CameraChunkHorizontalRadius,
                CameraChunkVerticalRadius
            )
        )
        {
            continue;
        }

        ++OutLoadedChunkCount;

        Hash = HashCombineFast(
            Hash,
            GetTypeHash(Chunk->GetChunkCoordinate())
        );
        Hash = HashCombineFast(
            Hash,
            GetTypeHash(ChunkData->GetVegetationInstances().Num())
        );

        const auto Instances =
            ChunkData->GetVegetationInstances();

        if (!Instances.IsEmpty())
        {
            const FCubusVegetationInstance& FirstInstance = Instances[0];
            const FCubusVegetationInstance& LastInstance =
                Instances[Instances.Num() - 1];

            Hash = HashCombineFast(Hash, GetTypeHash(FirstInstance.WorldVoxel));
            Hash = HashCombineFast(Hash, GetTypeHash(FirstInstance.TypeId));
            Hash = HashCombineFast(Hash, GetTypeHash(LastInstance.WorldVoxel));
            Hash = HashCombineFast(Hash, GetTypeHash(LastInstance.TypeId));
        }
    }

    return Hash;
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
        CatalogStaticBatchComponents,
        CatalogSkeletalBatchComponents
    );
}