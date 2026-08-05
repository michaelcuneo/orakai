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

    template <typename TExpression>
    TExpression* AddExpression(UMaterial* Material, int32 X, int32 Y)
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
        int32 OutputIndex = 0
    )
    {
        Input.Expression = Expression;
        Input.OutputIndex = OutputIndex;
    }

    void AddCustomInput(
        UMaterialExpressionCustom* Custom,
        const TCHAR* Name,
        UMaterialExpression* Expression,
        int32 OutputIndex = 0
    )
    {
        FCustomInput& Input = Custom->Inputs.AddDefaulted_GetRef();
        Input.InputName = FName(Name);
        Connect(Input.Input, Expression, OutputIndex);
    }

    UMaterialExpressionTextureObjectParameter* AddTextureArray(
        UMaterial* Material,
        const TCHAR* Name,
        UTexture* Texture,
        EMaterialSamplerType SamplerType,
        int32 X,
        int32 Y
    )
    {
        UMaterialExpressionTextureObjectParameter* Node =
            AddExpression<UMaterialExpressionTextureObjectParameter>(
                Material,
                X,
                Y
            );
        Node->ParameterName = Name;
        Node->Texture = Texture;
        Node->SamplerType = SamplerType;
        return Node;
    }

    UMaterialExpressionScalarParameter* AddScalar(
        UMaterial* Material,
        const TCHAR* Name,
        float Value,
        int32 X,
        int32 Y
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
        bool bR,
        bool bG,
        bool bB,
        int32 X,
        int32 Y
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

    if (
        !IsValid(BaseColorAsset) ||
        !IsValid(NormalAsset) ||
        !IsValid(OrmAsset) ||
        !IsValid(HeightAsset) ||
        !IsValid(MacroAsset) ||
        !IsValid(DetailNormalAsset)
    )
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Build all six Cubus density texture arrays before building M_CubusDensityPBR.")
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
        AddExpression<UMaterialExpressionWorldPosition>(Material, -1800, -700);
    UMaterialExpressionVertexNormalWS* VertexNormal =
        AddExpression<UMaterialExpressionVertexNormalWS>(Material, -1800, -520);
    UMaterialExpressionTextureCoordinate* Palette =
        AddExpression<UMaterialExpressionTextureCoordinate>(Material, -1800, -340);
    Palette->CoordinateIndex = 0;
    UMaterialExpressionVertexColor* Weights =
        AddExpression<UMaterialExpressionVertexColor>(Material, -1800, -160);

    UMaterialExpressionTextureObjectParameter* BaseColorArray =
        AddTextureArray(Material, TEXT("DensityBaseColorArray"), BaseColorAsset, SAMPLERTYPE_Color, -1800, 80);
    UMaterialExpressionTextureObjectParameter* NormalArray =
        AddTextureArray(Material, TEXT("DensityNormalArray"), NormalAsset, SAMPLERTYPE_Normal, -1800, 240);
    UMaterialExpressionTextureObjectParameter* OrmArray =
        AddTextureArray(Material, TEXT("DensityORMArray"), OrmAsset, SAMPLERTYPE_LinearColor, -1800, 400);
    UMaterialExpressionTextureObjectParameter* HeightArray =
        AddTextureArray(Material, TEXT("DensityHeightArray"), HeightAsset, SAMPLERTYPE_LinearColor, -1800, 560);
    UMaterialExpressionTextureObjectParameter* MacroArray =
        AddTextureArray(Material, TEXT("DensityMacroColorArray"), MacroAsset, SAMPLERTYPE_Color, -1800, 720);
    UMaterialExpressionTextureObjectParameter* DetailArray =
        AddTextureArray(Material, TEXT("DensityDetailNormalArray"), DetailNormalAsset, SAMPLERTYPE_Normal, -1800, 880);

    UMaterialExpressionScalarParameter* WorldScale =
        AddScalar(Material, TEXT("CubusBaseColorWorldScale"), 0.01f, -1800, 1100);
    UMaterialExpressionScalarParameter* MacroScale =
        AddScalar(Material, TEXT("CubusMacroWorldScale"), 0.0005f, -1800, 1180);
    UMaterialExpressionScalarParameter* MacroStrength =
        AddScalar(Material, TEXT("CubusMacroColorStrength"), 0.35f, -1800, 1260);
    UMaterialExpressionScalarParameter* DetailScale =
        AddScalar(Material, TEXT("CubusDetailNormalWorldScale"), 0.04f, -1800, 1340);
    UMaterialExpressionScalarParameter* DetailStrength =
        AddScalar(Material, TEXT("CubusDetailNormalStrength"), 0.35f, -1800, 1420);
    UMaterialExpressionScalarParameter* BlendSharpness =
        AddScalar(Material, TEXT("CubusTriplanarBlendSharpness"), 4.0f, -1800, 1500);
    UMaterialExpressionScalarParameter* HeightBlendStrength =
        AddScalar(Material, TEXT("CubusHeightBlendStrength"), 3.0f, -1800, 1580);
    UMaterialExpressionScalarParameter* PackingBase =
        AddScalar(Material, TEXT("DensityMaterialIdPackingBase"), 32.0f, -1800, 1660);
    UMaterialExpressionScalarParameter* Wetness =
        AddScalar(Material, TEXT("CubusWeatherWetness"), 0.0f, -1800, 1740);
    UMaterialExpressionScalarParameter* WetDarkening =
        AddScalar(Material, TEXT("CubusWeatherWetDarkening"), 0.65f, -1800, 1820);
    UMaterialExpressionScalarParameter* WetRoughness =
        AddScalar(Material, TEXT("CubusWeatherWetRoughness"), 0.12f, -1800, 1900);

    const TCHAR* SharedCode = TEXT(R"(
float scale = max(WorldScale, 0.000001);
float sharpness = max(BlendSharpness, 1.0);
float packingBase = max(MaterialIdPackingBase, 2.0);
float packed01 = floor(MaterialPalette.x + 0.5);
float packed23 = floor(MaterialPalette.y + 0.5);
float4 materialIds = max(float4(
    fmod(packed01, packingBase), floor(packed01 / packingBase),
    fmod(packed23, packingBase), floor(packed23 / packingBase)), 1.0);
float4 blendWeights = saturate(float4(MaterialWeightsRgb, MaterialWeightA));
float weightSum = dot(blendWeights, 1.0);
blendWeights = weightSum > 0.000001 ? blendWeights / weightSum : float4(1,0,0,0);
float3 geometryNormal = normalize(VertexNormal);
float3 projectionWeights = pow(abs(geometryNormal), sharpness);
projectionWeights /= max(dot(projectionWeights, 1.0), 0.000001);
float heightStrength = max(HeightBlendStrength, 0.0);
if (heightStrength > 0.000001)
{
    float4 heights = 0.0;
    [unroll] for (int i = 0; i < 4; ++i)
    {
        float slice = materialIds[i];
        heights[i] =
            Texture2DArraySample(HeightArray, HeightArraySampler, float3(WorldPosition.yz * scale, slice)).r * projectionWeights.x +
            Texture2DArraySample(HeightArray, HeightArraySampler, float3(WorldPosition.xz * scale, slice)).r * projectionWeights.y +
            Texture2DArraySample(HeightArray, HeightArraySampler, float3(WorldPosition.xy * scale, slice)).r * projectionWeights.z;
    }
    float4 shaped = blendWeights * exp2((heights - 0.5) * heightStrength);
    float shapedSum = dot(shaped, 1.0);
    if (shapedSum > 0.000001) blendWeights = shaped / shapedSum;
}
)");

    auto AddSharedInputs = [&](UMaterialExpressionCustom* Custom)
    {
        AddCustomInput(Custom, TEXT("WorldPosition"), WorldPosition);
        AddCustomInput(Custom, TEXT("VertexNormal"), VertexNormal);
        AddCustomInput(Custom, TEXT("MaterialPalette"), Palette);
        AddCustomInput(Custom, TEXT("MaterialWeightsRgb"), Weights, 0);
        AddCustomInput(Custom, TEXT("MaterialWeightA"), Weights, 4);
        AddCustomInput(Custom, TEXT("WorldScale"), WorldScale);
        AddCustomInput(Custom, TEXT("BlendSharpness"), BlendSharpness);
        AddCustomInput(Custom, TEXT("HeightBlendStrength"), HeightBlendStrength);
        AddCustomInput(Custom, TEXT("HeightArray"), HeightArray);
        AddCustomInput(Custom, TEXT("MaterialIdPackingBase"), PackingBase);
    };

    UMaterialExpressionCustom* BaseColor =
        AddExpression<UMaterialExpressionCustom>(Material, -700, -620);
    BaseColor->Description = TEXT("Cubus material and macro colour");
    BaseColor->OutputType = CMOT_Float3;
    BaseColor->Code = FString(SharedCode) + TEXT(R"(
float3 baseResult = 0.0;
float3 macroResult = 0.0;
float macroScale = max(MacroScale, 0.000001);
[unroll] for (int i = 0; i < 4; ++i)
{
    float slice = materialIds[i];
    float w = blendWeights[i];
    float3 baseSample =
        Texture2DArraySample(BaseColorArray, BaseColorArraySampler, float3(WorldPosition.yz * scale, slice)).rgb * projectionWeights.x +
        Texture2DArraySample(BaseColorArray, BaseColorArraySampler, float3(WorldPosition.xz * scale, slice)).rgb * projectionWeights.y +
        Texture2DArraySample(BaseColorArray, BaseColorArraySampler, float3(WorldPosition.xy * scale, slice)).rgb * projectionWeights.z;
    float3 macroSample =
        Texture2DArraySample(MacroColorArray, MacroColorArraySampler, float3(WorldPosition.yz * macroScale, slice)).rgb * projectionWeights.x +
        Texture2DArraySample(MacroColorArray, MacroColorArraySampler, float3(WorldPosition.xz * macroScale, slice)).rgb * projectionWeights.y +
        Texture2DArraySample(MacroColorArray, MacroColorArraySampler, float3(WorldPosition.xy * macroScale, slice)).rgb * projectionWeights.z;
    baseResult += baseSample * w;
    macroResult += macroSample * w;
}
float3 macroMultiplier = lerp(1.0, macroResult * 2.0, saturate(MacroStrength));
float3 dryColor = baseResult * macroMultiplier;
float wetFactor = saturate(WeatherWetness) * saturate(WeatherWetDarkening);
return dryColor * lerp(1.0, 0.55, wetFactor);
)");
    AddSharedInputs(BaseColor);
    AddCustomInput(BaseColor, TEXT("BaseColorArray"), BaseColorArray);
    AddCustomInput(BaseColor, TEXT("MacroColorArray"), MacroArray);
    AddCustomInput(BaseColor, TEXT("MacroScale"), MacroScale);
    AddCustomInput(BaseColor, TEXT("MacroStrength"), MacroStrength);
    AddCustomInput(BaseColor, TEXT("WeatherWetness"), Wetness);
    AddCustomInput(BaseColor, TEXT("WeatherWetDarkening"), WetDarkening);

    UMaterialExpressionCustom* WorldNormal =
        AddExpression<UMaterialExpressionCustom>(Material, -700, -80);
    WorldNormal->Description = TEXT("Cubus base and micro world normal");
    WorldNormal->OutputType = CMOT_Float3;
    WorldNormal->Code = FString(SharedCode) + TEXT(R"(
float3 baseResult = 0.0;
float3 detailResult = 0.0;
float detailScale = max(DetailScale, 0.000001);
float3 signs = float3(
    geometryNormal.x < 0.0 ? -1.0 : 1.0,
    geometryNormal.y < 0.0 ? -1.0 : 1.0,
    geometryNormal.z < 0.0 ? -1.0 : 1.0
);
[unroll] for (int i = 0; i < 4; ++i)
{
    float slice = materialIds[i];
    float w = blendWeights[i];
    float2 bx = Texture2DArraySample(NormalArray, NormalArraySampler, float3(WorldPosition.yz * scale, slice)).rg * 2.0 - 1.0;
    float2 by = Texture2DArraySample(NormalArray, NormalArraySampler, float3(WorldPosition.xz * scale, slice)).rg * 2.0 - 1.0;
    float2 bz = Texture2DArraySample(NormalArray, NormalArraySampler, float3(WorldPosition.xy * scale, slice)).rg * 2.0 - 1.0;
    float2 dx = Texture2DArraySample(DetailNormalArray, DetailNormalArraySampler, float3(WorldPosition.yz * detailScale, slice)).rg * 2.0 - 1.0;
    float2 dy = Texture2DArraySample(DetailNormalArray, DetailNormalArraySampler, float3(WorldPosition.xz * detailScale, slice)).rg * 2.0 - 1.0;
    float2 dz = Texture2DArraySample(DetailNormalArray, DetailNormalArraySampler, float3(WorldPosition.xy * detailScale, slice)).rg * 2.0 - 1.0;
    float3 bnx = float3(sqrt(saturate(1.0-dot(bx,bx))) * signs.x, bx.x, bx.y);
    float3 bny = float3(by.x, sqrt(saturate(1.0-dot(by,by))) * signs.y, by.y);
    float3 bnz = float3(bz.x, bz.y, sqrt(saturate(1.0-dot(bz,bz))) * signs.z);
    float3 dnx = float3(sqrt(saturate(1.0-dot(dx,dx))) * signs.x, dx.x, dx.y);
    float3 dny = float3(dy.x, sqrt(saturate(1.0-dot(dy,dy))) * signs.y, dy.y);
    float3 dnz = float3(dz.x, dz.y, sqrt(saturate(1.0-dot(dz,dz))) * signs.z);
    baseResult += normalize(bnx*projectionWeights.x + bny*projectionWeights.y + bnz*projectionWeights.z) * w;
    detailResult += normalize(dnx*projectionWeights.x + dny*projectionWeights.y + dnz*projectionWeights.z) * w;
}
float3 baseNormal = normalize(baseResult);
float3 detailNormal = normalize(detailResult);
return normalize(lerp(baseNormal, normalize(baseNormal + detailNormal - geometryNormal), saturate(DetailStrength)));
)");
    AddSharedInputs(WorldNormal);
    AddCustomInput(WorldNormal, TEXT("NormalArray"), NormalArray);
    AddCustomInput(WorldNormal, TEXT("DetailNormalArray"), DetailArray);
    AddCustomInput(WorldNormal, TEXT("DetailScale"), DetailScale);
    AddCustomInput(WorldNormal, TEXT("DetailStrength"), DetailStrength);

    UMaterialExpressionCustom* Orm =
        AddExpression<UMaterialExpressionCustom>(Material, -700, 500);
    Orm->Description = TEXT("Cubus blended ORM and wet roughness");
    Orm->OutputType = CMOT_Float3;
    Orm->Code = FString(SharedCode) + TEXT(R"(
float3 result = 0.0;
[unroll] for (int i = 0; i < 4; ++i)
{
    float slice = materialIds[i];
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

    Save(Material);

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Built Cubus density terrain material: palette, macro colour, PBR micro detail and weather response.")
    );

    return Material;
#else
    return nullptr;
#endif
}
