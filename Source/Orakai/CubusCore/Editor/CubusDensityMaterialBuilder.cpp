#include "CubusCore/Editor/CubusMaterialBuilderLibrary.h"

#include "Materials/Material.h"

#if WITH_EDITOR

#include "AssetToolsModule.h"
#include "Factories/MaterialFactoryNew.h"
#include "MaterialEditingLibrary.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
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

    Material->Modify();
    Material->PreEditChange(nullptr);
    UMaterialEditingLibrary::DeleteAllMaterialExpressions(Material);
    Material->BlendMode = BLEND_Opaque;
    Material->SetShadingModel(MSM_DefaultLit);
    Material->TwoSided = true;
    Material->bUseMaterialAttributes = false;
    Material->bTangentSpaceNormal = true;

    UMaterialExpressionConstant3Vector* BaseColor =
        AddExpression<UMaterialExpressionConstant3Vector>(Material, -400, -250);
    BaseColor->Constant = FLinearColor(0.18f, 0.42f, 0.08f, 1.0f);

    UMaterialExpressionConstant* Roughness =
        AddExpression<UMaterialExpressionConstant>(Material, -400, 0);
    Roughness->R = 0.8f;

    UMaterialExpressionConstant* AmbientOcclusion =
        AddExpression<UMaterialExpressionConstant>(Material, -400, 150);
    AmbientOcclusion->R = 1.0f;

    UMaterialExpressionConstant* Metallic =
        AddExpression<UMaterialExpressionConstant>(Material, -400, 300);
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
        TEXT("Built diagnostic Cubus density material: constant green, two-sided, no arrays, no custom HLSL and no normal override.")
    );

    return Material;
#else
    return nullptr;
#endif
}
