#include "CubusCore/Editor/CubusMaterialBuilderLibrary.h"

#include "Materials/Material.h"

#if WITH_EDITOR

#include "AssetToolsModule.h"
#include "Factories/MaterialFactoryNew.h"
#include "MaterialEditingLibrary.h"
#include "Materials/MaterialExpressionCameraPositionWS.h"
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

    UMaterialExpressionScalarParameter* Scalar(
        UMaterial* Material,
        const FName Name,
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

    UMaterialExpressionTextureObjectParameter* TextureObject(
        UMaterial* Material,
        const FName Name,
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

    UMaterialExpressionCustom* Custom(
        UMaterial* Material,
        const FString& Description,
        const FString& Code,
        const ECustomMaterialOutputType OutputType,
        const int32 X,
        const int32 Y
    )
    {
        UMaterialExpressionCustom* Node =
            AddExpression<UMaterialExpressionCustom>(Material, X, Y);
        Node->Description = Description;
        Node->Code = Code;
        Node->OutputType = OutputType;
        return Node;
    }

    UMaterialExpressionComponentMask* Mask(
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

    FString CommonKernelCode()
    {
        return TEXT(R"(
float packed01 = floor(PackedMaterialIds.x + 0.5);
float packed23 = floor(PackedMaterialIds.y + 0.5);
float id0 = fmod(packed01, MaterialIdPackingBase);
float id1 = floor(packed01 / MaterialIdPackingBase);
float id2 = fmod(packed23, MaterialIdPackingBase);
float id3 = floor(packed23 / MaterialIdPackingBase);

float alphaWeight = saturate(1.0 - VertexWeights.r - VertexWeights.g - VertexWeights.b);
float4 vertexWeights = max(float4(VertexWeights.rgb, alphaWeight), 0.0.xxxx);
vertexWeights /= max(dot(vertexWeights, 1.0.xxxx), 0.0001);

float2 dataUv0 = float2((id0 + 0.5) / MaterialTableWidth, 0.375);
float2 dataUv1 = float2((id1 + 0.5) / MaterialTableWidth, 0.375);
float2 dataUv2 = float2((id2 + 0.5) / MaterialTableWidth, 0.375);
float2 dataUv3 = float2((id3 + 0.5) / MaterialTableWidth, 0.375);
float4 projection0 = Texture2DSampleLevel(MaterialData, MaterialDataSampler, dataUv0, 0);
float4 projection1 = Texture2DSampleLevel(MaterialData, MaterialDataSampler, dataUv1, 0);
float4 projection2 = Texture2DSampleLevel(MaterialData, MaterialDataSampler, dataUv2, 0);
float4 projection3 = Texture2DSampleLevel(MaterialData, MaterialDataSampler, dataUv3, 0);

float3 axis = max(abs(VertexNormal), 0.0001.xxx);
float3 axes0 = pow(axis, max(0.1, projection0.g)); axes0 /= dot(axes0, 1.0.xxx);
float3 axes1 = pow(axis, max(0.1, projection1.g)); axes1 /= dot(axes1, 1.0.xxx);
float3 axes2 = pow(axis, max(0.1, projection2.g)); axes2 /= dot(axes2, 1.0.xxx);
float3 axes3 = pow(axis, max(0.1, projection3.g)); axes3 /= dot(axes3, 1.0.xxx);

float3 p0 = WorldPosition * projection0.r;
float3 p1 = WorldPosition * projection1.r;
float3 p2 = WorldPosition * projection2.r;
float3 p3 = WorldPosition * projection3.r;

float h0 = Texture2DArraySample(HeightArray, HeightArraySampler, float3(p0.yz, id0)).r * axes0.x + Texture2DArraySample(HeightArray, HeightArraySampler, float3(p0.xz, id0)).r * axes0.y + Texture2DArraySample(HeightArray, HeightArraySampler, float3(p0.xy, id0)).r * axes0.z;
float h1 = Texture2DArraySample(HeightArray, HeightArraySampler, float3(p1.yz, id1)).r * axes1.x + Texture2DArraySample(HeightArray, HeightArraySampler, float3(p1.xz, id1)).r * axes1.y + Texture2DArraySample(HeightArray, HeightArraySampler, float3(p1.xy, id1)).r * axes1.z;
float h2 = Texture2DArraySample(HeightArray, HeightArraySampler, float3(p2.yz, id2)).r * axes2.x + Texture2DArraySample(HeightArray, HeightArraySampler, float3(p2.xz, id2)).r * axes2.y + Texture2DArraySample(HeightArray, HeightArraySampler, float3(p2.xy, id2)).r * axes2.z;
float h3 = Texture2DArraySample(HeightArray, HeightArraySampler, float3(p3.yz, id3)).r * axes3.x + Texture2DArraySample(HeightArray, HeightArraySampler, float3(p3.xz, id3)).r * axes3.y + Texture2DArraySample(HeightArray, HeightArraySampler, float3(p3.xy, id3)).r * axes3.z;

float globalHeightScale = max(GlobalHeightStrength, 0.0) / 0.25;
float4 blendWeights = vertexWeights * exp2(float4(
    (h0 - 0.5) * projection0.b * projection0.a * globalHeightScale,
    (h1 - 0.5) * projection1.b * projection1.a * globalHeightScale,
    (h2 - 0.5) * projection2.b * projection2.a * globalHeightScale,
    (h3 - 0.5) * projection3.b * projection3.a * globalHeightScale
));
blendWeights /= max(dot(blendWeights, 1.0.xxxx), 0.0001);
)");
    }

    void AddCommonInputs(
        UMaterialExpressionCustom* Node,
        UMaterialExpression* WorldPosition,
        UMaterialExpression* VertexNormal,
        UMaterialExpression* CameraPosition,
        UMaterialExpression* PackedMaterialIds,
        UMaterialExpression* VertexWeights,
        UMaterialExpression* HeightArray,
        UMaterialExpression* MaterialData,
        UMaterialExpression* MaterialTableWidth,
        UMaterialExpression* MaterialIdPackingBase,
        UMaterialExpression* GlobalHeightStrength
    )
    {
        AddCustomInput(Node, TEXT("WorldPosition"), WorldPosition);
        AddCustomInput(Node, TEXT("VertexNormal"), VertexNormal);
        AddCustomInput(Node, TEXT("CameraPosition"), CameraPosition);
        AddCustomInput(Node, TEXT("PackedMaterialIds"), PackedMaterialIds);
        AddCustomInput(Node, TEXT("VertexWeights"), VertexWeights);
        AddCustomInput(Node, TEXT("HeightArray"), HeightArray);
        AddCustomInput(Node, TEXT("MaterialData"), MaterialData);
        AddCustomInput(Node, TEXT("MaterialTableWidth"), MaterialTableWidth);
        AddCustomInput(Node, TEXT("MaterialIdPackingBase"), MaterialIdPackingBase);
        AddCustomInput(Node, TEXT("GlobalHeightStrength"), GlobalHeightStrength);
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

    UTexture* BaseColorArrayAsset = LoadObject<UTexture>(nullptr, TEXT("/Game/Cubus/Materials/Arrays/TA_CubusDensityBaseColor.TA_CubusDensityBaseColor"));
    UTexture* NormalArrayAsset = LoadObject<UTexture>(nullptr, TEXT("/Game/Cubus/Materials/Arrays/TA_CubusDensityNormal.TA_CubusDensityNormal"));
    UTexture* OrmArrayAsset = LoadObject<UTexture>(nullptr, TEXT("/Game/Cubus/Materials/Arrays/TA_CubusDensityORM.TA_CubusDensityORM"));
    UTexture* HeightArrayAsset = LoadObject<UTexture>(nullptr, TEXT("/Game/Cubus/Materials/Arrays/TA_CubusDensityHeight.TA_CubusDensityHeight"));
    UTexture* MacroArrayAsset = LoadObject<UTexture>(nullptr, TEXT("/Game/Cubus/Materials/Arrays/TA_CubusDensityMacroColor.TA_CubusDensityMacroColor"));
    UTexture* DetailArrayAsset = LoadObject<UTexture>(nullptr, TEXT("/Game/Cubus/Materials/Arrays/TA_CubusDensityDetailNormal.TA_CubusDensityDetailNormal"));
    UTexture* DefaultData = LoadObject<UTexture>(nullptr, TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture"));

    if (!IsValid(BaseColorArrayAsset) || !IsValid(NormalArrayAsset) ||
        !IsValid(OrmArrayAsset) || !IsValid(HeightArrayAsset) ||
        !IsValid(MacroArrayAsset) || !IsValid(DetailArrayAsset) ||
        !IsValid(DefaultData))
    {
        UE_LOG(LogTemp, Error, TEXT("Build the Cubus density texture arrays before building M_CubusDensityPBR."));
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

    UMaterialExpressionWorldPosition* WorldPosition = AddExpression<UMaterialExpressionWorldPosition>(Material, -3600, -900);
    UMaterialExpressionVertexNormalWS* VertexNormal = AddExpression<UMaterialExpressionVertexNormalWS>(Material, -3600, -700);
    UMaterialExpressionCameraPositionWS* CameraPosition = AddExpression<UMaterialExpressionCameraPositionWS>(Material, -3600, -500);
    UMaterialExpressionTextureCoordinate* PackedIds = AddExpression<UMaterialExpressionTextureCoordinate>(Material, -3600, -300);
    PackedIds->CoordinateIndex = 0;
    UMaterialExpressionVertexColor* VertexWeights = AddExpression<UMaterialExpressionVertexColor>(Material, -3600, -100);

    UMaterialExpressionTextureObjectParameter* BaseColorArray = TextureObject(Material, TEXT("DensityBaseColorArray"), BaseColorArrayAsset, SAMPLERTYPE_Color, -3200, -1100);
    UMaterialExpressionTextureObjectParameter* NormalArray = TextureObject(Material, TEXT("DensityNormalArray"), NormalArrayAsset, SAMPLERTYPE_Normal, -3200, -900);
    UMaterialExpressionTextureObjectParameter* OrmArray = TextureObject(Material, TEXT("DensityORMArray"), OrmArrayAsset, SAMPLERTYPE_LinearColor, -3200, -700);
    UMaterialExpressionTextureObjectParameter* HeightArray = TextureObject(Material, TEXT("DensityHeightArray"), HeightArrayAsset, SAMPLERTYPE_LinearColor, -3200, -500);
    UMaterialExpressionTextureObjectParameter* MacroArray = TextureObject(Material, TEXT("DensityMacroColorArray"), MacroArrayAsset, SAMPLERTYPE_Color, -3200, -300);
    UMaterialExpressionTextureObjectParameter* DetailArray = TextureObject(Material, TEXT("DensityDetailNormalArray"), DetailArrayAsset, SAMPLERTYPE_Normal, -3200, -100);
    UMaterialExpressionTextureObjectParameter* MaterialData = TextureObject(Material, TEXT("DensityMaterialData"), DefaultData, SAMPLERTYPE_LinearColor, -3200, 100);

    UMaterialExpressionScalarParameter* TableWidth = Scalar(Material, TEXT("DensityMaterialTableWidth"), 2.0f, -2900, 250);
    UMaterialExpressionScalarParameter* PackingBase = Scalar(Material, TEXT("DensityMaterialIdPackingBase"), 4096.0f, -2900, 400);
    UMaterialExpressionScalarParameter* WeatherWetness = Scalar(Material, TEXT("CubusWeatherWetness"), 0.0f, -2900, 550);
    UMaterialExpressionScalarParameter* WeatherWetDarkening = Scalar(Material, TEXT("CubusWeatherWetDarkening"), 0.65f, -2900, 700);
    UMaterialExpressionScalarParameter* WeatherWetRoughness = Scalar(Material, TEXT("CubusWeatherWetRoughness"), 0.12f, -2900, 850);

    UMaterialExpressionScalarParameter* MacroWorldSize = Scalar(Material, TEXT("CubusMacroWorldSize"), 2000.0f, -2500, 250);
    UMaterialExpressionScalarParameter* MacroColourStrength = Scalar(Material, TEXT("CubusMacroColourStrength"), 0.22f, -2500, 400);
    UMaterialExpressionScalarParameter* MacroRoughnessStrength = Scalar(Material, TEXT("CubusMacroRoughnessStrength"), 0.12f, -2500, 550);
    UMaterialExpressionScalarParameter* MicroWorldSize = Scalar(Material, TEXT("CubusMicroWorldSize"), 12.5f, -2500, 700);
    UMaterialExpressionScalarParameter* MicroNormalStrength = Scalar(Material, TEXT("CubusMicroNormalStrength"), 0.35f, -2500, 850);
    UMaterialExpressionScalarParameter* MicroHeightStrength = Scalar(Material, TEXT("CubusMicroHeightStrength"), 0.25f, -2500, 1000);
    UMaterialExpressionScalarParameter* MicroRoughnessStrength = Scalar(Material, TEXT("CubusMicroRoughnessStrength"), 0.18f, -2500, 1150);

    const FString Common = CommonKernelCode();

    FString ColorCode = Common + TEXT(R"(
float4 tint0 = Texture2DSampleLevel(MaterialData, MaterialDataSampler, float2((id0 + 0.5) / MaterialTableWidth, 0.125), 0);
float4 tint1 = Texture2DSampleLevel(MaterialData, MaterialDataSampler, float2((id1 + 0.5) / MaterialTableWidth, 0.125), 0);
float4 tint2 = Texture2DSampleLevel(MaterialData, MaterialDataSampler, float2((id2 + 0.5) / MaterialTableWidth, 0.125), 0);
float4 tint3 = Texture2DSampleLevel(MaterialData, MaterialDataSampler, float2((id3 + 0.5) / MaterialTableWidth, 0.125), 0);
float4 detail0 = Texture2DSampleLevel(MaterialData, MaterialDataSampler, float2((id0 + 0.5) / MaterialTableWidth, 0.625), 0);
float4 detail1 = Texture2DSampleLevel(MaterialData, MaterialDataSampler, float2((id1 + 0.5) / MaterialTableWidth, 0.625), 0);
float4 detail2 = Texture2DSampleLevel(MaterialData, MaterialDataSampler, float2((id2 + 0.5) / MaterialTableWidth, 0.625), 0);
float4 detail3 = Texture2DSampleLevel(MaterialData, MaterialDataSampler, float2((id3 + 0.5) / MaterialTableWidth, 0.625), 0);
float macroScale = 1.0 / max(MacroWorldSize, 1.0);
float3 macroPosition = WorldPosition * macroScale;
#define CUBUS_BASE_COLOR(ID,P,AX,TINT) ((Texture2DArraySample(BaseColorArray, BaseColorArraySampler, float3(P.yz, ID)).rgb * AX.x + Texture2DArraySample(BaseColorArray, BaseColorArraySampler, float3(P.xz, ID)).rgb * AX.y + Texture2DArraySample(BaseColorArray, BaseColorArraySampler, float3(P.xy, ID)).rgb * AX.z) * TINT.rgb)
#define CUBUS_MACRO_COLOR(ID,AX) (Texture2DArraySample(MacroArray, MacroArraySampler, float3(macroPosition.yz, ID)).rgb * AX.x + Texture2DArraySample(MacroArray, MacroArraySampler, float3(macroPosition.xz, ID)).rgb * AX.y + Texture2DArraySample(MacroArray, MacroArraySampler, float3(macroPosition.xy, ID)).rgb * AX.z)
float3 c0 = CUBUS_BASE_COLOR(id0, p0, axes0, tint0) * lerp(1.0.xxx, CUBUS_MACRO_COLOR(id0, axes0) * 2.0, saturate(detail0.g + MacroColourStrength));
float3 c1 = CUBUS_BASE_COLOR(id1, p1, axes1, tint1) * lerp(1.0.xxx, CUBUS_MACRO_COLOR(id1, axes1) * 2.0, saturate(detail1.g + MacroColourStrength));
float3 c2 = CUBUS_BASE_COLOR(id2, p2, axes2, tint2) * lerp(1.0.xxx, CUBUS_MACRO_COLOR(id2, axes2) * 2.0, saturate(detail2.g + MacroColourStrength));
float3 c3 = CUBUS_BASE_COLOR(id3, p3, axes3, tint3) * lerp(1.0.xxx, CUBUS_MACRO_COLOR(id3, axes3) * 2.0, saturate(detail3.g + MacroColourStrength));
float3 dryColor = c0 * blendWeights.r + c1 * blendWeights.g + c2 * blendWeights.b + c3 * blendWeights.a;
return dryColor * lerp(1.0, saturate(WeatherWetDarkening), saturate(WeatherWetness));
)");

    UMaterialExpressionCustom* ColorKernel = Custom(Material, TEXT("Cubus Layered Density Color"), ColorCode, CMOT_Float3, -1400, -500);
    AddCommonInputs(ColorKernel, WorldPosition, VertexNormal, CameraPosition, PackedIds, VertexWeights, HeightArray, MaterialData, TableWidth, PackingBase, MicroHeightStrength);
    AddCustomInput(ColorKernel, TEXT("BaseColorArray"), BaseColorArray);
    AddCustomInput(ColorKernel, TEXT("MacroArray"), MacroArray);
    AddCustomInput(ColorKernel, TEXT("MacroWorldSize"), MacroWorldSize);
    AddCustomInput(ColorKernel, TEXT("MacroColourStrength"), MacroColourStrength);
    AddCustomInput(ColorKernel, TEXT("WeatherWetness"), WeatherWetness);
    AddCustomInput(ColorKernel, TEXT("WeatherWetDarkening"), WeatherWetDarkening);

    FString NormalCode = Common + TEXT(R"(
float4 d0 = Texture2DSampleLevel(MaterialData, MaterialDataSampler, float2((id0 + 0.5) / MaterialTableWidth, 0.625), 0);
float4 d1 = Texture2DSampleLevel(MaterialData, MaterialDataSampler, float2((id1 + 0.5) / MaterialTableWidth, 0.625), 0);
float4 d2 = Texture2DSampleLevel(MaterialData, MaterialDataSampler, float2((id2 + 0.5) / MaterialTableWidth, 0.625), 0);
float4 d3 = Texture2DSampleLevel(MaterialData, MaterialDataSampler, float2((id3 + 0.5) / MaterialTableWidth, 0.625), 0);
float sx = VertexNormal.x < 0.0 ? -1.0 : 1.0; float sy = VertexNormal.y < 0.0 ? -1.0 : 1.0; float sz = VertexNormal.z < 0.0 ? -1.0 : 1.0;
#define CUBUS_WORLD_NORMAL(TEX,SAMP,ID,P,AX) normalize(float3((Texture2DArraySample(TEX,SAMP,float3(P.yz,ID)).z*2-1)*sx, Texture2DArraySample(TEX,SAMP,float3(P.yz,ID)).x*2-1, Texture2DArraySample(TEX,SAMP,float3(P.yz,ID)).y*2-1)*AX.x + float3(Texture2DArraySample(TEX,SAMP,float3(P.xz,ID)).x*2-1, (Texture2DArraySample(TEX,SAMP,float3(P.xz,ID)).z*2-1)*sy, Texture2DArraySample(TEX,SAMP,float3(P.xz,ID)).y*2-1)*AX.y + float3(Texture2DArraySample(TEX,SAMP,float3(P.xy,ID)).x*2-1, Texture2DArraySample(TEX,SAMP,float3(P.xy,ID)).y*2-1, (Texture2DArraySample(TEX,SAMP,float3(P.xy,ID)).z*2-1)*sz)*AX.z)
float3 n0 = CUBUS_WORLD_NORMAL(NormalArray, NormalArraySampler, id0, p0, axes0);
float3 n1 = CUBUS_WORLD_NORMAL(NormalArray, NormalArraySampler, id1, p1, axes1);
float3 n2 = CUBUS_WORLD_NORMAL(NormalArray, NormalArraySampler, id2, p2, axes2);
float3 n3 = CUBUS_WORLD_NORMAL(NormalArray, NormalArraySampler, id3, p3, axes3);
float fade = saturate(1.0 - distance(WorldPosition, CameraPosition) / 30000.0);
float microScale = 1.0 / max(MicroWorldSize, 1.0);
float3 q = WorldPosition * microScale;
float3 dn0 = CUBUS_WORLD_NORMAL(DetailArray, DetailArraySampler, id0, q, axes0);
float3 dn1 = CUBUS_WORLD_NORMAL(DetailArray, DetailArraySampler, id1, q, axes1);
float3 dn2 = CUBUS_WORLD_NORMAL(DetailArray, DetailArraySampler, id2, q, axes2);
float3 dn3 = CUBUS_WORLD_NORMAL(DetailArray, DetailArraySampler, id3, q, axes3);
float globalNormalScale = max(MicroNormalStrength, 0.0) / 0.35;
n0 = normalize(lerp(n0, normalize(n0 + dn0 * d0.a * globalNormalScale), fade));
n1 = normalize(lerp(n1, normalize(n1 + dn1 * d1.a * globalNormalScale), fade));
n2 = normalize(lerp(n2, normalize(n2 + dn2 * d2.a * globalNormalScale), fade));
n3 = normalize(lerp(n3, normalize(n3 + dn3 * d3.a * globalNormalScale), fade));
return normalize(n0 * blendWeights.r + n1 * blendWeights.g + n2 * blendWeights.b + n3 * blendWeights.a);
)");

    UMaterialExpressionCustom* NormalKernel = Custom(Material, TEXT("Cubus Layered Density Normal"), NormalCode, CMOT_Float3, -1400, 0);
    AddCommonInputs(NormalKernel, WorldPosition, VertexNormal, CameraPosition, PackedIds, VertexWeights, HeightArray, MaterialData, TableWidth, PackingBase, MicroHeightStrength);
    AddCustomInput(NormalKernel, TEXT("NormalArray"), NormalArray);
    AddCustomInput(NormalKernel, TEXT("DetailArray"), DetailArray);
    AddCustomInput(NormalKernel, TEXT("MicroWorldSize"), MicroWorldSize);
    AddCustomInput(NormalKernel, TEXT("MicroNormalStrength"), MicroNormalStrength);

    FString OrmCode = Common + TEXT(R"(
#define CUBUS_ORM(ID,P,AX) (Texture2DArraySample(OrmArray, OrmArraySampler, float3(P.yz, ID)).rgb * AX.x + Texture2DArraySample(OrmArray, OrmArraySampler, float3(P.xz, ID)).rgb * AX.y + Texture2DArraySample(OrmArray, OrmArraySampler, float3(P.xy, ID)).rgb * AX.z)
float3 o0 = CUBUS_ORM(id0, p0, axes0); float3 o1 = CUBUS_ORM(id1, p1, axes1); float3 o2 = CUBUS_ORM(id2, p2, axes2); float3 o3 = CUBUS_ORM(id3, p3, axes3);
float3 orm = o0 * blendWeights.r + o1 * blendWeights.g + o2 * blendWeights.b + o3 * blendWeights.a;
float macroScale = 1.0 / max(MacroWorldSize, 1.0);
float3 mp = WorldPosition * macroScale;
#define CUBUS_MACRO_LUMA(ID,AX) dot(Texture2DArraySample(MacroArray, MacroArraySampler, float3(mp.yz, ID)).rgb * AX.x + Texture2DArraySample(MacroArray, MacroArraySampler, float3(mp.xz, ID)).rgb * AX.y + Texture2DArraySample(MacroArray, MacroArraySampler, float3(mp.xy, ID)).rgb * AX.z, float3(0.299, 0.587, 0.114))
float macroLuma = CUBUS_MACRO_LUMA(id0, axes0) * blendWeights.r + CUBUS_MACRO_LUMA(id1, axes1) * blendWeights.g + CUBUS_MACRO_LUMA(id2, axes2) * blendWeights.b + CUBUS_MACRO_LUMA(id3, axes3) * blendWeights.a;
float3 microCell = floor(WorldPosition / max(MicroWorldSize, 1.0));
float microNoise = frac(sin(dot(microCell, float3(12.9898, 78.233, 37.719))) * 43758.5453);
orm.g = saturate(orm.g + (macroLuma - 0.5) * 2.0 * MacroRoughnessStrength + (microNoise - 0.5) * 2.0 * MicroRoughnessStrength);
orm.g = lerp(orm.g, saturate(WeatherWetRoughness), saturate(WeatherWetness));
return orm;
)");

    UMaterialExpressionCustom* OrmKernel = Custom(Material, TEXT("Cubus Layered Density ORM"), OrmCode, CMOT_Float3, -1400, 500);
    AddCommonInputs(OrmKernel, WorldPosition, VertexNormal, CameraPosition, PackedIds, VertexWeights, HeightArray, MaterialData, TableWidth, PackingBase, MicroHeightStrength);
    AddCustomInput(OrmKernel, TEXT("OrmArray"), OrmArray);
    AddCustomInput(OrmKernel, TEXT("MacroArray"), MacroArray);
    AddCustomInput(OrmKernel, TEXT("MacroWorldSize"), MacroWorldSize);
    AddCustomInput(OrmKernel, TEXT("MacroRoughnessStrength"), MacroRoughnessStrength);
    AddCustomInput(OrmKernel, TEXT("MicroWorldSize"), MicroWorldSize);
    AddCustomInput(OrmKernel, TEXT("MicroRoughnessStrength"), MicroRoughnessStrength);
    AddCustomInput(OrmKernel, TEXT("WeatherWetness"), WeatherWetness);
    AddCustomInput(OrmKernel, TEXT("WeatherWetRoughness"), WeatherWetRoughness);

    FString EmissiveCode = Common + TEXT(R"(
float4 e0 = Texture2DSampleLevel(MaterialData, MaterialDataSampler, float2((id0 + 0.5) / MaterialTableWidth, 0.875), 0);
float4 e1 = Texture2DSampleLevel(MaterialData, MaterialDataSampler, float2((id1 + 0.5) / MaterialTableWidth, 0.875), 0);
float4 e2 = Texture2DSampleLevel(MaterialData, MaterialDataSampler, float2((id2 + 0.5) / MaterialTableWidth, 0.875), 0);
float4 e3 = Texture2DSampleLevel(MaterialData, MaterialDataSampler, float2((id3 + 0.5) / MaterialTableWidth, 0.875), 0);
return e0.rgb * e0.a * blendWeights.r + e1.rgb * e1.a * blendWeights.g + e2.rgb * e2.a * blendWeights.b + e3.rgb * e3.a * blendWeights.a;
)");

    UMaterialExpressionCustom* EmissiveKernel = Custom(Material, TEXT("Cubus Four-Way Density Emissive"), EmissiveCode, CMOT_Float3, -1400, 900);
    AddCommonInputs(EmissiveKernel, WorldPosition, VertexNormal, CameraPosition, PackedIds, VertexWeights, HeightArray, MaterialData, TableWidth, PackingBase, MicroHeightStrength);

    UMaterialExpressionComponentMask* AmbientOcclusion = Mask(Material, OrmKernel, true, false, false, -900, 420);
    UMaterialExpressionComponentMask* Roughness = Mask(Material, OrmKernel, false, true, false, -900, 540);
    UMaterialExpressionComponentMask* Metallic = Mask(Material, OrmKernel, false, false, true, -900, 660);

    UMaterialEditorOnlyData* Data = Material->GetEditorOnlyData();
    Connect(Data->BaseColor, ColorKernel);
    Connect(Data->Normal, NormalKernel);
    Connect(Data->AmbientOcclusion, AmbientOcclusion);
    Connect(Data->Roughness, Roughness);
    Connect(Data->Metallic, Metallic);
    Connect(Data->EmissiveColor, EmissiveKernel);

    Save(Material);

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Built layered Cubus density material with global macro, height blend and micro detail controls.")
    );

    return Material;
#else
    return nullptr;
#endif
}
