#include "CubusCore/Vegetation/CubusVegetationAssetResolver.h"

#include "CubusCore/Vegetation/CubusVegetationTypes.h"

#include "Animation/TransformProviderData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "UObject/Class.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/UnrealType.h"

namespace{
  bool TryResolveActorClassFromReferencedObject(
      UObject* CandidateObject,
      UClass*& OutClass,
      TSet<const UObject*>& VisitedObjects,
      int32 RemainingDepth
  );

  bool TryResolveActorClassFromStringPathCandidate(
      const FString& RawPathCandidate,
      UClass*& OutClass
  );


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
  );

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
  );

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
}

FName FCubusVegetationAssetResolver::NormalizeSpeciesId(const FName SpeciesId)
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

TSoftObjectPtr<UMaterialInterface> FCubusVegetationAssetResolver::ResolveFoliageMaterial(
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

UTransformProviderData* FCubusVegetationAssetResolver::ResolveTransformProvider(
    const FName SpeciesId
)
{
  const FName NormalizedSpeciesId =
      FCubusVegetationAssetResolver::NormalizeSpeciesId(
          SpeciesId
      );

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

UClass* FCubusVegetationAssetResolver::ResolveHeroPveActorClass(
    const FCubusVegetationSpeciesCatalogEntry& SpeciesEntry
)
{
    const FName NormalizedSpeciesId =
        NormalizeSpeciesId(SpeciesEntry.SpeciesId);

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
