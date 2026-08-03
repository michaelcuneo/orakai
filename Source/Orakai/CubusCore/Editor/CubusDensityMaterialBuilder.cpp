#include "CubusCore/Editor/CubusMaterialBuilderLibrary.h"

#include "Materials/Material.h"

#if WITH_EDITOR

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Factories/MaterialFactoryNew.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace CubusDensityMaterialBuilder
{
    constexpr const TCHAR* PackagePath =
        TEXT("/Game/Cubus/Materials/M_CubusDensityPBR");
    constexpr const TCHAR* AssetName =
        TEXT("M_CubusDensityPBR");

    template <typename TExpression>
    TExpression* AddExpression(
        UMaterial* Material,
        const int32 X,
        const int32 Y
    )
    {
        TExpression* Expression =
            NewObject<TExpression>(Material);

        Expression->MaterialExpressionEditorX = X;
        Expression->MaterialExpressionEditorY = Y;

        Material
            ->GetEditorOnlyData()
            ->ExpressionCollection
            .Expressions
            .Add(Expression);

        return Expression;
    }

    void Connect(
        FExpressionInput& Input,
        UMaterialExpression* Expression,
        const int32 OutputIndex = 0
    )
    {
        Input.Expression = Expression;
        Input.OutputIndex = OutputIndex;
    }

    UMaterialExpressionScalarParameter* AddScalar(
        UMaterial* Material,
        const FName Name,
        const float Value,
        const int32 X,
        const int32 Y
    )
    {
        UMaterialExpressionScalarParameter* Expression =
            AddExpression<UMaterialExpressionScalarParameter>(
                Material,
                X,
                Y
            );

        Expression->ParameterName = Name;
        Expression->DefaultValue = Value;
        return Expression;
    }

    UMaterialExpressionVectorParameter* AddVector(
        UMaterial* Material,
        const FName Name,
        const FLinearColor& Value,
        const int32 X,
        const int32 Y
    )
    {
        UMaterialExpressionVectorParameter* Expression =
            AddExpression<UMaterialExpressionVectorParameter>(
                Material,
                X,
                Y
            );

        Expression->ParameterName = Name;
        Expression->DefaultValue = Value;
        return Expression;
    }

    UMaterialExpressionMultiply* AddMultiply(
        UMaterial* Material,
        UMaterialExpression* A,
        UMaterialExpression* B,
        const int32 X,
        const int32 Y,
        const int32 AOutput = 0,
        const int32 BOutput = 0
    )
    {
        UMaterialExpressionMultiply* Expression =
            AddExpression<UMaterialExpressionMultiply>(
                Material,
                X,
                Y
            );

        Connect(Expression->A, A, AOutput);
        Connect(Expression->B, B, BOutput);
        return Expression;
    }

    UMaterialExpressionLinearInterpolate* AddLerp(
        UMaterial* Material,
        UMaterialExpression* A,
        UMaterialExpression* B,
        UMaterialExpression* Alpha,
        const int32 X,
        const int32 Y,
        const int32 AOutput = 0,
        const int32 BOutput = 0,
        const int32 AlphaOutput = 0
    )
    {
        UMaterialExpressionLinearInterpolate* Expression =
            AddExpression<UMaterialExpressionLinearInterpolate>(
                Material,
                X,
                Y
            );

        Connect(Expression->A, A, AOutput);
        Connect(Expression->B, B, BOutput);
        Connect(Expression->Alpha, Alpha, AlphaOutput);
        return Expression;
    }

    UMaterialExpressionComponentMask* AddMask(
        UMaterial* Material,
        UMaterialExpression* Source,
        const bool R,
        const bool G,
        const bool B,
        const bool A,
        const int32 X,
        const int32 Y
    )
    {
        UMaterialExpressionComponentMask* Expression =
            AddExpression<UMaterialExpressionComponentMask>(
                Material,
                X,
                Y
            );

        Expression->R = R;
        Expression->G = G;
        Expression->B = B;
        Expression->A = A;
        Connect(Expression->Input, Source);
        return Expression;
    }

    UMaterialExpressionTextureSampleParameter2D* AddTexture(
        UMaterial* Material,
        const FName Name,
        UTexture* DefaultTexture,
        const EMaterialSamplerType SamplerType,
        UMaterialExpression* UV,
        const int32 X,
        const int32 Y
    )
    {
        UMaterialExpressionTextureSampleParameter2D* Expression =
            AddExpression<UMaterialExpressionTextureSampleParameter2D>(
                Material,
                X,
                Y
            );

        Expression->ParameterName = Name;
        Expression->Texture = DefaultTexture;
        Expression->SamplerType = SamplerType;
        Connect(Expression->Coordinates, UV);
        return Expression;
    }

    struct FSurface
    {
        UMaterialExpressionTextureSampleParameter2D* BaseColor = nullptr;
        UMaterialExpressionTextureSampleParameter2D* Normal = nullptr;
        UMaterialExpressionTextureSampleParameter2D* ORM = nullptr;
        UMaterialExpressionVectorParameter* Tint = nullptr;
        UMaterialExpressionVectorParameter* EmissiveColor = nullptr;
        UMaterialExpressionScalarParameter* EmissiveStrength = nullptr;
    };

    FSurface AddSurface(
        UMaterial* Material,
        const TCHAR* Prefix,
        UMaterialExpression* TexCoord,
        UTexture* DefaultColor,
        UTexture* DefaultNormal,
        const int32 X,
        const int32 Y
    )
    {
        const FString PrefixString(Prefix);

        UMaterialExpressionScalarParameter* TextureScale =
            AddScalar(
                Material,
                FName(PrefixString + TEXT("TextureScale")),
                1.0f,
                X,
                Y
            );

        UMaterialExpressionMultiply* UV =
            AddMultiply(
                Material,
                TexCoord,
                TextureScale,
                X + 220,
                Y
            );

        FSurface Surface;
        Surface.BaseColor = AddTexture(
            Material,
            FName(PrefixString + TEXT("BaseColor")),
            DefaultColor,
            SAMPLERTYPE_Color,
            UV,
            X + 460,
            Y - 180
        );
        Surface.Normal = AddTexture(
            Material,
            FName(PrefixString + TEXT("Normal")),
            DefaultNormal,
            SAMPLERTYPE_Normal,
            UV,
            X + 460,
            Y + 40
        );
        Surface.ORM = AddTexture(
            Material,
            FName(PrefixString + TEXT("ORM")),
            DefaultColor,
            SAMPLERTYPE_LinearColor,
            UV,
            X + 460,
            Y + 260
        );
        Surface.Tint = AddVector(
            Material,
            FName(PrefixString + TEXT("Tint")),
            FLinearColor::White,
            X + 460,
            Y - 360
        );
        Surface.EmissiveColor = AddVector(
            Material,
            FName(PrefixString + TEXT("EmissiveColor")),
            FLinearColor::Black,
            X + 460,
            Y + 480
        );
        Surface.EmissiveStrength = AddScalar(
            Material,
            FName(PrefixString + TEXT("EmissiveStrength")),
            0.0f,
            X + 460,
            Y + 620
        );
        return Surface;
    }

    UMaterial* FindOrCreateMaterial()
    {
        if (UMaterial* Existing =
            LoadObject<UMaterial>(nullptr, PackagePath))
        {
            return Existing;
        }

        UMaterialFactoryNew* Factory =
            NewObject<UMaterialFactoryNew>();

        return Cast<UMaterial>(
            FAssetToolsModule::GetModule()
                .Get()
                .CreateAsset(
                    AssetName,
                    TEXT("/Game/Cubus/Materials"),
                    UMaterial::StaticClass(),
                    Factory
                )
        );
    }

    void SaveMaterial(UMaterial* Material)
    {
        Material->PreEditChange(nullptr);
        Material->PostEditChange();
        Material->MarkPackageDirty();
        FAssetRegistryModule::AssetCreated(Material);

        UPackage* Package = Material->GetOutermost();
        const FString Filename =
            FPackageName::LongPackageNameToFilename(
                Package->GetName(),
                FPackageName::GetAssetPackageExtension()
            );

        FSavePackageArgs SaveArgs;
        SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
        SaveArgs.SaveFlags = SAVE_NoError;

        UPackage::SavePackage(
            Package,
            Material,
            *Filename,
            SaveArgs
        );
    }
}

#endif

UMaterial* UCubusMaterialBuilderLibrary::BuildCubusDensityPbrMaterial()
{
#if WITH_EDITOR
    using namespace CubusDensityMaterialBuilder;

    UMaterial* Material = FindOrCreateMaterial();

    if (!IsValid(Material))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Unable to create M_CubusDensityPBR.")
        );
        return nullptr;
    }

    UMaterialEditorOnlyData* EditorData =
        Material->GetEditorOnlyData();

    EditorData->ExpressionCollection.Empty();

    Material->BlendMode = BLEND_Opaque;
    Material->SetShadingModel(MSM_DefaultLit);
    Material->TwoSided = false;
    Material->bUseMaterialAttributes = false;

    UTexture* DefaultColor = LoadObject<UTexture>(
        nullptr,
        TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture")
    );
    UTexture* DefaultNormal = LoadObject<UTexture>(
        nullptr,
        TEXT("/Engine/EngineResources/DefaultNormal.DefaultNormal")
    );

    if (!IsValid(DefaultColor) || !IsValid(DefaultNormal))
    {
        return nullptr;
    }

    UMaterialExpressionTextureCoordinate* TexCoord =
        AddExpression<UMaterialExpressionTextureCoordinate>(
            Material,
            -2400,
            0
        );

    UMaterialExpressionVertexColor* VertexColor =
        AddExpression<UMaterialExpressionVertexColor>(
            Material,
            -2400,
            700
        );

    UMaterialExpressionComponentMask* BlendAlpha = AddMask(
        Material,
        VertexColor,
        false,
        false,
        false,
        true,
        -2150,
        700
    );

    const FSurface SurfaceA = AddSurface(
        Material,
        TEXT("A"),
        TexCoord,
        DefaultColor,
        DefaultNormal,
        -2000,
        -450
    );

    const FSurface SurfaceB = AddSurface(
        Material,
        TEXT("B"),
        TexCoord,
        DefaultColor,
        DefaultNormal,
        -2000,
        850
    );

    UMaterialExpressionMultiply* ColorA = AddMultiply(
        Material,
        SurfaceA.BaseColor,
        SurfaceA.Tint,
        -850,
        -300
    );
    UMaterialExpressionMultiply* ColorB = AddMultiply(
        Material,
        SurfaceB.BaseColor,
        SurfaceB.Tint,
        -850,
        100
    );

    UMaterialExpressionLinearInterpolate* BaseColor = AddLerp(
        Material,
        ColorA,
        ColorB,
        BlendAlpha,
        -500,
        -100
    );

    UMaterialExpressionLinearInterpolate* Normal = AddLerp(
        Material,
        SurfaceA.Normal,
        SurfaceB.Normal,
        BlendAlpha,
        -500,
        180
    );

    UMaterialExpressionLinearInterpolate* ORM = AddLerp(
        Material,
        SurfaceA.ORM,
        SurfaceB.ORM,
        BlendAlpha,
        -500,
        460
    );

    UMaterialExpressionMultiply* EmissiveA = AddMultiply(
        Material,
        SurfaceA.EmissiveColor,
        SurfaceA.EmissiveStrength,
        -850,
        740
    );
    UMaterialExpressionMultiply* EmissiveB = AddMultiply(
        Material,
        SurfaceB.EmissiveColor,
        SurfaceB.EmissiveStrength,
        -850,
        940
    );

    UMaterialExpressionLinearInterpolate* Emissive = AddLerp(
        Material,
        EmissiveA,
        EmissiveB,
        BlendAlpha,
        -500,
        840
    );

    Connect(EditorData->BaseColor, BaseColor);
    Connect(EditorData->Normal, Normal);
    Connect(EditorData->AmbientOcclusion, ORM, 0);
    Connect(EditorData->Roughness, ORM, 1);
    Connect(EditorData->Metallic, ORM, 2);
    Connect(EditorData->EmissiveColor, Emissive);

    SaveMaterial(Material);
    return Material;
#else
    return nullptr;
#endif
}
