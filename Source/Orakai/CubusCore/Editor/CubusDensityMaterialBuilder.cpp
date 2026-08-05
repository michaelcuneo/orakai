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
    constexpr const TCHAR* PackagePath = TEXT("/Game/Cubus/Materials/M_CubusDensityPBR");
    constexpr const TCHAR* AssetName = TEXT("M_CubusDensityPBR");
    constexpr const TCHAR* BaseColorArrayPath = TEXT("/Game/Cubus/Materials/Arrays/TA_CubusDensityBaseColor.TA_CubusDensityBaseColor");
    constexpr const TCHAR* NormalArrayPath = TEXT("/Game/Cubus/Materials/Arrays/TA_CubusDensityNormal.TA_CubusDensityNormal");
    constexpr const TCHAR* OrmArrayPath = TEXT("/Game/Cubus/Materials/Arrays/TA_CubusDensityORM.TA_CubusDensityORM");
    constexpr const TCHAR* HeightArrayPath = TEXT("/Game/Cubus/Materials/Arrays/TA_CubusDensityHeight.TA_CubusDensityHeight");
    constexpr const TCHAR* MacroColorArrayPath = TEXT("/Game/Cubus/Materials/Arrays/TA_CubusDensityMacroColor.TA_CubusDensityMacroColor");
    constexpr const TCHAR* DetailNormalArrayPath = TEXT("/Game/Cubus/Materials/Arrays/TA_CubusDensityDetailNormal.TA_CubusDensityDetailNormal");
    constexpr const TCHAR* NeutralDataTexturePath = TEXT("/Game/Cubus/Materials/Arrays/T_CubusDensityNeutralBaseColor.T_CubusDensityNeutralBaseColor");

    template <typename TExpression>
    TExpression* AddExpression(UMaterial* Material, int32 X, int32 Y)
    {
        TExpression* Expression = Cast<TExpression>(UMaterialEditingLibrary::CreateMaterialExpression(Material, TExpression::StaticClass(), X, Y));
        check(Expression != nullptr);
        return Expression;
    }

    void Connect(FExpressionInput& Input, UMaterialExpression* Expression, int32 OutputIndex = 0)
    {
        Input.Expression = Expression;
        Input.OutputIndex = OutputIndex;
    }

    void AddCustomInput(UMaterialExpressionCustom* Custom, const TCHAR* Name, UMaterialExpression* Expression, int32 OutputIndex = 0)
    {
        FCustomInput& Input = Custom->Inputs.AddDefaulted_GetRef();
        Input.InputName = FName(Name);
        Connect(Input.Input, Expression, OutputIndex);
    }

    UMaterialExpressionTextureObjectParameter* AddTextureObject(UMaterial* Material, const TCHAR* Name, UTexture* Texture, EMaterialSamplerType SamplerType, int32 X, int32 Y)
    {
        UMaterialExpressionTextureObjectParameter* Node = AddExpression<UMaterialExpressionTextureObjectParameter>(Material, X, Y);
        Node->ParameterName = Name;
        Node->Texture = Texture;
        Node->SamplerType = SamplerType;
        return Node;
    }

    UMaterialExpressionScalarParameter* AddScalar(UMaterial* Material, const TCHAR* Name, float Value, int32 X, int32 Y)
    {
        UMaterialExpressionScalarParameter* Node = AddExpression<UMaterialExpressionScalarParameter>(Material, X, Y);
        Node->ParameterName = Name;
        Node->DefaultValue = Value;
        return Node;
    }

    UMaterialExpressionComponentMask* AddMask(UMaterial* Material, UMaterialExpression* Input, bool bR, bool bG, bool bB, int32 X, int32 Y)
    {
        UMaterialExpressionComponentMask* Node = AddExpression<UMaterialExpressionComponentMask>(Material, X, Y);
        Node->R = bR;
        Node->G = bG;
        Node->B = bB;
        Node->A = false;
        Connect(Node->Input, Input);
        return Node;
    }

    UMaterial* FindOrCreateMaterial()
    {
        if (UMaterial* Existing = LoadObject<UMaterial>(nullptr, PackagePath)) return Existing;
        UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
        return Cast<UMaterial>(FAssetToolsModule::GetModule().Get().CreateAsset(AssetName, TEXT("/Game/Cubus/Materials"), UMaterial::StaticClass(), Factory));
    }

    void Save(UMaterial* Material)
    {
        UMaterialEditingLibrary::RecompileMaterial(Material);
        Material->PostEditChange();
        Material->MarkPackageDirty();
        UPackage* Package = Material->GetOutermost();
        const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
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
    if (!IsValid(Material)) return nullptr;

    UTexture* BaseColorAsset = LoadObject<UTexture>(nullptr, BaseColorArrayPath);
    UTexture* NormalAsset = LoadObject<UTexture>(nullptr, NormalArrayPath);
    UTexture* OrmAsset = LoadObject<UTexture>(nullptr, OrmArrayPath);
    UTexture* HeightAsset = LoadObject<UTexture>(nullptr, HeightArrayPath);
    UTexture* MacroAsset = LoadObject<UTexture>(nullptr, MacroColorArrayPath);
    UTexture* DetailNormalAsset = LoadObject<UTexture>(nullptr, DetailNormalArrayPath);
    UTexture* NeutralDataAsset = LoadObject<UTexture>(nullptr, NeutralDataTexturePath);

    if (!IsValid(BaseColorAsset) || !IsValid(NormalAsset) || !IsValid(OrmAsset) || !IsValid(HeightAsset) || !IsValid(MacroAsset) || !IsValid(DetailNormalAsset) || !IsValid(NeutralDataAsset))
    {
        UE_LOG(LogTemp, Error, TEXT("Build all Cubus density texture assets before building M_CubusDensityPBR."));
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

    UMaterialExpressionWorldPosition* WorldPosition = AddExpression<UMaterialExpressionWorldPosition>(Material, -2100, -760);
    UMaterialExpressionCameraPositionWS* CameraPosition = AddExpression<UMaterialExpressionCameraPositionWS>(Material, -2100, -600);
    UMaterialExpressionVertexNormalWS* VertexNormal = AddExpression<UMaterialExpressionVertexNormalWS>(Material, -2100, -440);
    UMaterialExpressionTextureCoordinate* Palette = AddExpression<UMaterialExpressionTextureCoordinate>(Material, -2100, -280);
    Palette->CoordinateIndex = 0;
    UMaterialExpressionVertexColor* Weights = AddExpression<UMaterialExpressionVertexColor>(Material, -2100, -120);

    UMaterialExpressionTextureObjectParameter* BaseColorArray = AddTextureObject(Material, TEXT("DensityBaseColorArray"), BaseColorAsset, SAMPLERTYPE_Color, -2100, 100);
    UMaterialExpressionTextureObjectParameter* NormalArray = AddTextureObject(Material, TEXT("DensityNormalArray"), NormalAsset, SAMPLERTYPE_Normal, -2100, 260);
    UMaterialExpressionTextureObjectParameter* OrmArray = AddTextureObject(Material, TEXT("DensityORMArray"), OrmAsset, SAMPLERTYPE_LinearColor, -2100, 420);
    UMaterialExpressionTextureObjectParameter* HeightArray = AddTextureObject(Material, TEXT("DensityHeightArray"), HeightAsset, SAMPLERTYPE_LinearColor, -2100, 580);
    UMaterialExpressionTextureObjectParameter* MacroArray = AddTextureObject(Material, TEXT("DensityMacroColorArray"), MacroAsset, SAMPLERTYPE_Color, -2100, 740);
    UMaterialExpressionTextureObjectParameter* DetailArray = AddTextureObject(Material, TEXT("DensityDetailNormalArray"), DetailNormalAsset, SAMPLERTYPE_Normal, -2100, 900);
    UMaterialExpressionTextureObjectParameter* MaterialData = AddTextureObject(Material, TEXT("DensityMaterialData"), NeutralDataAsset, SAMPLERTYPE_LinearColor, -2100, 1060);

    UMaterialExpressionScalarParameter* TableWidth = AddScalar(Material, TEXT("DensityMaterialTableWidth"), 1.0f, -2100, 1260);
    UMaterialExpressionScalarParameter* PackingBase = AddScalar(Material, TEXT("DensityMaterialIdPackingBase"), 32.0f, -2100, 1340);
    UMaterialExpressionScalarParameter* NearScale = AddScalar(Material, TEXT("CubusNearTextureScaleMultiplier"), 1.75f, -2100, 1420);
    UMaterialExpressionScalarParameter* FarScale = AddScalar(Material, TEXT("CubusFarTextureScaleMultiplier"), 0.14f, -2100, 1500);
    UMaterialExpressionScalarParameter* DistanceStart = AddScalar(Material, TEXT("CubusDistanceBlendStart"), 3000.0f, -2100, 1580);
    UMaterialExpressionScalarParameter* DistanceEnd = AddScalar(Material, TEXT("CubusDistanceBlendEnd"), 30000.0f, -2100, 1660);
    UMaterialExpressionScalarParameter* DetailStart = AddScalar(Material, TEXT("CubusDetailFadeStart"), 1800.0f, -2100, 1740);
    UMaterialExpressionScalarParameter* DetailEnd = AddScalar(Material, TEXT("CubusDetailFadeEnd"), 12000.0f, -2100, 1820);
    UMaterialExpressionScalarParameter* Wetness = AddScalar(Material, TEXT("CubusWeatherWetness"), 0.0f, -2100, 1900);
    UMaterialExpressionScalarParameter* WetDarkening = AddScalar(Material, TEXT("CubusWeatherWetDarkening"), 0.65f, -2100, 1980);
    UMaterialExpressionScalarParameter* WetRoughness = AddScalar(Material, TEXT("CubusWeatherWetRoughness"), 0.12f, -2100, 2060);

    const TCHAR* SharedCode = TEXT(R"(
float packingBase=max(MaterialIdPackingBase,2.0);
float tableWidth=max(MaterialTableWidth,1.0);
float packed01=floor(MaterialPalette.x+0.5);
float packed23=floor(MaterialPalette.y+0.5);
float4 materialIds=max(float4(fmod(packed01,packingBase),floor(packed01/packingBase),fmod(packed23,packingBase),floor(packed23/packingBase)),1.0);
float4 blendWeights=saturate(float4(MaterialWeightsRgb,MaterialWeightA));
float weightSum=dot(blendWeights,1.0);
blendWeights=weightSum>0.000001?blendWeights/weightSum:float4(1,0,0,0);
float4 tint[4]; float4 projectionData[4]; float4 detailData[4]; float4 emissiveData[4];
[unroll] for(int i=0;i<4;++i){float u=(materialIds[i]+0.5)/tableWidth;tint[i]=Texture2DSampleLevel(MaterialData,MaterialDataSampler,float2(u,0.125),0);projectionData[i]=Texture2DSampleLevel(MaterialData,MaterialDataSampler,float2(u,0.375),0);detailData[i]=Texture2DSampleLevel(MaterialData,MaterialDataSampler,float2(u,0.625),0);emissiveData[i]=Texture2DSampleLevel(MaterialData,MaterialDataSampler,float2(u,0.875),0);}
float sharpness=0.0;[unroll] for(int i=0;i<4;++i)sharpness+=max(projectionData[i].y,0.1)*blendWeights[i];
float3 geometryNormal=normalize(VertexNormal);
float3 projectionWeights=pow(abs(geometryNormal),max(sharpness,0.1));projectionWeights/=max(dot(projectionWeights,1.0),0.000001);
float4 contrasted=0.0;[unroll] for(int i=0;i<4;++i)contrasted[i]=pow(max(blendWeights[i],0.000001),max(projectionData[i].w,0.01));
float contrastedSum=dot(contrasted,1.0);if(contrastedSum>0.000001)blendWeights=contrasted/contrastedSum;
float4 heights=0.0;[unroll] for(int i=0;i<4;++i){float s=materialIds[i];float scale=max(projectionData[i].x,0.000001);heights[i]=Texture2DArraySample(HeightArray,HeightArraySampler,float3(WorldPosition.yz*scale,s)).r*projectionWeights.x+Texture2DArraySample(HeightArray,HeightArraySampler,float3(WorldPosition.xz*scale,s)).r*projectionWeights.y+Texture2DArraySample(HeightArray,HeightArraySampler,float3(WorldPosition.xy*scale,s)).r*projectionWeights.z;}
float4 shaped=0.0;[unroll] for(int i=0;i<4;++i)shaped[i]=blendWeights[i]*exp2((heights[i]-0.5)*max(projectionData[i].z,0.0));
float shapedSum=dot(shaped,1.0);if(shapedSum>0.000001)blendWeights=shaped/shapedSum;
float cameraDistance=length(WorldPosition-CameraPosition);
float distanceBlend=smoothstep(max(DistanceBlendStart,0.0),max(DistanceBlendEnd,DistanceBlendStart+1.0),cameraDistance);
float detailVisibility=1.0-smoothstep(max(DetailFadeStart,0.0),max(DetailFadeEnd,DetailFadeStart+1.0),cameraDistance);
)");

    auto AddSharedInputs = [&](UMaterialExpressionCustom* Custom)
    {
        AddCustomInput(Custom,TEXT("WorldPosition"),WorldPosition); AddCustomInput(Custom,TEXT("CameraPosition"),CameraPosition);
        AddCustomInput(Custom,TEXT("VertexNormal"),VertexNormal); AddCustomInput(Custom,TEXT("MaterialPalette"),Palette);
        AddCustomInput(Custom,TEXT("MaterialWeightsRgb"),Weights,0); AddCustomInput(Custom,TEXT("MaterialWeightA"),Weights,4);
        AddCustomInput(Custom,TEXT("HeightArray"),HeightArray); AddCustomInput(Custom,TEXT("MaterialData"),MaterialData);
        AddCustomInput(Custom,TEXT("MaterialTableWidth"),TableWidth); AddCustomInput(Custom,TEXT("MaterialIdPackingBase"),PackingBase);
        AddCustomInput(Custom,TEXT("DistanceBlendStart"),DistanceStart); AddCustomInput(Custom,TEXT("DistanceBlendEnd"),DistanceEnd);
        AddCustomInput(Custom,TEXT("DetailFadeStart"),DetailStart); AddCustomInput(Custom,TEXT("DetailFadeEnd"),DetailEnd);
    };

    UMaterialExpressionCustom* BaseColor = AddExpression<UMaterialExpressionCustom>(Material,-760,-680);
    BaseColor->Description=TEXT("Cubus near/far anti-tiling base colour"); BaseColor->OutputType=CMOT_Float3;
    BaseColor->Code=FString(SharedCode)+TEXT(R"(
float3 result=0.0;
[unroll] for(int i=0;i<4;++i){float s=materialIds[i];float nearScale=max(projectionData[i].x*NearScaleMultiplier,0.000001);float farScale=max(projectionData[i].x*FarScaleMultiplier,0.000001);float macroScale=max(detailData[i].x,0.000001);float3 offset=float3(s*131.0+317.0,s*233.0+509.0,s*353.0+733.0);
float3 nearSample=Texture2DArraySample(BaseColorArray,BaseColorArraySampler,float3(WorldPosition.yz*nearScale,s)).rgb*projectionWeights.x+Texture2DArraySample(BaseColorArray,BaseColorArraySampler,float3(WorldPosition.xz*nearScale,s)).rgb*projectionWeights.y+Texture2DArraySample(BaseColorArray,BaseColorArraySampler,float3(WorldPosition.xy*nearScale,s)).rgb*projectionWeights.z;
float3 farSample=Texture2DArraySample(BaseColorArray,BaseColorArraySampler,float3((WorldPosition.yz+offset.yz)*farScale,s)).rgb*projectionWeights.x+Texture2DArraySample(BaseColorArray,BaseColorArraySampler,float3((WorldPosition.xz+offset.xz)*farScale,s)).rgb*projectionWeights.y+Texture2DArraySample(BaseColorArray,BaseColorArraySampler,float3((WorldPosition.xy+offset.xy)*farScale,s)).rgb*projectionWeights.z;
float3 macro=Texture2DArraySample(MacroColorArray,MacroColorArraySampler,float3(WorldPosition.yz*macroScale,s)).rgb*projectionWeights.x+Texture2DArraySample(MacroColorArray,MacroColorArraySampler,float3(WorldPosition.xz*macroScale,s)).rgb*projectionWeights.y+Texture2DArraySample(MacroColorArray,MacroColorArraySampler,float3(WorldPosition.xy*macroScale,s)).rgb*projectionWeights.z;
result+=lerp(nearSample,farSample,distanceBlend)*tint[i].rgb*lerp(1.0,macro*2.0,saturate(detailData[i].y))*blendWeights[i];}
float wet=saturate(WeatherWetness)*saturate(WeatherWetDarkening);return result*lerp(1.0,0.55,wet);
)");
    AddSharedInputs(BaseColor); AddCustomInput(BaseColor,TEXT("BaseColorArray"),BaseColorArray); AddCustomInput(BaseColor,TEXT("MacroColorArray"),MacroArray);
    AddCustomInput(BaseColor,TEXT("NearScaleMultiplier"),NearScale); AddCustomInput(BaseColor,TEXT("FarScaleMultiplier"),FarScale);
    AddCustomInput(BaseColor,TEXT("WeatherWetness"),Wetness); AddCustomInput(BaseColor,TEXT("WeatherWetDarkening"),WetDarkening);

    UMaterialExpressionCustom* WorldNormal = AddExpression<UMaterialExpressionCustom>(Material,-760,-40);
    WorldNormal->Description=TEXT("Cubus distance-scaled world normal"); WorldNormal->OutputType=CMOT_Float3;
    WorldNormal->Code=FString(SharedCode)+TEXT(R"(
float3 result=0.0;float3 signs=float3(geometryNormal.x<0?-1:1,geometryNormal.y<0?-1:1,geometryNormal.z<0?-1:1);
[unroll] for(int i=0;i<4;++i){float s=materialIds[i];float nearScale=max(projectionData[i].x*NearScaleMultiplier,0.000001);float farScale=max(projectionData[i].x*FarScaleMultiplier,0.000001);float detailScale=max(detailData[i].z,0.000001);float strength=saturate(detailData[i].w*detailVisibility);float3 o=float3(s*131.0+317.0,s*233.0+509.0,s*353.0+733.0);
float2 nx=lerp(Texture2DArraySample(NormalArray,NormalArraySampler,float3(WorldPosition.yz*nearScale,s)).rg,Texture2DArraySample(NormalArray,NormalArraySampler,float3((WorldPosition.yz+o.yz)*farScale,s)).rg,distanceBlend)*2-1;
float2 ny=lerp(Texture2DArraySample(NormalArray,NormalArraySampler,float3(WorldPosition.xz*nearScale,s)).rg,Texture2DArraySample(NormalArray,NormalArraySampler,float3((WorldPosition.xz+o.xz)*farScale,s)).rg,distanceBlend)*2-1;
float2 nz=lerp(Texture2DArraySample(NormalArray,NormalArraySampler,float3(WorldPosition.xy*nearScale,s)).rg,Texture2DArraySample(NormalArray,NormalArraySampler,float3((WorldPosition.xy+o.xy)*farScale,s)).rg,distanceBlend)*2-1;
float2 dx=Texture2DArraySample(DetailNormalArray,DetailNormalArraySampler,float3(WorldPosition.yz*detailScale,s)).rg*2-1;float2 dy=Texture2DArraySample(DetailNormalArray,DetailNormalArraySampler,float3(WorldPosition.xz*detailScale,s)).rg*2-1;float2 dz=Texture2DArraySample(DetailNormalArray,DetailNormalArraySampler,float3(WorldPosition.xy*detailScale,s)).rg*2-1;
float3 bx=float3(sqrt(saturate(1-dot(nx,nx)))*signs.x,nx.x,nx.y);float3 by=float3(ny.x,sqrt(saturate(1-dot(ny,ny)))*signs.y,ny.y);float3 bz=float3(nz.x,nz.y,sqrt(saturate(1-dot(nz,nz)))*signs.z);
float3 tx=float3(sqrt(saturate(1-dot(dx,dx)))*signs.x,dx.x,dx.y);float3 ty=float3(dy.x,sqrt(saturate(1-dot(dy,dy)))*signs.y,dy.y);float3 tz=float3(dz.x,dz.y,sqrt(saturate(1-dot(dz,dz)))*signs.z);
float3 baseN=normalize(bx*projectionWeights.x+by*projectionWeights.y+bz*projectionWeights.z);float3 detailN=normalize(tx*projectionWeights.x+ty*projectionWeights.y+tz*projectionWeights.z);result+=normalize(lerp(baseN,normalize(baseN+detailN-geometryNormal),strength))*blendWeights[i];}
return normalize(result);
)");
    AddSharedInputs(WorldNormal); AddCustomInput(WorldNormal,TEXT("NormalArray"),NormalArray); AddCustomInput(WorldNormal,TEXT("DetailNormalArray"),DetailArray);
    AddCustomInput(WorldNormal,TEXT("NearScaleMultiplier"),NearScale); AddCustomInput(WorldNormal,TEXT("FarScaleMultiplier"),FarScale);

    UMaterialExpressionCustom* Orm = AddExpression<UMaterialExpressionCustom>(Material,-760,600);
    Orm->Description=TEXT("Cubus blended ORM and wet roughness"); Orm->OutputType=CMOT_Float3;
    Orm->Code=FString(SharedCode)+TEXT(R"(
float3 result=0.0;[unroll] for(int i=0;i<4;++i){float s=materialIds[i];float scale=max(projectionData[i].x,0.000001);float3 v=Texture2DArraySample(OrmArray,OrmArraySampler,float3(WorldPosition.yz*scale,s)).rgb*projectionWeights.x+Texture2DArraySample(OrmArray,OrmArraySampler,float3(WorldPosition.xz*scale,s)).rgb*projectionWeights.y+Texture2DArraySample(OrmArray,OrmArraySampler,float3(WorldPosition.xy*scale,s)).rgb*projectionWeights.z;result+=v*blendWeights[i];}result.g=lerp(result.g,saturate(WeatherWetRoughness),saturate(WeatherWetness));return result;
)");
    AddSharedInputs(Orm); AddCustomInput(Orm,TEXT("OrmArray"),OrmArray); AddCustomInput(Orm,TEXT("WeatherWetness"),Wetness); AddCustomInput(Orm,TEXT("WeatherWetRoughness"),WetRoughness);

    UMaterialExpressionCustom* Emissive = AddExpression<UMaterialExpressionCustom>(Material,-760,1040);
    Emissive->Description=TEXT("Cubus per-material emissive"); Emissive->OutputType=CMOT_Float3;
    Emissive->Code=FString(SharedCode)+TEXT(R"(
float3 result=0.0;[unroll] for(int i=0;i<4;++i)result+=emissiveData[i].rgb*max(emissiveData[i].a,0.0)*blendWeights[i];return result;
)");
    AddSharedInputs(Emissive);

    UMaterialExpressionComponentMask* AmbientOcclusion=AddMask(Material,Orm,true,false,false,-180,580);
    UMaterialExpressionComponentMask* Roughness=AddMask(Material,Orm,false,true,false,-180,700);
    UMaterialExpressionComponentMask* Metallic=AddMask(Material,Orm,false,false,true,-180,820);

    UMaterialEditorOnlyData* Data=Material->GetEditorOnlyData();
    Connect(Data->BaseColor,BaseColor); Connect(Data->Normal,WorldNormal); Connect(Data->AmbientOcclusion,AmbientOcclusion);
    Connect(Data->Roughness,Roughness); Connect(Data->Metallic,Metallic); Connect(Data->EmissiveColor,Emissive);
    Save(Material);

    UE_LOG(LogTemp,Display,TEXT("Built Cubus density terrain material stage 7: near/far texture scaling and distance-faded micro detail."));
    return Material;
#else
    return nullptr;
#endif
}
