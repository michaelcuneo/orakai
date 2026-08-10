from pathlib import Path


def once(path: Path, old: str, new: str, label: str):
    text = path.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected 1 match, found {count}")
    path.write_text(text.replace(old, new, 1), encoding="utf-8")


build = Path("Source/Orakai/Orakai.Build.cs")
once(
    build,
    '\t\t\tPrivateDependencyModuleNames.AddRange(new string[] {\n\t\t\t\t"UnrealEd",\n\t\t\t\t"AssetTools",\n\t\t\t\t"MaterialEditor"\n\t\t\t});',
    '\t\t\tPrivateDependencyModuleNames.AddRange(new string[] {\n\t\t\t\t"UnrealEd",\n\t\t\t\t"AssetTools",\n\t\t\t\t"MaterialEditor",\n\t\t\t\t"MeshMergeUtilities"\n\t\t\t});',
    "editor build dependencies",
)

h = Path("Source/Orakai/CubusCore/Actors/CubusWorldVegetationActor.h")
once(
    h,
    '    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cubus|Vegetation")\n    void ClearWorldVegetation();',
    '    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cubus|Vegetation")\n    void ClearWorldVegetation();\n\n    /** Bake whole-tree static far proxies from skeletal growth stages. */\n    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cubus|Vegetation|Far Proxies")\n    void BakeFarVegetationProxies();\n\n    bool EnsureFarVegetationProxyAssets(bool bSaveGeneratedAssets);',
    "bake public API",
)
once(
    h,
    '    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Catalog")\n    TArray<FCubusVegetationSpeciesCatalogEntry> SpeciesCatalog;',
    '    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Catalog")\n    TArray<FCubusVegetationSpeciesCatalogEntry> SpeciesCatalog;\n\n    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Far Proxies")\n    bool bAutoBakeMissingFarProxies = true;\n\n    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Far Proxies")\n    FString FarProxyPackageRoot = TEXT("/Game/OrakaiGenerated/Vegetation/Far");',
    "proxy settings",
)

cpp = Path("Source/Orakai/CubusCore/Actors/CubusWorldVegetationActor.cpp")
text = cpp.read_text(encoding="utf-8")
include_anchor = '#include "UObject/SoftObjectPtr.h"\n'
editor_includes = '''#include "UObject/SoftObjectPtr.h"

#if WITH_EDITOR
#include "FileHelpers.h"
#include "IMeshMergeUtilities.h"
#include "MeshMergeModule.h"
#include "Modules/ModuleManager.h"
#include "Engine/MeshMerging.h"
#include "UObject/Package.h"
#endif
'''
if text.count(include_anchor) != 1:
    raise RuntimeError("editor include anchor mismatch")
text = text.replace(include_anchor, editor_includes, 1)

marker = 'void\nACubusWorldVegetationActor::RefreshFarVegetationBatches()\n{'
if text.count(marker) != 1:
    raise RuntimeError("far refresh marker mismatch")

baker = r'''bool ACubusWorldVegetationActor::EnsureFarVegetationProxyAssets(
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

    IMeshMergeModule& MeshMergeModule =
        FModuleManager::LoadModuleChecked<IMeshMergeModule>(
            TEXT("MeshMergeUtilities")
        );

    IMeshMergeUtilities& MeshMergeUtilities =
        MeshMergeModule.GetUtilities();

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
                USkeletalMeshComponent* PreviewComponent =
                    NewObject<USkeletalMeshComponent>(this, NAME_None, RF_Transient);

                if (!IsValid(PreviewComponent))
                {
                    ++FailedProxyCount;
                    continue;
                }

                PreviewComponent->SetSkinnedAssetAndUpdate(SourceSkeletalMesh);
                PreviewComponent->SetWorldTransform(FTransform::Identity);
                PreviewComponent->RegisterComponent();
                PreviewComponent->RefreshBoneTransforms();
                PreviewComponent->UpdateComponentToWorld();

                TArray<UPrimitiveComponent*> ComponentsToMerge;
                ComponentsToMerge.Add(PreviewComponent);

                FMeshMergingSettings MergeSettings;
                TArray<UObject*> GeneratedAssets;
                FVector MergedActorLocation = FVector::ZeroVector;

                MeshMergeUtilities.MergeComponentsToStaticMesh(
                    ComponentsToMerge,
                    World,
                    MergeSettings,
                    nullptr,
                    nullptr,
                    PackageName,
                    GeneratedAssets,
                    MergedActorLocation,
                    1.0f,
                    true
                );

                PreviewComponent->DestroyComponent();

                for (UObject* GeneratedAsset : GeneratedAssets)
                {
                    if (UStaticMesh* Candidate = Cast<UStaticMesh>(GeneratedAsset))
                    {
                        GeneratedMesh = Candidate;
                        break;
                    }
                }

                if (IsValid(GeneratedMesh))
                {
                    GeneratedMesh->MarkPackageDirty();
                    PackagesToSave.AddUnique(GeneratedMesh->GetPackage());
                    ++BakedProxyCount;
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

'''
text = text.replace(marker, baker + marker, 1)

rebuild_anchor = '''void ACubusWorldVegetationActor::RebuildWorldVegetation()
{
    ResolveBlockWorld();'''
rebuild_replacement = '''void ACubusWorldVegetationActor::RebuildWorldVegetation()
{
    ResolveBlockWorld();

#if WITH_EDITOR
    if (bAutoBakeMissingFarProxies)
    {
        EnsureFarVegetationProxyAssets(true);
    }
#endif'''
if text.count(rebuild_anchor) != 1:
    raise RuntimeError("rebuild anchor mismatch")
text = text.replace(rebuild_anchor, rebuild_replacement, 1)

probe_start = text.find("    // TEMPORARY FAR-VISIBILITY PROBE.")
if probe_start != -1:
    end_marker = "    bFarVegetationRenderDirty =\n        false;"
    probe_end = text.find(end_marker, probe_start)
    if probe_end == -1:
        raise RuntimeError("visibility probe end marker missing")
    text = text[:probe_start] + text[probe_end:]

cpp.write_text(text, encoding="utf-8")
