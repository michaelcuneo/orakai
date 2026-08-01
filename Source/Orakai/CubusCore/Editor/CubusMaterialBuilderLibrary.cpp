#include "CubusCore/Editor/CubusMaterialBuilderLibrary.h"

#include "Materials/Material.h"

#if WITH_EDITOR

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Factories/MaterialFactoryNew.h"
#include "MaterialEditingLibrary.h"
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
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace CubusMaterialBuilder
{
    constexpr const TCHAR* PackagePath = TEXT("/Game/Cubus/Materials/M_CubusBlockPBR");
    constexpr const TCHAR* AssetName = TEXT("M_CubusBlockPBR");

    template <typename TExpression>
    TExpression* AddExpression(UMaterial* Material, const int32 X, const int32 Y)
    {
        UMaterialExpression* Created = UMaterialEditingLibrary::CreateMaterialExpression(
            Material,
            TExpression::StaticClass(),
            X,
            Y
        );

        TExpression* Expression = Cast<TExpression>(Created);
        check(Expression != nullptr);
        return Expression;
    }

    void Connect(FExpressionInput& Input, UMaterialExpression* Expression, const int32 OutputIndex = 0)
    {
        Input.Expression = Expression;
        Input.OutputIndex = OutputIndex;
    }

    UMaterialExpressionConstant* Constant(UMaterial* Material, const float Value, const int32 X, const int32 Y)
    {
        UMaterialExpressionConstant* Node = AddExpression<UMaterialExpressionConstant>(Material, X, Y);
        Node->R = Value;
        return Node;
    }

    UMaterialExpressionScalarParameter* Scalar(
        UMaterial* Material,
        const FName Name,
        const float Value,
        const int32 X,
        const int32 Y
    )
    {
        UMaterialExpressionScalarParameter* Node = AddExpression<UMaterialExpressionScalarParameter>(Material, X, Y);
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
        UMaterialExpressionVectorParameter* Node = AddExpression<UMaterialExpressionVectorParameter>(Material, X, Y);
        Node->ParameterName = Name;
        Node->DefaultValue = Value;
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
            AddExpression<UMaterialExpressionTextureSampleParameter2D>(Material, X, Y);
        Node->ParameterName = Name;
        Node->Texture = DefaultTexture;
        Node->SamplerType = SamplerType;
        Connect(Node->Coordinates, UV);
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
        UMaterialExpressionMultiply* Node = AddExpression<UMaterialExpressionMultiply>(Material, X, Y);
        Connect(Node->A, A, AOutput);
        Connect(Node->B, B, BOutput);
        return Node;
    }

    UMaterialExpressionAdd* Add(
        UMaterial* Material,
        UMaterialExpression* A,
        UMaterialExpression* B,
        const int32 X,
        const int32 Y
    )
    {
        UMaterialExpressionAdd* Node = AddExpression<UMaterialExpressionAdd>(Material, X, Y);
        Connect(Node->A, A);
        Connect(Node->B, B);
        return Node;
    }

    UMaterialExpressionSubtract* Subtract(
        UMaterial* Material,
        UMaterialExpression* A,
        UMaterialExpression* B,
        const int32 X,
        const int32 Y
    )
    {
        UMaterialExpressionSubtract* Node = AddExpression<UMaterialExpressionSubtract>(Material, X, Y);
        Connect(Node->A, A);
        Connect(Node->B, B);
        return Node;
    }

    UMaterialExpressionOneMinus* OneMinus(
        UMaterial* Material,
        UMaterialExpression* Input,
        const int32 X,
        const int32 Y
    )
    {
        UMaterialExpressionOneMinus* Node = AddExpression<UMaterialExpressionOneMinus>(Material, X, Y);
        Connect(Node->Input, Input);
        return Node;
    }

    UMaterialExpressionSaturate* Saturate(
        UMaterial* Material,
        UMaterialExpression* Input,
        const int32 X,
        const int32 Y
    )
    {
        UMaterialExpressionSaturate* Node = AddExpression<UMaterialExpressionSaturate>(Material, X, Y);
        Connect(Node->Input, Input);
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
            AddExpression<UMaterialExpressionLinearInterpolate>(Material, X, Y);
        Connect(Node->A, A);
        Connect(Node->B, B);
        Connect(Node->Alpha, Alpha);
        return Node;
    }

    UMaterialExpressionComponentMask* Mask(
        UMaterial* Material,
        UMaterialExpression* Input,
        const bool R,
        const bool G,
        const bool B,
        const bool A,
        const int32 X,
        const int32 Y
    )
    {
        UMaterialExpressionComponentMask* Node = AddExpression<UMaterialExpressionComponentMask>(Material, X, Y);
        Node->R = R;
        Node->G = G;
        Node->B = B;
        Node->A = A;
        Connect(Node->Input, Input);
        return Node;
    }

    struct FSurface
    {
        UMaterialExpressionTextureSampleParameter2D* Base = nullptr;
        UMaterialExpressionTextureSampleParameter2D* Normal = nullptr;
        UMaterialExpressionTextureSampleParameter2D* ORM = nullptr;
        UMaterialExpressionTextureSampleParameter2D* Height = nullptr;
    };

    FSurface Surface(
        UMaterial* Material,
        const TCHAR* Prefix,
        UMaterialExpression* UV,
        UTexture* DefaultColor,
        UTexture* DefaultNormal,
        const int32 X
    )
    {
        const FString P(Prefix);
        FSurface Result;
        Result.Base = Texture(Material, FName(P + TEXT("BaseColor")), DefaultColor, SAMPLERTYPE_Color, UV, X, -900);
        Result.Normal = Texture(Material, FName(P + TEXT("Normal")), DefaultNormal, SAMPLERTYPE_Normal, UV, X, -650);
        Result.ORM = Texture(Material, FName(P + TEXT("ORM")), DefaultColor, SAMPLERTYPE_LinearColor, UV, X, -400);
        Result.Height = Texture(Material, FName(P + TEXT("Height")), DefaultColor, SAMPLERTYPE_LinearGrayscale, UV, X, -150);
        return Result;
    }

    UMaterial* FindOrCreateMaterial()
    {
        if (UMaterial* Existing = LoadObject<UMaterial>(nullptr, PackagePath))
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
        const FString Filename = FPackageName::LongPackageNameToFilename(
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

UMaterial* UCubusMaterialBuilderLibrary::BuildCubusBlockPbrMaterial()
{
#if WITH_EDITOR
    using namespace CubusMaterialBuilder;

    UE_LOG(LogTemp, Display, TEXT("Cubus material builder: starting."));

    UMaterial* Material = FindOrCreateMaterial();
    if (!IsValid(Material))
    {
        UE_LOG(LogTemp, Error, TEXT("Cubus material builder: failed to load or create material."));
        return nullptr;
    }

    Material->Modify();
    Material->PreEditChange(nullptr);
    UMaterialEditingLibrary::DeleteAllMaterialExpressions(Material);

    Material->BlendMode = BLEND_Opaque;
    Material->SetShadingModel(MSM_DefaultLit);
    Material->TwoSided = false;

    UTexture* DefaultColor = LoadObject<UTexture>(
        nullptr,
        TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture")
    );

    UTexture* DefaultNormal = LoadObject<UTexture>(
        nullptr,
        TEXT("/Engine/EngineMaterials/DefaultNormal.DefaultNormal")
    );

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Cubus material builder defaults: Color=%s Normal=%s"),
        IsValid(DefaultColor) ? TEXT("valid") : TEXT("missing"),
        IsValid(DefaultNormal) ? TEXT("valid") : TEXT("missing")
    );

    if (!IsValid(DefaultColor) || !IsValid(DefaultNormal))
    {
        UE_LOG(LogTemp, Error, TEXT("Cubus material builder: required default texture missing."));
        return nullptr;
    }

    UMaterialExpressionTextureCoordinate* TexCoord =
        AddExpression<UMaterialExpressionTextureCoordinate>(Material, -2600, -700);
    UMaterialExpressionScalarParameter* TextureScale =
        Scalar(Material, TEXT("TextureScale"), 1.0f, -2600, -500);
    UMaterialExpressionMultiply* UV =
        Multiply(Material, TexCoord, TextureScale, -2350, -650);

    UMaterialExpressionVertexColor* VertexColor =
        AddExpression<UMaterialExpressionVertexColor>(Material, -2600, 300);
    UMaterialExpressionComponentMask* Selector =
        Mask(Material, VertexColor, false, false, false, true, -2350, 300);

    UMaterialExpressionConstant* Two = Constant(Material, 2.0f, -2350, 500);
    UMaterialExpressionConstant* Half = Constant(Material, 0.5f, -2350, 650);

    UMaterialExpressionSaturate* BottomMask = Saturate(
        Material,
        Multiply(Material, Subtract(Material, Selector, Half, -2100, 650), Two, -1850, 650),
        -1600,
        650
    );

    UMaterialExpressionSaturate* TopMask = Saturate(
        Material,
        Multiply(Material, Selector, Two, -1850, 500),
        -1600,
        500
    );

    UMaterialExpressionSaturate* SideMask = Saturate(
        Material,
        OneMinus(Material, Add(Material, TopMask, BottomMask, -1350, 575), -1100, 575),
        -850,
        575
    );

    const FSurface Side = Surface(Material, TEXT("Side"), UV, DefaultColor, DefaultNormal, -1900);
    const FSurface Top = Surface(Material, TEXT("Top"), UV, DefaultColor, DefaultNormal, -1450);
    const FSurface Bottom = Surface(Material, TEXT("Bottom"), UV, DefaultColor, DefaultNormal, -1000);

    UMaterialExpressionLinearInterpolate* TopBottomBase =
        Lerp(Material, Top.Base, Bottom.Base, BottomMask, -500, -800);
    UMaterialExpressionLinearInterpolate* FinalBase =
        Lerp(Material, Side.Base, TopBottomBase, Add(Material, TopMask, BottomMask, -500, -600), -200, -800);

    UMaterialExpressionLinearInterpolate* TopBottomNormal =
        Lerp(Material, Top.Normal, Bottom.Normal, BottomMask, -500, -500);
    UMaterialExpressionLinearInterpolate* FinalNormal =
        Lerp(Material, Side.Normal, TopBottomNormal, Add(Material, TopMask, BottomMask, -500, -300), -200, -500);

    UMaterialExpressionLinearInterpolate* TopBottomORM =
        Lerp(Material, Top.ORM, Bottom.ORM, BottomMask, -500, -200);
    UMaterialExpressionLinearInterpolate* FinalORM =
        Lerp(Material, Side.ORM, TopBottomORM, Add(Material, TopMask, BottomMask, -500, 0), -200, -200);

    UMaterialExpressionVectorParameter* Tint =
        Vector(Material, TEXT("Tint"), FLinearColor::White, -200, -1000);
    UMaterialExpressionMultiply* TintedBase =
        Multiply(Material, FinalBase, Tint, 100, -800);

    UMaterialExpressionVectorParameter* EmissiveColor =
        Vector(Material, TEXT("EmissiveColor"), FLinearColor::Black, -200, 250);
    UMaterialExpressionScalarParameter* EmissiveStrength =
        Scalar(Material, TEXT("EmissiveStrength"), 0.0f, -200, 400);
    UMaterialExpressionMultiply* Emissive =
        Multiply(Material, EmissiveColor, EmissiveStrength, 100, 300);

    UMaterialEditorOnlyData* Data = Material->GetEditorOnlyData();
    Connect(Data->BaseColor, TintedBase);
    Connect(Data->Normal, FinalNormal);
    Connect(Data->AmbientOcclusion, FinalORM, 1);
    Connect(Data->Roughness, FinalORM, 2);
    Connect(Data->Metallic, FinalORM, 3);
    Connect(Data->EmissiveColor, Emissive);

    Save(Material);

    const int32 ExpressionCount =
        Material->GetEditorOnlyData()->ExpressionCollection.Expressions.Num();

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Cubus material builder: built and saved %d registered expressions."),
        ExpressionCount
    );

    return Material;
#else
    return nullptr;
#endif
}
