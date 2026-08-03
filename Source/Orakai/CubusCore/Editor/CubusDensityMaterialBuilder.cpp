#include "CubusCore/Editor/CubusMaterialBuilderLibrary.h"

#include "Materials/Material.h"

#if WITH_EDITOR

#include "AssetToolsModule.h"
#include "Factories/MaterialFactoryNew.h"
#include "MaterialEditingLibrary.h"
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
        UMaterialExpression* Created =
            UMaterialEditingLibrary::CreateMaterialExpression(
                Material,
                TExpression::StaticClass(),
                X,
                Y
            );

        TExpression* Expression = Cast<TExpression>(Created);
        check(Expression != nullptr);
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

    UMaterialExpressionScalarParameter* Scalar(
        UMaterial* Material,
        const FName Name,
        const float Value,
        const int32 X,
        const int32 Y
    )
    {
        UMaterialExpressionScalarParameter* Node =
            AddExpression<UMaterialExpressionScalarParameter>(
                Material,
                X,
                Y
            );
        Node->ParameterName = Name;
        Node->DefaultValue = Value;
        return Node;
    }

    UMaterialExpressionVectorParameter* Vector(
        UMaterial* Material,
        const FName Name,
        const FLinearColor& Value,
        const int32 X,
        const int32 Y
    )
    {
        UMaterialExpressionVectorParameter* Node =
            AddExpression<UMaterialExpressionVectorParameter>(
                Material,
                X,
                Y
            );
        Node->ParameterName = Name;
        Node->DefaultValue = Value;
        return Node;
    }

    UMaterialExpressionMultiply* Multiply(
        UMaterial* Material,
        UMaterialExpression* A,
        UMaterialExpression* B,
        const int32 X,
        const int32 Y,
        const int32 AOutput = 0,
        const int32 BOutput = 0
    )
    {
        UMaterialExpressionMultiply* Node =
            AddExpression<UMaterialExpressionMultiply>(Material, X, Y);
        Connect(Node->A, A, AOutput);
        Connect(Node->B, B, BOutput);
        return Node;
    }

    UMaterialExpressionLinearInterpolate* Lerp(
        UMaterial* Material,
        UMaterialExpression* A,
        UMaterialExpression* B,
        UMaterialExpression* Alpha,
        const int32 X,
        const int32 Y
    )
    {
        UMaterialExpressionLinearInterpolate* Node =
            AddExpression<UMaterialExpressionLinearInterpolate>(
                Material,
                X,
                Y
            );
        Connect(Node->A, A);
        Connect(Node->B, B);
        Connect(Node->Alpha, Alpha);
        return Node;
    }

    UMaterialExpressionComponentMask* AlphaMask(
        UMaterial* Material,
        UMaterialExpression* Input,
        const int32 X,
        const int32 Y
    )
    {
        UMaterialExpressionComponentMask* Node =
            AddExpression<UMaterialExpressionComponentMask>(
                Material,
                X,
                Y
            );
        Node->A = true;
        Connect(Node->Input, Input);
        return Node;
    }

    UMaterialExpressionTextureSampleParameter2D* Texture(
        UMaterial* Material,
        const FName Name,
        UTexture* DefaultTexture,
        const EMaterialSamplerType SamplerType,
        UMaterialExpression* UV,
        const int32 X,
        const int32 Y
    )
    {
        UMaterialExpressionTextureSampleParameter2D* Node =
            AddExpression<UMaterialExpressionTextureSampleParameter2D>(
                Material,
                X,
                Y
            );
        Node->ParameterName = Name;
        Node->Texture = DefaultTexture;
        Node->SamplerType = SamplerType;
        Connect(Node->Coordinates, UV);
        return Node;
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

    FSurface Surface(
        UMaterial* Material,
        const TCHAR* Prefix,
        UMaterialExpression* TexCoord,
        UTexture* DefaultColor,
        UTexture* DefaultNormal,
        const int32 X,
        const int32 Y
    )
    {
        const FString P(Prefix);
        UMaterialExpressionScalarParameter* Scale =
            Scalar(Material, FName(P + TEXT("TextureScale")), 1.0f, X, Y);
        UMaterialExpressionMultiply* UV =
            Multiply(Material, TexCoord, Scale, X + 220, Y);

        FSurface Result;
        Result.Tint = Vector(
            Material,
            FName(P + TEXT("Tint")),
            FLinearColor::White,
            X + 460,
            Y - 380
        );
        Result.BaseColor = Texture(
            Material,
            FName(P + TEXT("BaseColor")),
            DefaultColor,
            SAMPLERTYPE_Color,
            UV,
            X + 460,
            Y - 180
        );
        Result.Normal = Texture(
            Material,
            FName(P + TEXT("Normal")),
            DefaultNormal,
            SAMPLERTYPE_Normal,
            UV,
            X + 460,
            Y + 40
        );
        Result.ORM = Texture(
            Material,
            FName(P + TEXT("ORM")),
            DefaultColor,
            SAMPLERTYPE_LinearColor,
            UV,
            X + 460,
            Y + 260
        );
        Result.EmissiveColor = Vector(
            Material,
            FName(P + TEXT("EmissiveColor")),
            FLinearColor::Black,
            X + 460,
            Y + 480
        );
        Result.EmissiveStrength = Scalar(
            Material,
            FName(P + TEXT("EmissiveStrength")),
            0.0f,
            X + 460,
            Y + 650
        );
        return Result;
    }

    UMaterial* FindOrCreateMaterial()
    {
        if (UMaterial* Existing =
            LoadObject<UMaterial>(nullptr, PackagePath))
        {
            return Existing;
        }

        UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
        return Cast<UMaterial>(
            FAssetToolsModule::GetModule().Get().CreateAsset(
                AssetName,
                TEXT("/Game/Cubus/Materials"),
                UMaterial::StaticClass(),
                Factory
            )
        );
    }

    void Save(UMaterial* Material)
    {
        UMaterialEditingLibrary::RecompileMaterial(Material);
        Material->PostEditChange();
        Material->MarkPackageDirty();

        UPackage* Package = Material->GetOutermost();
        const FString Filename =
            FPackageName::LongPackageNameToFilename(
                Package->GetName(),
                FPackageName::GetAssetPackageExtension()
            );

        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        Args.SaveFlags = SAVE_NoError;
        UPackage::SavePackage(Package, Material, *Filename, Args);
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
        UE_LOG(LogTemp, Error, TEXT("Unable to create M_CubusDensityPBR."));
        return nullptr;
    }

    Material->Modify();
    Material->PreEditChange(nullptr);
    UMaterialEditingLibrary::DeleteAllMaterialExpressions(Material);

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
        TEXT("/Engine/EngineMaterials/DefaultNormal.DefaultNormal")
    );

    if (!IsValid(DefaultColor) || !IsValid(DefaultNormal))
    {
        UE_LOG(LogTemp, Error, TEXT("Density material defaults are missing."));
        return nullptr;
    }

    UMaterialExpressionTextureCoordinate* TexCoord =
        AddExpression<UMaterialExpressionTextureCoordinate>(Material, -2500, 0);
    UMaterialExpressionVertexColor* VertexColor =
        AddExpression<UMaterialExpressionVertexColor>(Material, -2500, 700);
    UMaterialExpressionComponentMask* BlendAlpha =
        AlphaMask(Material, VertexColor, -2250, 700);

    const FSurface A = Surface(
        Material,
        TEXT("A"),
        TexCoord,
        DefaultColor,
        DefaultNormal,
        -2050,
        -500
    );
    const FSurface B = Surface(
        Material,
        TEXT("B"),
        TexCoord,
        DefaultColor,
        DefaultNormal,
        -2050,
        850
    );

    UMaterialExpressionMultiply* ColorA =
        Multiply(Material, A.BaseColor, A.Tint, -850, -350);
    UMaterialExpressionMultiply* ColorB =
        Multiply(Material, B.BaseColor, B.Tint, -850, 50);
    UMaterialExpressionLinearInterpolate* FinalColor =
        Lerp(Material, ColorA, ColorB, BlendAlpha, -450, -150);

    UMaterialExpressionLinearInterpolate* FinalNormal =
        Lerp(Material, A.Normal, B.Normal, BlendAlpha, -450, 150);
    UMaterialExpressionLinearInterpolate* FinalORM =
        Lerp(Material, A.ORM, B.ORM, BlendAlpha, -450, 450);

    UMaterialExpressionMultiply* EmissiveA =
        Multiply(Material, A.EmissiveColor, A.EmissiveStrength, -850, 750);
    UMaterialExpressionMultiply* EmissiveB =
        Multiply(Material, B.EmissiveColor, B.EmissiveStrength, -850, 950);
    UMaterialExpressionLinearInterpolate* FinalEmissive =
        Lerp(Material, EmissiveA, EmissiveB, BlendAlpha, -450, 850);

    UMaterialEditorOnlyData* Data = Material->GetEditorOnlyData();
    Connect(Data->BaseColor, FinalColor);
    Connect(Data->Normal, FinalNormal);
    Connect(Data->AmbientOcclusion, FinalORM, 1);
    Connect(Data->Roughness, FinalORM, 2);
    Connect(Data->Metallic, FinalORM, 3);
    Connect(Data->EmissiveColor, FinalEmissive);

    Save(Material);

    const int32 ExpressionCount =
        Material->GetEditorOnlyData()->ExpressionCollection.Expressions.Num();

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Cubus density material built with %d registered expressions."),
        ExpressionCount
    );

    return Material;
#else
    return nullptr;
#endif
}
