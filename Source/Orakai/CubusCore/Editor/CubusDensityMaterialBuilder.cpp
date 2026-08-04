#include "CubusCore/Editor/CubusMaterialBuilderLibrary.h"

#include "Materials/Material.h"

#if WITH_EDITOR

#include "AssetToolsModule.h"
#include "Factories/MaterialFactoryNew.h"
#include "MaterialEditingLibrary.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace CubusDensityMaterialBuilder
{
    constexpr const TCHAR* PackagePath =
        TEXT("/Game/Cubus/Materials/M_CubusDensityPBR");
    constexpr const TCHAR* AssetName = TEXT("M_CubusDensityPBR");

    constexpr const TCHAR* BaseColorArrayPath =
        TEXT("/Game/Cubus/Materials/Arrays/TA_CubusDensityBaseColor.TA_CubusDensityBaseColor");
    constexpr const TCHAR* NormalArrayPath =
        TEXT("/Game/Cubus/Materials/Arrays/TA_CubusDensityNormal.TA_CubusDensityNormal");
    constexpr const TCHAR* OrmArrayPath =
        TEXT("/Game/Cubus/Materials/Arrays/TA_CubusDensityORM.TA_CubusDensityORM");

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

    void AddCustomInput(
        UMaterialExpressionCustom* Custom,
        const TCHAR* Name,
        UMaterialExpression* Expression,
        const int32 OutputIndex = 0
    )
    {
        FCustomInput Input;
        Input.InputName = FName(Name);
        Connect(Input.Input, Expression, OutputIndex);
        Custom->Inputs.Add(Input);
    }

    UMaterialExpressionComponentMask* AddMask(
        UMaterial* Material,
        UMaterialExpression* Input,
        const bool bR,
        const bool bG,
        const bool bB,
        const int32 X,
        const int32 Y
    )
    {
        UMaterialExpressionComponentMask* Mask =
            AddExpression<UMaterialExpressionComponentMask>(Material, X, Y);

        Mask->R = bR;
        Mask->G = bG;
        Mask->B = bB;
        Mask->A = false;
        Connect(Mask->Input, Input);
        return Mask;
    }

    UMaterialExpressionTextureObjectParameter* AddTextureArray(
        UMaterial* Material,
        const TCHAR* ParameterName,
        UTexture* Texture,
        const EMaterialSamplerType SamplerType,
        const int32 X,
        const int32 Y
    )
    {
        UMaterialExpressionTextureObjectParameter* Result =
            AddExpression<UMaterialExpressionTextureObjectParameter>(
                Material,
                X,
                Y
            );

        Result->ParameterName = ParameterName;
        Result->Texture = Texture;
        Result->SamplerType = SamplerType;
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

    UTexture* BaseColorArrayAsset =
        LoadObject<UTexture>(nullptr, BaseColorArrayPath);
    UTexture* NormalArrayAsset =
        LoadObject<UTexture>(nullptr, NormalArrayPath);
    UTexture* OrmArrayAsset =
        LoadObject<UTexture>(nullptr, OrmArrayPath);

    if (
        !IsValid(BaseColorArrayAsset) ||
        !IsValid(NormalArrayAsset) ||
        !IsValid(OrmArrayAsset)
    )
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "Build the Cubus density BaseColor, Normal and ORM texture "
                "arrays before building M_CubusDensityPBR. "
                "BaseColor=%s Normal=%s ORM=%s"
            ),
            IsValid(BaseColorArrayAsset) ? TEXT("valid") : TEXT("missing"),
            IsValid(NormalArrayAsset) ? TEXT("valid") : TEXT("missing"),
            IsValid(OrmArrayAsset) ? TEXT("valid") : TEXT("missing")
        );
        return nullptr;
    }

    Material->Modify();
    Material->PreEditChange(nullptr);
    UMaterialEditingLibrary::DeleteAllMaterialExpressions(Material);
    Material->BlendMode = BLEND_Opaque;
    Material->SetShadingModel(MSM_DefaultLit);
    Material->TwoSided = false;
    Material->bUseMaterialAttributes = false;
    Material->bTangentSpaceNormal = false;

    UMaterialExpressionWorldPosition* WorldPosition =
        AddExpression<UMaterialExpressionWorldPosition>(Material, -1450, -620);

    UMaterialExpressionVertexNormalWS* VertexNormal =
        AddExpression<UMaterialExpressionVertexNormalWS>(Material, -1450, -440);

    UMaterialExpressionTextureCoordinate* MaterialPalette =
        AddExpression<UMaterialExpressionTextureCoordinate>(Material, -1450, -260);
    MaterialPalette->CoordinateIndex = 0;

    UMaterialExpressionVertexColor* MaterialWeights =
        AddExpression<UMaterialExpressionVertexColor>(Material, -1450, -80);

    UMaterialExpressionTextureObjectParameter* BaseColorArray =
        AddTextureArray(
            Material,
            TEXT("DensityBaseColorArray"),
            BaseColorArrayAsset,
            SAMPLERTYPE_Color,
            -1450,
            120
        );

    UMaterialExpressionTextureObjectParameter* NormalArray =
        AddTextureArray(
            Material,
            TEXT("DensityNormalArray"),
            NormalArrayAsset,
            SAMPLERTYPE_Normal,
            -1450,
            300
        );
    (void)NormalArray;

    UMaterialExpressionTextureObjectParameter* OrmArray =
        AddTextureArray(
            Material,
            TEXT("DensityORMArray"),
            OrmArrayAsset,
            SAMPLERTYPE_LinearColor,
            -1450,
            480
        );

    UMaterialExpressionScalarParameter* WorldScale =
        AddExpression<UMaterialExpressionScalarParameter>(Material, -1450, 680);
    WorldScale->ParameterName = TEXT("CubusBaseColorWorldScale");
    WorldScale->DefaultValue = 0.01f;

    UMaterialExpressionScalarParameter* BlendSharpness =
        AddExpression<UMaterialExpressionScalarParameter>(Material, -1450, 840);
    BlendSharpness->ParameterName = TEXT("CubusTriplanarBlendSharpness");
    BlendSharpness->DefaultValue = 4.0f;

    UMaterialExpressionScalarParameter* PackingBase =
        AddExpression<UMaterialExpressionScalarParameter>(Material, -1450, 1000);
    PackingBase->ParameterName = TEXT("DensityMaterialIdPackingBase");
    PackingBase->DefaultValue = 4096.0f;

    const TCHAR* SharedCode = TEXT(R"(
float scale = max(WorldScale, 0.000001);
float sharpness = max(BlendSharpness, 1.0);
float packingBase = max(MaterialIdPackingBase, 2.0);

float packed01 = floor(MaterialPalette.x + 0.5);
float packed23 = floor(MaterialPalette.y + 0.5);

float4 materialIds = float4(
    fmod(packed01, packingBase),
    floor(packed01 / packingBase),
    fmod(packed23, packingBase),
    floor(packed23 / packingBase)
);
materialIds = max(materialIds, 1.0);

float4 blendWeights = saturate(
    float4(MaterialWeightsRgb, MaterialWeightA)
);
float blendWeightSum = dot(blendWeights, 1.0);
blendWeights = blendWeightSum > 0.000001
    ? blendWeights / blendWeightSum
    : float4(1.0, 0.0, 0.0, 0.0);

float3 geometryNormal = normalize(VertexNormal);
float3 projectionWeights = pow(abs(geometryNormal), sharpness);
projectionWeights /= max(
    projectionWeights.x + projectionWeights.y + projectionWeights.z,
    0.000001
);
)");

    UMaterialExpressionCustom* BaseColor =
        AddExpression<UMaterialExpressionCustom>(Material, -500, -500);
    BaseColor->Description = TEXT("Cubus Density Palette Triplanar Base Color");
    BaseColor->OutputType = CMOT_Float3;
    BaseColor->Code = FString(SharedCode) + TEXT(R"(
float3 result = 0.0;

[unroll]
for (int materialIndex = 0; materialIndex < 4; ++materialIndex)
{
    float slice = materialIds[materialIndex];
    float materialWeight = blendWeights[materialIndex];

    float3 sampleX = Texture2DArraySample(
        BaseColorArray,
        BaseColorArraySampler,
        float3(WorldPosition.yz * scale, slice)
    ).rgb;
    float3 sampleY = Texture2DArraySample(
        BaseColorArray,
        BaseColorArraySampler,
        float3(WorldPosition.xz * scale, slice)
    ).rgb;
    float3 sampleZ = Texture2DArraySample(
        BaseColorArray,
        BaseColorArraySampler,
        float3(WorldPosition.xy * scale, slice)
    ).rgb;

    float3 triplanar =
        sampleX * projectionWeights.x +
        sampleY * projectionWeights.y +
        sampleZ * projectionWeights.z;

    result += triplanar * materialWeight;
}

return result;
)");
    AddCustomInput(BaseColor, TEXT("WorldPosition"), WorldPosition);
    AddCustomInput(BaseColor, TEXT("VertexNormal"), VertexNormal);
    AddCustomInput(BaseColor, TEXT("MaterialPalette"), MaterialPalette);
    AddCustomInput(BaseColor, TEXT("MaterialWeightsRgb"), MaterialWeights, 0);
    AddCustomInput(BaseColor, TEXT("MaterialWeightA"), MaterialWeights, 4);
    AddCustomInput(BaseColor, TEXT("BaseColorArray"), BaseColorArray);
    AddCustomInput(BaseColor, TEXT("WorldScale"), WorldScale);
    AddCustomInput(BaseColor, TEXT("BlendSharpness"), BlendSharpness);
    AddCustomInput(BaseColor, TEXT("MaterialIdPackingBase"), PackingBase);

    UMaterialExpressionCustom* Orm =
        AddExpression<UMaterialExpressionCustom>(Material, -500, 180);
    Orm->Description = TEXT("Cubus Density Palette Triplanar ORM");
    Orm->OutputType = CMOT_Float3;
    Orm->Code = FString(SharedCode) + TEXT(R"(
float3 result = 0.0;

[unroll]
for (int materialIndex = 0; materialIndex < 4; ++materialIndex)
{
    float slice = materialIds[materialIndex];
    float materialWeight = blendWeights[materialIndex];

    float3 sampleX = Texture2DArraySample(
        OrmArray,
        OrmArraySampler,
        float3(WorldPosition.yz * scale, slice)
    ).rgb;
    float3 sampleY = Texture2DArraySample(
        OrmArray,
        OrmArraySampler,
        float3(WorldPosition.xz * scale, slice)
    ).rgb;
    float3 sampleZ = Texture2DArraySample(
        OrmArray,
        OrmArraySampler,
        float3(WorldPosition.xy * scale, slice)
    ).rgb;

    float3 triplanar =
        sampleX * projectionWeights.x +
        sampleY * projectionWeights.y +
        sampleZ * projectionWeights.z;

    result += triplanar * materialWeight;
}

return result;
)");
    AddCustomInput(Orm, TEXT("WorldPosition"), WorldPosition);
    AddCustomInput(Orm, TEXT("VertexNormal"), VertexNormal);
    AddCustomInput(Orm, TEXT("MaterialPalette"), MaterialPalette);
    AddCustomInput(Orm, TEXT("MaterialWeightsRgb"), MaterialWeights, 0);
    AddCustomInput(Orm, TEXT("MaterialWeightA"), MaterialWeights, 4);
    AddCustomInput(Orm, TEXT("OrmArray"), OrmArray);
    AddCustomInput(Orm, TEXT("WorldScale"), WorldScale);
    AddCustomInput(Orm, TEXT("BlendSharpness"), BlendSharpness);
    AddCustomInput(Orm, TEXT("MaterialIdPackingBase"), PackingBase);

    UMaterialExpressionComponentMask* AmbientOcclusion =
        AddMask(Material, Orm, true, false, false, -100, 180);
    UMaterialExpressionComponentMask* Roughness =
        AddMask(Material, Orm, false, true, false, -100, 320);
    UMaterialExpressionComponentMask* Metallic =
        AddMask(Material, Orm, false, false, true, -100, 460);

    UMaterialEditorOnlyData* Data = Material->GetEditorOnlyData();
    Connect(Data->BaseColor, BaseColor);
    Connect(Data->Normal, VertexNormal);
    Connect(Data->AmbientOcclusion, AmbientOcclusion);
    Connect(Data->Roughness, Roughness);
    Connect(Data->Metallic, Metallic);

    Save(Material);

    UE_LOG(
        LogTemp,
        Warning,
        TEXT(
            "Built Cubus density material stage 4: decoded UV0 material "
            "palette and blended BaseColor/ORM by vertex RGBA weights."
        )
    );

    return Material;
#else
    return nullptr;
#endif
}
