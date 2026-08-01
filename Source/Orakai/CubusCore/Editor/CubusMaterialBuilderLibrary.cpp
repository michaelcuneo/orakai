#include "CubusCore/Editor/CubusMaterialBuilderLibrary.h"

#include "Materials/Material.h"

#if WITH_EDITOR

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Factories/MaterialFactoryNew.h"
#include "Materials/MaterialExpressionAbs.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Materials/MaterialExpressionSaturate.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialEditorOnlyData.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace CubusMaterialBuilder
{
    constexpr const TCHAR* PackagePath =
        TEXT("/Game/Cubus/Materials/M_CubusBlockPBR");

    constexpr const TCHAR* AssetName =
        TEXT("M_CubusBlockPBR");

    template <typename TExpression>
    TExpression* AddExpression(
        UMaterial* Material,
        const int32 X,
        const int32 Y
    )
    {
        check(Material != nullptr);

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

    UMaterialExpressionConstant* AddConstant(
        UMaterial* Material,
        const float Value,
        const int32 X,
        const int32 Y
    )
    {
        UMaterialExpressionConstant* Expression =
            AddExpression<UMaterialExpressionConstant>(
                Material,
                X,
                Y
            );

        Expression->R = Value;
        return Expression;
    }

    UMaterialExpressionScalarParameter* AddScalarParameter(
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

    UMaterialExpressionVectorParameter* AddVectorParameter(
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

    UMaterialExpressionTextureSampleParameter2D* AddTextureParameter(
        UMaterial* Material,
        const FName Name,
        UTexture* DefaultTexture,
        const EMaterialSamplerType SamplerType,
        UMaterialExpression* Coordinates,
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
        Connect(Expression->Coordinates, Coordinates);
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

    UMaterialExpressionAdd* AddAdd(
        UMaterial* Material,
        UMaterialExpression* A,
        UMaterialExpression* B,
        const int32 X,
        const int32 Y
    )
    {
        UMaterialExpressionAdd* Expression =
            AddExpression<UMaterialExpressionAdd>(
                Material,
                X,
                Y
            );

        Connect(Expression->A, A);
        Connect(Expression->B, B);
        return Expression;
    }

    UMaterialExpressionSubtract* AddSubtract(
        UMaterial* Material,
        UMaterialExpression* A,
        UMaterialExpression* B,
        const int32 X,
        const int32 Y,
        const int32 AOutput = 0
    )
    {
        UMaterialExpressionSubtract* Expression =
            AddExpression<UMaterialExpressionSubtract>(
                Material,
                X,
                Y
            );

        Connect(Expression->A, A, AOutput);
        Connect(Expression->B, B);
        return Expression;
    }

    UMaterialExpressionSaturate* AddSaturate(
        UMaterial* Material,
        UMaterialExpression* Source,
        const int32 X,
        const int32 Y
    )
    {
        UMaterialExpressionSaturate* Expression =
            AddExpression<UMaterialExpressionSaturate>(
                Material,
                X,
                Y
            );

        Connect(Expression->Input, Source);
        return Expression;
    }

    UMaterialExpressionOneMinus* AddOneMinus(
        UMaterial* Material,
        UMaterialExpression* Source,
        const int32 X,
        const int32 Y
    )
    {
        UMaterialExpressionOneMinus* Expression =
            AddExpression<UMaterialExpressionOneMinus>(
                Material,
                X,
                Y
            );

        Connect(Expression->Input, Source);
        return Expression;
    }

    UMaterialExpressionAbs* AddAbs(
        UMaterial* Material,
        UMaterialExpression* Source,
        const int32 X,
        const int32 Y
    )
    {
        UMaterialExpressionAbs* Expression =
            AddExpression<UMaterialExpressionAbs>(
                Material,
                X,
                Y
            );

        Connect(Expression->Input, Source);
        return Expression;
    }

    UMaterialExpressionLinearInterpolate* AddLerp(
        UMaterial* Material,
        UMaterialExpression* A,
        UMaterialExpression* B,
        UMaterialExpression* Alpha,
        const int32 X,
        const int32 Y
    )
    {
        UMaterialExpressionLinearInterpolate* Expression =
            AddExpression<UMaterialExpressionLinearInterpolate>(
                Material,
                X,
                Y
            );

        Connect(Expression->A, A);
        Connect(Expression->B, B);
        Connect(Expression->Alpha, Alpha);
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

    struct FSurfaceExpressions
    {
        UMaterialExpressionTextureSampleParameter2D* BaseColor = nullptr;
        UMaterialExpressionTextureSampleParameter2D* Normal = nullptr;
        UMaterialExpressionTextureSampleParameter2D* ORM = nullptr;
        UMaterialExpressionTextureSampleParameter2D* Height = nullptr;
    };

    FSurfaceExpressions AddSurface(
        UMaterial* Material,
        const TCHAR* Prefix,
        UMaterialExpression* UV,
        UTexture* DefaultColor,
        UTexture* DefaultNormal,
        const int32 X,
        const int32 Y
    )
    {
        const FString PrefixString(Prefix);

        FSurfaceExpressions Surface;

        Surface.BaseColor = AddTextureParameter(
            Material,
            FName(PrefixString + TEXT("BaseColor")),
            DefaultColor,
            SAMPLERTYPE_Color,
            UV,
            X,
            Y
        );

        Surface.Normal = AddTextureParameter(
            Material,
            FName(PrefixString + TEXT("Normal")),
            DefaultNormal,
            SAMPLERTYPE_Normal,
            UV,
            X,
            Y + 220
        );

        Surface.ORM = AddTextureParameter(
            Material,
            FName(PrefixString + TEXT("ORM")),
            DefaultColor,
            SAMPLERTYPE_LinearColor,
            UV,
            X,
            Y + 440
        );

        Surface.Height = AddTextureParameter(
            Material,
            FName(PrefixString + TEXT("Height")),
            DefaultColor,
            SAMPLERTYPE_LinearGrayscale,
            UV,
            X,
            Y + 660
        );

        return Surface;
    }

    UMaterial* FindOrCreateMaterial()
    {
        UMaterial* Material =
            LoadObject<UMaterial>(nullptr, PackagePath);

        if (IsValid(Material))
        {
            return Material;
        }

        UMaterialFactoryNew* Factory =
            NewObject<UMaterialFactoryNew>();

        UObject* CreatedAsset =
            FAssetToolsModule::GetModule()
                .Get()
                .CreateAsset(
                    AssetName,
                    TEXT("/Game/Cubus/Materials"),
                    UMaterial::StaticClass(),
                    Factory
                );

        return Cast<UMaterial>(CreatedAsset);
    }

    void SaveMaterial(UMaterial* Material)
    {
        check(Material != nullptr);

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

UMaterial* UCubusMaterialBuilderLibrary::BuildCubusBlockPbrMaterial()
{
#if WITH_EDITOR
    using namespace CubusMaterialBuilder;

    UMaterial* Material = FindOrCreateMaterial();

    if (!IsValid(Material))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Unable to create M_CubusBlockPBR.")
        );
        return nullptr;
    }

    UMaterialEditorOnlyData* EditorData =
        Material->GetEditorOnlyData();

    EditorData->ExpressionCollection.Empty();

    Material->BlendMode = BLEND_Opaque;
    Material->SetShadingModel(MSM_DefaultLit);
    Material->TwoSided = false;

    UTexture* DefaultColor =
        LoadObject<UTexture>(
            nullptr,
            TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture")
        );

    UTexture* DefaultNormal =
        LoadObject<UTexture>(
            nullptr,
            TEXT("/Engine/EngineResources/DefaultNormal.DefaultNormal")
        );

    if (!IsValid(DefaultColor) || !IsValid(DefaultNormal))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Cubus material builder could not load engine default textures.")
        );
        return nullptr;
    }

    UMaterialExpressionTextureCoordinate* TexCoord =
        AddExpression<UMaterialExpressionTextureCoordinate>(
            Material,
            -2800,
            -250
        );

    UMaterialExpressionScalarParameter* TextureScale =
        AddScalarParameter(
            Material,
            TEXT("TextureScale"),
            1.0f,
            -2800,
            -50
        );

    UMaterialExpressionMultiply* UV =
        AddMultiply(
            Material,
            TexCoord,
            TextureScale,
            -2550,
            -230
        );

    UMaterialExpressionVertexColor* VertexColor =
        AddExpression<UMaterialExpressionVertexColor>(
            Material,
            -2800,
            500
        );

    UMaterialExpressionComponentMask* Selector =
        AddMask(
            Material,
            VertexColor,
            false,
            false,
            false,
            true,
            -2550,
            500
        );

    UMaterialExpressionConstant* Four =
        AddConstant(Material, 4.0f, -2320, 760);

    UMaterialExpressionConstant* Half =
        AddConstant(Material, 0.5f, -2320, 900);

    UMaterialExpressionConstant* ThreeQuarters =
        AddConstant(Material, 0.75f, -2320, 1040);

    UMaterialExpressionSaturate* SideMask =
        AddSaturate(
            Material,
            AddOneMinus(
                Material,
                AddMultiply(
                    Material,
                    Selector,
                    Four,
                    -2080,
                    760
                ),
                -1840,
                760
            ),
            -1600,
            760
        );

    UMaterialExpressionSaturate* TopMask =
        AddSaturate(
            Material,
            AddOneMinus(
                Material,
                AddMultiply(
                    Material,
                    AddAbs(
                        Material,
                        AddSubtract(
                            Material,
                            Selector,
                            Half,
                            -2080,
                            900
                        ),
                        -1840,
                        900
                    ),
                    Four,
                    -1600,
                    900
                ),
                -1360,
                900
            ),
            -1120,
            900
        );

    UMaterialExpressionSaturate* BottomMask =
        AddSaturate(
            Material,
            AddMultiply(
                Material,
                AddSubtract(
                    Material,
                    Selector,
                    ThreeQuarters,
                    -2080,
                    1040
                ),
                Four,
                -1840,
                1040
            ),
            -1600,
            1040
        );

    const FSurfaceExpressions Side =
        AddSurface(
            Material,
            TEXT("Side"),
            UV,
            DefaultColor,
            DefaultNormal,
            -1900,
            -1050
        );

    const FSurfaceExpressions Top =
        AddSurface(
            Material,
            TEXT("Top"),
            UV,
            DefaultColor,
            DefaultNormal,
            -1500,
            -1050
        );

    const FSurfaceExpressions Bottom =
        AddSurface(
            Material,
            TEXT("Bottom"),
            UV,
            DefaultColor,
            DefaultNormal,
            -1100,
            -1050
        );

    UMaterialExpressionComponentMask* UVY =
        AddMask(
            Material,
            UV,
            false,
            true,
            false,
            false,
            -1900,
            1280
        );

    UMaterialExpressionScalarParameter* BlendStart =
        AddScalarParameter(
            Material,
            TEXT("SideTopBlendStart"),
            0.7f,
            -1900,
            1430
        );

    UMaterialExpressionScalarParameter* BlendSharpness =
        AddScalarParameter(
            Material,
            TEXT("SideTopBlendSharpness"),
            4.0f,
            -1900,
            1560
        );

    UMaterialExpressionSaturate* SideBand =
        AddSaturate(
            Material,
            AddMultiply(
                Material,
                AddSubtract(
                    Material,
                    UVY,
                    BlendStart,
                    -1640,
                    1320
                ),
                BlendSharpness,
                -1400,
                1320
            ),
            -1160,
            1320
        );

    UMaterialExpressionScalarParameter* HeightStrength =
        AddScalarParameter(
            Material,
            TEXT("HeightStrength"),
            0.25f,
            -1160,
            1510
        );

    UMaterialExpressionMultiply* SideBandMasked =
        AddMultiply(
            Material,
            SideBand,
            SideMask,
            -920,
            1320
        );

    UMaterialExpressionMultiply* HeightBias =
        AddMultiply(
            Material,
            AddSubtract(
                Material,
                Top.Height,
                Side.Height,
                -920,
                1510,
                1
            ),
            HeightStrength,
            -680,
            1510
        );

    UMaterialExpressionMultiply* FinalSideAlpha =
        AddMultiply(
            Material,
            AddSaturate(
                Material,
                AddAdd(
                    Material,
                    SideBandMasked,
                    HeightBias,
                    -440,
                    1370
                ),
                -200,
                1370
            ),
            SideMask,
            40,
            1370
        );

    UMaterialExpressionLinearInterpolate* SideBase =
        AddLerp(
            Material,
            Side.BaseColor,
            Top.BaseColor,
            FinalSideAlpha,
            -650,
            -770
        );

    UMaterialExpressionLinearInterpolate* SideNormal =
        AddLerp(
            Material,
            Side.Normal,
            Top.Normal,
            FinalSideAlpha,
            -650,
            -470
        );

    UMaterialExpressionLinearInterpolate* SideOrm =
        AddLerp(
            Material,
            Side.ORM,
            Top.ORM,
            FinalSideAlpha,
            -650,
            -170
        );

    UMaterialExpressionAdd* FinalBase =
        AddAdd(
            Material,
            AddAdd(
                Material,
                AddMultiply(
                    Material,
                    Top.BaseColor,
                    TopMask,
                    -400,
                    -970
                ),
                AddMultiply(
                    Material,
                    Bottom.BaseColor,
                    BottomMask,
                    -400,
                    -830
                ),
                -140,
                -900
            ),
            AddMultiply(
                Material,
                SideBase,
                SideMask,
                -400,
                -690
            ),
            100,
            -820
        );

    UMaterialExpressionVectorParameter* Tint =
        AddVectorParameter(
            Material,
            TEXT("Tint"),
            FLinearColor::White,
            100,
            -650
        );

    UMaterialExpressionMultiply* TintedBase =
        AddMultiply(
            Material,
            FinalBase,
            Tint,
            350,
            -820
        );

    UMaterialExpressionAdd* FinalNormal =
        AddAdd(
            Material,
            AddAdd(
                Material,
                AddMultiply(
                    Material,
                    Top.Normal,
                    TopMask,
                    -400,
                    -450
                ),
                AddMultiply(
                    Material,
                    Bottom.Normal,
                    BottomMask,
                    -400,
                    -310
                ),
                -140,
                -380
            ),
            AddMultiply(
                Material,
                SideNormal,
                SideMask,
                -400,
                -170
            ),
            100,
            -300
        );

    UMaterialExpressionAdd* FinalOrm =
        AddAdd(
            Material,
            AddAdd(
                Material,
                AddMultiply(
                    Material,
                    Top.ORM,
                    TopMask,
                    -400,
                    50
                ),
                AddMultiply(
                    Material,
                    Bottom.ORM,
                    BottomMask,
                    -400,
                    190
                ),
                -140,
                120
            ),
            AddMultiply(
                Material,
                SideOrm,
                SideMask,
                -400,
                330
            ),
            100,
            220
        );

    UMaterialExpressionComponentMask* AmbientOcclusion =
        AddMask(
            Material,
            FinalOrm,
            true,
            false,
            false,
            false,
            350,
            40
        );

    UMaterialExpressionComponentMask* Roughness =
        AddMask(
            Material,
            FinalOrm,
            false,
            true,
            false,
            false,
            350,
            170
        );

    UMaterialExpressionComponentMask* Metallic =
        AddMask(
            Material,
            FinalOrm,
            false,
            false,
            true,
            false,
            350,
            300
        );

    UMaterialExpressionVectorParameter* EmissiveColor =
        AddVectorParameter(
            Material,
            TEXT("EmissiveColor"),
            FLinearColor::Black,
            100,
            500
        );

    UMaterialExpressionScalarParameter* EmissiveStrength =
        AddScalarParameter(
            Material,
            TEXT("EmissiveStrength"),
            0.0f,
            100,
            630
        );

    UMaterialExpressionMultiply* Emissive =
        AddMultiply(
            Material,
            EmissiveColor,
            EmissiveStrength,
            350,
            540
        );

    Connect(EditorData->BaseColor, TintedBase);
    Connect(EditorData->Normal, FinalNormal);
    Connect(EditorData->AmbientOcclusion, AmbientOcclusion);
    Connect(EditorData->Roughness, Roughness);
    Connect(EditorData->Metallic, Metallic);
    Connect(EditorData->EmissiveColor, Emissive);

    SaveMaterial(Material);

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Built and saved %s using direct C++ expression inputs."),
        PackagePath
    );

    return Material;
#else
    return nullptr;
#endif
}
