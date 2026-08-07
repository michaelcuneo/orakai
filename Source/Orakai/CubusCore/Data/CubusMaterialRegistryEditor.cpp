#include "CubusCore/Data/CubusMaterialRegistry.h"

#include "CubusCore/Editor/CubusMaterialBuilderLibrary.h"
#include "CubusCore/Meshing/CubusDensityMesher.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Texture2D.h"
#include "Engine/Texture2DArray.h"
#include "Materials/Material.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

#if WITH_EDITOR
namespace CubusMaterialRegistryEditor
{
    constexpr uint64 MaximumArraySourceBytes = 512ull * 1024ull * 1024ull;

    uint64 EstimateArraySourceBytes(const UTexture2D* Texture, const int32 SliceCount)
    {
        if (!IsValid(Texture) || SliceCount <= 0)
        {
            return 0;
        }

        const uint64 Width = static_cast<uint64>(FMath::Max(Texture->Source.GetSizeX(), 1));
        const uint64 Height = static_cast<uint64>(FMath::Max(Texture->Source.GetSizeY(), 1));
        return Width * Height * 4ull * static_cast<uint64>(SliceCount);
    }

    UTexture2DArray* FindOrCreateArray(const TCHAR* PackagePath, const TCHAR* AssetName, bool& bOutCreated)
    {
        if (UTexture2DArray* Existing = LoadObject<UTexture2DArray>(nullptr, PackagePath))
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
        const FString Filename = FPackageName::LongPackageNameToFilename(
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
        SourceData.Append({Pixel.B, Pixel.G, Pixel.R, Pixel.A});

        Texture->Source.Init(1, 1, 1, 1, TSF_BGRA8, SourceData.GetData());
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

    FString GetDefinitionLabel(const FCubusMaterialDefinition& Definition)
    {
        if (!Definition.DisplayName.IsEmpty())
        {
            return Definition.DisplayName.ToString();
        }

        if (!Definition.Name.IsNone())
        {
            return Definition.Name.ToString();
        }

        return FString::Printf(TEXT("Material %d"), Definition.MaterialId);
    }

    void LogTextureDetails(
        const TCHAR* AssetName,
        const int32 Slice,
        const FString& MaterialLabel,
        const UTexture2D* Texture,
        const bool bFallback
    )
    {
        if (!IsValid(Texture))
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT("%s slice %d (%s) has no valid texture."),
                AssetName,
                Slice,
                *MaterialLabel
            );
            return;
        }

        if (bFallback)
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("%s slice %d (%s) -> %s [NEUTRAL FALLBACK] | %dx%d format=%d compression=%d sRGB=%s"),
                AssetName,
                Slice,
                *MaterialLabel,
                *Texture->GetPathName(),
                Texture->Source.GetSizeX(),
                Texture->Source.GetSizeY(),
                static_cast<int32>(Texture->Source.GetFormat()),
                static_cast<int32>(Texture->CompressionSettings),
                Texture->SRGB ? TEXT("true") : TEXT("false")
            );
            return;
        }

        UE_LOG(
            LogTemp,
            Display,
            TEXT("%s slice %d (%s) -> %s | %dx%d format=%d compression=%d sRGB=%s"),
            AssetName,
            Slice,
            *MaterialLabel,
            *Texture->GetPathName(),
            Texture->Source.GetSizeX(),
            Texture->Source.GetSizeY(),
            static_cast<int32>(Texture->Source.GetFormat()),
            static_cast<int32>(Texture->CompressionSettings),
            Texture->SRGB ? TEXT("true") : TEXT("false")
        );
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

        for (const FCubusMaterialDefinition& Definition : Materials)
        {
            if (Definition.MaterialId < 0 ||
                Definition.MaterialId > FCubusDensityMesher::MaximumDensityMaterialId)
            {
                continue;
            }

            MaximumMaterialId = FMath::Max(MaximumMaterialId, Definition.MaterialId);
        }

        UTexture2D* Fallback = EngineFallback;
        if (!IsValid(Fallback))
        {
            UE_LOG(LogTemp, Error, TEXT("Unable to build %s: no neutral fallback texture."), AssetName);
            return nullptr;
        }

        UE_LOG(
            LogTemp,
            Display,
            TEXT("Building %s with neutral fallback %s."),
            AssetName,
            *Fallback->GetPathName()
        );

        const int32 SliceCount = MaximumMaterialId + 1;
        const uint64 EstimatedBytes = EstimateArraySourceBytes(Fallback, SliceCount);

        if (EstimatedBytes > MaximumArraySourceBytes)
        {
            UTexture2DArray* Existing = LoadObject<UTexture2DArray>(nullptr, PackagePath);
            const double EstimatedMiB = static_cast<double>(EstimatedBytes) / (1024.0 * 1024.0);
            const double BudgetMiB = static_cast<double>(MaximumArraySourceBytes) / (1024.0 * 1024.0);

            if (IsValid(Existing))
            {
                UE_LOG(
                    LogTemp,
                    Warning,
                    TEXT("Skipped rebuilding %s: %d slices at %dx%d would require at least %.1f MiB of contiguous source memory (budget %.1f MiB). Reusing the last saved array."),
                    AssetName,
                    SliceCount,
                    Fallback->Source.GetSizeX(),
                    Fallback->Source.GetSizeY(),
                    EstimatedMiB,
                    BudgetMiB
                );
                return Existing;
            }

            UE_LOG(
                LogTemp,
                Error,
                TEXT("Unable to build %s: %d slices at %dx%d would require at least %.1f MiB of contiguous source memory (budget %.1f MiB), and no previous array exists to reuse."),
                AssetName,
                SliceCount,
                Fallback->Source.GetSizeX(),
                Fallback->Source.GetSizeY(),
                EstimatedMiB,
                BudgetMiB
            );
            return nullptr;
        }

        bool bCreated = false;
        UTexture2DArray* Array = FindOrCreateArray(PackagePath, AssetName, bCreated);
        if (!IsValid(Array))
        {
            return nullptr;
        }

        Array->Modify();
        Array->SourceTextures.Reset();
        Array->SourceTextures.SetNum(SliceCount);

        for (int32 Slice = 0; Slice < SliceCount; ++Slice)
        {
            Array->SourceTextures[Slice] = Fallback;
        }

        TMap<int32, const FCubusMaterialDefinition*> DefinitionById;
        for (const FCubusMaterialDefinition& Definition : Materials)
        {
            if (Definition.MaterialId < 0 || Definition.MaterialId > MaximumMaterialId)
            {
                continue;
            }

            DefinitionById.Add(Definition.MaterialId, &Definition);

            UTexture2D* Texture = Selector(Definition.DensitySurface);
            if (IsValid(Texture))
            {
                Array->SourceTextures[Definition.MaterialId] = Texture;
            }
        }

        for (int32 Slice = 0; Slice < SliceCount; ++Slice)
        {
            const FCubusMaterialDefinition* const* DefinitionPtr = DefinitionById.Find(Slice);
            const FCubusMaterialDefinition* Definition = DefinitionPtr ? *DefinitionPtr : nullptr;
            const FString Label = Definition
                ? GetDefinitionLabel(*Definition)
                : FString::Printf(TEXT("Unassigned ID %d"), Slice);
            UTexture2D* ChosenTexture = Array->SourceTextures[Slice];
            const bool bFallback = ChosenTexture == Fallback;

            LogTextureDetails(AssetName, Slice, Label, ChosenTexture, bFallback);
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
                TEXT("Unable to build %s: source textures are incompatible. Compare the per-slice dimensions, source format, compression and sRGB values logged above."),
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

        UE_LOG(LogTemp, Display, TEXT("Built %s with %d slices."), AssetName, SliceCount);
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

    UMaterial* BuiltMaterial = UCubusMaterialBuilderLibrary::BuildCubusDensityPbrMaterial();
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
        TEXT("Cubus built unified density arrays with palette-blended triplanar normals.")
    );
#else
    UE_LOG(LogTemp, Warning, TEXT("BuildDensityMaterial is only available in the Unreal Editor."));
#endif
}

void UCubusMaterialRegistry::BuildWeatherResponsiveMaterials()
{
#if WITH_EDITOR
    UMaterial* BlockMaterial = UCubusMaterialBuilderLibrary::BuildCubusBlockPbrMaterial();
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
