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
    constexpr const TCHAR* HeightArrayPath =
        TEXT("/Game/Cubus/Materials/Arrays/TA_CubusDensityHeight.TA_CubusDensityHeight");
    constexpr const TCHAR* MacroColorArrayPath =
        TEXT("/Game/Cubus/Materials/Arrays/TA_CubusDensityMacroColor.TA_CubusDensityMacroColor");
    constexpr const TCHAR* DetailNormalArrayPath =
        TEXT("/Game/Cubus/Materials/Arrays/TA_CubusDensityDetailNormal.TA_CubusDensityDetailNormal");
    constexpr const TCHAR* NeutralDataTexturePath =
        TEXT("/Game/Cubus/Materials/Arrays/T_CubusDensityNeutralBaseColor.T_CubusDensityNeutralBaseColor");

    template <typename TExpression>
    TExpression* AddExpression(UMaterial* Material, const int32 X, const int32 Y)
    {
        TExpression* Expression = Cast<TExpression>(
            UMaterialEditingLibrary::CreateMaterialExpression(
                Material,
                TExpression::StaticClass(),
                X,
                Y
            )
        );
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
        FCustomInput& Input = Custom->Inputs.AddDefaulted_GetRef();
        Input.InputName = FName(Name);
        Connect(Input.Input, Expression, OutputIndex);
    }

    UMaterialExpressionTextureObjectParameter* AddTextureObject(
        UMaterial* Material,
        const TCHAR* Name,
        UTexture* Texture,
        const EMaterialSamplerType SamplerType,
        const int32 X,
        const int32 Y
    )
    {
        UMaterialExpressionTextureObjectParameter* Node =
            AddExpression<UMaterialExpressionTextureObjectParameter>(Material, X, Y);
        Node->ParameterName = Name;
        Node->Texture = Texture;
        Node->SamplerType = SamplerType;
        return Node;
    }

    UMaterialExpressionScalarParameter* AddScalar(
        UMaterial* Material,
        const TCHAR* Name,
        const float Value,
        const int32 X,
        const int32 Y
    )
    {
        UMaterialExpressionScalarParameter* Node =
            AddExpression<UMaterialExpressionScalarParameter>(Material, X, Y);
        Node->ParameterName = Name;
        Node->DefaultValue = Value;
        return Node;
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
        UMaterialExpressionComponentMask* Node =
            AddExpression<UMaterialExpressionComponentMask>(Material, X, Y);
        Node->R = bR;
        Node->G = bG;
        Node->B = bB;
        Node->A = false;
        Connect(Node->Input, Input);
        return Node;
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

    UTexture* BaseColorAsset = LoadObject<UTexture>(nullptr, BaseColorArrayPath);
    UTexture* NormalAsset = LoadObject<UTexture>(nullptr, NormalArrayPath);
    UTexture* OrmAsset = LoadObject<UTexture>(nullptr, OrmArrayPath);
    UTexture* HeightAsset = LoadObject<UTexture>(nullptr, HeightArrayPath);
    UTexture* MacroAsset = LoadObject<UTexture>(nullptr, MacroColorArrayPath);
    UTexture* DetailNormalAsset = LoadObject<UTexture>(nullptr, DetailNormalArrayPath);
    UTexture* NeutralDataAsset = LoadObject<UTexture>(nullptr, NeutralDataTexturePath);

    if (
        !IsValid(BaseColorAsset) ||
        !IsValid(NormalAsset) ||
        !IsValid(OrmAsset) ||
        !IsValid(HeightAsset) ||
        !IsValid(MacroAsset) ||
        !IsValid(DetailNormalAsset) ||
        !IsValid(NeutralDataAsset)
    )
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Build all Cubus density texture assets before building M_CubusDensityPBR.")
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
        AddExpression<UMaterialExpressionWorldPosition>(Material, -1900, -720);
    UMaterialExpressionVertexNormalWS* VertexNormal =
        AddExpression<UMaterialExpressionVertexNormalWS>(Material, -1900, -540);
    UMaterialExpressionTextureCoordinate* Palette =
        AddExpression<UMaterialExpressionTextureCoordinate>(Material, -1900, -360);
    Palette->CoordinateIndex = 0;
    UMaterialExpressionVertexColor* Weights =
        AddExpression<UMaterialExpressionVertexColor>(Material, -1900, -180);

    UMaterialExpressionTextureObjectParameter* BaseColorArray =
        AddTextureObject(Material, TEXT("DensityBaseColorArray"), BaseColorAsset, SAMPLERTYPE_Color, -1900, 80);
    UMaterialExpressionTextureObjectParameter* NormalArray =
        AddTextureObject(Material, TEXT("DensityNormalArray"), NormalAsset, SAMPLERTYPE_Normal, -1900, 240);
    UMaterialExpressionTextureObjectParameter* OrmArray =
        AddTextureObject(Material, TEXT("DensityORMArray"), OrmAsset, SAMPLERTYPE_LinearColor, -1900, 400);
    UMaterialExpressionTextureObjectParameter* HeightArray =
        AddTextureObject(Material, TEXT("DensityHeightArray"), HeightAsset, SAMPLERTYPE_LinearColor, -1900, 560);
    UMaterialExpressionTextureObjectParameter* MacroArray =
        AddTextureObject(Material, TEXT("DensityMacroColorArray"), MacroAsset, SAMPLERTYPE_Color, -1900, 720);
    UMaterialExpressionTextureObjectParameter* DetailArray =
        AddTextureObject(Material, TEXT("DensityDetailNormalArray"), DetailNormalAsset, SAMPLERTYPE_Normal, -1900, 880);
    UMaterialExpressionTextureObjectParameter* MaterialData =
        AddTextureObject(Material, TEXT("DensityMaterialData"), NeutralDataAsset, SAMPLERTYPE_LinearColor, -1900, 1040);

    UMaterialExpressionScalarParameter* TableWidth =
        AddScalar(Material, TEXT("DensityMaterialTableWidth"), 1.0f, -1900, 1240);
    UMaterialExpressionScalarParameter* PackingBase =
        AddScalar(Material, TEXT("DensityMaterialIdPackingBase"), 32.0f, -1900, 1320);
    UMaterialExpressionScalarParameter* Wetness =
        AddScalar(Material, TEXT("CubusWeatherWetness"), 0.0f, -1900, 1400);
    UMaterialExpressionScalarParameter* WetDarkening =
        AddScalar(Material, TEXT("CubusWeatherWetDarkening"), 0.65f, -1900, 1480);
    UMaterialExpressionScalarParameter* WetRoughness =
        AddScalar(Material, TEXT("CubusWeatherWetRoughness"), 0.12f, -1900, 1560);

    const TCHAR* SharedCode = TEXT(R"(
float packingBase = max(MaterialIdPackingBase, 2.0);
float tableWidth = max(MaterialTableWidth, 1.0);
float packed01 = floor(MaterialPalette.x + 0.5);
float packed23 = floor(MaterialPalette.y + 0.5);
float4 materialIds = max(float4(
    fmod(packed01, packingBase), floor(packed01 / packingBase),
    fmod(packed23, packingBase), floor(packed23 / packingBase)), 1.0);

float4 blendWeights = saturate(float4(MaterialWeightsRgb, MaterialWeightA));
float weightSum = dot(blendWeights, 1.0);
blendWeights = weightSum > 0.000001
    ? blendWeights / weightSum
    : float4(1.0, 0.0, 0.0, 0.0);

float4 tint[4];
float4 projectionData[4];
float4 detailData[4];
float4 emissiveData[4];

[unroll] for (int i = 0; i < 4; ++i)
{
    float tableU = (materialIds[i] + 0.5) / tableWidth;
    tint[i] = Texture2DSampleLevel(MaterialData, MaterialDataSampler, float2(tableU, 0.125), 0);
    projectionData[i] = Texture2DSampleLevel(MaterialData, MaterialDataSampler, float2(tableU, 0.375), 0);
    detailData[i] = Texture2DSampleLevel(MaterialData, MaterialDataSampler, float2(tableU, 0.625), 0);
    emissiveData[i] = Texture2DSampleLevel(MaterialData, MaterialDataSampler, float2(tableU, 0.875), 0);
}

float blendedSharpness = 0.0;
[unroll] for (int i = 0; i < 4; ++i)
{
    blendedSharpness += max(projectionData[i].y, 0.1) * blendWeights[i];
}

float3 geometryNormal = normalize(VertexNormal);
float3 projectionWeights = pow(abs(geometryNormal), max(blendedSharpness, 0.1));
projectionWeights /= max(dot(projectionWeights, 1.0), 0.000001);

float4 contrastedWeights = 0.0;
[unroll] for (int i = 0; i < 4; ++i)
{
    contrastedWeights[i] = pow(
        max(blendWeights[i], 0.000001),
        max(projectionData[i].w, 0.01)
    );
}
float contrastedSum = dot(contrastedWeights, 1.0);
if (contrastedSum > 0.000001)
{
    blendWeights = contrastedWeights / contrastedSum;
}

float4 heights = 0.0;
[unroll] for (int i = 0; i < 4; ++i)
{
    float slice = materialIds[i];
    float scale = max(projectionData[i].x, 0.000001);
    heights[i] =
        Texture2DArraySample(HeightArray, HeightArraySampler, float3(WorldPosition.yz * scale, slice)).r * projectionWeights.x +
        Texture2DArraySample(HeightArray, HeightArraySampler, float3(WorldPosition.xz * scale, slice)).r * projectionWeights.y +
        Texture2DArraySample(HeightArray, HeightArraySampler, float3(WorldPosition.xy * scale, slice)).r * projectionWeights.z;
}

float4 shapedWeights = 0.0;
[unroll] for (int i = 0; i < 4; ++i)
{
    float heightStrength = max(projectionData[i].z, 0.0);
    shapedWeights[i] = blendWeights[i] * exp2((heights[i] - 0.5) * heightStrength);
}
float shapedSum = dot(shapedWeights, 1.0);
if (shapedSum > 0.000001)
{
    blendWeights = shapedWeights / shapedSum;
}
)");

    auto AddSharedInputs = [&](UMaterialExpressionCustom* Custom)
    {
        AddCustomInput(Custom, TEXT("WorldPosition"), WorldPosition);
        AddCustomInput(Custom, TEXT("VertexNormal"), VertexNormal);
        AddCustomInput(Custom, TEXT("MaterialPalette"), Palette);
        AddCustomInput(Custom, TEXT("MaterialWeightsRgb"), Weights, 0);
        AddCustomInput(Custom, TEXT("MaterialWeightA"), Weights, 4);
        AddCustomInput(Custom, TEXT("HeightArray"), HeightArray);
        AddCustomInput(Custom, TEXT("MaterialData"), MaterialData);
        AddCustomInput(Custom, TEXT("MaterialTableWidth"), TableWidth);
        AddCustomInput(Custom, TEXT("MaterialIdPackingBase"), PackingBase);
    };

    UMaterialExpressionCustom* BaseColor =
        AddExpression<UMaterialExpressionCustom>(Material, -720, -660);
    BaseColor->Description = TEXT("Cubus density base colour, tint, macro variation and wetness");
    BaseColor->OutputType = CMOT_Float3;
    BaseColor->Code = FString(SharedCode) + TEXT(R"(
float3 result = 0.0;
[unroll] for (int i = 0; i < 4; ++i)
{
    float slice = materialIds[i];
    float scale = max(projectionData[i].x, 0.000001);
    float macroScale = max(detailData[i].x, 0.000001);
    float macroStrength = saturate(detailData[i].y);

    float3 baseSample =
        Texture2DArraySample(BaseColorArray, BaseColorArraySampler, float3(WorldPosition.yz * scale, slice)).rgb * projectionWeights.x +
        Texture2DArraySample(BaseColorArray, BaseColorArraySampler, float3(WorldPosition.xz * scale, slice)).rgb * projectionWeights.y +
        Texture2DArraySample(BaseColorArray, BaseColorArraySampler, float3(WorldPosition.xy * scale, slice)).rgb * projectionWeights.z;

    float3 macroSample =
        Texture2DArraySample(MacroColorArray, MacroColorArraySampler, float3(WorldPosition.yz * macroScale, slice)).rgb * projectionWeights.x +
        Texture2DArraySample(MacroColorArray, MacroColorArraySampler, float3(WorldPosition.xz * macroScale, slice)).rgb * projectionWeights.y +
        Texture2DArraySample(MacroColorArray, MacroColorArraySampler, float3(WorldPosition.xy * macroScale, slice)).rgb * projectionWeights.z;

    float3 macroMultiplier = lerp(1.0, macroSample * 2.0, macroStrength);
    result += baseSample * tint[i].rgb * macroMultiplier * blendWeights[i];
}

float wetFactor = saturate(WeatherWetness) * saturate(WeatherWetDarkening);
return result * lerp(1.0, 0.55, wetFactor);
)");
    AddSharedInputs(BaseColor);
    AddCustomInput(BaseColor, TEXT("BaseColorArray"), BaseColorArray);
    AddCustomInput(BaseColor, TEXT("MacroColorArray"), MacroArray);
    AddCustomInput(BaseColor, TEXT("WeatherWetness"), Wetness);
    AddCustomInput(BaseColor, TEXT("WeatherWetDarkening"), WetDarkening);

    UMaterialExpressionCustom* WorldNormal =
        AddExpression<UMaterialExpressionCustom>(Material, -720, -80);
    WorldNormal->Description = TEXT("Cubus density base and micro world normal");
    WorldNormal->OutputType = CMOT_Float3;
    WorldNormal->Code = FString(SharedCode) + TEXT(R"(
float3 result = 0.0;
float3 signs = float3(
    geometryNormal.x < 0.0 ? -1.0 : 1.0,
    geometryNormal.y < 0.0 ? -1.0 : 1.0,
    geometryNormal.z < 0.0 ? -1.0 : 1.0
);

[unroll] for (int i = 0; i < 4; ++i)
{
    float slice = materialIds[i];
    float scale = max(projectionData[i].x, 0.000001);
    float detailScale = max(detailData[i].z, 0.000001);
    float detailStrength = max(detailData[i].w, 0.0);

    float2 bx = Texture2DArraySample(NormalArray, NormalArraySampler, float3(WorldPosition.yz * scale, slice)).rg * 2.0 - 1.0;
    float2 by = Texture2DArraySample(NormalArray, NormalArraySampler, float3(WorldPosition.xz * scale, slice)).rg * 2.0 - 1.0;
    float2 bz = Texture2DArraySample(NormalArray, NormalArraySampler, float3(WorldPosition.xy * scale, slice)).rg * 2.0 - 1.0;
    float2 dx = Texture2DArraySample(DetailNormalArray, DetailNormalArraySampler, float3(WorldPosition.yz * detailScale, slice)).rg * 2.0 - 1.0;
    float2 dy = Texture2DArraySample(DetailNormalArray, DetailNormalArraySampler, float3(WorldPosition.xz * detailScale, slice)).rg * 2.0 - 1.0;
    float2 dz = Texture2DArraySample(DetailNormalArray, DetailNormalArraySampler, float3(WorldPosition.xy * detailScale, slice)).rg * 2.0 - 1.0;

    float3 baseX = float3(sqrt(saturate(1.0 - dot(bx, bx))) * signs.x, bx.x, bx.y);
    float3 baseY = float3(by.x, sqrt(saturate(1.0 - dot(by, by))) * signs.y, by.y);
    float3 baseZ = float3(bz.x, bz.y, sqrt(saturate(1.0 - dot(bz, bz))) * signs.z);
    float3 detailX = float3(sqrt(saturate(1.0 - dot(dx, dx))) * signs.x, dx.x, dx.y);
    float3 detailY = float3(dy.x, sqrt(saturate(1.0 - dot(dy, dy))) * signs.y, dy.y);
    float3 detailZ = float3(dz.x, dz.y, sqrt(saturate(1.0 - dot(dz, dz))) * signs.z);

    float3 baseNormal = normalize(
        baseX * projectionWeights.x +
        baseY * projectionWeights.y +
        baseZ * projectionWeights.z
    );
    float3 detailNormal = normalize(
        detailX * projectionWeights.x +
        detailY * projectionWeights.y +
        detailZ * projectionWeights.z
    );
    float3 combinedNormal = normalize(
        lerp(
            baseNormal,
            normalize(baseNormal + detailNormal - geometryNormal),
            saturate(detailStrength)
        )
    );
    result += combinedNormal * blendWeights[i];
}
return normalize(result);
)");
    AddSharedInputs(WorldNormal);
    AddCustomInput(WorldNormal, TEXT("NormalArray"), NormalArray);
    AddCustomInput(WorldNormal, TEXT("DetailNormalArray"), DetailArray);

    UMaterialExpressionCustom* Orm =
        AddExpression<UMaterialExpressionCustom>(Material, -720, 500);
    Orm->Description = TEXT("Cubus density ORM and wet roughness");
    Orm->OutputType = CMOT_Float3;
    Orm->Code = FString(SharedCode) + TEXT(R"(
float3 result = 0.0;
[unroll] for (int i = 0; i < 4; ++i)
{
    float slice = materialIds[i];
    float scale = max(projectionData[i].x, 0.000001);
    float3 sampleValue =
        Texture2DArraySample(OrmArray, OrmArraySampler, float3(WorldPosition.yz * scale, slice)).rgb * projectionWeights.x +
        Texture2DArraySample(OrmArray, OrmArraySampler, float3(WorldPosition.xz * scale, slice)).rgb * projectionWeights.y +
        Texture2DArraySample(OrmArray, OrmArraySampler, float3(WorldPosition.xy * scale, slice)).rgb * projectionWeights.z;
    result += sampleValue * blendWeights[i];
}
result.g = lerp(result.g, saturate(WeatherWetRoughness), saturate(WeatherWetness));
return result;
)");
    AddSharedInputs(Orm);
    AddCustomInput(Orm, TEXT("OrmArray"), OrmArray);
    AddCustomInput(Orm, TEXT("WeatherWetness"), Wetness);
    AddCustomInput(Orm, TEXT("WeatherWetRoughness"), WetRoughness);

    UMaterialExpressionCustom* Emissive =
        AddExpression<UMaterialExpressionCustom>(Material, -720, 980);
    Emissive->Description = TEXT("Cubus density per-material emissive");
    Emissive->OutputType = CMOT_Float3;
    Emissive->Code = FString(SharedCode) + TEXT(R"(
float3 result = 0.0;
[unroll] for (int i = 0; i < 4; ++i)
{
    result += emissiveData[i].rgb * max(emissiveData[i].a, 0.0) * blendWeights[i];
}
return result;
)");
    AddSharedInputs(Emissive);

    UMaterialExpressionComponentMask* AmbientOcclusion =
        AddMask(Material, Orm, true, false, false, -180, 480);
    UMaterialExpressionComponentMask* Roughness =
        AddMask(Material, Orm, false, true, false, -180, 600);
    UMaterialExpressionComponentMask* Metallic =
        AddMask(Material, Orm, false, false, true, -180, 720);

    UMaterialEditorOnlyData* Data = Material->GetEditorOnlyData();
    Connect(Data->BaseColor, BaseColor);
    Connect(Data->Normal, WorldNormal);
    Connect(Data->AmbientOcclusion, AmbientOcclusion);
    Connect(Data->Roughness, Roughness);
    Connect(Data->Metallic, Metallic);
    Connect(Data->EmissiveColor, Emissive);

    Save(Material);

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Built Cubus density terrain material stage 6: per-material projection, macro, micro, tint, emissive and weather response.")
    );

    return Material;
#else
    return nullptr;
#endif
}
