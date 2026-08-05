#include "CubusCore/Editor/CubusMaterialBuilderLibrary.h"

#include "Materials/Material.h"

#if WITH_EDITOR

#include "AssetToolsModule.h"
#include "Factories/MaterialFactoryNew.h"
#include "MaterialEditingLibrary.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionSaturate.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
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

    UMaterialExpressionConstant* AddConstant(
        UMaterial* Material,
        const float Value,
        const int32 X,
        const int32 Y
    )
    {
        UMaterialExpressionConstant* Node = AddExpression<UMaterialExpressionConstant>(Material, X, Y);
        Node->R = Value;
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
        UMaterialExpressionScalarParameter* Node = AddExpression<UMaterialExpressionScalarParameter>(Material, X, Y);
        Node->ParameterName = Name;
        Node->DefaultValue = Value;
        return Node;
    }

    UMaterialExpressionVectorParameter* AddVector(
        UMaterial* Material,
        const TCHAR* Name,
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
        UMaterialExpressionMultiply* Node = AddExpression<UMaterialExpressionMultiply>(Material, X, Y);
        Connect(Node->A, A, AOutput);
        Connect(Node->B, B, BOutput);
        return Node;
    }

    UMaterialExpressionAdd* AddAdd(
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

    UMaterialExpressionOneMinus* AddOneMinus(
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

    UMaterialExpressionSaturate* AddSaturate(
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

    UMaterialExpressionLinearInterpolate* AddLerp(
        UMaterial* Material,
        UMaterialExpression* A,
        UMaterialExpression* B,
        UMaterialExpression* Alpha,
        const int32 X,
        const int32 Y
    )
    {
        UMaterialExpressionLinearInterpolate* Node = AddExpression<UMaterialExpressionLinearInterpolate>(Material, X, Y);
        Connect(Node->A, A);
        Connect(Node->B, B);
        Connect(Node->Alpha, Alpha);
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

        UTexture* BaseColorArrayAsset =
        LoadObject<UTexture>(nullptr, BaseColorArrayPath);
    UTexture* NormalArrayAsset =
        LoadObject<UTexture>(nullptr, NormalArrayPath);
    UTexture* OrmArrayAsset =
        LoadObject<UTexture>(nullptr, OrmArrayPath);
    UTexture* HeightArrayAsset =
        LoadObject<UTexture>(nullptr, HeightArrayPath);
    UTexture* MacroColorArrayAsset =
        LoadObject<UTexture>(nullptr, MacroColorArrayPath);
    UTexture* DetailNormalArrayAsset =
        LoadObject<UTexture>(nullptr, DetailNormalArrayPath);

    if (
        !IsValid(BaseColorArrayAsset) ||
        !IsValid(NormalArrayAsset) ||
        !IsValid(OrmArrayAsset) ||
        !IsValid(HeightArrayAsset) ||
        !IsValid(MacroColorArrayAsset) ||
        !IsValid(DetailNormalArrayAsset)
    )
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "Build the Cubus density texture arrays before building M_CubusDensityPBR. "
                "BaseColor=%s Normal=%s ORM=%s Height=%s MacroColor=%s DetailNormal=%s"
            ),
            IsValid(BaseColorArrayAsset) ? TEXT("valid") : TEXT("missing"),
            IsValid(NormalArrayAsset) ? TEXT("valid") : TEXT("missing"),
            IsValid(OrmArrayAsset) ? TEXT("valid") : TEXT("missing"),
            IsValid(HeightArrayAsset) ? TEXT("valid") : TEXT("missing"),
            IsValid(MacroColorArrayAsset) ? TEXT("valid") : TEXT("missing"),
            IsValid(DetailNormalArrayAsset) ? TEXT("valid") : TEXT("missing")
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
        AddExpression<UMaterialExpressionWorldPosition>(Material, -1500, -650);

    UMaterialExpressionVertexNormalWS* VertexNormal =
        AddExpression<UMaterialExpressionVertexNormalWS>(Material, -1500, -470);

    UMaterialExpressionTextureCoordinate* MaterialPalette =
        AddExpression<UMaterialExpressionTextureCoordinate>(Material, -1500, -290);
    MaterialPalette->CoordinateIndex = 0;

    UMaterialExpressionVertexColor* MaterialWeights =
        AddExpression<UMaterialExpressionVertexColor>(Material, -1500, -110);

    UMaterialExpressionTextureObjectParameter* BaseColorArray =
        AddTextureArray(
            Material,
            TEXT("DensityBaseColorArray"),
            BaseColorArrayAsset,
            SAMPLERTYPE_Color,
            -1500,
            100
        );

    UMaterialExpressionTextureObjectParameter* NormalArray =
        AddTextureArray(
            Material,
            TEXT("DensityNormalArray"),
            NormalArrayAsset,
            SAMPLERTYPE_Normal,
            -1500,
            280
        );

    UMaterialExpressionTextureObjectParameter* OrmArray =
        AddTextureArray(
            Material,
            TEXT("DensityORMArray"),
            OrmArrayAsset,
            SAMPLERTYPE_LinearColor,
            -1500,
            460
        );










        UMaterialExpressionTextureObjectParameter* HeightArray =
        AddTextureArray(
            Material,
            TEXT("DensityHeightArray"),
            HeightArrayAsset,
            SAMPLERTYPE_LinearColor,
            -1500,
            640
        );

    UMaterialExpressionTextureObjectParameter* MacroColorArray =
        AddTextureArray(
            Material,
            TEXT("DensityMacroColorArray"),
            MacroColorArrayAsset,
            SAMPLERTYPE_Color,
            -1500,
            820
        );

    UMaterialExpressionTextureObjectParameter* DetailNormalArray =
        AddTextureArray(
            Material,
            TEXT("DensityDetailNormalArray"),
            DetailNormalArrayAsset,
            SAMPLERTYPE_Normal,
            -1500,
            1000
        );

    UMaterialExpressionTextureObjectParameter* MaterialDataTable =
        AddTextureArray(
            Material,
            TEXT("DensityMaterialData"),
            nullptr,
            SAMPLERTYPE_LinearColor,
            -1500,
            1180
        );

    UMaterialExpressionScalarParameter* TableWidth =
        AddScalar(Material, TEXT("DensityMaterialTableWidth"), 1.0f, -1500, 1340);

    UMaterialExpressionScalarParameter* PackingBase =
        AddScalar(Material, TEXT("DensityMaterialIdPackingBase"), 32.0f, -1500, 1420);

    UMaterialExpressionScalarParameter* WeatherWetness =
        AddScalar(Material, TEXT("CubusWeatherWetness"), 0.0f, -1500, 1500);

    UMaterialExpressionScalarParameter* WeatherWetDarkening =
        AddScalar(Material, TEXT("CubusWeatherWetDarkening"), 0.65f, -1500, 1580);

    UMaterialExpressionScalarParameter* WeatherWetRoughness =
        AddScalar(Material, TEXT("CubusWeatherWetRoughness"), 0.12f, -1500, 1660);





















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

float heightBlend = max(HeightBlendStrength, 0.0);
if (heightBlend > 0.000001)
{
    float4 heights = 0.0;

    [unroll]
    for (int materialIndex = 0; materialIndex < 4; ++materialIndex)
    {
        float slice = materialIds[materialIndex];

        float heightX = Texture2DArraySample(
            HeightArray,
            HeightArraySampler,
            float3(WorldPosition.yz * scale, slice)
        ).r;
        float heightY = Texture2DArraySample(
            HeightArray,
            HeightArraySampler,
            float3(WorldPosition.xz * scale, slice)
        ).r;
        float heightZ = Texture2DArraySample(
            HeightArray,
            HeightArraySampler,
            float3(WorldPosition.xy * scale, slice)
        ).r;

        heights[materialIndex] =
            heightX * projectionWeights.x +
            heightY * projectionWeights.y +
            heightZ * projectionWeights.z;
    }

    float4 shapedWeights = blendWeights * exp2((heights - 0.5) * heightBlend);
    float shapedSum = dot(shapedWeights, 1.0);

    if (shapedSum > 0.000001)
    {
        blendWeights = shapedWeights / shapedSum;
    }
}
)");

    auto AddSharedInputs = [
        WorldPosition,
        VertexNormal,
        MaterialPalette,
        MaterialWeights,
        WorldScale,
        BlendSharpness,
        HeightBlendStrength,
        HeightArray,
        PackingBase
    ](UMaterialExpressionCustom* Custom)
    {
        AddCustomInput(Custom, TEXT("WorldPosition"), WorldPosition);
        AddCustomInput(Custom, TEXT("VertexNormal"), VertexNormal);
        AddCustomInput(Custom, TEXT("MaterialPalette"), MaterialPalette);
        AddCustomInput(Custom, TEXT("MaterialWeightsRgb"), MaterialWeights, 0);
        AddCustomInput(Custom, TEXT("MaterialWeightA"), MaterialWeights, 4);
        AddCustomInput(Custom, TEXT("WorldScale"), WorldScale);
        AddCustomInput(Custom, TEXT("BlendSharpness"), BlendSharpness);
        AddCustomInput(Custom, TEXT("HeightBlendStrength"), HeightBlendStrength);
        AddCustomInput(Custom, TEXT("HeightArray"), HeightArray);
        AddCustomInput(Custom, TEXT("MaterialIdPackingBase"), PackingBase);
    };

    UMaterialExpressionCustom* BaseColor =
        AddExpression<UMaterialExpressionCustom>(Material, -560, -560);
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

    result += (
        sampleX * projectionWeights.x +
        sampleY * projectionWeights.y +
        sampleZ * projectionWeights.z
    ) * materialWeight;
}

return result;
)");
    AddSharedInputs(BaseColor);
    AddCustomInput(BaseColor, TEXT("BaseColorArray"), BaseColorArray);

    UMaterialExpressionCustom* WorldNormal =
        AddExpression<UMaterialExpressionCustom>(Material, -560, -80);
    WorldNormal->Description = TEXT("Cubus Density Palette Triplanar World Normal");
    WorldNormal->OutputType = CMOT_Float3;
    WorldNormal->Code = FString(SharedCode) + TEXT(R"(
float3 result = 0.0;
float signX = geometryNormal.x < 0.0 ? -1.0 : 1.0;
float signY = geometryNormal.y < 0.0 ? -1.0 : 1.0;
float signZ = geometryNormal.z < 0.0 ? -1.0 : 1.0;

[unroll]
for (int materialIndex = 0; materialIndex < 4; ++materialIndex)
{
    float slice = materialIds[materialIndex];
    float materialWeight = blendWeights[materialIndex];

    float2 encodedX = Texture2DArraySample(
        NormalArray,
        NormalArraySampler,
        float3(WorldPosition.yz * scale, slice)
    ).rg * 2.0 - 1.0;
    float2 encodedY = Texture2DArraySample(
        NormalArray,
        NormalArraySampler,
        float3(WorldPosition.xz * scale, slice)
    ).rg * 2.0 - 1.0;
    float2 encodedZ = Texture2DArraySample(
        NormalArray,
        NormalArraySampler,
        float3(WorldPosition.xy * scale, slice)
    ).rg * 2.0 - 1.0;

    float3 tangentX = float3(
        encodedX,
        sqrt(saturate(1.0 - dot(encodedX, encodedX)))
    );
    float3 tangentY = float3(
        encodedY,
        sqrt(saturate(1.0 - dot(encodedY, encodedY)))
    );
    float3 tangentZ = float3(
        encodedZ,
        sqrt(saturate(1.0 - dot(encodedZ, encodedZ)))
    );

    float3 normalX = float3(
        tangentX.z * signX,
        tangentX.x,
        tangentX.y
    );
    float3 normalY = float3(
        tangentY.x,
        tangentY.z * signY,
        tangentY.y
    );
    float3 normalZ = float3(
        tangentZ.x,
        tangentZ.y,
        tangentZ.z * signZ
    );

    float3 materialNormal = normalize(
        normalX * projectionWeights.x +
        normalY * projectionWeights.y +
        normalZ * projectionWeights.z
    );

    result += materialNormal * materialWeight;
}

return normalize(result);
)");
    AddSharedInputs(WorldNormal);
    AddCustomInput(WorldNormal, TEXT("NormalArray"), NormalArray);

    UMaterialExpressionCustom* Orm =
        AddExpression<UMaterialExpressionCustom>(Material, -560, 400);
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

    result += (
        sampleX * projectionWeights.x +
        sampleY * projectionWeights.y +
        sampleZ * projectionWeights.z
    ) * materialWeight;
}

return result;
)");
    AddSharedInputs(Orm);
    AddCustomInput(Orm, TEXT("OrmArray"), OrmArray);

    UMaterialExpressionComponentMask* AmbientOcclusion =
        AddMask(Material, Orm, true, false, false, -100, 390);
    UMaterialExpressionComponentMask* Roughness =
        AddMask(Material, Orm, false, true, false, -100, 520);
    UMaterialExpressionComponentMask* Metallic =
        AddMask(Material, Orm, false, false, true, -100, 650);

    UMaterialEditorOnlyData* Data = Material->GetEditorOnlyData();
    Connect(Data->BaseColor, BaseColor);
    Connect(Data->Normal, WorldNormal);
    Connect(Data->AmbientOcclusion, AmbientOcclusion);
    Connect(Data->Roughness, Roughness);
    Connect(Data->Metallic, Metallic);

    Save(Material);

    UE_LOG(
        LogTemp,
        Warning,
        TEXT(
            "Built Cubus density material stage 4: decoded UV0 palette and "
            "blended triplanar BaseColor, world-space Normal and ORM."
        )
    );

    return Material;
#else
    return nullptr;
#endif
}
