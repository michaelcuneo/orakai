#include "CubusCore/Data/CubusMaterialRegistry.h"

#include "CubusCore/Editor/CubusMaterialBuilderLibrary.h"
#include "CubusCore/Meshing/CubusDensityMesher.h"

#include "Engine/Texture2D.h"
#include "Engine/Texture2DArray.h"
#include "Materials/Material.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

#if WITH_EDITOR
namespace CubusMaterialRegistryEditor
{
    UTexture2DArray* FindOrCreateArray(
        const TCHAR* PackagePath,
        const TCHAR* AssetName,
        bool& bOutCreated
    )
    {
        if (UTexture2DArray* Existing =
            LoadObject<UTexture2DArray>(nullptr, PackagePath))
        {
            bOutCreated = false;
            return Existing;
        }

        UPackage* Package = CreatePackage(PackagePath);
        if (!IsValid(Package))
        {
            return nullptr;
        }

        bOutCreated = true;
        return NewObject<UTexture2DArray>(
            Package,
            FName(AssetName),
            RF_Public | RF_Standalone | RF_Transactional
        );
    }

    bool SaveAsset(UObject* Asset)
    {
        if (!IsValid(Asset))
        {
            return false;
        }

        Asset->MarkPackageDirty();
        UPackage* Package = Asset->GetOutermost();
        const FString Filename =
            FPackageName::LongPackageNameToFilename(
                Package->GetName(),
                FPackageName::GetAssetPackageExtension()
            );

        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        Args.SaveFlags = SAVE_NoError;
        return UPackage::SavePackage(Package, Asset, *Filename, Args);
    }

    template <typename TSelector>
    UTexture2DArray* BuildArray(
        const TArray<FCubusMaterialDefinition>& Materials,
        const TCHAR* PackagePath,
        const TCHAR* AssetName,
        const TSelector& Selector,
        UTexture2D* EngineFallback,
        const TextureCompressionSettings CompressionSettings,
        const bool bSrgb
    )
    {
        int32 MaximumMaterialId = 1;
        UTexture2D* Fallback = nullptr;

        for (const FCubusMaterialDefinition& Definition : Materials)
        {
            if (Definition.MaterialId < 0 ||
                Definition.MaterialId > FCubusDensityMesher::MaximumDensityMaterialId)
            {
                continue;
            }

            MaximumMaterialId = FMath::Max(MaximumMaterialId, Definition.MaterialId);
            if (!IsValid(Fallback))
            {
                Fallback = Selector(Definition.DensitySurface);
            }
        }

        if (!IsValid(Fallback))
        {
            Fallback = EngineFallback;
        }

        if (!IsValid(Fallback))
        {
            UE_LOG(LogTemp, Error, TEXT("Unable to build %s: no fallback texture."), AssetName);
            return nullptr;
        }

        bool bCreated = false;
        UTexture2DArray* Array = FindOrCreateArray(
            PackagePath,
            AssetName,
            bCreated
        );

        if (!IsValid(Array))
        {
            return nullptr;
        }

        Array->Modify();
        Array->SourceTextures.Reset();
        Array->SourceTextures.SetNum(MaximumMaterialId + 1);

        for (int32 Slice = 0; Slice <= MaximumMaterialId; ++Slice)
        {
            Array->SourceTextures[Slice] = Fallback;
        }

        for (const FCubusMaterialDefinition& Definition : Materials)
        {
            if (Definition.MaterialId < 0 || Definition.MaterialId > MaximumMaterialId)
            {
                continue;
            }

            if (UTexture2D* Texture = Selector(Definition.DensitySurface))
            {
                Array->SourceTextures[Definition.MaterialId] = Texture;
            }
        }

        Array->SRGB = bSrgb;
        Array->CompressionSettings = CompressionSettings;
        Array->AddressX = TA_Wrap;
        Array->AddressY = TA_Wrap;
        Array->AddressZ = TA_Clamp;

        if (!Array->CheckArrayTexturesCompatibility())
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT("Unable to build %s: every source texture must use compatible dimensions and source formats."),
                AssetName
            );
            return nullptr;
        }

        if (!Array->UpdateSourceFromSourceTextures(bCreated))
        {
            UE_LOG(LogTemp, Error, TEXT("UpdateSourceFromSourceTextures failed for %s."), AssetName);
            return nullptr;
        }

        Array->PostEditChange();
        Array->UpdateResource();
        SaveAsset(Array);
        return Array;
    }
}
#endif

void UCubusMaterialRegistry::BuildDensityMaterial()
{
#if WITH_EDITOR
    using namespace CubusMaterialRegistryEditor;

    UTexture2D* DefaultColor = LoadObject<UTexture2D>(
        nullptr,
        TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture")
    );
    UTexture2D* DefaultNormal = LoadObject<UTexture2D>(
        nullptr,
        TEXT("/Engine/EngineMaterials/DefaultNormal.DefaultNormal")
    );

    Modify();

    DensityBaseColorArray = BuildArray(
        Materials,
        TEXT("/Game/Cubus/Materials/Arrays/TA_CubusDensityBaseColor"),
        TEXT("TA_CubusDensityBaseColor"),
        [](const FCubusDensitySurfaceTextures& Surface) { return Surface.BaseColor.Get(); },
        DefaultColor,
        TC_Default,
        true
    );
    DensityNormalArray = BuildArray(
        Materials,
        TEXT("/Game/Cubus/Materials/Arrays/TA_CubusDensityNormal"),
        TEXT("TA_CubusDensityNormal"),
        [](const FCubusDensitySurfaceTextures& Surface) { return Surface.Normal.Get(); },
        DefaultNormal,
        TC_Normalmap,
        false
    );
    DensityOrmArray = BuildArray(
        Materials,
        TEXT("/Game/Cubus/Materials/Arrays/TA_CubusDensityORM"),
        TEXT("TA_CubusDensityORM"),
        [](const FCubusDensitySurfaceTextures& Surface) { return Surface.ORM.Get(); },
        DefaultColor,
        TC_Masks,
        false
    );
    DensityHeightArray = BuildArray(
        Materials,
        TEXT("/Game/Cubus/Materials/Arrays/TA_CubusDensityHeight"),
        TEXT("TA_CubusDensityHeight"),
        [](const FCubusDensitySurfaceTextures& Surface) { return Surface.Height.Get(); },
        DefaultColor,
        TC_Grayscale,
        false
    );
    DensityMacroColorArray = BuildArray(
        Materials,
        TEXT("/Game/Cubus/Materials/Arrays/TA_CubusDensityMacroColor"),
        TEXT("TA_CubusDensityMacroColor"),
        [](const FCubusDensitySurfaceTextures& Surface) { return Surface.MacroColor.Get(); },
        DefaultColor,
        TC_Default,
        true
    );
    DensityDetailNormalArray = BuildArray(
        Materials,
        TEXT("/Game/Cubus/Materials/Arrays/TA_CubusDensityDetailNormal"),
        TEXT("TA_CubusDensityDetailNormal"),
        [](const FCubusDensitySurfaceTextures& Surface) { return Surface.DetailNormal.Get(); },
        DefaultNormal,
        TC_Normalmap,
        false
    );

    if (!IsValid(DensityBaseColorArray.Get()) ||
        !IsValid(DensityNormalArray.Get()) ||
        !IsValid(DensityOrmArray.Get()) ||
        !IsValid(DensityHeightArray.Get()) ||
        !IsValid(DensityMacroColorArray.Get()) ||
        !IsValid(DensityDetailNormalArray.Get()))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Cubus density texture-array build failed. Fix incompatible or missing source textures and rebuild.")
        );
        return;
    }

    UMaterial* BuiltMaterial =
        UCubusMaterialBuilderLibrary::BuildCubusDensityPbrMaterial();

    if (!IsValid(BuiltMaterial))
    {
        UE_LOG(LogTemp, Error, TEXT("Cubus material registry failed to build M_CubusDensityPBR."));
        return;
    }

    DensityMaterial = BuiltMaterial;
    DensityRuntimeMaterialByKey.Reset();
    UnifiedDensityRuntimeMaterial.Reset();
    DensityMaterialDataTexture = nullptr;
    RebuildDensityMaterialDataTexture();
    MarkPackageDirty();
    PostEditChange();

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Cubus built one unified density material and six MaterialId-indexed texture arrays.")
    );
#else
    UE_LOG(LogTemp, Warning, TEXT("BuildDensityMaterial is only available in the Unreal Editor."));
#endif
}
