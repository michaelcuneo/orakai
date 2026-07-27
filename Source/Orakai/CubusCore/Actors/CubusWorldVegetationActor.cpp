#include "CubusCore/Actors/CubusWorldVegetationActor.h"

#include "CubusCore/Actors/CubusBlockWorldActor.h"
#include "CubusCore/Actors/CubusVoxelVolumeActor.h"
#include "CubusCore/Chunks/CubusBlockChunkData.h"
#include "CubusCore/Chunks/CubusChunkConstants.h"
#include "CubusCore/Data/CubusVegetationInstance.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/InstancedSkinnedMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
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
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "UObject/Class.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UnrealType.h"


namespace
{
    constexpr int32 GrassType = 1;
    constexpr int32 ShrubType = 2;
    constexpr int32 BroadleafType = 3;
    constexpr int32 ReedsType = 4;
    constexpr int32 AlpineType = 5;
    constexpr int32 ConiferType = 6;

    FName NormalizeMegaplantSpeciesId(const FName SpeciesId);

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
            case BroadleafType:
                return BroadleafScaleMultiplier;
            case ConiferType:
                return ConiferScaleMultiplier;
            case ShrubType:
                return ShrubScaleMultiplier;
            case GrassType:
                return GrassScaleMultiplier;
            case ReedsType:
                return ReedsScaleMultiplier;
            case AlpineType:
                return AlpineScaleMultiplier;
            default:
                return 1.0f;
        }
    }

    float HashToUnitFloat(const uint32 Hash)
    {
        return static_cast<float>(Hash & 0x00ffffffu) /
            static_cast<float>(0x01000000u);
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

    bool LooksLikeWindDirectionParameterName(const FString& Name)
    {
        if (!Name.Contains(TEXT("Wind"), ESearchCase::IgnoreCase) &&
            !Name.Contains(TEXT("Breeze"), ESearchCase::IgnoreCase) &&
            !Name.Contains(TEXT("Gust"), ESearchCase::IgnoreCase) &&
            !Name.Contains(TEXT("Sway"), ESearchCase::IgnoreCase))
        {
            return false;
        }

        return
            Name.Contains(TEXT("Direction"), ESearchCase::IgnoreCase) ||
            Name.Contains(TEXT("Dir"), ESearchCase::IgnoreCase) ||
            Name.Equals(TEXT("Wind"), ESearchCase::IgnoreCase) ||
            Name.Contains(TEXT("Vector"), ESearchCase::IgnoreCase) ||
            Name.Contains(TEXT("Main"), ESearchCase::IgnoreCase);
    }

    bool LooksLikeWindScalarParameterName(const FString& Name)
    {
        if (!Name.Contains(TEXT("Wind"), ESearchCase::IgnoreCase) &&
            !Name.Contains(TEXT("Breeze"), ESearchCase::IgnoreCase) &&
            !Name.Contains(TEXT("Gust"), ESearchCase::IgnoreCase) &&
            !Name.Contains(TEXT("Sway"), ESearchCase::IgnoreCase))
        {
            return false;
        }

        return
            Name.Contains(TEXT("Intensity"), ESearchCase::IgnoreCase) ||
            Name.Contains(TEXT("Strength"), ESearchCase::IgnoreCase) ||
            Name.Contains(TEXT("Speed"), ESearchCase::IgnoreCase) ||
            Name.Contains(TEXT("Scale"), ESearchCase::IgnoreCase) ||
            Name.Contains(TEXT("Sway"), ESearchCase::IgnoreCase) ||
            Name.Contains(TEXT("Amplitude"), ESearchCase::IgnoreCase) ||
            Name.Contains(TEXT("Amount"), ESearchCase::IgnoreCase) ||
            Name.Contains(TEXT("Influence"), ESearchCase::IgnoreCase) ||
            Name.Contains(TEXT("Weight"), ESearchCase::IgnoreCase) ||
            Name.Contains(TEXT("Alpha"), ESearchCase::IgnoreCase) ||
            Name.Contains(TEXT("Power"), ESearchCase::IgnoreCase) ||
            Name.Equals(TEXT("WindIntensity"), ESearchCase::IgnoreCase);
    }

    float ResolveMaterialWindScalarValue(
        const FString& ParameterName,
        const float WindIntensityRaw,
        const float WindIntensityNormalized
    )
    {
        if (ParameterName.Contains(TEXT("Speed"), ESearchCase::IgnoreCase) ||
            ParameterName.Contains(TEXT("Frequency"), ESearchCase::IgnoreCase) ||
            ParameterName.Contains(TEXT("Freq"), ESearchCase::IgnoreCase))
        {
            return WindIntensityRaw;
        }

        if (ParameterName.Contains(TEXT("Intensity"), ESearchCase::IgnoreCase) ||
            ParameterName.Contains(TEXT("Strength"), ESearchCase::IgnoreCase) ||
            ParameterName.Contains(TEXT("Scale"), ESearchCase::IgnoreCase) ||
            ParameterName.Contains(TEXT("Amount"), ESearchCase::IgnoreCase) ||
            ParameterName.Contains(TEXT("Influence"), ESearchCase::IgnoreCase) ||
            ParameterName.Contains(TEXT("Weight"), ESearchCase::IgnoreCase) ||
            ParameterName.Contains(TEXT("Alpha"), ESearchCase::IgnoreCase) ||
            ParameterName.Contains(TEXT("Blend"), ESearchCase::IgnoreCase))
        {
            return WindIntensityNormalized;
        }

        return WindIntensityRaw;
    }

    int32 ApplyWindToMaterialInstanceDynamic(
        UMaterialInstanceDynamic* MaterialInstance,
        const FLinearColor& WindDirectionColor,
        const float WindIntensity
    )
    {
        if (!IsValid(MaterialInstance))
        {
            return 0;
        }

        int32 AppliedValueCount = 0;

        const float WindIntensityRaw = FMath::Max(0.0f, WindIntensity);
        const float WindIntensityNormalized = FMath::Clamp(
            WindIntensityRaw / 10.0f,
            0.0f,
            1.0f
        );

        const FLinearColor WindVectorWithStrength(
            WindDirectionColor.R,
            WindDirectionColor.G,
            WindDirectionColor.B,
            WindIntensityRaw
        );

        TArray<FMaterialParameterInfo> VectorParameterInfo;
        TArray<FGuid> VectorParameterIds;
        MaterialInstance->GetAllVectorParameterInfo(
            VectorParameterInfo,
            VectorParameterIds
        );

        for (const FMaterialParameterInfo& ParameterInfo : VectorParameterInfo)
        {
            const FString ParameterName = ParameterInfo.Name.ToString();

            const bool bLooksLikeDirection =
                LooksLikeWindDirectionParameterName(ParameterName);

            const bool bLooksLikeGenericWindVector =
                (ParameterName.Contains(TEXT("Wind"), ESearchCase::IgnoreCase) ||
                 ParameterName.Contains(TEXT("Breeze"), ESearchCase::IgnoreCase) ||
                 ParameterName.Contains(TEXT("Gust"), ESearchCase::IgnoreCase) ||
                 ParameterName.Contains(TEXT("Sway"), ESearchCase::IgnoreCase)) &&
                !ParameterName.Contains(TEXT("Mask"), ESearchCase::IgnoreCase);

            if (!bLooksLikeDirection && !bLooksLikeGenericWindVector)
            {
                continue;
            }

            MaterialInstance->SetVectorParameterValue(
                ParameterInfo.Name,
                bLooksLikeDirection
                    ? WindDirectionColor
                    : WindVectorWithStrength
            );

            ++AppliedValueCount;
        }

        TArray<FMaterialParameterInfo> ScalarParameterInfo;
        TArray<FGuid> ScalarParameterIds;
        MaterialInstance->GetAllScalarParameterInfo(
            ScalarParameterInfo,
            ScalarParameterIds
        );

        for (const FMaterialParameterInfo& ParameterInfo : ScalarParameterInfo)
        {
            const FString ParameterName = ParameterInfo.Name.ToString();

            if (!LooksLikeWindScalarParameterName(ParameterName))
            {
                continue;
            }

            MaterialInstance->SetScalarParameterValue(
                ParameterInfo.Name,
                ResolveMaterialWindScalarValue(
                    ParameterName,
                    WindIntensityRaw,
                    WindIntensityNormalized
                )
            );

            ++AppliedValueCount;
        }

        return AppliedValueCount;
    }

    bool TryReadFloatProperty(
        const UObject* Source,
        const FName PropertyName,
        float& OutValue
    )
    {
        if (!IsValid(Source))
        {
            return false;
        }

        const FProperty* Property =
            Source->GetClass()->FindPropertyByName(PropertyName);

        if (const FFloatProperty* FloatProperty = CastField<FFloatProperty>(Property))
        {
            OutValue = FloatProperty->GetPropertyValue_InContainer(Source);
            return true;
        }

        if (const FDoubleProperty* DoubleProperty = CastField<FDoubleProperty>(Property))
        {
            OutValue = static_cast<float>(
                DoubleProperty->GetPropertyValue_InContainer(Source)
            );
            return true;
        }

        return false;
    }

    bool TryWriteFloatProperty(
        UObject* Target,
        const FName PropertyName,
        const float Value
    )
    {
        if (!IsValid(Target))
        {
            return false;
        }

        FProperty* Property =
            Target->GetClass()->FindPropertyByName(PropertyName);

        if (FFloatProperty* FloatProperty = CastField<FFloatProperty>(Property))
        {
            FloatProperty->SetPropertyValue_InContainer(Target, Value);
            return true;
        }

        if (FDoubleProperty* DoubleProperty = CastField<FDoubleProperty>(Property))
        {
            DoubleProperty->SetPropertyValue_InContainer(
                Target,
                static_cast<double>(Value)
            );
            return true;
        }

        return false;
    }

    bool TryReadVectorLikeProperty(
        const UObject* Source,
        const FName PropertyName,
        FVector& OutValue
    )
    {
        if (!IsValid(Source))
        {
            return false;
        }

        const FProperty* Property =
            Source->GetClass()->FindPropertyByName(PropertyName);

        const FStructProperty* StructProperty =
            CastField<FStructProperty>(Property);

        if (StructProperty == nullptr)
        {
            return false;
        }

        if (StructProperty->Struct == TBaseStructure<FVector>::Get())
        {
            const FVector* Value =
                StructProperty->ContainerPtrToValuePtr<FVector>(Source);

            if (Value == nullptr)
            {
                return false;
            }

            OutValue = *Value;
            return true;
        }

        if (StructProperty->Struct == TBaseStructure<FRotator>::Get())
        {
            const FRotator* Value =
                StructProperty->ContainerPtrToValuePtr<FRotator>(Source);

            if (Value == nullptr)
            {
                return false;
            }

            OutValue = Value->Vector();
            return true;
        }

        return false;
    }

    bool TryWriteBoolProperty(
        UObject* Target,
        const FName PropertyName,
        const bool Value
    )
    {
        if (!IsValid(Target))
        {
            return false;
        }

        FProperty* Property =
            Target->GetClass()->FindPropertyByName(PropertyName);

        if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
        {
            BoolProperty->SetPropertyValue_InContainer(Target, Value);
            return true;
        }

        return false;
    }

    bool TryWriteVectorLikeProperty(
        UObject* Target,
        const FName PropertyName,
        const FVector& Value
    )
    {
        if (!IsValid(Target))
        {
            return false;
        }

        FProperty* Property =
            Target->GetClass()->FindPropertyByName(PropertyName);

        FStructProperty* StructProperty =
            CastField<FStructProperty>(Property);

        if (StructProperty == nullptr)
        {
            return false;
        }

        if (StructProperty->Struct == TBaseStructure<FVector>::Get())
        {
            FVector* Dest =
                StructProperty->ContainerPtrToValuePtr<FVector>(Target);

            if (Dest == nullptr)
            {
                return false;
            }

            *Dest = Value;
            return true;
        }

        if (StructProperty->Struct == TBaseStructure<FRotator>::Get())
        {
            FRotator* Dest =
                StructProperty->ContainerPtrToValuePtr<FRotator>(Target);

            if (Dest == nullptr)
            {
                return false;
            }

            *Dest = Value.Rotation();
            return true;
        }

        return false;
    }

    AActor* ResolveUltraDynamicWeatherActor(UWorld* World)
    {
        if (!IsValid(World))
        {
            return nullptr;
        }

        for (TActorIterator<AActor> Iterator(World); Iterator; ++Iterator)
        {
            AActor* Candidate = *Iterator;

            if (!IsValid(Candidate))
            {
                continue;
            }

            const FString Name = Candidate->GetName();
            const FString ClassName = Candidate->GetClass()->GetName();

            if (
                Name.Contains(TEXT("Ultra_Dynamic_Weather")) ||
                Name.Contains(TEXT("UltraDynamicWeather")) ||
                ClassName.Contains(TEXT("Ultra_Dynamic_Weather")) ||
                ClassName.Contains(TEXT("UltraDynamicWeather")) ||
                Candidate->ActorHasTag(TEXT("UltraDynamicWeather"))
            )
            {
                return Candidate;
            }
        }

        return nullptr;
    }

    TSoftObjectPtr<UMaterialInterface> ResolveMegaplantFoliageMaterialBySpecies(
        const FName SpeciesId
    )
    {
        if (SpeciesId == TEXT("Elder"))
        {
            return TSoftObjectPtr<UMaterialInterface>(
                FSoftObjectPath(
                    TEXT("/Game/Megaplant_Library/Tree_Elder/Materials/MI_Elder_01_Foliage.MI_Elder_01_Foliage")
                )
            );
        }

        if (SpeciesId == TEXT("NorwaySpruce"))
        {
            return TSoftObjectPtr<UMaterialInterface>(
                FSoftObjectPath(
                    TEXT("/Game/Megaplant_Library/Tree_Norway_Spruce/Materials/MI_Norway_Spruce_Foliage_01.MI_Norway_Spruce_Foliage_01")
                )
            );
        }

        if (SpeciesId == TEXT("Greasewood"))
        {
            return TSoftObjectPtr<UMaterialInterface>(
                FSoftObjectPath(
                    TEXT("/Game/Megaplant_Library/Shrub_Greasewood/Materials/MI_Greasewood_01_Foliage.MI_Greasewood_01_Foliage")
                )
            );
        }

        return TSoftObjectPtr<UMaterialInterface>();
    }

    UTransformProviderData* ResolveMegaplantTransformProviderBySpecies(
        const FName SpeciesId
    )
    {
        const FName NormalizedSpeciesId =
            NormalizeMegaplantSpeciesId(SpeciesId);

        static TMap<FName, TWeakObjectPtr<UTransformProviderData>> CachedProviders;
        static TSet<FName> CachedMissingProviders;

        if (const TWeakObjectPtr<UTransformProviderData>* CachedProvider =
                CachedProviders.Find(NormalizedSpeciesId))
        {
            return CachedProvider->Get();
        }

        if (CachedMissingProviders.Contains(NormalizedSpeciesId))
        {
            return nullptr;
        }

        TArray<FString> CandidateProviderPaths;

        if (NormalizedSpeciesId == TEXT("NorwaySpruce"))
        {
            CandidateProviderPaths.Add(
                TEXT("/Game/Megaplant_Library/Tree_Norway_Spruce/Tree_Norway_Spruce_01/PVE_Norway_Spruce_01_Data.PVE_Norway_Spruce_01_Data")
            );
            CandidateProviderPaths.Add(
                TEXT("/Game/Megaplant_Library/Tree_Norway_Spruce/Tree_Norway_Spruce_01/PVE_Norway_Spruce_01.PVE_Norway_Spruce_01")
            );
        }
        else if (NormalizedSpeciesId == TEXT("YoshinoCherry"))
        {
            CandidateProviderPaths.Add(
                TEXT("/Game/Megaplant_Library/Tree_Yoshino_Cherry/Tree_Yoshino_Cherry_01/PVE_Yoshino_Cherry_01_Data.PVE_Yoshino_Cherry_01_Data")
            );
            CandidateProviderPaths.Add(
                TEXT("/Game/Megaplant_Library/Tree_Yoshino_Cherry/Tree_Yoshino_Cherry_01/PVE_Yoshino_Cherry_01.PVE_Yoshino_Cherry_01")
            );
        }
        else if (NormalizedSpeciesId == TEXT("Greasewood"))
        {
            CandidateProviderPaths.Add(
                TEXT("/Game/Megaplant_Library/Shrub_Greasewood/Shrub_Greasewood_01/PVE_Greasewood_01_Data.PVE_Greasewood_01_Data")
            );
            CandidateProviderPaths.Add(
                TEXT("/Game/Megaplant_Library/Shrub_Greasewood/Shrub_Greasewood_01/PVE_Greasewood_01.PVE_Greasewood_01")
            );
        }
        else if (NormalizedSpeciesId == TEXT("Elder"))
        {
            CandidateProviderPaths.Add(
                TEXT("/Game/Megaplant_Library/Tree_Elder/Tree_Elder_01/PVE_Elder_01_Data.PVE_Elder_01_Data")
            );
            CandidateProviderPaths.Add(
                TEXT("/Game/Megaplant_Library/Tree_Elder/Tree_Elder_01/Tree_Elder_01.Tree_Elder_01")
            );
        }

        for (const FString& ProviderPath : CandidateProviderPaths)
        {
            UObject* LoadedObject = StaticLoadObject(
                UObject::StaticClass(),
                nullptr,
                *ProviderPath,
                nullptr,
                LOAD_NoWarn
            );

            UTransformProviderData* Provider =
                Cast<UTransformProviderData>(LoadedObject);

            if (IsValid(Provider))
            {
                CachedProviders.Add(NormalizedSpeciesId, Provider);
                return Provider;
            }
        }

        CachedMissingProviders.Add(NormalizedSpeciesId);
        return nullptr;
    }

    FName NormalizeMegaplantSpeciesId(const FName SpeciesId)
    {
        const FString SpeciesName = SpeciesId.ToString();

        if (SpeciesName.Contains(TEXT("Elder"), ESearchCase::IgnoreCase))
        {
            return TEXT("Elder");
        }

        if (
            SpeciesName.Contains(TEXT("Norway"), ESearchCase::IgnoreCase) &&
            SpeciesName.Contains(TEXT("Spruce"), ESearchCase::IgnoreCase)
        )
        {
            return TEXT("NorwaySpruce");
        }

        if (SpeciesName.Contains(TEXT("Spruce"), ESearchCase::IgnoreCase))
        {
            return TEXT("NorwaySpruce");
        }

        if (SpeciesName.Contains(TEXT("Yoshino"), ESearchCase::IgnoreCase))
        {
            return TEXT("YoshinoCherry");
        }

        if (SpeciesName.Contains(TEXT("Greasewood"), ESearchCase::IgnoreCase))
        {
            return TEXT("Greasewood");
        }

        return SpeciesId;
    }

    void AppendMegaplantSearchRootsBySpecies(
        const FName NormalizedSpeciesId,
        TArray<FName>& OutSearchRoots
    )
    {
        OutSearchRoots.AddUnique(TEXT("/Game/Megaplant_Library"));

        if (NormalizedSpeciesId == TEXT("Elder"))
        {
            OutSearchRoots.AddUnique(TEXT("/Game/Megaplant_Library/Tree_Elder"));
            OutSearchRoots.AddUnique(TEXT("/Game/Megaplant_Library/Tree_Elder/Tree_Elder_01"));
            return;
        }

        if (NormalizedSpeciesId == TEXT("NorwaySpruce"))
        {
            OutSearchRoots.AddUnique(TEXT("/Game/Megaplant_Library/Tree_Norway_Spruce"));
            OutSearchRoots.AddUnique(TEXT("/Game/Megaplant_Library/Tree_Norway_Spruce/Tree_Norway_Spruce_01"));
            return;
        }

        if (NormalizedSpeciesId == TEXT("YoshinoCherry"))
        {
            OutSearchRoots.AddUnique(TEXT("/Game/Megaplant_Library/Tree_Yoshino_Cherry"));
            OutSearchRoots.AddUnique(TEXT("/Game/Megaplant_Library/Tree_Yoshino_Cherry/Tree_Yoshino_Cherry_01"));
            return;
        }

        if (NormalizedSpeciesId == TEXT("Greasewood"))
        {
            OutSearchRoots.AddUnique(TEXT("/Game/Megaplant_Library/Shrub_Greasewood"));
            OutSearchRoots.AddUnique(TEXT("/Game/Megaplant_Library/Shrub_Greasewood/Shrub_Greasewood_01"));
            return;
        }
    }

    void AppendExplicitPveCandidateObjectPathsBySpecies(
        const FName NormalizedSpeciesId,
        TArray<FString>& OutCandidateObjectPaths
    )
    {
        if (NormalizedSpeciesId == TEXT("Elder"))
        {
            OutCandidateObjectPaths.AddUnique(
                TEXT("/Game/Megaplant_Library/Tree_Elder/Tree_Elder_01/Tree_Elder_01.Tree_Elder_01")
            );
            return;
        }

        if (NormalizedSpeciesId == TEXT("NorwaySpruce"))
        {
            OutCandidateObjectPaths.AddUnique(
                TEXT("/Game/Megaplant_Library/Tree_Norway_Spruce/Tree_Norway_Spruce_01/PVE_Norway_Spruce_01.PVE_Norway_Spruce_01")
            );
            OutCandidateObjectPaths.AddUnique(
                TEXT("/Game/Megaplant_Library/Tree_Norway_Spruce/Tree_Norway_Spruce_01/PVE_Norway_Spruce_01.PVE_Norway_Spruce_01_C")
            );
            OutCandidateObjectPaths.AddUnique(
                TEXT("/Game/Megaplant_Library/Tree_Norway_Spruce/Tree_Norway_Spruce_01/Tree_Norway_Spruce_01.Tree_Norway_Spruce_01")
            );
            return;
        }

        if (NormalizedSpeciesId == TEXT("YoshinoCherry"))
        {
            OutCandidateObjectPaths.AddUnique(
                TEXT("/Game/Megaplant_Library/Tree_Yoshino_Cherry/Tree_Yoshino_Cherry_01/PVE_Yoshino_Cherry_01.PVE_Yoshino_Cherry_01")
            );
            OutCandidateObjectPaths.AddUnique(
                TEXT("/Game/Megaplant_Library/Tree_Yoshino_Cherry/Tree_Yoshino_Cherry_01/PVE_Yoshino_Cherry_01.PVE_Yoshino_Cherry_01_C")
            );
            OutCandidateObjectPaths.AddUnique(
                TEXT("/Game/Megaplant_Library/Tree_Yoshino_Cherry/Tree_Yoshino_Cherry_01/Tree_Yoshino_Cherry_01.Tree_Yoshino_Cherry_01")
            );
            return;
        }

        if (NormalizedSpeciesId == TEXT("Greasewood"))
        {
            OutCandidateObjectPaths.AddUnique(
                TEXT("/Game/Megaplant_Library/Shrub_Greasewood/Shrub_Greasewood_01/PVE_Greasewood_01.PVE_Greasewood_01")
            );
            OutCandidateObjectPaths.AddUnique(
                TEXT("/Game/Megaplant_Library/Shrub_Greasewood/Shrub_Greasewood_01/PVE_Greasewood_01.PVE_Greasewood_01_C")
            );
            return;
        }
    }

    void AppendSearchRootsFromSpeciesEntry(
        const FCubusVegetationSpeciesCatalogEntry& SpeciesEntry,
        TArray<FName>& OutSearchRoots
    )
    {
        for (const TSoftObjectPtr<UObject>& GrowthStageAssetRef : SpeciesEntry.GrowthStageMeshes)
        {
            const FSoftObjectPath AssetPath = GrowthStageAssetRef.ToSoftObjectPath();

            if (!AssetPath.IsValid())
            {
                continue;
            }

            const FString AssetPathString = AssetPath.GetAssetPathString();
            const FString StageFolderPath =
                FPackageName::GetLongPackagePath(AssetPathString);

            if (StageFolderPath.IsEmpty())
            {
                continue;
            }

            OutSearchRoots.AddUnique(FName(*StageFolderPath));

            const FString ParentFolderPath =
                FPackageName::GetLongPackagePath(StageFolderPath);

            if (!ParentFolderPath.IsEmpty())
            {
                OutSearchRoots.AddUnique(FName(*ParentFolderPath));
            }
        }
    }

    bool TryResolveActorClassFromReferencedObject(
        UObject* CandidateObject,
        UClass*& OutClass,
        TSet<const UObject*>& VisitedObjects,
        const int32 RemainingDepth
    )
    ;

    bool TryResolveActorClassFromStringPathCandidate(
        const FString& RawPathCandidate,
        UClass*& OutClass
    )
    ;

    bool TryResolveActorClassFromReferencedObject(
        UObject* CandidateObject,
        UClass*& OutClass,
        TSet<const UObject*>& VisitedObjects,
        const int32 RemainingDepth
    )
    {
        OutClass = nullptr;

        if (!IsValid(CandidateObject) || RemainingDepth < 0)
        {
            return false;
        }

        if (VisitedObjects.Contains(CandidateObject))
        {
            return false;
        }

        VisitedObjects.Add(CandidateObject);

        if (const UBlueprint* BlueprintAsset = Cast<UBlueprint>(CandidateObject))
        {
            if (
                IsValid(BlueprintAsset->GeneratedClass) &&
                BlueprintAsset->GeneratedClass->IsChildOf(AActor::StaticClass())
            )
            {
                OutClass = BlueprintAsset->GeneratedClass;
                return true;
            }
        }

        if (const UClass* ClassAsset = Cast<UClass>(CandidateObject))
        {
            if (ClassAsset->IsChildOf(AActor::StaticClass()))
            {
                OutClass = const_cast<UClass*>(ClassAsset);
                return true;
            }
        }

        for (TFieldIterator<FProperty> FieldIt(CandidateObject->GetClass()); FieldIt; ++FieldIt)
        {
            FProperty* Property = *FieldIt;

            if (const FClassProperty* ClassProperty = CastField<FClassProperty>(Property))
            {
                UObject* ClassObjectValue =
                    ClassProperty->GetObjectPropertyValue_InContainer(CandidateObject);

                UClass* ClassValue = Cast<UClass>(ClassObjectValue);

                if (IsValid(ClassValue) && ClassValue->IsChildOf(AActor::StaticClass()))
                {
                    OutClass = ClassValue;
                    return true;
                }

                continue;
            }

            if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
            {
                UObject* ReferencedObject =
                    ObjectProperty->GetObjectPropertyValue_InContainer(CandidateObject);

                if (
                    TryResolveActorClassFromReferencedObject(
                        ReferencedObject,
                        OutClass,
                        VisitedObjects,
                        RemainingDepth - 1
                    )
                )
                {
                    return true;
                }

                continue;
            }

            if (const FSoftObjectProperty* SoftObjectProperty = CastField<FSoftObjectProperty>(Property))
            {
                const FSoftObjectPtr SoftObjectPtr =
                    SoftObjectProperty->GetPropertyValue_InContainer(CandidateObject);

                const FSoftObjectPath SoftPath = SoftObjectPtr.ToSoftObjectPath();

                if (!SoftPath.IsValid())
                {
                    continue;
                }

                UObject* LoadedSoftObject =
                    SoftPath.TryLoad(
                        nullptr,
                        static_cast<ELoadFlags>(LOAD_NoWarn | LOAD_Quiet)
                    );

                if (
                    TryResolveActorClassFromReferencedObject(
                        LoadedSoftObject,
                        OutClass,
                        VisitedObjects,
                        RemainingDepth - 1
                    )
                )
                {
                    return true;
                }

                continue;
            }

            if (const FSoftClassProperty* SoftClassProperty = CastField<FSoftClassProperty>(Property))
            {
                const FSoftObjectPtr SoftClassPtr =
                    SoftClassProperty->GetPropertyValue_InContainer(CandidateObject);

                const FSoftObjectPath SoftClassPath = SoftClassPtr.ToSoftObjectPath();

                if (!SoftClassPath.IsValid())
                {
                    continue;
                }

                UClass* LoadedSoftClass =
                    Cast<UClass>(
                        SoftClassPath.TryLoad(
                            nullptr,
                            static_cast<ELoadFlags>(LOAD_NoWarn | LOAD_Quiet)
                        )
                    );

                if (IsValid(LoadedSoftClass) && LoadedSoftClass->IsChildOf(AActor::StaticClass()))
                {
                    OutClass = LoadedSoftClass;
                    return true;
                }

                continue;
            }

            if (const FStrProperty* StringProperty = CastField<FStrProperty>(Property))
            {
                const FString StringValue =
                    StringProperty->GetPropertyValue_InContainer(CandidateObject);

                if (StringValue.Contains(TEXT("/Game/"), ESearchCase::IgnoreCase))
                {
                    if (TryResolveActorClassFromStringPathCandidate(StringValue, OutClass))
                    {
                        return true;
                    }
                }

                continue;
            }

            if (const FNameProperty* NameProperty = CastField<FNameProperty>(Property))
            {
                const FString NameValue =
                    NameProperty->GetPropertyValue_InContainer(CandidateObject).ToString();

                if (NameValue.Contains(TEXT("/Game/"), ESearchCase::IgnoreCase))
                {
                    if (TryResolveActorClassFromStringPathCandidate(NameValue, OutClass))
                    {
                        return true;
                    }
                }

                continue;
            }
        }

        return false;
    }

    bool TryResolveActorClassFromObjectPath(
        const FString& CandidateObjectPath,
        UClass*& OutClass
    )
    {
        OutClass = nullptr;

        if (CandidateObjectPath.IsEmpty())
        {
            return false;
        }

        UObject* LoadedObject = StaticLoadObject(
            UObject::StaticClass(),
            nullptr,
            *CandidateObjectPath,
            nullptr,
            LOAD_NoWarn
        );

        TSet<const UObject*> VisitedObjects;
        if (
            TryResolveActorClassFromReferencedObject(
                LoadedObject,
                OutClass,
                VisitedObjects,
                3
            )
        )
        {
            return true;
        }

        const FString ClassObjectPath =
            CandidateObjectPath.EndsWith(TEXT("_C"), ESearchCase::CaseSensitive)
                ? CandidateObjectPath
                : FString::Printf(TEXT("%s_C"), *CandidateObjectPath);

        UClass* LoadedClass = Cast<UClass>(
            StaticLoadObject(
                UClass::StaticClass(),
                nullptr,
                *ClassObjectPath,
                nullptr,
                LOAD_NoWarn
            )
        );

        if (IsValid(LoadedClass) && LoadedClass->IsChildOf(AActor::StaticClass()))
        {
            OutClass = LoadedClass;
            return true;
        }

        return false;
    }

    bool TryResolveActorClassFromStringPathCandidate(
        const FString& RawPathCandidate,
        UClass*& OutClass
    )
    {
        OutClass = nullptr;

        FString Candidate = RawPathCandidate;
        Candidate.TrimStartAndEndInline();

        if (Candidate.IsEmpty())
        {
            return false;
        }

        TArray<FString> ClassPathCandidates;

        const FString ExportObjectPath =
            FPackageName::ExportTextPathToObjectPath(Candidate);

        if (!ExportObjectPath.IsEmpty())
        {
            ClassPathCandidates.AddUnique(ExportObjectPath);
        }

        if (Candidate.StartsWith(TEXT("/Game/"), ESearchCase::IgnoreCase))
        {
            ClassPathCandidates.AddUnique(Candidate);

            if (!Candidate.Contains(TEXT(".")))
            {
                const FString AssetName = FPackageName::GetLongPackageAssetName(Candidate);
                if (!AssetName.IsEmpty())
                {
                    ClassPathCandidates.AddUnique(
                        FString::Printf(TEXT("%s.%s"), *Candidate, *AssetName)
                    );
                }
            }
        }

        for (const FString& ClassPath : ClassPathCandidates)
        {
            const FString GeneratedClassPath =
                ClassPath.EndsWith(TEXT("_C"), ESearchCase::CaseSensitive)
                    ? ClassPath
                    : FString::Printf(TEXT("%s_C"), *ClassPath);

            UClass* LoadedClass = Cast<UClass>(
                StaticLoadObject(
                    UClass::StaticClass(),
                    nullptr,
                    *GeneratedClassPath,
                    nullptr,
                    LOAD_NoWarn
                )
            );

            if (IsValid(LoadedClass) && LoadedClass->IsChildOf(AActor::StaticClass()))
            {
                OutClass = LoadedClass;
                return true;
            }
        }

        return false;
    }

    UClass* ResolveMegaplantPveActorClassBySpecies(
        const FCubusVegetationSpeciesCatalogEntry& SpeciesEntry
    )
    {
        const FName NormalizedSpeciesId =
            NormalizeMegaplantSpeciesId(SpeciesEntry.SpeciesId);

        static TMap<FName, TWeakObjectPtr<UClass>> CachedResolvedClasses;
        static TSet<FName> CachedFailedSpecies;

        if (!SpeciesEntry.HeroPveActorClassOverride.IsNull())
        {
            UClass* OverrideClass =
                SpeciesEntry.HeroPveActorClassOverride.LoadSynchronous();

            if (IsValid(OverrideClass) && OverrideClass->IsChildOf(AActor::StaticClass()))
            {
                return OverrideClass;
            }

            UE_LOG(
                LogTemp,
                Warning,
                TEXT("Cubus PVE class override failed to load for %s (class path: %s)"),
                *SpeciesEntry.SpeciesId.ToString(),
                *SpeciesEntry.HeroPveActorClassOverride.ToSoftObjectPath().ToString()
            );
        }

        if (!SpeciesEntry.HeroPveActorAssetOverride.IsNull())
        {
            UObject* OverrideAsset =
                SpeciesEntry.HeroPveActorAssetOverride.LoadSynchronous();

            UClass* ResolvedFromOverrideAsset = nullptr;
            TSet<const UObject*> VisitedObjects;

            if (
                TryResolveActorClassFromReferencedObject(
                    OverrideAsset,
                    ResolvedFromOverrideAsset,
                    VisitedObjects,
                    5
                )
            )
            {
                CachedResolvedClasses.Add(
                    NormalizedSpeciesId,
                    ResolvedFromOverrideAsset
                );

                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT("Cubus PVE class resolved from asset override for %s: %s"),
                    *SpeciesEntry.SpeciesId.ToString(),
                    *SpeciesEntry.HeroPveActorAssetOverride.ToSoftObjectPath().ToString()
                );

                return ResolvedFromOverrideAsset;
            }

            UE_LOG(
                LogTemp,
                Warning,
                TEXT("Cubus PVE asset override did not resolve actor class for %s (asset path: %s)"),
                *SpeciesEntry.SpeciesId.ToString(),
                *SpeciesEntry.HeroPveActorAssetOverride.ToSoftObjectPath().ToString()
            );
        }

        if (const TWeakObjectPtr<UClass>* CachedClass =
                CachedResolvedClasses.Find(NormalizedSpeciesId))
        {
            return CachedClass->Get();
        }

        if (CachedFailedSpecies.Contains(NormalizedSpeciesId))
        {
            return nullptr;
        }

        TArray<FName> SearchRoots;
        AppendSearchRootsFromSpeciesEntry(SpeciesEntry, SearchRoots);
        AppendMegaplantSearchRootsBySpecies(NormalizedSpeciesId, SearchRoots);

        if (SearchRoots.IsEmpty())
        {
            return nullptr;
        }

        TArray<FString> CandidateObjectPaths;
        AppendExplicitPveCandidateObjectPathsBySpecies(
            NormalizedSpeciesId,
            CandidateObjectPaths
        );

        FAssetRegistryModule& AssetRegistryModule =
            FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
                TEXT("AssetRegistry")
            );

        IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
        const bool bRegistryReady = !AssetRegistry.IsLoadingAssets();

        int32 DiscoveredAssetCount = 0;
        int32 GeneratedClassTagCount = 0;

        for (const FName& SearchRoot : SearchRoots)
        {
            FARFilter AssetFilter;
            AssetFilter.bRecursivePaths = true;
            AssetFilter.PackagePaths.Add(SearchRoot);

            TArray<FAssetData> FoundAssets;
            AssetRegistry.GetAssets(AssetFilter, FoundAssets);
            DiscoveredAssetCount += FoundAssets.Num();

            for (const FAssetData& AssetData : FoundAssets)
            {
                CandidateObjectPaths.AddUnique(AssetData.GetObjectPathString());

                const FString PackageName = AssetData.PackageName.ToString();
                const FString AssetName = AssetData.AssetName.ToString();

                CandidateObjectPaths.AddUnique(
                    FString::Printf(TEXT("%s.%s_C"), *PackageName, *AssetName)
                );

                FString GeneratedClassTag;
                if (
                    AssetData.GetTagValue(FName(TEXT("GeneratedClass")), GeneratedClassTag) ||
                    AssetData.GetTagValue(FName(TEXT("GeneratedClassPath")), GeneratedClassTag)
                )
                {
                    ++GeneratedClassTagCount;

                    const FString GeneratedClassObjectPath =
                        FPackageName::ExportTextPathToObjectPath(GeneratedClassTag);

                    if (!GeneratedClassObjectPath.IsEmpty())
                    {
                        CandidateObjectPaths.AddUnique(GeneratedClassObjectPath);
                    }
                }
            }
        }

        for (const FString& CandidatePath : CandidateObjectPaths)
        {
            UClass* ResolvedClass = nullptr;

            if (!TryResolveActorClassFromObjectPath(CandidatePath, ResolvedClass))
            {
                continue;
            }

            CachedResolvedClasses.Add(NormalizedSpeciesId, ResolvedClass);

            UE_LOG(
                LogTemp,
                Display,
                TEXT("Cubus PVE class resolved for %s (from %s): %s"),
                *NormalizedSpeciesId.ToString(),
                *SpeciesEntry.SpeciesId.ToString(),
                *CandidatePath
            );

            return ResolvedClass;
        }

        if (bRegistryReady)
        {
            CachedFailedSpecies.Add(NormalizedSpeciesId);
        }

        if (!CandidateObjectPaths.IsEmpty() || !SearchRoots.IsEmpty())
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("Cubus PVE class unresolved for %s (from %s); roots=%d assets=%d generatedTags=%d candidates=%d registryReady=%s"),
                *NormalizedSpeciesId.ToString(),
                *SpeciesEntry.SpeciesId.ToString(),
                SearchRoots.Num(),
                DiscoveredAssetCount,
                GeneratedClassTagCount,
                CandidateObjectPaths.Num()
                ,
                bRegistryReady ? TEXT("yes") : TEXT("no")
            );
        }

        return nullptr;
    }

    void AssignLikelyWindProviderActor(
        UObject* Target,
        AActor* WindProviderActor
    )
    {
        if (!IsValid(Target) || !IsValid(WindProviderActor))
        {
            return;
        }

        for (TFieldIterator<FProperty> FieldIt(Target->GetClass()); FieldIt; ++FieldIt)
        {
            FProperty* Property = *FieldIt;

            FObjectPropertyBase* ObjectProperty =
                CastField<FObjectPropertyBase>(Property);

            if (ObjectProperty == nullptr)
            {
                continue;
            }

            if (!ObjectProperty->PropertyClass->IsChildOf(AActor::StaticClass()))
            {
                continue;
            }

            const FString PropertyName = Property->GetName();

            const bool bLooksLikeWindProviderField =
                PropertyName.Contains(TEXT("Wind"), ESearchCase::IgnoreCase) &&
                (
                    PropertyName.Contains(TEXT("Provider"), ESearchCase::IgnoreCase) ||
                    PropertyName.Contains(TEXT("Source"), ESearchCase::IgnoreCase) ||
                    PropertyName.Contains(TEXT("Actor"), ESearchCase::IgnoreCase) ||
                    PropertyName.Contains(TEXT("Controller"), ESearchCase::IgnoreCase)
                );

            if (!bLooksLikeWindProviderField)
            {
                continue;
            }

            ObjectProperty->SetObjectPropertyValue_InContainer(
                Target,
                WindProviderActor
            );
        }
    }

    void ApplyFoliageMaterialOverrideToSkinnedComponent(
        USkinnedMeshComponent* Component,
        UMaterialInterface* OverrideMaterial
    )
    {
        if (!IsValid(Component) || !IsValid(OverrideMaterial))
        {
            return;
        }

        const int32 MaterialCount = Component->GetNumMaterials();

        if (MaterialCount <= 0)
        {
            return;
        }

        bool bOverrodeAnySlot = false;

        for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
        {
            UMaterialInterface* ExistingMaterial =
                Component->GetMaterial(MaterialIndex);

            const FString ExistingName = IsValid(ExistingMaterial)
                ? ExistingMaterial->GetName()
                : FString();

            const bool bLooksLikeFoliageSlot =
                ExistingName.IsEmpty() ||
                ExistingName.Contains(TEXT("Foliage"), ESearchCase::IgnoreCase) ||
                ExistingName.Contains(TEXT("Leaf"), ESearchCase::IgnoreCase) ||
                ExistingName.Contains(TEXT("Needle"), ESearchCase::IgnoreCase) ||
                ExistingName.Contains(TEXT("Twig"), ESearchCase::IgnoreCase);

            if (bLooksLikeFoliageSlot)
            {
                Component->SetMaterial(MaterialIndex, OverrideMaterial);
                bOverrodeAnySlot = true;
            }
        }

        if (!bOverrodeAnySlot && MaterialCount == 1)
        {
            Component->SetMaterial(0, OverrideMaterial);
        }
    }

    UObject* ResolveTransformProviderDataFromObject(UObject* Candidate)
    {
        if (!IsValid(Candidate))
        {
            return nullptr;
        }

        if (UInstancedSkinnedMeshComponent* InstancedSkinned =
                Cast<UInstancedSkinnedMeshComponent>(Candidate))
        {
            return InstancedSkinned->GetTransformProvider();
        }

        for (TFieldIterator<FObjectPropertyBase> FieldIt(Candidate->GetClass()); FieldIt; ++FieldIt)
        {
            FObjectPropertyBase* Property = *FieldIt;

            if (Property == nullptr)
            {
                continue;
            }

            const FString ClassName =
                IsValid(Property->PropertyClass)
                    ? Property->PropertyClass->GetName()
                    : FString();

            const FString PropertyName = Property->GetName();

            const bool bLooksLikeTransformProvider =
                ClassName.Contains(TEXT("TransformProvider"), ESearchCase::IgnoreCase) ||
                PropertyName.Contains(TEXT("TransformProvider"), ESearchCase::IgnoreCase);

            if (!bLooksLikeTransformProvider)
            {
                continue;
            }

            UObject* Value =
                Property->GetObjectPropertyValue_InContainer(Candidate);

            if (IsValid(Value))
            {
                return Value;
            }
        }

        return nullptr;
    }

    UObject* ResolveWindTransformProviderFromActor(AActor* CandidateActor)
    {
        if (!IsValid(CandidateActor))
        {
            return nullptr;
        }

        if (UObject* DirectProvider = ResolveTransformProviderDataFromObject(CandidateActor))
        {
            return DirectProvider;
        }

        TInlineComponentArray<UActorComponent*> Components(CandidateActor);
        for (UActorComponent* Component : Components)
        {
            if (UObject* Provider = ResolveTransformProviderDataFromObject(Component))
            {
                return Provider;
            }
        }

        return nullptr;
    }

    AActor* ResolveGlobalFoliageActor(UWorld* World)
    {
        if (!IsValid(World))
        {
            return nullptr;
        }

        AActor* BestCandidate = nullptr;
        int32 BestScore = MIN_int32;

        for (TActorIterator<AActor> Iterator(World); Iterator; ++Iterator)
        {
            AActor* Candidate = *Iterator;

            if (!IsValid(Candidate))
            {
                continue;
            }

            const FString Name = Candidate->GetName();
            const FString ClassName = Candidate->GetClass()->GetName();

            int32 Score = 0;

            if (
                Name.Contains(TEXT("GlobalFoliage"), ESearchCase::IgnoreCase) ||
                Name.Contains(TEXT("Global_Foliage"), ESearchCase::IgnoreCase)
            )
            {
                Score += 120;
            }

            if (
                ClassName.Contains(TEXT("GlobalFoliage"), ESearchCase::IgnoreCase) ||
                ClassName.Contains(TEXT("Global_Foliage"), ESearchCase::IgnoreCase)
            )
            {
                Score += 120;
            }

            if (Name.Contains(TEXT("Megascans"), ESearchCase::IgnoreCase))
            {
                Score += 50;
            }

            if (ClassName.Contains(TEXT("Megascans"), ESearchCase::IgnoreCase))
            {
                Score += 50;
            }

            if (Candidate->ActorHasTag(TEXT("GlobalFoliage")))
            {
                Score += 80;
            }

            if (Score > BestScore)
            {
                BestScore = Score;
                BestCandidate = Candidate;
            }
        }

        return BestScore > 0 ? BestCandidate : nullptr;
    }

    int32 ApplyWindToGlobalFoliageObject(
        UObject* Target,
        const FVector& WindDirection,
        const float WindIntensity
    )
    {
        if (!IsValid(Target))
        {
            return 0;
        }

        int32 UpdatedPropertyCount = 0;

        static const FName DirectionPropertyCandidates[] =
        {
            TEXT("WindDirection"),
            TEXT("Wind Direction"),
            TEXT("Wind_Direction"),
            TEXT("GlobalWindDirection"),
            TEXT("WindDir"),
            TEXT("FoliageWindDirection")
        };

        static const FName ScalarPropertyCandidates[] =
        {
            TEXT("WindIntensity"),
            TEXT("Wind Intensity"),
            TEXT("Wind_Intensity"),
            TEXT("GlobalWindIntensity"),
            TEXT("WindSpeed"),
            TEXT("Wind Speed"),
            TEXT("GlobalWindSpeed"),
            TEXT("WindStrength"),
            TEXT("FoliageWindSpeed"),
            TEXT("FoliageWindIntensity")
        };

        const float WindDirectionYaw =
            FRotator::NormalizeAxis(WindDirection.Rotation().Yaw);

        // Explicit first-pass sync for deterministic UDS->GlobalFoliage speed matching.
        for (const FName PropertyName : DirectionPropertyCandidates)
        {
            if (
                TryWriteVectorLikeProperty(Target, PropertyName, WindDirection) ||
                TryWriteFloatProperty(Target, PropertyName, WindDirectionYaw)
            )
            {
                ++UpdatedPropertyCount;
            }
        }

        for (const FName PropertyName : ScalarPropertyCandidates)
        {
            if (TryWriteFloatProperty(Target, PropertyName, WindIntensity))
            {
                ++UpdatedPropertyCount;
            }
        }

        for (TFieldIterator<FProperty> FieldIt(Target->GetClass()); FieldIt; ++FieldIt)
        {
            FProperty* Property = *FieldIt;

            if (Property == nullptr)
            {
                continue;
            }

            const FString PropertyName = Property->GetName();
            const FName PropertyFName = Property->GetFName();

            const bool bLooksLikeWindDirection =
                PropertyName.Contains(TEXT("Wind"), ESearchCase::IgnoreCase) &&
                (
                    PropertyName.Contains(TEXT("Direction"), ESearchCase::IgnoreCase) ||
                    PropertyName.Contains(TEXT("Dir"), ESearchCase::IgnoreCase)
                );

            if (bLooksLikeWindDirection)
            {
                if (
                    TryWriteVectorLikeProperty(Target, PropertyFName, WindDirection) ||
                    TryWriteFloatProperty(Target, PropertyFName, WindDirectionYaw)
                )
                {
                    ++UpdatedPropertyCount;
                    continue;
                }
            }

            const bool bLooksLikeWindScalar =
                PropertyName.Contains(TEXT("Wind"), ESearchCase::IgnoreCase) &&
                (
                    PropertyName.Contains(TEXT("Intensity"), ESearchCase::IgnoreCase) ||
                    PropertyName.Contains(TEXT("Speed"), ESearchCase::IgnoreCase) ||
                    PropertyName.Contains(TEXT("Strength"), ESearchCase::IgnoreCase) ||
                    PropertyName.Contains(TEXT("Gust"), ESearchCase::IgnoreCase)
                );

            if (bLooksLikeWindScalar)
            {
                if (TryWriteFloatProperty(Target, PropertyFName, WindIntensity))
                {
                    ++UpdatedPropertyCount;
                }
            }
        }

        return UpdatedPropertyCount;
    }

    int32 InvokeLikelyWindRefreshFunctions(UObject* Target)
    {
        if (!IsValid(Target))
        {
            return 0;
        }

        int32 InvokedFunctionCount = 0;

        for (TFieldIterator<UFunction> FunctionIt(Target->GetClass()); FunctionIt; ++FunctionIt)
        {
            UFunction* Function = *FunctionIt;

            if (Function == nullptr || Function->HasAnyFunctionFlags(FUNC_Const | FUNC_BlueprintPure))
            {
                continue;
            }

            const FString FunctionName = Function->GetName();

            const bool bLooksLikeWindRefresh =
                FunctionName.Contains(TEXT("Wind"), ESearchCase::IgnoreCase) &&
                (
                    FunctionName.Contains(TEXT("Update"), ESearchCase::IgnoreCase) ||
                    FunctionName.Contains(TEXT("Refresh"), ESearchCase::IgnoreCase) ||
                    FunctionName.Contains(TEXT("Apply"), ESearchCase::IgnoreCase) ||
                    FunctionName.Contains(TEXT("Sync"), ESearchCase::IgnoreCase)
                );

            if (!bLooksLikeWindRefresh)
            {
                continue;
            }

            bool bHasInputParameters = false;

            for (TFieldIterator<FProperty> PropertyIt(Function); PropertyIt; ++PropertyIt)
            {
                const FProperty* Property = *PropertyIt;

                if (
                    Property != nullptr &&
                    Property->HasAnyPropertyFlags(CPF_Parm) &&
                    !Property->HasAnyPropertyFlags(CPF_ReturnParm)
                )
                {
                    bHasInputParameters = true;
                    break;
                }
            }

            if (bHasInputParameters)
            {
                continue;
            }

            Target->ProcessEvent(Function, nullptr);
            ++InvokedFunctionCount;
        }

        return InvokedFunctionCount;
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
    RuntimeRandomizationSamplesByPlant.Reset();
    bRuntimeRandomizationStreamInitialized = false;

    if (HasActorBegunPlay())
    {
        RebuildWorldVegetation();
    }
}

void ACubusWorldVegetationActor::ConfigureForWorld(
    ACubusBlockWorldActor* InBlockWorld,
    UPCGGraphInterface* InVegetationGraph,
    const bool bInEnableRuntimeVegetation
)
{
    ConfigureForWorld(InBlockWorld);
}

void ACubusWorldVegetationActor::OnConstruction(
    const FTransform& Transform
)
{
    Super::OnConstruction(Transform);

    ResolveBlockWorld();
    EnsurePointCarriers();

    if (!GetWorld() || !GetWorld()->IsGameWorld())
    {
        RebuildWorldVegetation();
    }
}

void ACubusWorldVegetationActor::BeginPlay()
{
    Super::BeginPlay();

    RuntimeRandomizationSamplesByPlant.Reset();
    bRuntimeRandomizationStreamInitialized = false;

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "Cubus wind runtime mode: heroPve=%s fallbackInstanced=%s heroMax=%d heroDist=%.0f"
        ),
        bUseHeroPveActorWindMode ? TEXT("on") : TEXT("off"),
        bUseInstancedSkeletalFallbackBeyondHeroDistance ? TEXT("on") : TEXT("off"),
        MaxHeroSkeletalWindComponents,
        HeroSkeletalWindMaxDistance
    );

    ResolveBlockWorld();
    EnsurePointCarriers();
    EnsurePlantBatches();
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

    for (const TPair<int64, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>>& Pair
         : CatalogStaticBatchComponents)
    {
        if (IsValid(Pair.Value))
        {
            Pair.Value->SetCastShadow(bCastWorldPlantShadows);
        }
    }

    for (const TPair<int64, TObjectPtr<UInstancedSkinnedMeshComponent>>& Pair
         : CatalogSkeletalBatchComponents)
    {
        if (IsValid(Pair.Value))
        {
            Pair.Value->SetCastShadow(bCastWorldPlantShadows);
        }
    }

    for (USkeletalMeshComponent* HeroComponent : HeroSkeletalWindComponents)
    {
        if (IsValid(HeroComponent))
        {
            HeroComponent->SetCastShadow(bCastWorldPlantShadows);
        }
    }

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
    RuntimeRandomizationSamplesByPlant.Reset();
    bRuntimeRandomizationStreamInitialized = false;
    Super::EndPlay(EndPlayReason);
}

void ACubusWorldVegetationActor::BuildDefaultSpeciesCatalogIfNeeded()
{
    if (!bAutoSeedCatalogDefaults)
    {
        return;
    }

    auto UpgradeSpeciesToSkeletonStages = [](
        FCubusVegetationSpeciesCatalogEntry& Entry,
        const TCHAR* OldStageA,
        const TCHAR* OldStageB,
        const TCHAR* OldStageC,
        const TCHAR* OldStageD,
        const TCHAR* NewStageA,
        const TCHAR* NewStageB,
        const TCHAR* NewStageC,
        const TCHAR* NewStageD
    ) -> bool
    {
        if (Entry.GrowthStageMeshes.Num() < 4)
        {
            return false;
        }

        const FString StageA =
            Entry.GrowthStageMeshes[0].ToSoftObjectPath().ToString();
        const FString StageB =
            Entry.GrowthStageMeshes[1].ToSoftObjectPath().ToString();
        const FString StageC =
            Entry.GrowthStageMeshes[2].ToSoftObjectPath().ToString();
        const FString StageD =
            Entry.GrowthStageMeshes[3].ToSoftObjectPath().ToString();

        if (
            !StageA.Equals(OldStageA, ESearchCase::CaseSensitive) ||
            !StageB.Equals(OldStageB, ESearchCase::CaseSensitive) ||
            !StageC.Equals(OldStageC, ESearchCase::CaseSensitive) ||
            !StageD.Equals(OldStageD, ESearchCase::CaseSensitive)
        )
        {
            return false;
        }

        Entry.GrowthStageMeshes[0] =
            TSoftObjectPtr<UObject>(FSoftObjectPath(NewStageA));
        Entry.GrowthStageMeshes[1] =
            TSoftObjectPtr<UObject>(FSoftObjectPath(NewStageB));
        Entry.GrowthStageMeshes[2] =
            TSoftObjectPtr<UObject>(FSoftObjectPath(NewStageC));
        Entry.GrowthStageMeshes[3] =
            TSoftObjectPtr<UObject>(FSoftObjectPath(NewStageD));
        return true;
    };

    if (!SpeciesCatalog.IsEmpty())
    {
        int32 UpgradedSpeciesCount = 0;

        for (FCubusVegetationSpeciesCatalogEntry& Entry : SpeciesCatalog)
        {
            bool bUpgraded = false;

            if (Entry.SpeciesId == TEXT("Elder"))
            {
                bUpgraded = UpgradeSpeciesToSkeletonStages(
                    Entry,
                    TEXT("/Game/Megaplant_Library/Tree_Elder/Tree_Elder_01/Tree_Elder_01_A_Skeleton.Tree_Elder_01_A_Skeleton"),
                    TEXT("/Game/Megaplant_Library/Tree_Elder/Tree_Elder_01/Tree_Elder_01_B_Skeleton.Tree_Elder_01_B_Skeleton"),
                    TEXT("/Game/Megaplant_Library/Tree_Elder/Tree_Elder_01/Tree_Elder_01_C_Skeleton.Tree_Elder_01_C_Skeleton"),
                    TEXT("/Game/Megaplant_Library/Tree_Elder/Tree_Elder_01/Tree_Elder_01_D_Skeleton.Tree_Elder_01_D_Skeleton"),
                    TEXT("/Game/Megaplant_Library/Tree_Elder/Tree_Elder_01/Tree_Elder_01_A.Tree_Elder_01_A"),
                    TEXT("/Game/Megaplant_Library/Tree_Elder/Tree_Elder_01/Tree_Elder_01_B.Tree_Elder_01_B"),
                    TEXT("/Game/Megaplant_Library/Tree_Elder/Tree_Elder_01/Tree_Elder_01_C.Tree_Elder_01_C"),
                    TEXT("/Game/Megaplant_Library/Tree_Elder/Tree_Elder_01/Tree_Elder_01_D.Tree_Elder_01_D")
                );
            }
            else if (Entry.SpeciesId == TEXT("NorwaySpruce"))
            {
                bUpgraded = UpgradeSpeciesToSkeletonStages(
                    Entry,
                    TEXT("/Game/Megaplant_Library/Tree_Norway_Spruce/Tree_Norway_Spruce_01/Tree_Norway_Spruce_01_A_Skeleton.Tree_Norway_Spruce_01_A_Skeleton"),
                    TEXT("/Game/Megaplant_Library/Tree_Norway_Spruce/Tree_Norway_Spruce_01/Tree_Norway_Spruce_01_B_Skeleton.Tree_Norway_Spruce_01_B_Skeleton"),
                    TEXT("/Game/Megaplant_Library/Tree_Norway_Spruce/Tree_Norway_Spruce_01/Tree_Norway_Spruce_01_C_Skeleton.Tree_Norway_Spruce_01_C_Skeleton"),
                    TEXT("/Game/Megaplant_Library/Tree_Norway_Spruce/Tree_Norway_Spruce_01/Tree_Norway_Spruce_01_D_Skeleton.Tree_Norway_Spruce_01_D_Skeleton"),
                    TEXT("/Game/Megaplant_Library/Tree_Norway_Spruce/Tree_Norway_Spruce_01/Tree_Norway_Spruce_01_A.Tree_Norway_Spruce_01_A"),
                    TEXT("/Game/Megaplant_Library/Tree_Norway_Spruce/Tree_Norway_Spruce_01/Tree_Norway_Spruce_01_B.Tree_Norway_Spruce_01_B"),
                    TEXT("/Game/Megaplant_Library/Tree_Norway_Spruce/Tree_Norway_Spruce_01/Tree_Norway_Spruce_01_C.Tree_Norway_Spruce_01_C"),
                    TEXT("/Game/Megaplant_Library/Tree_Norway_Spruce/Tree_Norway_Spruce_01/Tree_Norway_Spruce_01_D.Tree_Norway_Spruce_01_D")
                );
            }
            else if (Entry.SpeciesId == TEXT("Greasewood"))
            {
                bUpgraded = UpgradeSpeciesToSkeletonStages(
                    Entry,
                    TEXT("/Game/Megaplant_Library/Shrub_Greasewood/Shrub_Greasewood_01/Shrub_Greasewood_01_A_Skeleton.Shrub_Greasewood_01_A_Skeleton"),
                    TEXT("/Game/Megaplant_Library/Shrub_Greasewood/Shrub_Greasewood_01/Shrub_Greasewood_01_B_Skeleton.Shrub_Greasewood_01_B_Skeleton"),
                    TEXT("/Game/Megaplant_Library/Shrub_Greasewood/Shrub_Greasewood_01/Shrub_Greasewood_01_C_Skeleton.Shrub_Greasewood_01_C_Skeleton"),
                    TEXT("/Game/Megaplant_Library/Shrub_Greasewood/Shrub_Greasewood_01/Shrub_Greasewood_01_D_Skeleton.Shrub_Greasewood_01_D_Skeleton"),
                    TEXT("/Game/Megaplant_Library/Shrub_Greasewood/Shrub_Greasewood_01/Shrub_Greasewood_01_A.Shrub_Greasewood_01_A"),
                    TEXT("/Game/Megaplant_Library/Shrub_Greasewood/Shrub_Greasewood_01/Shrub_Greasewood_01_B.Shrub_Greasewood_01_B"),
                    TEXT("/Game/Megaplant_Library/Shrub_Greasewood/Shrub_Greasewood_01/Shrub_Greasewood_01_C.Shrub_Greasewood_01_C"),
                    TEXT("/Game/Megaplant_Library/Shrub_Greasewood/Shrub_Greasewood_01/Shrub_Greasewood_01_D.Shrub_Greasewood_01_D")
                );
            }

            if (bUpgraded)
            {
                ++UpgradedSpeciesCount;
            }
        }

        if (UpgradedSpeciesCount > 0)
        {
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Cubus vegetation catalog: corrected %d species stage asset object paths"),
                UpgradedSpeciesCount
            );
        }

        return;
    }

    auto AddDefaultSpecies = [this](
        const FName SpeciesId,
        const int32 TypeId,
        const TCHAR* StageA,
        const TCHAR* StageB,
        const TCHAR* StageC,
        const TCHAR* StageD
    )
    {
        FCubusVegetationSpeciesCatalogEntry Entry;
        Entry.SpeciesId = SpeciesId;
        Entry.TypeId = TypeId;
        Entry.Weight = 1.0f;

        if (TypeId == BroadleafType)
        {
            Entry.BiomeMask =
                static_cast<int32>(ECubusVegetationBiome::Forest) |
                static_cast<int32>(ECubusVegetationBiome::Plains) |
                static_cast<int32>(ECubusVegetationBiome::Wetland);
        }
        else if (TypeId == ConiferType)
        {
            Entry.BiomeMask =
                static_cast<int32>(ECubusVegetationBiome::Forest) |
                static_cast<int32>(ECubusVegetationBiome::Rocky);
        }
        else if (TypeId == ShrubType)
        {
            Entry.BiomeMask =
                static_cast<int32>(ECubusVegetationBiome::Plains) |
                static_cast<int32>(ECubusVegetationBiome::Forest) |
                static_cast<int32>(ECubusVegetationBiome::Wetland);
        }

        Entry.GrowthStageMeshes.Reserve(4);
        Entry.GrowthStageMeshes.Add(
            TSoftObjectPtr<UObject>(FSoftObjectPath(StageA))
        );
        Entry.GrowthStageMeshes.Add(
            TSoftObjectPtr<UObject>(FSoftObjectPath(StageB))
        );
        Entry.GrowthStageMeshes.Add(
            TSoftObjectPtr<UObject>(FSoftObjectPath(StageC))
        );
        Entry.GrowthStageMeshes.Add(
            TSoftObjectPtr<UObject>(FSoftObjectPath(StageD))
        );
        SpeciesCatalog.Add(MoveTemp(Entry));
    };

    AddDefaultSpecies(
        TEXT("Elder"),
        BroadleafType,
        TEXT("/Game/Megaplant_Library/Tree_Elder/Tree_Elder_01/Tree_Elder_01_A.Tree_Elder_01_A"),
        TEXT("/Game/Megaplant_Library/Tree_Elder/Tree_Elder_01/Tree_Elder_01_B.Tree_Elder_01_B"),
        TEXT("/Game/Megaplant_Library/Tree_Elder/Tree_Elder_01/Tree_Elder_01_C.Tree_Elder_01_C"),
        TEXT("/Game/Megaplant_Library/Tree_Elder/Tree_Elder_01/Tree_Elder_01_D.Tree_Elder_01_D")
    );

    AddDefaultSpecies(
        TEXT("NorwaySpruce"),
        ConiferType,
        TEXT("/Game/Megaplant_Library/Tree_Norway_Spruce/Tree_Norway_Spruce_01/Tree_Norway_Spruce_01_A.Tree_Norway_Spruce_01_A"),
        TEXT("/Game/Megaplant_Library/Tree_Norway_Spruce/Tree_Norway_Spruce_01/Tree_Norway_Spruce_01_B.Tree_Norway_Spruce_01_B"),
        TEXT("/Game/Megaplant_Library/Tree_Norway_Spruce/Tree_Norway_Spruce_01/Tree_Norway_Spruce_01_C.Tree_Norway_Spruce_01_C"),
        TEXT("/Game/Megaplant_Library/Tree_Norway_Spruce/Tree_Norway_Spruce_01/Tree_Norway_Spruce_01_D.Tree_Norway_Spruce_01_D")
    );

    AddDefaultSpecies(
        TEXT("Greasewood"),
        ShrubType,
        TEXT("/Game/Megaplant_Library/Shrub_Greasewood/Shrub_Greasewood_01/Shrub_Greasewood_01_A.Shrub_Greasewood_01_A"),
        TEXT("/Game/Megaplant_Library/Shrub_Greasewood/Shrub_Greasewood_01/Shrub_Greasewood_01_B.Shrub_Greasewood_01_B"),
        TEXT("/Game/Megaplant_Library/Shrub_Greasewood/Shrub_Greasewood_01/Shrub_Greasewood_01_C.Shrub_Greasewood_01_C"),
        TEXT("/Game/Megaplant_Library/Shrub_Greasewood/Shrub_Greasewood_01/Shrub_Greasewood_01_D.Shrub_Greasewood_01_D")
    );
}

void ACubusWorldVegetationActor::RebuildCatalogLookups()
{
    CatalogSpeciesIndicesByType.Reset();
    CatalogTotalWeightByType.Reset();

    for (int32 SpeciesIndex = 0; SpeciesIndex < SpeciesCatalog.Num(); ++SpeciesIndex)
    {
        const FCubusVegetationSpeciesCatalogEntry& Entry =
            SpeciesCatalog[SpeciesIndex];

        if (Entry.TypeId <= 0 || Entry.GrowthStageMeshes.IsEmpty())
        {
            continue;
        }

        const float SafeWeight = FMath::Max(0.001f, Entry.Weight);
        CatalogSpeciesIndicesByType.FindOrAdd(Entry.TypeId).Add(SpeciesIndex);
        CatalogTotalWeightByType.FindOrAdd(Entry.TypeId) += SafeWeight;
    }
}

int32 ACubusWorldVegetationActor::SelectCatalogSpeciesIndex(
    const FCubusVegetationInstance& Instance
) const
{
    const TArray<int32>* SpeciesIndices =
        CatalogSpeciesIndicesByType.Find(Instance.TypeId);

    if (SpeciesIndices == nullptr || SpeciesIndices->IsEmpty())
    {
        return INDEX_NONE;
    }

    TArray<int32, TInlineAllocator<32>> EligibleSpeciesIndices;
    float EligibleTotalWeight = 0.0f;

    for (const int32 SpeciesIndex : *SpeciesIndices)
    {
        const FCubusVegetationSpeciesCatalogEntry& Entry =
            SpeciesCatalog[SpeciesIndex];

        if ((Entry.BiomeMask & Instance.BiomeMask) == 0)
        {
            continue;
        }

        EligibleSpeciesIndices.Add(SpeciesIndex);
        EligibleTotalWeight += FMath::Max(0.001f, Entry.Weight);
    }

    if (EligibleSpeciesIndices.IsEmpty())
    {
        EligibleSpeciesIndices.Append(*SpeciesIndices);

        for (const int32 SpeciesIndex : EligibleSpeciesIndices)
        {
            EligibleTotalWeight += FMath::Max(
                0.001f,
                SpeciesCatalog[SpeciesIndex].Weight
            );
        }
    }

    if (EligibleTotalWeight <= 0.0f)
    {
        return EligibleSpeciesIndices[0];
    }

    const bool bTreeType =
        Instance.TypeId == BroadleafType ||
        Instance.TypeId == ConiferType;

    uint32 SelectionHash = 0;

    if (bClusterTreeFamilies && bTreeType)
    {
        const int32 CellSize = FMath::Max(4, TreeFamilyCellSizeVoxels);
        const FIntVector FamilyCell(
            FMath::FloorToInt(
                static_cast<double>(Instance.WorldVoxel.X) /
                static_cast<double>(CellSize)
            ),
            FMath::FloorToInt(
                static_cast<double>(Instance.WorldVoxel.Y) /
                static_cast<double>(CellSize)
            ),
            0
        );

        SelectionHash = GetTypeHash(FamilyCell);
        SelectionHash = HashCombineFast(
            SelectionHash,
            GetTypeHash(Instance.TypeId)
        );
        SelectionHash = HashCombineFast(
            SelectionHash,
            GetTypeHash(RuntimeRandomizationSeed)
        );
    }
    else
    {
        SelectionHash = GetTypeHash(Instance.WorldVoxel);
        SelectionHash = HashCombineFast(
            SelectionHash,
            GetTypeHash(Instance.RotationYaw)
        );
    }

    const float Unit =
        static_cast<float>(SelectionHash & 0x00ffffffu) /
        static_cast<float>(0x01000000u);

    float Remaining = Unit * EligibleTotalWeight;

    for (const int32 SpeciesIndex : EligibleSpeciesIndices)
    {
        const FCubusVegetationSpeciesCatalogEntry& Entry =
            SpeciesCatalog[SpeciesIndex];

        const float SafeWeight =
            FMath::Max(0.001f, Entry.Weight);

        Remaining -= SafeWeight;

        if (Remaining <= 0.0f)
        {
            return SpeciesIndex;
        }
    }

    return EligibleSpeciesIndices.Last();
}

int32 ACubusWorldVegetationActor::ResolveGrowthStageIndex(
    const FCubusVegetationInstance& Instance,
    const int32 StageCount
) const
{
    if (StageCount <= 1)
    {
        return 0;
    }

    const bool bTreeType =
        Instance.TypeId == BroadleafType ||
        Instance.TypeId == ConiferType;

    if (bClusterTreeFamilies && bTreeType)
    {
        const int32 CellSize = FMath::Max(4, TreeFamilyCellSizeVoxels);
        const int32 CellX = FMath::FloorToInt(
            static_cast<double>(Instance.WorldVoxel.X) /
            static_cast<double>(CellSize)
        );
        const int32 CellY = FMath::FloorToInt(
            static_cast<double>(Instance.WorldVoxel.Y) /
            static_cast<double>(CellSize)
        );
        const FIntVector FamilyCell(CellX, CellY, 0);

        uint32 FamilyHash = GetTypeHash(FamilyCell);
        FamilyHash = HashCombineFast(FamilyHash, GetTypeHash(Instance.TypeId));
        FamilyHash = HashCombineFast(
            FamilyHash,
            GetTypeHash(RuntimeRandomizationSeed)
        );

        const float CenterJitter = FMath::Clamp(
            TreeFamilyCenterJitterFraction,
            0.0f,
            0.4f
        );
        const float CenterOffsetX =
            (HashToUnitFloat(HashCombineFast(FamilyHash, 0x68bc21ebu)) * 2.0f - 1.0f) *
            CenterJitter;
        const float CenterOffsetY =
            (HashToUnitFloat(HashCombineFast(FamilyHash, 0x02e5be93u)) * 2.0f - 1.0f) *
            CenterJitter;

        const float CenterX =
            (static_cast<float>(CellX) + 0.5f + CenterOffsetX) *
            static_cast<float>(CellSize);
        const float CenterY =
            (static_cast<float>(CellY) + 0.5f + CenterOffsetY) *
            static_cast<float>(CellSize);

        const float DeltaX =
            static_cast<float>(Instance.WorldVoxel.X) + 0.5f - CenterX;
        const float DeltaY =
            static_cast<float>(Instance.WorldVoxel.Y) + 0.5f - CenterY;
        const float MaxFamilyRadius =
            static_cast<float>(CellSize) * FMath::Sqrt(2.0f) * 0.5f;

        float NormalizedDistance =
            FVector2D(DeltaX, DeltaY).Size() /
            FMath::Max(1.0f, MaxFamilyRadius);

        uint32 GrowthNoiseHash = GetTypeHash(Instance.WorldVoxel);
        GrowthNoiseHash = HashCombineFast(GrowthNoiseHash, FamilyHash);
        const float GrowthNoise =
            (HashToUnitFloat(GrowthNoiseHash) * 2.0f - 1.0f) *
            FMath::Clamp(TreeFamilyGrowthNoise, 0.0f, 0.3f);

        NormalizedDistance = FMath::Clamp(
            NormalizedDistance + GrowthNoise,
            0.0f,
            1.0f
        );

        const float MatureRadius = FMath::Clamp(
            MatureTreeCoreRadius,
            0.02f,
            0.4f
        );
        const float YoungRadius = FMath::Clamp(
            FMath::Max(MatureRadius, YoungTreeRingRadius),
            MatureRadius,
            0.8f
        );
        const float SaplingRadius = FMath::Clamp(
            FMath::Max(YoungRadius, SaplingTreeRingRadius),
            YoungRadius,
            1.0f
        );

        if (StageCount >= 4)
        {
            if (NormalizedDistance <= MatureRadius)
            {
                return StageCount - 1;
            }

            if (NormalizedDistance <= YoungRadius)
            {
                return StageCount - 2;
            }

            if (NormalizedDistance <= SaplingRadius)
            {
                return StageCount - 3;
            }

            return 0;
        }

        return FMath::Clamp(
            FMath::RoundToInt(
                (1.0f - NormalizedDistance) *
                static_cast<float>(StageCount - 1)
            ),
            0,
            StageCount - 1
        );
    }

    uint32 GrowthHash = GetTypeHash(Instance.WorldVoxel);
    GrowthHash = HashCombineFast(
        GrowthHash,
        GetTypeHash(Instance.RotationYaw)
    );
    GrowthHash = HashCombineFast(
        GrowthHash,
        GetTypeHash(Instance.Scale)
    );

    return static_cast<int32>(GrowthHash % static_cast<uint32>(StageCount));
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
        CachedUltraDynamicWeatherActor =
            ResolveUltraDynamicWeatherActor(World);
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
            TryReadVectorLikeProperty(
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
        TryReadFloatProperty(
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
            TryReadFloatProperty(
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

            if (TryReadFloatProperty(CachedUltraDynamicWeatherActor, Property->GetFName(), CandidateIntensity))
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
                TryWriteBoolProperty(
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
            CachedGlobalFoliageActor = ResolveGlobalFoliageActor(World);
        }

        if (IsValid(CachedGlobalFoliageActor))
        {
            UpdatedGlobalFoliagePropertyCount +=
                ApplyGlobalFoliageDirectionFlip(CachedGlobalFoliageActor);

            AssignLikelyWindProviderActor(
                CachedGlobalFoliageActor,
                CachedUltraDynamicWeatherActor
            );

            UpdatedGlobalFoliagePropertyCount +=
                ApplyWindToGlobalFoliageObject(
                    CachedGlobalFoliageActor,
                    GlobalFoliageWindDirection,
                    GlobalFoliageWindSpeed
                );

            TInlineComponentArray<UActorComponent*> Components(CachedGlobalFoliageActor);
            for (UActorComponent* ComponentObject : Components)
            {
                UpdatedGlobalFoliagePropertyCount +=
                    ApplyGlobalFoliageDirectionFlip(ComponentObject);

                AssignLikelyWindProviderActor(
                    ComponentObject,
                    CachedUltraDynamicWeatherActor
                );

                UpdatedGlobalFoliagePropertyCount +=
                    ApplyWindToGlobalFoliageObject(
                        ComponentObject,
                        GlobalFoliageWindDirection,
                        GlobalFoliageWindSpeed
                    );
            }

            InvokedGlobalFoliageWindFunctionCount +=
                InvokeLikelyWindRefreshFunctions(CachedGlobalFoliageActor);

            for (UActorComponent* ComponentObject : Components)
            {
                InvokedGlobalFoliageWindFunctionCount +=
                    InvokeLikelyWindRefreshFunctions(ComponentObject);
            }

            SharedWindTransformProvider =
                ResolveWindTransformProviderFromActor(CachedGlobalFoliageActor);
        }
    }

    for (USkeletalMeshComponent* HeroComponent : HeroSkeletalWindComponents)
    {
        if (!IsValid(HeroComponent))
        {
            continue;
        }

        UpdatedSpawnedSkeletalPropertyCount +=
            ApplyWindToGlobalFoliageObject(
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
            ApplyWindToGlobalFoliageObject(
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
        UpdatedSpawnedSkeletalPropertyCount <= 0 &&
        !bLoggedSpawnedSkeletalWindPropertyScan
    )
    {
        UObject* ProbeTarget = nullptr;

        for (USkeletalMeshComponent* HeroComponent : HeroSkeletalWindComponents)
        {
            if (IsValid(HeroComponent))
            {
                ProbeTarget = HeroComponent;
                break;
            }
        }

        if (ProbeTarget == nullptr)
        {
            for (const TPair<int64, TObjectPtr<UInstancedSkinnedMeshComponent>>& Pair
                 : CatalogSkeletalBatchComponents)
            {
                if (IsValid(Pair.Value))
                {
                    ProbeTarget = Pair.Value;
                    break;
                }
            }
        }

        if (IsValid(ProbeTarget))
        {
            FString WindPropertyList;
            int32 WindPropertyCount = 0;

            for (TFieldIterator<FProperty> FieldIt(ProbeTarget->GetClass()); FieldIt; ++FieldIt)
            {
                const FProperty* Property = *FieldIt;

                if (Property == nullptr)
                {
                    continue;
                }

                const FString PropertyName = Property->GetName();

                const bool bLooksWindLike =
                    PropertyName.Contains(TEXT("Wind"), ESearchCase::IgnoreCase) ||
                    PropertyName.Contains(TEXT("Gust"), ESearchCase::IgnoreCase) ||
                    PropertyName.Contains(TEXT("Breeze"), ESearchCase::IgnoreCase);

                if (!bLooksWindLike)
                {
                    continue;
                }

                if (!WindPropertyList.IsEmpty())
                {
                    WindPropertyList.Append(TEXT(", "));
                }

                WindPropertyList.Append(PropertyName);
                ++WindPropertyCount;

                if (WindPropertyCount >= 24)
                {
                    break;
                }
            }

            UE_LOG(
                LogTemp,
                Display,
                TEXT("Cubus spawned skeletal wind property scan: class=%s count=%d names=%s"),
                *ProbeTarget->GetClass()->GetName(),
                WindPropertyCount,
                WindPropertyList.IsEmpty() ? TEXT("none") : *WindPropertyList
            );
        }

        bLoggedSpawnedSkeletalWindPropertyScan = true;
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

void ACubusWorldVegetationActor::ApplyWindParametersToHeroMaterials()
{
    if (!bEnableHeroSkeletalWindMode || HeroSkeletalWindComponents.IsEmpty())
    {
        return;
    }

    const FVector WindDirection = LastBridgedWindDirection.GetSafeNormal();
    const float WindIntensity = FMath::Max(0.0f, LastBridgedWindIntensity);

    if (WindDirection.IsNearlyZero() && WindIntensity <= KINDA_SMALL_NUMBER)
    {
        return;
    }

    const FVector4 WindDirection4(
        WindDirection.X,
        WindDirection.Y,
        WindDirection.Z,
        1.0f
    );

    static const FName DirectionParamNames[] =
    {
        TEXT("WindDirection"),
        TEXT("Wind_Direction"),
        TEXT("Wind Direction"),
        TEXT("DynamicWindDirection"),
        TEXT("GlobalWindDirection"),
        TEXT("UDW_WindDirection"),
        TEXT("TreeWindDirection"),
        TEXT("MainWindDirection"),
        TEXT("PivotPainter_WindDirection")
    };

    static const FName ScalarParamNames[] =
    {
        TEXT("WindIntensity"),
        TEXT("Wind_Intensity"),
        TEXT("Wind Intensity"),
        TEXT("DynamicWindIntensity"),
        TEXT("GlobalWindIntensity"),
        TEXT("WindSpeed"),
        TEXT("GlobalWindSpeed"),
        TEXT("WindStrength"),
        TEXT("Wind_Strength"),
        TEXT("TreeWindIntensity"),
        TEXT("MainWindIntensity"),
        TEXT("SimpleWindIntensity"),
        TEXT("GustStrength"),
        TEXT("GustIntensity"),
        TEXT("PivotPainter_WindIntensity")
    };

    int32 UpdatedMaterialCount = 0;

    for (USkeletalMeshComponent* HeroComponent : HeroSkeletalWindComponents)
    {
        if (!IsValid(HeroComponent))
        {
            continue;
        }

        const int32 MaterialCount = HeroComponent->GetNumMaterials();

        for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
        {
            UMaterialInstanceDynamic* MaterialInstance =
                Cast<UMaterialInstanceDynamic>(
                    HeroComponent->GetMaterial(MaterialIndex)
                );

            if (!IsValid(MaterialInstance))
            {
                MaterialInstance =
                    HeroComponent->CreateAndSetMaterialInstanceDynamic(
                        MaterialIndex
                    );
            }

            if (!IsValid(MaterialInstance))
            {
                continue;
            }

            for (const FName ParamName : DirectionParamNames)
            {
                MaterialInstance->SetVectorParameterValue(
                    ParamName,
                    FLinearColor(
                        WindDirection4.X,
                        WindDirection4.Y,
                        WindDirection4.Z,
                        WindDirection4.W
                    )
                );
            }

            for (const FName ParamName : ScalarParamNames)
            {
                MaterialInstance->SetScalarParameterValue(
                    ParamName,
                    WindIntensity
                );
            }

            // Also target any wind-like parameter names authored in asset materials.
            UpdatedMaterialCount += ApplyWindToMaterialInstanceDynamic(
                MaterialInstance,
                FLinearColor(
                    WindDirection4.X,
                    WindDirection4.Y,
                    WindDirection4.Z,
                    WindDirection4.W
                ),
                WindIntensity
            );

            ++UpdatedMaterialCount;
        }
    }

    if (!bLoggedHeroMaterialWindBridge && UpdatedMaterialCount > 0)
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Cubus hero material wind bridge: updated %d material slots"),
            UpdatedMaterialCount
        );

        bLoggedHeroMaterialWindBridge = true;
    }
}

void ACubusWorldVegetationActor::ApplyWindParametersToSpawnedSkinnedMaterials()
{
    if (CatalogSkeletalBatchComponents.IsEmpty())
    {
        return;
    }

    const FVector WindDirection = LastBridgedWindDirection.GetSafeNormal();
    const float WindIntensity = FMath::Max(0.0f, LastBridgedWindIntensity);

    if (WindDirection.IsNearlyZero() && WindIntensity <= KINDA_SMALL_NUMBER)
    {
        return;
    }

    static const FName DirectionParamNames[] =
    {
        TEXT("WindDirection"),
        TEXT("Wind_Direction"),
        TEXT("Wind Direction"),
        TEXT("DynamicWindDirection"),
        TEXT("GlobalWindDirection"),
        TEXT("UDW_WindDirection"),
        TEXT("TreeWindDirection"),
        TEXT("MainWindDirection"),
        TEXT("PivotPainter_WindDirection")
    };

    static const FName ScalarParamNames[] =
    {
        TEXT("WindIntensity"),
        TEXT("Wind_Intensity"),
        TEXT("Wind Intensity"),
        TEXT("DynamicWindIntensity"),
        TEXT("GlobalWindIntensity"),
        TEXT("WindSpeed"),
        TEXT("GlobalWindSpeed"),
        TEXT("WindStrength"),
        TEXT("Wind_Strength"),
        TEXT("TreeWindIntensity"),
        TEXT("MainWindIntensity"),
        TEXT("SimpleWindIntensity"),
        TEXT("GustStrength"),
        TEXT("GustIntensity"),
        TEXT("PivotPainter_WindIntensity")
    };

    const FLinearColor WindDirectionColor(
        WindDirection.X,
        WindDirection.Y,
        WindDirection.Z,
        1.0f
    );

    int32 UpdatedMaterialCount = 0;
    static bool bLoggedInstancedWindParameterDiscovery = false;

    for (const TPair<int64, TObjectPtr<UInstancedSkinnedMeshComponent>>& Pair
         : CatalogSkeletalBatchComponents)
    {
        UInstancedSkinnedMeshComponent* Component = Pair.Value;

        if (!IsValid(Component))
        {
            continue;
        }

        const int32 MaterialCount = Component->GetNumMaterials();

        for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
        {
            UMaterialInstanceDynamic* MaterialInstance =
                Cast<UMaterialInstanceDynamic>(Component->GetMaterial(MaterialIndex));

            if (!IsValid(MaterialInstance))
            {
                MaterialInstance = Component->CreateAndSetMaterialInstanceDynamic(
                    MaterialIndex
                );
            }

            if (!IsValid(MaterialInstance))
            {
                continue;
            }

            if (!bLoggedInstancedWindParameterDiscovery)
            {
                FString ScalarNames;
                FString VectorNames;
                int32 ScalarCount = 0;
                int32 VectorCount = 0;

                TArray<FMaterialParameterInfo> VectorParameterInfo;
                TArray<FGuid> VectorParameterIds;
                MaterialInstance->GetAllVectorParameterInfo(
                    VectorParameterInfo,
                    VectorParameterIds
                );

                for (const FMaterialParameterInfo& ParameterInfo : VectorParameterInfo)
                {
                    const FString Name = ParameterInfo.Name.ToString();
                    const bool bWindLike =
                        Name.Contains(TEXT("Wind"), ESearchCase::IgnoreCase) ||
                        Name.Contains(TEXT("Breeze"), ESearchCase::IgnoreCase) ||
                        Name.Contains(TEXT("Gust"), ESearchCase::IgnoreCase) ||
                        Name.Contains(TEXT("Sway"), ESearchCase::IgnoreCase);

                    if (!bWindLike)
                    {
                        continue;
                    }

                    if (!VectorNames.IsEmpty())
                    {
                        VectorNames.Append(TEXT(", "));
                    }

                    VectorNames.Append(Name);
                    ++VectorCount;

                    if (VectorCount >= 24)
                    {
                        break;
                    }
                }

                TArray<FMaterialParameterInfo> ScalarParameterInfo;
                TArray<FGuid> ScalarParameterIds;
                MaterialInstance->GetAllScalarParameterInfo(
                    ScalarParameterInfo,
                    ScalarParameterIds
                );

                for (const FMaterialParameterInfo& ParameterInfo : ScalarParameterInfo)
                {
                    const FString Name = ParameterInfo.Name.ToString();
                    const bool bWindLike =
                        Name.Contains(TEXT("Wind"), ESearchCase::IgnoreCase) ||
                        Name.Contains(TEXT("Breeze"), ESearchCase::IgnoreCase) ||
                        Name.Contains(TEXT("Gust"), ESearchCase::IgnoreCase) ||
                        Name.Contains(TEXT("Sway"), ESearchCase::IgnoreCase);

                    if (!bWindLike)
                    {
                        continue;
                    }

                    if (!ScalarNames.IsEmpty())
                    {
                        ScalarNames.Append(TEXT(", "));
                    }

                    ScalarNames.Append(Name);
                    ++ScalarCount;

                    if (ScalarCount >= 24)
                    {
                        break;
                    }
                }

                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT("Cubus instanced material wind params: material=%s vectors=%d[%s] scalars=%d[%s]"),
                    *MaterialInstance->GetName(),
                    VectorCount,
                    VectorNames.IsEmpty() ? TEXT("none") : *VectorNames,
                    ScalarCount,
                    ScalarNames.IsEmpty() ? TEXT("none") : *ScalarNames
                );

                bLoggedInstancedWindParameterDiscovery = true;
            }

            for (const FName ParamName : DirectionParamNames)
            {
                MaterialInstance->SetVectorParameterValue(
                    ParamName,
                    WindDirectionColor
                );
            }

            for (const FName ParamName : ScalarParamNames)
            {
                MaterialInstance->SetScalarParameterValue(
                    ParamName,
                    WindIntensity
                );
            }

            // Also target any wind-like parameter names authored in asset materials.
            UpdatedMaterialCount += ApplyWindToMaterialInstanceDynamic(
                MaterialInstance,
                WindDirectionColor,
                WindIntensity
            );

            ++UpdatedMaterialCount;
        }

        Component->MarkRenderDynamicDataDirty();
    }

    if (!bLoggedInstancedMaterialWindBridge && UpdatedMaterialCount > 0)
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Cubus instanced material wind bridge: updated %d material slots"),
            UpdatedMaterialCount
        );

        bLoggedInstancedMaterialWindBridge = true;
    }
}

void ACubusWorldVegetationActor::ApplyHeroWindVisualSway(float DeltaSeconds)
{
    if (!bEnableHeroSkeletalWindMode || !bForceHeroWindVisualSway)
    {
        return;
    }

    if (HeroSkeletalWindComponents.IsEmpty())
    {
        return;
    }

    HeroWindSwayTime += FMath::Max(0.0f, DeltaSeconds);

    const FVector WindDirection = LastBridgedWindDirection.GetSafeNormal();

    if (WindDirection.IsNearlyZero())
    {
        return;
    }

    const float WindStrength = FMath::Clamp(
        LastBridgedWindIntensity * HeroWindSwayIntensityScale,
        0.0f,
        1.0f
    );

    if (WindStrength <= KINDA_SMALL_NUMBER)
    {
        return;
    }

    const float MaxSwayDegrees = FMath::Max(0.0f, HeroWindSwayMaxDegrees);
    const float Frequency = FMath::Max(0.01f, HeroWindSwayFrequency);
    const FVector WindRight =
        FVector::CrossProduct(FVector::UpVector, WindDirection).GetSafeNormal();

    for (int32 ComponentIndex = 0;
         ComponentIndex < HeroSkeletalWindComponents.Num();
         ++ComponentIndex)
    {
        USkeletalMeshComponent* HeroComponent =
            HeroSkeletalWindComponents[ComponentIndex];

        if (!IsValid(HeroComponent) || !HeroComponent->IsVisible())
        {
            continue;
        }

        if (!HeroSkeletalWindBaseLocalTransforms.IsValidIndex(ComponentIndex))
        {
            continue;
        }

        const FTransform BaseTransform =
            HeroSkeletalWindBaseLocalTransforms[ComponentIndex];

        const float Phase =
            HeroWindSwayTime * Frequency +
            static_cast<float>(ComponentIndex) * 0.79f;

        const float PrimarySwayRadians =
            FMath::DegreesToRadians(
                FMath::Sin(Phase) * MaxSwayDegrees * WindStrength
            );

        const float SecondarySwayRadians =
            FMath::DegreesToRadians(
                FMath::Cos(Phase * 0.61f) * MaxSwayDegrees * 0.42f * WindStrength
            );

        const FQuat PrimaryTilt =
            FQuat(WindRight, PrimarySwayRadians);

        const FQuat SecondaryTilt =
            FQuat(WindDirection, SecondarySwayRadians);

        FTransform SwayedTransform = BaseTransform;
        SwayedTransform.SetRotation(
            (SecondaryTilt * PrimaryTilt * BaseTransform.GetRotation())
                .GetNormalized()
        );

        HeroComponent->SetRelativeTransform(
            SwayedTransform,
            false,
            nullptr,
            ETeleportType::None
        );
    }

    static double LastSwayDebugLogTime = 0.0;
    const double Now = FPlatformTime::Seconds();

    if (Now - LastSwayDebugLogTime >= 2.0)
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Cubus hero wind sway: active=%d strength=%.3f maxDeg=%.2f"),
            HeroSkeletalWindComponents.Num(),
            WindStrength,
            MaxSwayDegrees
        );

        LastSwayDebugLogTime = Now;
    }
}

void ACubusWorldVegetationActor::RebuildWorldVegetation()
{
    ResolveBlockWorld();
    EnsurePointCarriers();

    if (
        !GetWorld() ||
        !GetWorld()->IsGameWorld() ||
        (
            CatalogStaticBatchComponents.IsEmpty() &&
            CatalogSkeletalBatchComponents.IsEmpty()
        )
    )
    {
        EnsurePlantBatches();
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

    const int32 PointLimit = MaximumPublishedPoints > 0
        ? MaximumPublishedPoints
        : MAX_int32;

    const int32 PlantLimit = MaximumRenderedPlants > 0
        ? MaximumRenderedPlants
        : MAX_int32;

    const uint32 CurrentVegetationSettingsHash =
        CalculateVegetationSettingsHash();
    const uint32 CurrentRandomizationSettingsHash =
        CalculateRuntimeRandomizationSettingsHash();

    if (
        bEnableRuntimeRandomization &&
        (
            !bRuntimeRandomizationStreamInitialized ||
            RuntimeRandomizationSeedSnapshot != RuntimeRandomizationSeed ||
            RuntimeRandomizationSettingsHashSnapshot !=
                CurrentRandomizationSettingsHash
        )
    )
    {
        const int32 SessionSeed = HashCombineFast(
            GetTypeHash(RuntimeRandomizationSeed),
            static_cast<uint32>(FPlatformTime::Cycles64())
        );

        RuntimeRandomizationStream.Initialize(SessionSeed);
        RuntimeRandomizationSamplesByPlant.Reset();
        RuntimeRandomizationSeedSnapshot = RuntimeRandomizationSeed;
        RuntimeRandomizationSettingsHashSnapshot =
            CurrentRandomizationSettingsHash;
        bRuntimeRandomizationStreamInitialized = true;
    }

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

    const auto IsChunkWithinCameraRadius =
        [this, bUseCameraChunkCulling, CameraLocation](
            const ACubusVoxelVolumeActor* Chunk
        ) -> bool
    {
        if (!bUseCameraChunkCulling)
        {
            return true;
        }

        const float SafeVoxelSize = FMath::Max(1.0f, Chunk->GetVoxelSize());
        const double ChunkWorldSize =
            static_cast<double>(Cubus::ChunkSize) *
            static_cast<double>(SafeVoxelSize);
        const double HalfChunkWorldSize = ChunkWorldSize * 0.5;

        const FIntVector CameraChunk(
            FMath::FloorToInt((CameraLocation.X + HalfChunkWorldSize) / ChunkWorldSize),
            FMath::FloorToInt((CameraLocation.Y + HalfChunkWorldSize) / ChunkWorldSize),
            FMath::FloorToInt((CameraLocation.Z + HalfChunkWorldSize) / ChunkWorldSize)
        );

        const FIntVector ChunkCoordinate = Chunk->GetChunkCoordinate();

        return
            FMath::Abs(ChunkCoordinate.X - CameraChunk.X) <= CameraChunkHorizontalRadius &&
            FMath::Abs(ChunkCoordinate.Y - CameraChunk.Y) <= CameraChunkHorizontalRadius &&
            FMath::Abs(ChunkCoordinate.Z - CameraChunk.Z) <= CameraChunkVerticalRadius;
    };

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
            !IsChunkWithinCameraRadius(Chunk)
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

    TMap<int64, TArray<FTransform>> CatalogTransformsByBatchKey;
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
            !IsChunkWithinCameraRadius(Chunk) ||
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

            const uint32 RandomKeyA = HashCombineFast(
                GetTypeHash(Instance.WorldVoxel),
                GetTypeHash(Instance.TypeId)
            );
            const uint32 RandomKeyB = HashCombineFast(
                GetTypeHash(Instance.RotationYaw),
                GetTypeHash(Instance.Scale)
            );

            const uint64 PlantRandomKey =
                (static_cast<uint64>(RandomKeyA) << 32) |
                static_cast<uint64>(RandomKeyB);

            FCubusRuntimeRandomizationSample* RuntimeSample = nullptr;

            if (bEnableRuntimeRandomization)
            {
                RuntimeSample =
                    RuntimeRandomizationSamplesByPlant.Find(PlantRandomKey);

                if (RuntimeSample == nullptr)
                {
                    FCubusRuntimeRandomizationSample NewSample;

                    const float JitterMin = FMath::Max(
                        0.01f,
                        FMath::Min(RandomScaleJitterMin, RandomScaleJitterMax)
                    );
                    const float JitterMax = FMath::Max(
                        0.01f,
                        FMath::Max(RandomScaleJitterMin, RandomScaleJitterMax)
                    );

                    NewSample.bPruned =
                        RuntimeRandomizationStream.FRand() <
                        FMath::Clamp(RandomPruneProbability, 0.0f, 1.0f);

                    NewSample.ScaleMultiplier =
                        RuntimeRandomizationStream.FRandRange(JitterMin, JitterMax);

                    NewSample.PositionJitterUnit = FVector2f(
                        RuntimeRandomizationStream.FRandRange(-1.0f, 1.0f),
                        RuntimeRandomizationStream.FRandRange(-1.0f, 1.0f)
                    );

                    NewSample.YawJitterUnit =
                        RuntimeRandomizationStream.FRandRange(-1.0f, 1.0f);

                    RuntimeRandomizationSamplesByPlant.Add(
                        PlantRandomKey,
                        NewSample
                    );

                    RuntimeSample =
                        RuntimeRandomizationSamplesByPlant.Find(PlantRandomKey);
                }

                if (RuntimeSample != nullptr)
                {
                    ++RandomizedPlantCount;
                    ObservedRandomScaleMin = FMath::Min(
                        ObservedRandomScaleMin,
                        RuntimeSample->ScaleMultiplier
                    );
                    ObservedRandomScaleMax = FMath::Max(
                        ObservedRandomScaleMax,
                        RuntimeSample->ScaleMultiplier
                    );

                    if (RuntimeSample->bPruned)
                    {
                        ++RandomPrunedPlantCount;
                        continue;
                    }
                }
            }

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

            float FinalScale = CombinedScale;
            FVector FinalLocation = WorldLocation;
            float FinalYaw = Instance.RotationYaw;

            if (bEnableRuntimeRandomization)
            {
                const float JitterScale =
                    RuntimeSample != nullptr
                        ? RuntimeSample->ScaleMultiplier
                        : 1.0f;

                FinalScale = FMath::Max(0.01f, CombinedScale * JitterScale);

                const float JitterFraction = FMath::Clamp(
                    RandomPositionJitterVoxelFraction,
                    0.0f,
                    0.49f
                );

                if (JitterFraction > 0.0f)
                {
                    const float JitterExtentCm = SafeVoxelSize * JitterFraction;

                    const FVector2f JitterUnit =
                        RuntimeSample != nullptr
                            ? RuntimeSample->PositionJitterUnit
                            : FVector2f::ZeroVector;

                    const float OffsetX =
                        static_cast<float>(JitterUnit.X) * JitterExtentCm;
                    const float OffsetY =
                        static_cast<float>(JitterUnit.Y) * JitterExtentCm;

                    FinalLocation.X += OffsetX;
                    FinalLocation.Y += OffsetY;
                }

                if (RandomYawJitterDegrees > 0.0f)
                {
                    const float YawOffset =
                        (RuntimeSample != nullptr
                            ? RuntimeSample->YawJitterUnit
                            : 0.0f) *
                        FMath::Clamp(RandomYawJitterDegrees, 0.0f, 180.0f);

                    FinalYaw += YawOffset;
                }
            }

            const FTransform WorldTransform(
                FRotator(0.0f, FinalYaw, 0.0f),
                FinalLocation,
                FVector(FinalScale)
            );

            if (PublishedPointCount < PointLimit)
            {
                UInstancedStaticMeshComponent* TargetCarrier =
                    ResolveCarrierForType(Instance.TypeId);

                if (IsValid(TargetCarrier))
                {
                    TargetCarrier->AddInstance(WorldTransform, true);
                    ++PublishedPointCount;
                }
            }

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
                SelectCatalogSpeciesIndex(Instance);

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
                ResolveGrowthStageIndex(Instance, StageCount);

            const int64 BatchKey =
                (static_cast<int64>(SpeciesIndex) << 32) |
                static_cast<uint32>(GrowthStage);

            if (
                !CatalogStaticBatchComponents.Contains(BatchKey) &&
                !CatalogSkeletalBatchComponents.Contains(BatchKey)
            )
            {
                continue;
            }

            CatalogTransformsByBatchKey
                .FindOrAdd(BatchKey)
                .Add(LocalTransform);
            ++RenderedPlantCount;
        }
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

    auto AcquireHeroComponent = [this](const int32 Index)
        -> USkeletalMeshComponent*
    {
        if (HeroSkeletalWindComponents.IsValidIndex(Index))
        {
            if (!HeroSkeletalWindBaseLocalTransforms.IsValidIndex(Index))
            {
                HeroSkeletalWindBaseLocalTransforms.SetNum(Index + 1);
            }

            return HeroSkeletalWindComponents[Index];
        }

        const FName ComponentName(
            *FString::Printf(
                TEXT("CubusWorldHeroSkeletalWind_%d"),
                Index
            )
        );

        USkeletalMeshComponent* NewComponent =
            CreateHeroSkeletalWindComponent(ComponentName);

        HeroSkeletalWindComponents.Add(NewComponent);

        if (!HeroSkeletalWindBaseLocalTransforms.IsValidIndex(Index))
        {
            HeroSkeletalWindBaseLocalTransforms.SetNum(Index + 1);
        }

        return NewComponent;
    };

    auto AcquireHeroPveActor = [this, World](
        const int32 Index,
        UClass* DesiredClass
    ) -> AActor*
    {
        if (!IsValid(World) || DesiredClass == nullptr)
        {
            return nullptr;
        }

        if (HeroPveWindActors.IsValidIndex(Index))
        {
            AActor* ExistingActor = HeroPveWindActors[Index];

            if (IsValid(ExistingActor) && ExistingActor->GetClass() == DesiredClass)
            {
                return ExistingActor;
            }

            if (IsValid(ExistingActor))
            {
                ExistingActor->Destroy();
            }
        }

        const FName ActorName(
            *FString::Printf(TEXT("CubusWorldHeroPveWind_%d"), Index)
        );

        FActorSpawnParameters SpawnParameters;
        SpawnParameters.Owner = this;
        SpawnParameters.Name = ActorName;
        SpawnParameters.SpawnCollisionHandlingOverride =
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        AActor* NewActor = World->SpawnActor<AActor>(
            DesiredClass,
            FTransform::Identity,
            SpawnParameters
        );

        if (!IsValid(NewActor))
        {
            return nullptr;
        }

        NewActor->AttachToComponent(
            Root,
            FAttachmentTransformRules::KeepRelativeTransform
        );

        if (!HeroPveWindActors.IsValidIndex(Index))
        {
            HeroPveWindActors.SetNum(Index + 1);
        }

        HeroPveWindActors[Index] = NewActor;
        return NewActor;
    };

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

        const TArray<FTransform>* Transforms =
            CatalogTransformsByBatchKey.Find(Pair.Key);

        if (Transforms == nullptr || Transforms->IsEmpty())
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
                    ResolveMegaplantTransformProviderBySpecies(
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

            const bool bHasNativeDynamicWindProvider =
                Cast<UDynamicWindData>(Component->GetTransformProvider()) != nullptr;

            if (bUseHeroPveActorWindMode && !bHasNativeDynamicWindProvider)
            {
                HeroPveActorClass =
                    ResolveMegaplantPveActorClassBySpecies(
                        SpeciesEntry
                    );
            }

            if (bForceMegaplantFoliageMaterialOverride)
            {
                const TSoftObjectPtr<UMaterialInterface> FoliageMaterialRef =
                    ResolveMegaplantFoliageMaterialBySpecies(
                        SpeciesEntry.SpeciesId
                    );

                if (!FoliageMaterialRef.IsNull())
                {
                    FoliageOverrideMaterial = FoliageMaterialRef.LoadSynchronous();
                    if (IsValid(FoliageOverrideMaterial))
                    {
                        ++FoliageMaterialOverrideComponentCount;
                    }
                    ApplyFoliageMaterialOverrideToSkinnedComponent(
                        Component,
                        FoliageOverrideMaterial
                    );
                }
            }
        }

        if (
            !bEnableHeroSkeletalWindMode ||
            Cast<UDynamicWindData>(Component->GetTransformProvider()) != nullptr
        )
        {
            TArray<int32> AnimationIndices;
            AnimationIndices.Init(0, Transforms->Num());

            Component->AddInstances(*Transforms, AnimationIndices, false, false);

            if (!bAppendOnly)
            {
                Component->OptimizeInstanceData(false);
            }

            SkeletalBatchTransformCount += Transforms->Num();
            continue;
        }

        const USkeletalMesh* SkeletalMesh =
            Cast<USkeletalMesh>(Component->GetSkinnedAsset());

        if (!IsValid(SkeletalMesh))
        {
            continue;
        }

        const float MaxHeroDistance =
            FMath::Max(0.0f, HeroSkeletalWindMaxDistance);
        const float MaxHeroDistanceSquared =
            MaxHeroDistance * MaxHeroDistance;

        const int32 HeroComponentLimit =
            MaxHeroSkeletalWindComponents > 0
                ? FMath::Clamp(MaxHeroSkeletalWindComponents, 0, 64)
                : 0;

        TArray<FTransform> FallbackTransforms;

        for (const FTransform& LocalTransform : *Transforms)
        {
            bool bUseHeroComponent =
                HeroComponentLimit > 0 &&
                ActiveHeroComponentCount < HeroComponentLimit;

            if (bUseHeroComponent && bHasCamera)
            {
                const FVector WorldLocation =
                    GetActorTransform().TransformPosition(
                        LocalTransform.GetLocation()
                    );

                const float DistanceSquared =
                    FVector::DistSquared(WorldLocation, CameraLocation);

                if (DistanceSquared > MaxHeroDistanceSquared)
                {
                    bUseHeroComponent = false;
                }
            }

            if (bUseHeroComponent)
            {
                const int32 HeroComponentIndex = ActiveHeroComponentCount;

                if (bUseHeroPveActorWindMode && HeroPveActorClass != nullptr)
                {
                    AActor* HeroPveActor =
                        AcquireHeroPveActor(
                            HeroComponentIndex,
                            HeroPveActorClass
                        );

                    if (IsValid(HeroPveActor))
                    {
                        HeroPveActor->SetActorRelativeTransform(
                            LocalTransform,
                            false,
                            nullptr,
                            ETeleportType::None
                        );
                        HeroPveActor->SetActorHiddenInGame(false);

                        AssignLikelyWindProviderActor(
                            HeroPveActor,
                            CachedUltraDynamicWeatherActor
                        );

                        TInlineComponentArray<UActorComponent*> Components(HeroPveActor);
                        for (UActorComponent* ComponentObject : Components)
                        {
                            AssignLikelyWindProviderActor(
                                ComponentObject,
                                CachedUltraDynamicWeatherActor
                            );
                        }

                        ++ActiveHeroPveActorCount;
                        ++ActiveHeroComponentCount;
                        continue;
                    }
                }

                USkeletalMeshComponent* HeroComponent =
                    AcquireHeroComponent(HeroComponentIndex);

                if (IsValid(HeroComponent))
                {
                    if (HeroComponent->GetSkeletalMeshAsset() != SkeletalMesh)
                    {
                        HeroComponent->SetSkeletalMesh(
                            const_cast<USkeletalMesh*>(SkeletalMesh)
                        );
                    }

                    if (!bForceMegaplantFoliageMaterialOverride)
                    {
                        HeroComponent->EmptyOverrideMaterials();
                    }

                    if (bForceMegaplantFoliageMaterialOverride)
                    {
                        ApplyFoliageMaterialOverrideToSkinnedComponent(
                            HeroComponent,
                            FoliageOverrideMaterial
                        );
                    }

                    HeroComponent->SetRelativeTransform(LocalTransform);

                    if (HeroSkeletalWindBaseLocalTransforms.IsValidIndex(HeroComponentIndex))
                    {
                        HeroSkeletalWindBaseLocalTransforms[HeroComponentIndex] =
                            LocalTransform;
                    }

                    HeroComponent->SetVisibility(true, true);
                    HeroComponent->SetHiddenInGame(false, true);
                    ++ActiveHeroComponentCount;
                    continue;
                }
            }

            if (
                bUseInstancedSkeletalFallbackBeyondHeroDistance &&
                RemainingInstancedSkeletalFallbackBudget > 0
            )
            {
                FallbackTransforms.Add(LocalTransform);
                --RemainingInstancedSkeletalFallbackBudget;
            }
        }

        if (bUseInstancedSkeletalFallbackBeyondHeroDistance)
        {
            if (!FallbackTransforms.IsEmpty())
            {
                TArray<int32> AnimationIndices;
                AnimationIndices.Init(0, FallbackTransforms.Num());

                Component->AddInstances(
                    FallbackTransforms,
                    AnimationIndices,
                    false,
                    false
                );

                if (!bAppendOnly)
                {
                    Component->OptimizeInstanceData(false);
                }

                SkeletalBatchTransformCount += FallbackTransforms.Num();
                InstancedSkeletalFallbackCount += FallbackTransforms.Num();
            }
        }
    }

    UInstancedStaticMeshComponent* Carriers[] =
    {
        GrassPoints,
        ShrubPoints,
        TreePoints,
        ConiferTreePoints,
        ReedsPoints,
        AlpinePoints
    };

    for (UInstancedStaticMeshComponent* Carrier : Carriers)
    {
        if (IsValid(Carrier))
        {
            Carrier->MarkRenderStateDirty();
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
            "Cubus world vegetation: %d chunks, %d points, %d world-batched plants (static=%d, skeletal-instanced=%d, hero=%d, heroPveActors=%d, fallback=%d, foliage-overrides=%d, heroPve=%s, fallbackInstanced=%s)"
        ),
        LoadedChunkCount,
        PublishedPointCount,
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
            TEXT("Cubus per-tree randomization: sampled=%d pruned=%d scaleRange=(%.3f, %.3f) cache=%d"),
            RandomizedPlantCount,
            RandomPrunedPlantCount,
            ObservedRandomScaleMin,
            ObservedRandomScaleMax,
            RuntimeRandomizationSamplesByPlant.Num()
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
    PublishedPointCount = 0;
    RenderedPlantCount = 0;
    HeroWindSwayTime = 0.0f;
    PublishedChunkVegetationSignatures.Reset();
    PublishedVegetationSettingsHash = 0;

    UInstancedStaticMeshComponent* Carriers[] =
    {
        GrassPoints,
        ShrubPoints,
        TreePoints,
        ConiferTreePoints,
        ReedsPoints,
        AlpinePoints
    };

    for (UInstancedStaticMeshComponent* Carrier : Carriers)
    {
        if (IsValid(Carrier))
        {
            Carrier->ClearInstances();
        }
    }

    for (const TPair<int64, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>>& Pair
         : CatalogStaticBatchComponents)
    {
        UHierarchicalInstancedStaticMeshComponent* PlantBatch = Pair.Value;

        if (IsValid(PlantBatch))
        {
            PlantBatch->ClearInstances();
        }
    }

    for (const TPair<int64, TObjectPtr<UInstancedSkinnedMeshComponent>>& Pair
         : CatalogSkeletalBatchComponents)
    {
        UInstancedSkinnedMeshComponent* PlantBatch = Pair.Value;

        if (IsValid(PlantBatch))
        {
            PlantBatch->ClearInstances();
        }
    }

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

    HeroSkeletalWindBaseLocalTransforms.Reset();
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

void ACubusWorldVegetationActor::EnsurePointCarriers()
{
    if (!IsValid(GrassPoints))
    {
        GrassPoints = CreatePointCarrier(
            TEXT("CubusWorldGrassPoints"),
            TEXT("Cubus.Vegetation.Grass")
        );
    }

    if (!IsValid(ShrubPoints))
    {
        ShrubPoints = CreatePointCarrier(
            TEXT("CubusWorldShrubPoints"),
            TEXT("Cubus.Vegetation.Shrub")
        );
    }

    if (!IsValid(TreePoints))
    {
        TreePoints = CreatePointCarrier(
            TEXT("CubusWorldTreePoints"),
            TEXT("Cubus.Vegetation.Tree.Broadleaf")
        );
    }

    if (!IsValid(ConiferTreePoints))
    {
        ConiferTreePoints = CreatePointCarrier(
            TEXT("CubusWorldConiferTreePoints"),
            TEXT("Cubus.Vegetation.Tree.Conifer")
        );
    }

    if (!IsValid(ReedsPoints))
    {
        ReedsPoints = CreatePointCarrier(
            TEXT("CubusWorldReedsPoints"),
            TEXT("Cubus.Vegetation.Reeds")
        );
    }

    if (!IsValid(AlpinePoints))
    {
        AlpinePoints = CreatePointCarrier(
            TEXT("CubusWorldAlpinePoints"),
            TEXT("Cubus.Vegetation.Alpine")
        );
    }

    UInstancedStaticMeshComponent* Carriers[] =
    {
        GrassPoints,
        ShrubPoints,
        TreePoints,
        ConiferTreePoints,
        ReedsPoints,
        AlpinePoints
    };

    for (UInstancedStaticMeshComponent* Carrier : Carriers)
    {
        if (!IsValid(Carrier))
        {
            continue;
        }

        Carrier->SetStaticMesh(MarkerMesh);
        Carrier->SetVisibility(bShowDebugMarkers, true);
        Carrier->SetHiddenInGame(!bShowDebugMarkers, true);
    }
}

void ACubusWorldVegetationActor::EnsurePlantBatches()
{
    if (!bRenderWorldPlantBatches)
    {
        return;
    }

    BuildDefaultSpeciesCatalogIfNeeded();
    RebuildCatalogLookups();

    TMap<int64, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> NewStaticBatches;
    TMap<int64, TObjectPtr<UInstancedSkinnedMeshComponent>> NewSkeletalBatches;

    for (int32 SpeciesIndex = 0; SpeciesIndex < SpeciesCatalog.Num(); ++SpeciesIndex)
    {
        const FCubusVegetationSpeciesCatalogEntry& Entry = SpeciesCatalog[SpeciesIndex];

        if (Entry.TypeId <= 0)
        {
            continue;
        }

        for (int32 StageIndex = 0; StageIndex < Entry.GrowthStageMeshes.Num(); ++StageIndex)
        {
            const TSoftObjectPtr<UObject>& MeshReference = Entry.GrowthStageMeshes[StageIndex];

            if (MeshReference.IsNull())
            {
                continue;
            }

            UObject* MeshAsset = MeshReference.LoadSynchronous();

            const int64 BatchKey =
                (static_cast<int64>(SpeciesIndex) << 32) |
                static_cast<uint32>(StageIndex);

            if (!IsValid(MeshAsset))
            {
                UE_LOG(
                    LogTemp,
                    Warning,
                    TEXT("Cubus vegetation could not load mesh asset %s"),
                    *MeshReference.ToSoftObjectPath().ToString()
                );
                continue;
            }

            const FString SpeciesToken = Entry.SpeciesId.IsNone()
                ? FString::Printf(TEXT("Species%d"), SpeciesIndex)
                : Entry.SpeciesId.ToString();

            if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(MeshAsset))
            {
                if (UInstancedSkinnedMeshComponent* OldSkeletal =
                        CatalogSkeletalBatchComponents.FindRef(BatchKey))
                {
                    OldSkeletal->ClearInstances();
                    OldSkeletal->DestroyComponent();
                    CatalogSkeletalBatchComponents.Remove(BatchKey);
                }

                UHierarchicalInstancedStaticMeshComponent* Component =
                    CatalogStaticBatchComponents.FindRef(BatchKey);

                if (!IsValid(Component))
                {
                    const FName ComponentName(
                        *FString::Printf(
                            TEXT("CubusWorldCatalogStatic_%s_%d"),
                            *SpeciesToken,
                            StageIndex
                        )
                    );

                    Component = CreatePlantBatch(ComponentName);
                }

                if (!IsValid(Component))
                {
                    continue;
                }

                Component->SetStaticMesh(StaticMesh);
                Component->SetCastShadow(bCastWorldPlantShadows);
                Component->SetCullDistances(
                    FMath::Max(0, PlantStartCullDistance),
                    FMath::Max(PlantStartCullDistance, PlantEndCullDistance)
                );

                NewStaticBatches.Add(BatchKey, Component);
                continue;
            }

            if (USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(MeshAsset))
            {
                if (UHierarchicalInstancedStaticMeshComponent* OldStatic =
                        CatalogStaticBatchComponents.FindRef(BatchKey))
                {
                    OldStatic->ClearInstances();
                    OldStatic->DestroyComponent();
                    CatalogStaticBatchComponents.Remove(BatchKey);
                }

                UInstancedSkinnedMeshComponent* Component =
                    CatalogSkeletalBatchComponents.FindRef(BatchKey);

                if (!IsValid(Component))
                {
                    const FName ComponentName(
                        *FString::Printf(
                            TEXT("CubusWorldCatalogSkeletal_%s_%d"),
                            *SpeciesToken,
                            StageIndex
                        )
                    );

                    Component = CreateSkeletalPlantBatch(ComponentName);
                }

                if (!IsValid(Component))
                {
                    continue;
                }

                Component->SetSkinnedAssetAndUpdate(SkeletalMesh);
                Component->VisibilityBasedAnimTickOption =
                    EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
                Component->SetAnimationMinScreenSize(-1.0f);

                static TSet<FName> LoggedDynamicWindMeshes;
                if (!LoggedDynamicWindMeshes.Contains(SkeletalMesh->GetFName()))
                {
                    const UDynamicWindSkeletalData* DynamicWindSkeletalData =
                        SkeletalMesh->GetAssetUserData<UDynamicWindSkeletalData>();

                    const bool bDynamicWindReady =
                        IsValid(DynamicWindSkeletalData) &&
                        DynamicWindSkeletalData->bIsEnabled;

                    UE_LOG(
                        LogTemp,
                        Display,
                        TEXT("Cubus DynamicWind mesh: mesh=%s data=%s enabled=%s groups=%d provider=%s"),
                        *SkeletalMesh->GetName(),
                        IsValid(DynamicWindSkeletalData) ? TEXT("present") : TEXT("missing"),
                        bDynamicWindReady ? TEXT("yes") : TEXT("no"),
                        IsValid(DynamicWindSkeletalData)
                            ? DynamicWindSkeletalData->SimulationGroups.Num()
                            : 0,
                        IsValid(Component->GetTransformProvider())
                            ? *Component->GetTransformProvider()->GetClass()->GetName()
                            : TEXT("None")
                    );

                    LoggedDynamicWindMeshes.Add(SkeletalMesh->GetFName());
                }

                Component->SetCastShadow(bCastWorldPlantShadows);
                Component->SetCullDistances(
                    FMath::Max(0, PlantStartCullDistance),
                    FMath::Max(PlantStartCullDistance, PlantEndCullDistance)
                );

                NewSkeletalBatches.Add(BatchKey, Component);
                continue;
            }

            UE_LOG(
                LogTemp,
                Warning,
                TEXT("Cubus vegetation stage asset is unsupported class %s for %s"),
                *MeshAsset->GetClass()->GetName(),
                *MeshReference.ToSoftObjectPath().ToString()
            );
        }
    }

    for (const TPair<int64, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>>& Pair
         : CatalogStaticBatchComponents)
    {
        if (NewStaticBatches.Contains(Pair.Key) || !IsValid(Pair.Value))
        {
            continue;
        }

        Pair.Value->ClearInstances();
        Pair.Value->DestroyComponent();
    }

    for (const TPair<int64, TObjectPtr<UInstancedSkinnedMeshComponent>>& Pair
         : CatalogSkeletalBatchComponents)
    {
        if (NewSkeletalBatches.Contains(Pair.Key) || !IsValid(Pair.Value))
        {
            continue;
        }

        Pair.Value->ClearInstances();
        Pair.Value->DestroyComponent();
    }

    CatalogStaticBatchComponents = MoveTemp(NewStaticBatches);
    CatalogSkeletalBatchComponents = MoveTemp(NewSkeletalBatches);

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "Cubus vegetation catalog batches: static=%d skeletal=%d"
        ),
        CatalogStaticBatchComponents.Num(),
        CatalogSkeletalBatchComponents.Num()
    );
}

uint32 ACubusWorldVegetationActor::CalculateRuntimeRandomizationSettingsHash() const
{
    uint32 Hash = GetTypeHash(bEnableRuntimeRandomization);
    Hash = HashCombineFast(Hash, GetTypeHash(RuntimeRandomizationSeed));
    Hash = HashCombineFast(Hash, GetTypeHash(RandomPruneProbability));
    Hash = HashCombineFast(Hash, GetTypeHash(RandomScaleJitterMin));
    Hash = HashCombineFast(Hash, GetTypeHash(RandomScaleJitterMax));
    Hash = HashCombineFast(Hash, GetTypeHash(RandomPositionJitterVoxelFraction));
    Hash = HashCombineFast(Hash, GetTypeHash(RandomYawJitterDegrees));
    return Hash;
}

uint32 ACubusWorldVegetationActor::CalculateVegetationSettingsHash() const
{
    uint32 Hash = CalculateRuntimeRandomizationSettingsHash();
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
    Hash = HashCombineFast(Hash, GetTypeHash(MaximumPublishedPoints));
    Hash = HashCombineFast(Hash, GetTypeHash(bClusterTreeFamilies));
    Hash = HashCombineFast(Hash, GetTypeHash(TreeFamilyCellSizeVoxels));
    Hash = HashCombineFast(Hash, GetTypeHash(TreeFamilyCenterJitterFraction));
    Hash = HashCombineFast(Hash, GetTypeHash(MatureTreeCoreRadius));
    Hash = HashCombineFast(Hash, GetTypeHash(YoungTreeRingRadius));
    Hash = HashCombineFast(Hash, GetTypeHash(SaplingTreeRingRadius));
    Hash = HashCombineFast(Hash, GetTypeHash(TreeFamilyGrowthNoise));

    for (const FCubusVegetationSpeciesCatalogEntry& Entry : SpeciesCatalog)
    {
        Hash = HashCombineFast(Hash, GetTypeHash(Entry.SpeciesId));
        Hash = HashCombineFast(Hash, GetTypeHash(Entry.TypeId));
        Hash = HashCombineFast(Hash, GetTypeHash(Entry.Weight));
        Hash = HashCombineFast(Hash, GetTypeHash(Entry.BiomeMask));

        for (const TSoftObjectPtr<UObject>& MeshReference : Entry.GrowthStageMeshes)
        {
            Hash = HashCombineFast(
                Hash,
                GetTypeHash(MeshReference.ToSoftObjectPath().ToString())
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

        if (bUseCameraChunkCulling)
        {
            const float SafeVoxelSize =
                FMath::Max(1.0f, Chunk->GetVoxelSize());

            const double ChunkWorldSize =
                static_cast<double>(Cubus::ChunkSize) *
                static_cast<double>(SafeVoxelSize);

            const double HalfChunkWorldSize =
                ChunkWorldSize * 0.5;

            const FIntVector CameraChunk(
                FMath::FloorToInt((CameraLocation.X + HalfChunkWorldSize) / ChunkWorldSize),
                FMath::FloorToInt((CameraLocation.Y + HalfChunkWorldSize) / ChunkWorldSize),
                FMath::FloorToInt((CameraLocation.Z + HalfChunkWorldSize) / ChunkWorldSize)
            );

            const FIntVector ChunkCoordinate = Chunk->GetChunkCoordinate();

            if (
                FMath::Abs(ChunkCoordinate.X - CameraChunk.X) > CameraChunkHorizontalRadius ||
                FMath::Abs(ChunkCoordinate.Y - CameraChunk.Y) > CameraChunkHorizontalRadius ||
                FMath::Abs(ChunkCoordinate.Z - CameraChunk.Z) > CameraChunkVerticalRadius
            )
            {
                continue;
            }
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

UInstancedStaticMeshComponent*
ACubusWorldVegetationActor::CreatePointCarrier(
    const FName ComponentName,
    const FName ComponentTag
)
{
    UInstancedStaticMeshComponent* Component =
        NewObject<UInstancedStaticMeshComponent>(
            this,
            ComponentName,
            RF_Transient
        );

    if (!IsValid(Component))
    {
        return nullptr;
    }

    Component->SetupAttachment(Root);
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component->SetGenerateOverlapEvents(false);
    Component->SetCanEverAffectNavigation(false);
    Component->SetCastShadow(false);
    Component->SetMobility(EComponentMobility::Movable);
    Component->ComponentTags.AddUnique(ComponentTag);
    Component->RegisterComponent();
    AddInstanceComponent(Component);

    return Component;
}

UHierarchicalInstancedStaticMeshComponent*
ACubusWorldVegetationActor::CreatePlantBatch(
    FName ComponentName
)
{
    UHierarchicalInstancedStaticMeshComponent* Component =
        NewObject<UHierarchicalInstancedStaticMeshComponent>(
            this,
            ComponentName,
            RF_Transient
        );

    if (!IsValid(Component))
    {
        return nullptr;
    }

    Component->SetupAttachment(Root);
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component->SetGenerateOverlapEvents(false);
    Component->SetCanEverAffectNavigation(false);
    Component->SetCastShadow(bCastWorldPlantShadows);
    Component->SetMobility(EComponentMobility::Movable);
    Component->SetCullDistances(
        FMath::Max(0, PlantStartCullDistance),
        FMath::Max(PlantStartCullDistance, PlantEndCullDistance)
    );
    Component->RegisterComponent();
    AddInstanceComponent(Component);

    return Component;
}

UInstancedSkinnedMeshComponent*
ACubusWorldVegetationActor::CreateSkeletalPlantBatch(
    FName ComponentName
)
{
    UInstancedSkinnedMeshComponent* Component =
        NewObject<UInstancedSkinnedMeshComponent>(
            this,
            ComponentName,
            RF_Transient
        );

    if (!IsValid(Component))
    {
        return nullptr;
    }

    Component->SetupAttachment(Root);
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component->SetGenerateOverlapEvents(false);
    Component->SetCanEverAffectNavigation(false);
    Component->SetCastShadow(bCastWorldPlantShadows);
    // Keep mobility attach-compatible with the BP root (often Movable in PIE).
    Component->SetMobility(EComponentMobility::Movable);
    Component->SetVisibility(true, true);
    Component->SetHiddenInGame(false, true);
    Component->SetComponentTickEnabled(true);
    Component->VisibilityBasedAnimTickOption =
        EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    Component->SetAnimationMinScreenSize(-1.0f);

    UDynamicWindData* DynamicWindProvider =
        NewObject<UDynamicWindData>(
            Component,
            TEXT("DynamicWindProvider"),
            RF_Transient
        );

    Component->SetTransformProvider(DynamicWindProvider);
    Component->SetCullDistances(
        FMath::Max(0, PlantStartCullDistance),
        FMath::Max(PlantStartCullDistance, PlantEndCullDistance)
    );
    Component->RegisterComponent();
    AddInstanceComponent(Component);

    return Component;
}

USkeletalMeshComponent*
ACubusWorldVegetationActor::CreateHeroSkeletalWindComponent(
    FName ComponentName
)
{
    USkeletalMeshComponent* Component =
        NewObject<USkeletalMeshComponent>(
            this,
            ComponentName,
            RF_Transient
        );

    if (!IsValid(Component))
    {
        return nullptr;
    }

    Component->SetupAttachment(Root);
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component->SetGenerateOverlapEvents(false);
    Component->SetCanEverAffectNavigation(false);
    Component->SetCastShadow(bCastWorldPlantShadows);
    Component->SetMobility(EComponentMobility::Movable);
    Component->SetVisibility(false, true);
    Component->SetHiddenInGame(true, true);
    Component->RegisterComponent();
    AddInstanceComponent(Component);

    return Component;
}

UInstancedStaticMeshComponent*
ACubusWorldVegetationActor::ResolveCarrierForType(
    const int32 TypeId
) const
{
    switch (TypeId)
    {
        case GrassType:
            return GrassPoints;
        case ShrubType:
            return ShrubPoints;
        case BroadleafType:
            return TreePoints;
        case ReedsType:
            return ReedsPoints;
        case AlpineType:
            return AlpinePoints;
        case ConiferType:
            return ConiferTreePoints;
        default:
            return nullptr;
    }
}
