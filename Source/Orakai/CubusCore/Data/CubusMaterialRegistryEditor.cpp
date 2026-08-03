#include "CubusCore/Data/CubusMaterialRegistry.h"

#include "CubusCore/Editor/CubusMaterialBuilderLibrary.h"
#include "CubusCore/Meshing/CubusDensityMesher.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Texture2D.h"
#include "Engine/Texture2DArray.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
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
        UTexture2DArray* Created = NewObject<UTexture2DArray>(
            Package,
            FName(AssetName),
            RF_Public | RF_Standalone | RF_Transactional
        );
        FAssetRegistryModule::AssetCreated(Created);
        return Created;
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

    UTexture2D* FindOrCreateSolidTexture(
        const TCHAR* PackagePath,
        const TCHAR* AssetName,
        const FColor Pixel,
        const TextureCompressionSettings CompressionSettings,
        const bool bSrgb
    )
    {
        UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, PackagePath);
        bool bCreated = false;

        if (!IsValid(Texture))
        {
            UPackage* Package = CreatePackage(PackagePath);
            if (!IsValid(Package))
            {
                return nullptr;
            }

            Texture = NewObject<UTexture2D>(
                Package,
                FName(AssetName),
                RF_Public | RF_Standalone | RF_Transactional
            );
            bCreated = true;
        }

        if (!IsValid(Texture))
        {
            return nullptr;
        }

        Texture->Modify();

        TArray64<uint8> SourceData;
        SourceData.Append(
        {
            Pixel.B,
            Pixel.G,
            Pixel.R,
            Pixel.A
        });

        Texture->Source.Init(
            1,
            1,
            1,
            1,
            TSF_BGRA8,
            SourceData.GetData()
        );
        Texture->SRGB = bSrgb;
        Texture->CompressionSettings = CompressionSettings;
        Texture->MipGenSettings = TMGS_NoMipmaps;
        Texture->AddressX = TA_Wrap;
        Texture->AddressY = TA_Wrap;
        Texture->NeverStream = true;
        Texture->PostEditChange();
        Texture->UpdateResource();

        if (bCreated)
        {
            FAssetRegistryModule::AssetCreated(Texture);
        }

        SaveAsset(Texture);
        return Texture;
    }

    void ForceGeometricDensityNormal(UMaterial* Material)
    {
        if (!IsValid(Material))
        {
            return;
        }

        UMaterialExpressionVertexNormalWS* VertexNormal = nullptr;
        for (UMaterialExpression* Expression :
            Material->GetEditorOnlyData()->ExpressionCollection.Expressions)
        {
            VertexNormal = Cast<UMaterialExpressionVertexNormalWS>(Expression);
            if (IsValid(VertexNormal))
            {
                break;
            }
        }

        if (!IsValid(VertexNormal))
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT("M_CubusDensityPBR contains no VertexNormalWS expression.")
            );
            return;
        }

        Material->Modify();
        FExpressionInput& NormalInput =
            Material->GetEditorOnlyData()->Normal;
        NormalInput.Expression = VertexNormal;
        NormalInput.OutputIndex = 0;

        UMaterialEditingLibrary::RecompileMaterial(Material);
        Material->PostEditChange();
        SaveAsset(Material);
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

    UTexture2D* NeutralBaseColor = FindOrCreateSolidTexture(
        TEXT("/Game/Cubus/Materials/Arrays/T_CubusDensityNeutralBaseColor"),
        TEXT("T_CubusDensityNeutralBaseColor"),
        FColor(255, 255, 255, 255),
        TC_Default,
        true
    );
    UTexture2D* NeutralNormal = FindOrCreateSolidTexture(
        TEXT("/Game/Cubus/Materials/Arrays/T_CubusDensityNeutralNormal"),
        TEXT("T_CubusDensityNeutralNormal"),
        FColor(128, 128, 255, 255),
        TC_Normalmap,
        false
    );
    UTexture2D* NeutralOrm = FindOrCreateSolidTexture(
        TEXT("/Game/Cubus/Materials/Arrays/T_CubusDensityNeutralORM"),
        TEXT("T_CubusDensityNeutralORM"),
        FColor(255, 128, 0, 255),
        TC_Masks,
        false
    );
    UTexture2D* NeutralHeight = FindOrCreateSolidTexture(
        TEXT("/Game/Cubus/Materials/Arrays/T_CubusDensityNeutralHeight"),
        TEXT("T_CubusDensityNeutralHeight"),
        FColor(128, 128, 128, 255),
        TC_Grayscale,
        false
    );

    if (!IsValid(NeutralBaseColor) ||
        !IsValid(NeutralNormal) ||
        !IsValid(NeutralOrm) ||
        !IsValid(NeutralHeight))
    {
        UE_LOG(LogTemp, Error, TEXT("Cubus could not create neutral density fallback textures."));
        return;
    }

    Modify();

    DensityBaseColorArray = BuildArray(
        Materials,
        TEXT("/Game/Cubus/Materials/Arrays/TA_CubusDensityBaseColor"),
        TEXT("TA_CubusDensityBaseColor"),
        [](const FCubusDensitySurfaceTextures& Surface) { return Surface.BaseColor.Get(); },
        NeutralBaseColor,
        TC_Default,
        true
    );
    DensityNormalArray = BuildArray(
        Materials,
        TEXT("/Game/Cubus/Materials/Arrays/TA_CubusDensityNormal"),
        TEXT("TA_CubusDensityNormal"),
        [](const FCubusDensitySurfaceTextures& Surface) { return Surface.Normal.Get(); },
        NeutralNormal,
        TC_Normalmap,
        false
    );
    DensityOrmArray = BuildArray(
        Materials,
        TEXT("/Game/Cubus/Materials/Arrays/TA_CubusDensityORM"),
        TEXT("TA_CubusDensityORM"),
        [](const FCubusDensitySurfaceTextures& Surface) { return Surface.ORM.Get(); },
        NeutralOrm,
        TC_Masks,
        false
    );
    DensityHeightArray = BuildArray(
        Materials,
        TEXT("/Game/Cubus/Materials/Arrays/TA_CubusDensityHeight"),
        TEXT("TA_CubusDensityHeight"),
        [](const FCubusDensitySurfaceTextures& Surface) { return Surface.Height.Get(); },
        NeutralHeight,
        TC_Grayscale,
        false
    );
    DensityMacroColorArray = BuildArray(
        Materials,
        TEXT("/Game/Cubus/Materials/Arrays/TA_CubusDensityMacroColor"),
        TEXT("TA_CubusDensityMacroColor"),
        [](const FCubusDensitySurfaceTextures& Surface) { return Surface.MacroColor.Get(); },
        NeutralBaseColor,
        TC_Default,
        true
    );
    DensityDetailNormalArray = BuildArray(
        Materials,
        TEXT("/Game/Cubus/Materials/Arrays/TA_CubusDensityDetailNormal"),
        TEXT("TA_CubusDensityDetailNormal"),
        [](const FCubusDensitySurfaceTextures& Surface) { return Surface.DetailNormal.Get(); },
        NeutralNormal,
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
            TEXT("Cubus density texture-array build failed. Fix incompatible source textures and rebuild.")
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

    ForceGeometricDensityNormal(BuiltMaterial);

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
        TEXT("Cubus built unified density arrays and uses geometric world normals for stable lighting.")
    );
#else
    UE_LOG(LogTemp, Warning, TEXT("BuildDensityMaterial is only available in the Unreal Editor."));
#endif
}

void UCubusMaterialRegistry::BuildWeatherResponsiveMaterials()
{
#if WITH_EDITOR
    UMaterial* BlockMaterial =
        UCubusMaterialBuilderLibrary::BuildCubusBlockPbrMaterial();

    if (!IsValid(BlockMaterial))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Cubus failed to build the weather-responsive block material.")
        );
        return;
    }

    BuildDensityMaterial();
    RuntimeMaterialById.Reset();

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Cubus rebuilt block and density materials with live weather response.")
    );
#else
    UE_LOG(
        LogTemp,
        Warning,
        TEXT("BuildWeatherResponsiveMaterials is only available in the Unreal Editor.")
    );
#endif
}
