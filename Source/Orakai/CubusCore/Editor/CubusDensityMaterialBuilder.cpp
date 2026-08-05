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

    void Connect(FExpressionInput& Input, UMaterialExpression* Expression, int32 OutputIndex = 0)
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

    UMaterialExpressionTextureObjectParameter* AddTextureObject(
        UMaterial* Material,
        const TCHAR* Name,
        UTexture* Texture,
        EMaterialSamplerType SamplerType,
        int32 X,
        int32 Y
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

    UMaterialExpressionWorldPosition* WorldPosition = AddExpression<UMaterialExpressionWorldPosition>(Material, -2200, -780);
    UMaterialExpressionCameraPositionWS* CameraPosition = AddExpression<UMaterialExpressionCameraPositionWS>(Material, -2200, -620);
    UMaterialExpressionVertexNormalWS* VertexNormal = AddExpression<UMaterialExpressionVertexNormalWS>(Material, -2200, -460);
    UMaterialExpressionTextureCoordinate* Palette = AddExpression<UMaterialExpressionTextureCoordinate>(Material, -2200, -300);
    Palette->CoordinateIndex = 0;
    UMaterialExpressionVertexColor* Weights = AddExpression<UMaterialExpressionVertexColor>(Material, -2200, -140);

    UMaterialExpressionTextureObjectParameter* BaseColorArray = AddTextureObject(Material, TEXT("DensityBaseColorArray"), BaseColorAsset, SAMPLERTYPE_Color, -2200, 100);
    UMaterialExpressionTextureObjectParameter* NormalArray = AddTextureObject(Material, TEXT("DensityNormalArray"), NormalAsset, SAMPLERTYPE_Normal, -2200, 260);
    UMaterialExpressionTextureObjectParameter* OrmArray = AddTextureObject(Material, TEXT("DensityORMArray"), OrmAsset, SAMPLERTYPE_LinearColor, -2200, 420);
    UMaterialExpressionTextureObjectParameter* HeightArray = AddTextureObject(Material, TEXT("DensityHeightArray"), HeightAsset, SAMPLERTYPE_LinearColor, -2200, 580);
    UMaterialExpressionTextureObjectParameter* MacroArray = AddTextureObject(Material, TEXT("DensityMacroColorArray"), MacroAsset, SAMPLERTYPE_Color, -2200, 740);
    UMaterialExpressionTextureObjectParameter* DetailArray = AddTextureObject(Material, TEXT("DensityDetailNormalArray"), DetailNormalAsset, SAMPLERTYPE_Normal, -2200, 900);
    UMaterialExpressionTextureObjectParameter* MaterialData = AddTextureObject(Material, TEXT("DensityMaterialData"), NeutralDataAsset, SAMPLERTYPE_LinearColor, -2200, 1060);

    UMaterialExpressionScalarParameter* TableWidth = AddScalar(Material, TEXT("DensityMaterialTableWidth"), 1.0f, -2200, 1260);
    UMaterialExpressionScalarParameter* PackingBase = AddScalar(Material, TEXT("DensityMaterialIdPackingBase"), 32.0f, -2200, 1340);
    UMaterialExpressionScalarParameter* NearScale = AddScalar(Material, TEXT("CubusNearTextureScaleMultiplier"), 3.0f, -2200, 1420);
    UMaterialExpressionScalarParameter* MidScale = AddScalar(Material, TEXT("CubusMidTextureScaleMultiplier"), 0.55f, -2200, 1500);
    UMaterialExpressionScalarParameter* FarScale = AddScalar(Material, TEXT("CubusFarTextureScaleMultiplier"), 0.08f, -2200, 1580);
    UMaterialExpressionScalarParameter* NearBlendStart = AddScalar(Material, TEXT("CubusNearBlendStart"), 800.0f, -2200, 1660);
    UMaterialExpressionScalarParameter* NearBlendEnd = AddScalar(Material, TEXT("CubusNearBlendEnd"), 3000.0f, -2200, 1740);
    UMaterialExpressionScalarParameter* FarBlendStart = AddScalar(Material, TEXT("CubusFarBlendStart"), 8000.0f, -2200, 1820);
    UMaterialExpressionScalarParameter* FarBlendEnd = AddScalar(Material, TEXT("CubusFarBlendEnd"), 40000.0f, -2200, 1900);
    UMaterialExpressionScalarParameter* DetailFadeStart = AddScalar(Material, TEXT("CubusDetailFadeStart"), 1200.0f, -2200, 1980);
    UMaterialExpressionScalarParameter* DetailFadeEnd = AddScalar(Material, TEXT("CubusDetailFadeEnd"), 6000.0f, -2200, 2060);
    UMaterialExpressionScalarParameter* Wetness = AddScalar(Material, TEXT("CubusWeatherWetness"), 0.0f, -2200, 2140);
    UMaterialExpressionScalarParameter* WetDarkening = AddScalar(Material, TEXT("CubusWeatherWetDarkening"), 0.65f, -2200, 2220);
    UMaterialExpressionScalarParameter* WetRoughness = AddScalar(Material, TEXT("CubusWeatherWetRoughness"), 0.12f, -2200, 2300);

    const TCHAR* SharedCode = TEXT(R"(
float packingBase=max(MaterialIdPackingBase,2.0);
float tableWidth=max(MaterialTableWidth,1.0);
float packed01=floor(MaterialPalette.x+0.5);
float packed23=floor(MaterialPalette.y+0.5);
float4 materialIds=max(float4(fmod(packed01,packingBase),floor(packed01/packingBase),fmod(packed23,packingBase),floor(packed23/packingBase)),1.0);
float4 blendWeights=saturate(float4(MaterialWeightsRgb,MaterialWeightA));
float weightSum=dot(blendWeights,1.0);
blendWeights=weightSum>0.000001?blendWeights/weightSum:float4(1,0,0,0);
float4 tint[4];float4 projectionData[4];float4 detailData[4];float4 emissiveData[4];
[unroll]for(int i=0;i<4;++i){float u=(materialIds[i]+0.5)/tableWidth;tint[i]=Texture2DSampleLevel(MaterialData,MaterialDataSampler,float2(u,0.125),0);projectionData[i]=Texture2DSampleLevel(MaterialData,MaterialDataSampler,float2(u,0.375),0);detailData[i]=Texture2DSampleLevel(MaterialData,MaterialDataSampler,float2(u,0.625),0);emissiveData[i]=Texture2DSampleLevel(MaterialData,MaterialDataSampler,float2(u,0.875),0);}
float sharpness=0.0;[unroll]for(int i=0;i<4;++i)sharpness+=max(projectionData[i].y,0.1)*blendWeights[i];
float3 geometryNormal=normalize(VertexNormal);
float3 projectionWeights=pow(abs(geometryNormal),max(sharpness,0.1));projectionWeights/=max(dot(projectionWeights,1.0),0.000001);
float4 contrasted=0.0;[unroll]for(int i=0;i<4;++i)contrasted[i]=pow(max(blendWeights[i],0.000001),max(projectionData[i].w,0.01));
float contrastedSum=dot(contrasted,1.0);if(contrastedSum>0.000001)blendWeights=contrasted/contrastedSum;
float4 heights=0.0;[unroll]for(int i=0;i<4;++i){float s=materialIds[i];float scale=max(projectionData[i].x,0.000001);heights[i]=Texture2DArraySample(HeightArray,HeightArraySampler,float3(WorldPosition.yz*scale,s)).r*projectionWeights.x+Texture2DArraySample(HeightArray,HeightArraySampler,float3(WorldPosition.xz*scale,s)).r*projectionWeights.y+Texture2DArraySample(HeightArray,HeightArraySampler,float3(WorldPosition.xy*scale,s)).r*projectionWeights.z;}
float4 shaped=0.0;[unroll]for(int i=0;i<4;++i)shaped[i]=blendWeights[i]*exp2((heights[i]-0.5)*max(projectionData[i].z,0.0));
float shapedSum=dot(shaped,1.0);if(shapedSum>0.000001)blendWeights=shaped/shapedSum;
float cameraDistance=length(WorldPosition-CameraPosition);
float nearFade=smoothstep(max(NearBlendStart,0.0),max(NearBlendEnd,NearBlendStart+1.0),cameraDistance);
float farFade=smoothstep(max(FarBlendStart,NearBlendEnd+1.0),max(FarBlendEnd,FarBlendStart+1.0),cameraDistance);
float3 bandWeights=float3(1.0-nearFade,nearFade*(1.0-farFade),farFade);
bandWeights/=max(dot(bandWeights,1.0),0.000001);
float detailVisibility=1.0-smoothstep(max(DetailFadeStart,0.0),max(DetailFadeEnd,DetailFadeStart+1.0),cameraDistance);
)");

    auto AddSharedInputs = [&](UMaterialExpressionCustom* Custom)
    {
        AddCustomInput(Custom, TEXT("WorldPosition"), WorldPosition);
        AddCustomInput(Custom, TEXT("CameraPosition"), CameraPosition);
        AddCustomInput(Custom, TEXT("VertexNormal"), VertexNormal);
        AddCustomInput(Custom, TEXT("MaterialPalette"), Palette);
        AddCustomInput(Custom, TEXT("MaterialWeightsRgb"), Weights, 0);
        AddCustomInput(Custom, TEXT("MaterialWeightA"), Weights, 4);
        AddCustomInput(Custom, TEXT("HeightArray"), HeightArray);
        AddCustomInput(Custom, TEXT("MaterialData"), MaterialData);
        AddCustomInput(Custom, TEXT("MaterialTableWidth"), TableWidth);
        AddCustomInput(Custom, TEXT("MaterialIdPackingBase"), PackingBase);
        AddCustomInput(Custom, TEXT("NearBlendStart"), NearBlendStart);
        AddCustomInput(Custom, TEXT("NearBlendEnd"), NearBlendEnd);
        AddCustomInput(Custom, TEXT("FarBlendStart"), FarBlendStart);
        AddCustomInput(Custom, TEXT("FarBlendEnd"), FarBlendEnd);
        AddCustomInput(Custom, TEXT("DetailFadeStart"), DetailFadeStart);
        AddCustomInput(Custom, TEXT("DetailFadeEnd"), DetailFadeEnd);
    };

    UMaterialExpressionCustom* BaseColor = AddExpression<UMaterialExpressionCustom>(Material, -760, -700);
    BaseColor->Description = TEXT("Cubus three-band rotated anti-tiling base colour");
    BaseColor->OutputType = CMOT_Float3;
    BaseColor->Code = FString(SharedCode) + TEXT(R"(
float3 result=0.0;const float cm=0.8386706;const float sm=0.5446390;const float cf=0.7547096;const float sf=-0.6560590;
[unroll]for(int i=0;i<4;++i){float s=materialIds[i];float ns=max(projectionData[i].x*NearScaleMultiplier,0.000001);float ms=max(projectionData[i].x*MidScaleMultiplier,0.000001);float fs=max(projectionData[i].x*FarScaleMultiplier,0.000001);float macroScale=max(detailData[i].x,0.000001);float2 oy=float2(s*0.173+0.31,s*0.271+0.57);float2 oz=float2(s*0.197+0.43,s*0.313+0.69);
float2 myz=float2(WorldPosition.y*cm-WorldPosition.z*sm,WorldPosition.y*sm+WorldPosition.z*cm)*ms+oy;float2 mxz=float2(WorldPosition.x*cm-WorldPosition.z*sm,WorldPosition.x*sm+WorldPosition.z*cm)*ms+oz;float2 mxy=float2(WorldPosition.x*cm-WorldPosition.y*sm,WorldPosition.x*sm+WorldPosition.y*cm)*ms+oy;
float2 fyz=float2(WorldPosition.y*cf-WorldPosition.z*sf,WorldPosition.y*sf+WorldPosition.z*cf)*fs+oz;float2 fxz=float2(WorldPosition.x*cf-WorldPosition.z*sf,WorldPosition.x*sf+WorldPosition.z*cf)*fs+oy;float2 fxy=float2(WorldPosition.x*cf-WorldPosition.y*sf,WorldPosition.x*sf+WorldPosition.y*cf)*fs+oz;
float3 n=Texture2DArraySample(BaseColorArray,BaseColorArraySampler,float3(WorldPosition.yz*ns,s)).rgb*projectionWeights.x+Texture2DArraySample(BaseColorArray,BaseColorArraySampler,float3(WorldPosition.xz*ns,s)).rgb*projectionWeights.y+Texture2DArraySample(BaseColorArray,BaseColorArraySampler,float3(WorldPosition.xy*ns,s)).rgb*projectionWeights.z;
float3 m=Texture2DArraySample(BaseColorArray,BaseColorArraySampler,float3(myz,s)).rgb*projectionWeights.x+Texture2DArraySample(BaseColorArray,BaseColorArraySampler,float3(mxz,s)).rgb*projectionWeights.y+Texture2DArraySample(BaseColorArray,BaseColorArraySampler,float3(mxy,s)).rgb*projectionWeights.z;
float3 f=Texture2DArraySample(BaseColorArray,BaseColorArraySampler,float3(fyz,s)).rgb*projectionWeights.x+Texture2DArraySample(BaseColorArray,BaseColorArraySampler,float3(fxz,s)).rgb*projectionWeights.y+Texture2DArraySample(BaseColorArray,BaseColorArraySampler,float3(fxy,s)).rgb*projectionWeights.z;
float3 macroA=Texture2DArraySample(MacroColorArray,MacroColorArraySampler,float3(WorldPosition.yz*macroScale,s)).rgb*projectionWeights.x+Texture2DArraySample(MacroColorArray,MacroColorArraySampler,float3(WorldPosition.xz*macroScale,s)).rgb*projectionWeights.y+Texture2DArraySample(MacroColorArray,MacroColorArraySampler,float3(WorldPosition.xy*macroScale,s)).rgb*projectionWeights.z;
float macroScaleB=macroScale*0.37;float3 macroB=Texture2DArraySample(MacroColorArray,MacroColorArraySampler,float3(myz*(macroScaleB/ms),s)).rgb*projectionWeights.x+Texture2DArraySample(MacroColorArray,MacroColorArraySampler,float3(mxz*(macroScaleB/ms),s)).rgb*projectionWeights.y+Texture2DArraySample(MacroColorArray,MacroColorArraySampler,float3(mxy*(macroScaleB/ms),s)).rgb*projectionWeights.z;
float3 surface=n*bandWeights.x+m*bandWeights.y+f*bandWeights.z;float macroStrength=saturate(detailData[i].y);float3 macro=lerp(macroA,macroA*macroB*2.0,0.55);result+=surface*tint[i].rgb*lerp(1.0,macro*2.0,macroStrength)*blendWeights[i];}
float wet=saturate(WeatherWetness)*saturate(WeatherWetDarkening);return result*lerp(1.0,0.55,wet);
)");
    AddSharedInputs(BaseColor);
    AddCustomInput(BaseColor, TEXT("BaseColorArray"), BaseColorArray);
    AddCustomInput(BaseColor, TEXT("MacroColorArray"), MacroArray);
    AddCustomInput(BaseColor, TEXT("NearScaleMultiplier"), NearScale);
    AddCustomInput(BaseColor, TEXT("MidScaleMultiplier"), MidScale);
    AddCustomInput(BaseColor, TEXT("FarScaleMultiplier"), FarScale);
    AddCustomInput(BaseColor, TEXT("WeatherWetness"), Wetness);
    AddCustomInput(BaseColor, TEXT("WeatherWetDarkening"), WetDarkening);

    UMaterialExpressionCustom* WorldNormal = AddExpression<UMaterialExpressionCustom>(Material, -760, -20);
    WorldNormal->Description = TEXT("Cubus three-band rotated normal with close micro detail");
    WorldNormal->OutputType = CMOT_Float3;
    WorldNormal->Code = FString(SharedCode) + TEXT(R"(
float3 result=0.0;const float cm=0.8386706;const float sm=0.5446390;const float cf=0.7547096;const float sf=-0.6560590;float3 signs=float3(geometryNormal.x<0?-1:1,geometryNormal.y<0?-1:1,geometryNormal.z<0?-1:1);
[unroll]for(int i=0;i<4;++i){float s=materialIds[i];float ns=max(projectionData[i].x*NearScaleMultiplier,0.000001);float ms=max(projectionData[i].x*MidScaleMultiplier,0.000001);float fs=max(projectionData[i].x*FarScaleMultiplier,0.000001);float ds=max(detailData[i].z,0.000001);float2 oy=float2(s*0.173+0.31,s*0.271+0.57);float2 oz=float2(s*0.197+0.43,s*0.313+0.69);
float2 myz=float2(WorldPosition.y*cm-WorldPosition.z*sm,WorldPosition.y*sm+WorldPosition.z*cm)*ms+oy;float2 mxz=float2(WorldPosition.x*cm-WorldPosition.z*sm,WorldPosition.x*sm+WorldPosition.z*cm)*ms+oz;float2 mxy=float2(WorldPosition.x*cm-WorldPosition.y*sm,WorldPosition.x*sm+WorldPosition.y*cm)*ms+oy;
float2 fyz=float2(WorldPosition.y*cf-WorldPosition.z*sf,WorldPosition.y*sf+WorldPosition.z*cf)*fs+oz;float2 fxz=float2(WorldPosition.x*cf-WorldPosition.z*sf,WorldPosition.x*sf+WorldPosition.z*cf)*fs+oy;float2 fxy=float2(WorldPosition.x*cf-WorldPosition.y*sf,WorldPosition.x*sf+WorldPosition.y*cf)*fs+oz;
float2 nx=(Texture2DArraySample(NormalArray,NormalArraySampler,float3(WorldPosition.yz*ns,s)).rg*bandWeights.x+Texture2DArraySample(NormalArray,NormalArraySampler,float3(myz,s)).rg*bandWeights.y+Texture2DArraySample(NormalArray,NormalArraySampler,float3(fyz,s)).rg*bandWeights.z)*2-1;
float2 ny=(Texture2DArraySample(NormalArray,NormalArraySampler,float3(WorldPosition.xz*ns,s)).rg*bandWeights.x+Texture2DArraySample(NormalArray,NormalArraySampler,float3(mxz,s)).rg*bandWeights.y+Texture2DArraySample(NormalArray,NormalArraySampler,float3(fxz,s)).rg*bandWeights.z)*2-1;
float2 nz=(Texture2DArraySample(NormalArray,NormalArraySampler,float3(WorldPosition.xy*ns,s)).rg*bandWeights.x+Texture2DArraySample(NormalArray,NormalArraySampler,float3(mxy,s)).rg*bandWeights.y+Texture2DArraySample(NormalArray,NormalArraySampler,float3(fxy,s)).rg*bandWeights.z)*2-1;
float2 dx=Texture2DArraySample(DetailNormalArray,DetailNormalArraySampler,float3(WorldPosition.yz*ds,s)).rg*2-1;float2 dy=Texture2DArraySample(DetailNormalArray,DetailNormalArraySampler,float3(WorldPosition.xz*ds,s)).rg*2-1;float2 dz=Texture2DArraySample(DetailNormalArray,DetailNormalArraySampler,float3(WorldPosition.xy*ds,s)).rg*2-1;
float3 bx=float3(sqrt(saturate(1-dot(nx,nx)))*signs.x,nx.x,nx.y);float3 by=float3(ny.x,sqrt(saturate(1-dot(ny,ny)))*signs.y,ny.y);float3 bz=float3(nz.x,nz.y,sqrt(saturate(1-dot(nz,nz)))*signs.z);float3 tx=float3(sqrt(saturate(1-dot(dx,dx)))*signs.x,dx.x,dx.y);float3 ty=float3(dy.x,sqrt(saturate(1-dot(dy,dy)))*signs.y,dy.y);float3 tz=float3(dz.x,dz.y,sqrt(saturate(1-dot(dz,dz)))*signs.z);
float3 baseN=normalize(bx*projectionWeights.x+by*projectionWeights.y+bz*projectionWeights.z);float3 detailN=normalize(tx*projectionWeights.x+ty*projectionWeights.y+tz*projectionWeights.z);float strength=saturate(detailData[i].w*detailVisibility);result+=normalize(lerp(baseN,normalize(baseN+detailN-geometryNormal),strength))*blendWeights[i];}
return normalize(result);
)");
    AddSharedInputs(WorldNormal);
    AddCustomInput(WorldNormal, TEXT("NormalArray"), NormalArray);
    AddCustomInput(WorldNormal, TEXT("DetailNormalArray"), DetailArray);
    AddCustomInput(WorldNormal, TEXT("NearScaleMultiplier"), NearScale);
    AddCustomInput(WorldNormal, TEXT("MidScaleMultiplier"), MidScale);
    AddCustomInput(WorldNormal, TEXT("FarScaleMultiplier"), FarScale);

    UMaterialExpressionCustom* Orm = AddExpression<UMaterialExpressionCustom>(Material, -760, 660);
    Orm->Description = TEXT("Cubus three-band rotated ORM and wetness");
    Orm->OutputType = CMOT_Float3;
    Orm->Code = FString(SharedCode) + TEXT(R"(
float3 result=0.0;const float cm=0.8386706;const float sm=0.5446390;const float cf=0.7547096;const float sf=-0.6560590;
[unroll]for(int i=0;i<4;++i){float s=materialIds[i];float ns=max(projectionData[i].x*NearScaleMultiplier,0.000001);float ms=max(projectionData[i].x*MidScaleMultiplier,0.000001);float fs=max(projectionData[i].x*FarScaleMultiplier,0.000001);float2 oy=float2(s*0.173+0.31,s*0.271+0.57);float2 oz=float2(s*0.197+0.43,s*0.313+0.69);
float2 myz=float2(WorldPosition.y*cm-WorldPosition.z*sm,WorldPosition.y*sm+WorldPosition.z*cm)*ms+oy;float2 mxz=float2(WorldPosition.x*cm-WorldPosition.z*sm,WorldPosition.x*sm+WorldPosition.z*cm)*ms+oz;float2 mxy=float2(WorldPosition.x*cm-WorldPosition.y*sm,WorldPosition.x*sm+WorldPosition.y*cm)*ms+oy;
float2 fyz=float2(WorldPosition.y*cf-WorldPosition.z*sf,WorldPosition.y*sf+WorldPosition.z*cf)*fs+oz;float2 fxz=float2(WorldPosition.x*cf-WorldPosition.z*sf,WorldPosition.x*sf+WorldPosition.z*cf)*fs+oy;float2 fxy=float2(WorldPosition.x*cf-WorldPosition.y*sf,WorldPosition.x*sf+WorldPosition.y*cf)*fs+oz;
float3 n=Texture2DArraySample(OrmArray,OrmArraySampler,float3(WorldPosition.yz*ns,s)).rgb*projectionWeights.x+Texture2DArraySample(OrmArray,OrmArraySampler,float3(WorldPosition.xz*ns,s)).rgb*projectionWeights.y+Texture2DArraySample(OrmArray,OrmArraySampler,float3(WorldPosition.xy*ns,s)).rgb*projectionWeights.z;
float3 m=Texture2DArraySample(OrmArray,OrmArraySampler,float3(myz,s)).rgb*projectionWeights.x+Texture2DArraySample(OrmArray,OrmArraySampler,float3(mxz,s)).rgb*projectionWeights.y+Texture2DArraySample(OrmArray,OrmArraySampler,float3(mxy,s)).rgb*projectionWeights.z;
float3 f=Texture2DArraySample(OrmArray,OrmArraySampler,float3(fyz,s)).rgb*projectionWeights.x+Texture2DArraySample(OrmArray,OrmArraySampler,float3(fxz,s)).rgb*projectionWeights.y+Texture2DArraySample(OrmArray,OrmArraySampler,float3(fxy,s)).rgb*projectionWeights.z;result+=(n*bandWeights.x+m*bandWeights.y+f*bandWeights.z)*blendWeights[i];}
result.g=lerp(result.g,saturate(WeatherWetRoughness),saturate(WeatherWetness));return result;
)");
    AddSharedInputs(Orm);
    AddCustomInput(Orm, TEXT("OrmArray"), OrmArray);
    AddCustomInput(Orm, TEXT("NearScaleMultiplier"), NearScale);
    AddCustomInput(Orm, TEXT("MidScaleMultiplier"), MidScale);
    AddCustomInput(Orm, TEXT("FarScaleMultiplier"), FarScale);
    AddCustomInput(Orm, TEXT("WeatherWetness"), Wetness);
    AddCustomInput(Orm, TEXT("WeatherWetRoughness"), WetRoughness);

    UMaterialExpressionCustom* Emissive = AddExpression<UMaterialExpressionCustom>(Material, -760, 1120);
    Emissive->Description = TEXT("Cubus per-material emissive");
    Emissive->OutputType = CMOT_Float3;
    Emissive->Code = FString(SharedCode) + TEXT(R"(
float3 result=0.0;[unroll]for(int i=0;i<4;++i)result+=emissiveData[i].rgb*max(emissiveData[i].a,0.0)*blendWeights[i];return result;
)");
    AddSharedInputs(Emissive);

    UMaterialExpressionComponentMask* AmbientOcclusion = AddMask(Material, Orm, true, false, false, -180, 640);
    UMaterialExpressionComponentMask* Roughness = AddMask(Material, Orm, false, true, false, -180, 760);
    UMaterialExpressionComponentMask* Metallic = AddMask(Material, Orm, false, false, true, -180, 880);

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
        TEXT("Built Cubus density terrain material stage 8: three-band rotated anti-tiling across colour, normal and ORM.")
    );

    return Material;
#else
    return nullptr;
#endif
}
