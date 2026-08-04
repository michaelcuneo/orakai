#include "CubusCore/Editor/CubusMaterialBuilderLibrary.h"

#include "Materials/Material.h"

#if WITH_EDITOR

#include "AssetToolsModule.h"
#include "Factories/MaterialFactoryNew.h"
#include "MaterialEditingLibrary.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace CubusDensityMaterialBuilder
{
    constexpr const TCHAR* PackagePath =
        TEXT("/Game/Cubus/Materials/M_CubusDensityPBR");
    constexpr const TCHAR* AssetName = TEXT("M_CubusDensityPBR");

    template <typename TExpression>
    TExpression* AddExpression(UMaterial* Material, const int32 X, const int32 Y)
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

    void AddCustomInput(
        UMaterialExpressionCustom* Custom,
        const TCHAR* Name,
        UMaterialExpression* Expression
    )
    {
        FCustomInput Input;
        Input.InputName = FName(Name);
        Connect(Input.Input, Expression);
        Custom->Inputs.Add(Input);
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

    UTexture* BaseColorArrayAsset = LoadObject<UTexture>(
        nullptr,
        TEXT("/Game/Cubus/Materials/Arrays/TA_CubusDensityBaseColor.TA_CubusDensityBaseColor")
    );

    if (!IsValid(BaseColorArrayAsset))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Build TA_CubusDensityBaseColor before building M_CubusDensityPBR.")
        );
        return nullptr;
    }

    Material->Modify();
    Material->PreEditChange(nullptr);
    UMaterialEditingLibrary::DeleteAllMaterialExpressions(Material);
    Material->BlendMode = BLEND_Opaque;
    Material->SetShadingModel(MSM_DefaultLit);
    Material->TwoSided = true;
    Material->bUseMaterialAttributes = false;
    Material->bTangentSpaceNormal = true;

    UMaterialExpressionWorldPosition* WorldPosition =
        AddExpression<UMaterialExpressionWorldPosition>(Material, -900, -250);

    UMaterialExpressionTextureObjectParameter* BaseColorArray =
        AddExpression<UMaterialExpressionTextureObjectParameter>(Material, -900, 0);
    BaseColorArray->ParameterName = TEXT("DensityBaseColorArray");
    BaseColorArray->Texture = BaseColorArrayAsset;
    BaseColorArray->SamplerType = SAMPLERTYPE_Color;

    UMaterialExpressionScalarParameter* WorldScale =
        AddExpression<UMaterialExpressionScalarParameter>(Material, -900, 200);
    WorldScale->ParameterName = TEXT("CubusBaseColorWorldScale");
    WorldScale->DefaultValue = 0.01f;

    UMaterialExpressionScalarParameter* GrassSlice =
        AddExpression<UMaterialExpressionScalarParameter>(Material, -900, 350);
    GrassSlice->ParameterName = TEXT("CubusGrassArraySlice");
    GrassSlice->DefaultValue = 1.0f;

    UMaterialExpressionCustom* BaseColor =
        AddExpression<UMaterialExpressionCustom>(Material, -350, -150);
    BaseColor->Description = TEXT("Cubus Grass Base Color Array Test");
    BaseColor->OutputType = CMOT_Float3;
    BaseColor->Code = TEXT(R"(
float2 uv = WorldPosition.xy * max(WorldScale, 0.000001);
return Texture2DArraySample(
    BaseColorArray,
    BaseColorArraySampler,
    float3(uv, floor(GrassSlice + 0.5))
).rgb;
)");
    AddCustomInput(BaseColor, TEXT("WorldPosition"), WorldPosition);
    AddCustomInput(BaseColor, TEXT("BaseColorArray"), BaseColorArray);
    AddCustomInput(BaseColor, TEXT("WorldScale"), WorldScale);
    AddCustomInput(BaseColor, TEXT("GrassSlice"), GrassSlice);

    UMaterialExpressionConstant* Roughness =
        AddExpression<UMaterialExpressionConstant>(Material, -350, 100);
    Roughness->R = 0.8f;

    UMaterialExpressionConstant* AmbientOcclusion =
        AddExpression<UMaterialExpressionConstant>(Material, -350, 250);
    AmbientOcclusion->R = 1.0f;

    UMaterialExpressionConstant* Metallic =
        AddExpression<UMaterialExpressionConstant>(Material, -350, 400);
    Metallic->R = 0.0f;

    UMaterialEditorOnlyData* Data = Material->GetEditorOnlyData();
    Connect(Data->BaseColor, BaseColor);
    Connect(Data->Roughness, Roughness);
    Connect(Data->AmbientOcclusion, AmbientOcclusion);
    Connect(Data->Metallic, Metallic);

    Save(Material);

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("Built Cubus density material stage 1: Grass base-color array slice 1 with planar world projection only.")
    );

    return Material;
#else
    return nullptr;
#endif
}
