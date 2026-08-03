#include "CubusCore/Editor/CubusMaterialBuilderLibrary.h"

#include "Materials/Material.h"

#if WITH_EDITOR

#include "AssetToolsModule.h"
#include "Factories/MaterialFactoryNew.h"
#include "MaterialEditingLibrary.h"
#include "Materials/MaterialExpressionCameraPositionWS.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionScalarParameter.h"
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

    UMaterialExpressionVectorParameter* Vector(
        UMaterial* Material,
        const FName Name,
        const FLinearColor Value,
        const int32 X,
        const int32 Y
    )
    {
        UMaterialExpressionVectorParameter* Node =
            AddExpression<UMaterialExpressionVectorParameter>(Material, X, Y);
        Node->ParameterName = Name;
        Node->DefaultValue = Value;
        return Node;
    }

    UMaterialExpressionTextureObjectParameter* TextureObject(
        UMaterial* Material,
        const FName Name,
        UTexture* DefaultTexture,
        const EMaterialSamplerType SamplerType,
        const int32 X,
        const int32 Y
    )
    {
        UMaterialExpressionTextureObjectParameter* Node =
            AddExpression<UMaterialExpressionTextureObjectParameter>(
                Material,
                X,
                Y
            );
        Node->ParameterName = Name;
        Node->Texture = DefaultTexture;
        Node->SamplerType = SamplerType;
        return Node;
    }

    UMaterialExpressionComponentMask* VertexAlpha(
        UMaterial* Material,
        UMaterialExpression* VertexColor,
        const int32 X,
        const int32 Y
    )
    {
        UMaterialExpressionComponentMask* Node =
            AddExpression<UMaterialExpressionComponentMask>(Material, X, Y);
        Node->R = true;
        Node->G = false;
        Node->B = false;
        Node->A = false;
        Connect(Node->Input, VertexColor, 4);
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
        const FString Filename =
            FPackageName::LongPackageNameToFilename(
                Package->GetName(),
                FPackageName::GetAssetPackageExtension()
            );

        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        Args.SaveFlags = SAVE_NoError;
        UPackage::SavePackage(Package, Material, *Filename, Args);
    }

    FString CommonBlendCode()
    {
        return TEXT(R"(
float3 weights = pow(max(abs(PixelNormal), 0.0001), max(1.0, 0.5 * (ATriplanarSharpness + BTriplanarSharpness)));
weights /= max(weights.x + weights.y + weights.z, 0.0001);
float3 ap = WorldPosition * AWorldScale;
float3 bp = WorldPosition * BWorldScale;
float ah =
    Texture2DSample(AHeight, AHeightSampler, ap.yz).r * weights.x +
    Texture2DSample(AHeight, AHeightSampler, ap.xz).r * weights.y +
    Texture2DSample(AHeight, AHeightSampler, ap.xy).r * weights.z;
float bh =
    Texture2DSample(BHeight, BHeightSampler, bp.yz).r * weights.x +
    Texture2DSample(BHeight, BHeightSampler, bp.xz).r * weights.y +
    Texture2DSample(BHeight, BHeightSampler, bp.xy).r * weights.z;
float aScore = (1.0 - VertexBlend) + (ah - 0.5) * AHeightStrength;
float bScore = VertexBlend + (bh - 0.5) * BHeightStrength;
float contrast = max(0.01, 0.5 * (ABlendContrast + BBlendContrast));
float materialBlend = saturate((bScore - aScore) * contrast + 0.5);
)");
    }

    void AddSharedInputs(
        UMaterialExpressionCustom* Node,
        UMaterialExpression* WorldPosition,
        UMaterialExpression* PixelNormal,
        UMaterialExpression* CameraPosition,
        UMaterialExpression* VertexBlend,
        UMaterialExpression* AHeight,
        UMaterialExpression* BHeight,
        UMaterialExpression* AWorldScale,
        UMaterialExpression* BWorldScale,
        UMaterialExpression* ASharpness,
        UMaterialExpression* BSharpness,
        UMaterialExpression* AHeightStrength,
        UMaterialExpression* BHeightStrength,
        UMaterialExpression* ABlendContrast,
        UMaterialExpression* BBlendContrast
    )
    {
        AddCustomInput(Node, TEXT("WorldPosition"), WorldPosition);
        AddCustomInput(Node, TEXT("PixelNormal"), PixelNormal);
        AddCustomInput(Node, TEXT("CameraPosition"), CameraPosition);
        AddCustomInput(Node, TEXT("VertexBlend"), VertexBlend);
        AddCustomInput(Node, TEXT("AHeight"), AHeight);
        AddCustomInput(Node, TEXT("BHeight"), BHeight);
        AddCustomInput(Node, TEXT("AWorldScale"), AWorldScale);
        AddCustomInput(Node, TEXT("BWorldScale"), BWorldScale);
        AddCustomInput(Node, TEXT("ATriplanarSharpness"), ASharpness);
        AddCustomInput(Node, TEXT("BTriplanarSharpness"), BSharpness);
        AddCustomInput(Node, TEXT("AHeightStrength"), AHeightStrength);
        AddCustomInput(Node, TEXT("BHeightStrength"), BHeightStrength);
        AddCustomInput(Node, TEXT("ABlendContrast"), ABlendContrast);
        AddCustomInput(Node, TEXT("BBlendContrast"), BBlendContrast);
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
    Material->TwoSided = false;
    Material->bUseMaterialAttributes = false;

    UTexture* DefaultColor = LoadObject<UTexture>(
        nullptr,
        TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture")
    );
    UTexture* DefaultNormal = LoadObject<UTexture>(
        nullptr,
        TEXT("/Engine/EngineMaterials/DefaultNormal.DefaultNormal")
    );

    if (!IsValid(DefaultColor) || !IsValid(DefaultNormal))
    {
        UE_LOG(LogTemp, Error, TEXT("Density material default textures are missing."));
        return nullptr;
    }

    UMaterialExpressionWorldPosition* WorldPosition =
        AddExpression<UMaterialExpressionWorldPosition>(Material, -3400, -900);
    UMaterialExpressionVertexNormalWS* PixelNormal =
        AddExpression<UMaterialExpressionVertexNormalWS>(Material, -3400, -700);
    UMaterialExpressionCameraPositionWS* CameraPosition =
        AddExpression<UMaterialExpressionCameraPositionWS>(Material, -3400, -500);
    UMaterialExpressionVertexColor* VertexColor =
        AddExpression<UMaterialExpressionVertexColor>(Material, -3400, -300);
    UMaterialExpressionComponentMask* BlendAlpha =
        VertexAlpha(Material, VertexColor, -3150, -300);

    auto MakeTexture = [&](const TCHAR* Name, const bool bNormal, const int32 X, const int32 Y)
    {
        return TextureObject(
            Material,
            FName(Name),
            bNormal ? DefaultNormal : DefaultColor,
            bNormal ? SAMPLERTYPE_Normal : SAMPLERTYPE_LinearColor,
            X,
            Y
        );
    };

    UMaterialExpressionTextureObjectParameter* ABaseColor = MakeTexture(TEXT("ABaseColor"), false, -3000, -1200);
    UMaterialExpressionTextureObjectParameter* ANormal = MakeTexture(TEXT("ANormal"), true, -3000, -1040);
    UMaterialExpressionTextureObjectParameter* AORM = MakeTexture(TEXT("AORM"), false, -3000, -880);
    UMaterialExpressionTextureObjectParameter* AHeight = MakeTexture(TEXT("AHeight"), false, -3000, -720);
    UMaterialExpressionTextureObjectParameter* AMacro = MakeTexture(TEXT("AMacroColor"), false, -3000, -560);
    UMaterialExpressionTextureObjectParameter* ADetail = MakeTexture(TEXT("ADetailNormal"), true, -3000, -400);

    UMaterialExpressionTextureObjectParameter* BBaseColor = MakeTexture(TEXT("BBaseColor"), false, -3000, 100);
    UMaterialExpressionTextureObjectParameter* BNormal = MakeTexture(TEXT("BNormal"), true, -3000, 260);
    UMaterialExpressionTextureObjectParameter* BORM = MakeTexture(TEXT("BORM"), false, -3000, 420);
    UMaterialExpressionTextureObjectParameter* BHeight = MakeTexture(TEXT("BHeight"), false, -3000, 580);
    UMaterialExpressionTextureObjectParameter* BMacro = MakeTexture(TEXT("BMacroColor"), false, -3000, 740);
    UMaterialExpressionTextureObjectParameter* BDetail = MakeTexture(TEXT("BDetailNormal"), true, -3000, 900);

    UMaterialExpressionScalarParameter* AWorldScale = Scalar(Material, TEXT("AWorldScale"), 0.01f, -2600, -1200);
    UMaterialExpressionScalarParameter* ASharpness = Scalar(Material, TEXT("ATriplanarSharpness"), 6.0f, -2600, -1080);
    UMaterialExpressionScalarParameter* AMacroScale = Scalar(Material, TEXT("AMacroScale"), 0.0005f, -2600, -960);
    UMaterialExpressionScalarParameter* AMacroStrength = Scalar(Material, TEXT("AMacroStrength"), 0.2f, -2600, -840);
    UMaterialExpressionScalarParameter* ADetailScale = Scalar(Material, TEXT("ADetailScale"), 0.08f, -2600, -720);
    UMaterialExpressionScalarParameter* ADetailStrength = Scalar(Material, TEXT("ADetailNormalStrength"), 0.35f, -2600, -600);
    UMaterialExpressionScalarParameter* AHeightStrength = Scalar(Material, TEXT("AHeightStrength"), 0.35f, -2600, -480);
    UMaterialExpressionScalarParameter* ABlendContrast = Scalar(Material, TEXT("ABlendContrast"), 4.0f, -2600, -360);
    UMaterialExpressionVectorParameter* ATint = Vector(Material, TEXT("ATint"), FLinearColor::White, -2600, -240);

    UMaterialExpressionScalarParameter* BWorldScale = Scalar(Material, TEXT("BWorldScale"), 0.01f, -2600, 100);
    UMaterialExpressionScalarParameter* BSharpness = Scalar(Material, TEXT("BTriplanarSharpness"), 6.0f, -2600, 220);
    UMaterialExpressionScalarParameter* BMacroScale = Scalar(Material, TEXT("BMacroScale"), 0.0005f, -2600, 340);
    UMaterialExpressionScalarParameter* BMacroStrength = Scalar(Material, TEXT("BMacroStrength"), 0.2f, -2600, 460);
    UMaterialExpressionScalarParameter* BDetailScale = Scalar(Material, TEXT("BDetailScale"), 0.08f, -2600, 580);
    UMaterialExpressionScalarParameter* BDetailStrength = Scalar(Material, TEXT("BDetailNormalStrength"), 0.35f, -2600, 700);
    UMaterialExpressionScalarParameter* BHeightStrength = Scalar(Material, TEXT("BHeightStrength"), 0.35f, -2600, 820);
    UMaterialExpressionScalarParameter* BBlendContrast = Scalar(Material, TEXT("BBlendContrast"), 4.0f, -2600, 940);
    UMaterialExpressionVectorParameter* BTint = Vector(Material, TEXT("BTint"), FLinearColor::White, -2600, 1060);

    FString ColorCode = CommonBlendCode() + TEXT(R"(
float4 ac =
    Texture2DSample(ABaseColor, ABaseColorSampler, ap.yz) * weights.x +
    Texture2DSample(ABaseColor, ABaseColorSampler, ap.xz) * weights.y +
    Texture2DSample(ABaseColor, ABaseColorSampler, ap.xy) * weights.z;
float4 bc =
    Texture2DSample(BBaseColor, BBaseColorSampler, bp.yz) * weights.x +
    Texture2DSample(BBaseColor, BBaseColorSampler, bp.xz) * weights.y +
    Texture2DSample(BBaseColor, BBaseColorSampler, bp.xy) * weights.z;
float3 amacroP = WorldPosition * AMacroScale;
float3 bmacroP = WorldPosition * BMacroScale;
float3 am =
    Texture2DSample(AMacroColor, AMacroColorSampler, amacroP.yz).rgb * weights.x +
    Texture2DSample(AMacroColor, AMacroColorSampler, amacroP.xz).rgb * weights.y +
    Texture2DSample(AMacroColor, AMacroColorSampler, amacroP.xy).rgb * weights.z;
float3 bm =
    Texture2DSample(BMacroColor, BMacroColorSampler, bmacroP.yz).rgb * weights.x +
    Texture2DSample(BMacroColor, BMacroColorSampler, bmacroP.xz).rgb * weights.y +
    Texture2DSample(BMacroColor, BMacroColorSampler, bmacroP.xy).rgb * weights.z;
float3 aColor = ac.rgb * ATint.rgb * lerp(1.0.xxx, am * 2.0, saturate(AMacroStrength));
float3 bColor = bc.rgb * BTint.rgb * lerp(1.0.xxx, bm * 2.0, saturate(BMacroStrength));
return lerp(aColor, bColor, materialBlend);
)");

    UMaterialExpressionCustom* ColorKernel =
        Custom(Material, TEXT("Cubus Density Color Kernel"), ColorCode, CMOT_Float3, -1200, -500);
    AddSharedInputs(ColorKernel, WorldPosition, PixelNormal, CameraPosition, BlendAlpha, AHeight, BHeight, AWorldScale, BWorldScale, ASharpness, BSharpness, AHeightStrength, BHeightStrength, ABlendContrast, BBlendContrast);
    AddCustomInput(ColorKernel, TEXT("ABaseColor"), ABaseColor);
    AddCustomInput(ColorKernel, TEXT("BBaseColor"), BBaseColor);
    AddCustomInput(ColorKernel, TEXT("AMacroColor"), AMacro);
    AddCustomInput(ColorKernel, TEXT("BMacroColor"), BMacro);
    AddCustomInput(ColorKernel, TEXT("AMacroScale"), AMacroScale);
    AddCustomInput(ColorKernel, TEXT("BMacroScale"), BMacroScale);
    AddCustomInput(ColorKernel, TEXT("AMacroStrength"), AMacroStrength);
    AddCustomInput(ColorKernel, TEXT("BMacroStrength"), BMacroStrength);
    AddCustomInput(ColorKernel, TEXT("ATint"), ATint);
    AddCustomInput(ColorKernel, TEXT("BTint"), BTint);

    FString NormalCode = CommonBlendCode() + TEXT(R"(
float3 anX = Texture2DSample(ANormal, ANormalSampler, ap.yz).xyz * 2.0 - 1.0;
float3 anY = Texture2DSample(ANormal, ANormalSampler, ap.xz).xyz * 2.0 - 1.0;
float3 anZ = Texture2DSample(ANormal, ANormalSampler, ap.xy).xyz * 2.0 - 1.0;
float3 bnX = Texture2DSample(BNormal, BNormalSampler, bp.yz).xyz * 2.0 - 1.0;
float3 bnY = Texture2DSample(BNormal, BNormalSampler, bp.xz).xyz * 2.0 - 1.0;
float3 bnZ = Texture2DSample(BNormal, BNormalSampler, bp.xy).xyz * 2.0 - 1.0;
float sx = PixelNormal.x < 0.0 ? -1.0 : 1.0;
float sy = PixelNormal.y < 0.0 ? -1.0 : 1.0;
float sz = PixelNormal.z < 0.0 ? -1.0 : 1.0;
float3 aWorld = normalize(float3(anX.z * sx, anX.x, anX.y) * weights.x + float3(anY.x, anY.z * sy, anY.y) * weights.y + float3(anZ.x, anZ.y, anZ.z * sz) * weights.z);
float3 bWorld = normalize(float3(bnX.z * sx, bnX.x, bnX.y) * weights.x + float3(bnY.x, bnY.z * sy, bnY.y) * weights.y + float3(bnZ.x, bnZ.y, bnZ.z * sz) * weights.z);
float distanceFade = saturate(1.0 - distance(WorldPosition, CameraPosition) / 30000.0);
float3 adp = WorldPosition * ADetailScale;
float3 bdp = WorldPosition * BDetailScale;
float3 ad = Texture2DSample(ADetailNormal, ADetailNormalSampler, adp.xy).xyz * 2.0 - 1.0;
float3 bd = Texture2DSample(BDetailNormal, BDetailNormalSampler, bdp.xy).xyz * 2.0 - 1.0;
aWorld = normalize(lerp(aWorld, normalize(aWorld + float3(ad.xy, 0.0) * ADetailNormalStrength), distanceFade));
bWorld = normalize(lerp(bWorld, normalize(bWorld + float3(bd.xy, 0.0) * BDetailNormalStrength), distanceFade));
return normalize(lerp(aWorld, bWorld, materialBlend));
)");

    UMaterialExpressionCustom* NormalKernel =
        Custom(Material, TEXT("Cubus Density Normal Kernel"), NormalCode, CMOT_Float3, -1200, 0);
    AddSharedInputs(NormalKernel, WorldPosition, PixelNormal, CameraPosition, BlendAlpha, AHeight, BHeight, AWorldScale, BWorldScale, ASharpness, BSharpness, AHeightStrength, BHeightStrength, ABlendContrast, BBlendContrast);
    AddCustomInput(NormalKernel, TEXT("ANormal"), ANormal);
    AddCustomInput(NormalKernel, TEXT("BNormal"), BNormal);
    AddCustomInput(NormalKernel, TEXT("ADetailNormal"), ADetail);
    AddCustomInput(NormalKernel, TEXT("BDetailNormal"), BDetail);
    AddCustomInput(NormalKernel, TEXT("ADetailScale"), ADetailScale);
    AddCustomInput(NormalKernel, TEXT("BDetailScale"), BDetailScale);
    AddCustomInput(NormalKernel, TEXT("ADetailNormalStrength"), ADetailStrength);
    AddCustomInput(NormalKernel, TEXT("BDetailNormalStrength"), BDetailStrength);

    FString ORMCode = CommonBlendCode() + TEXT(R"(
float3 ao =
    Texture2DSample(AORM, AORMSampler, ap.yz).rgb * weights.x +
    Texture2DSample(AORM, AORMSampler, ap.xz).rgb * weights.y +
    Texture2DSample(AORM, AORMSampler, ap.xy).rgb * weights.z;
float3 bo =
    Texture2DSample(BORM, BORMSampler, bp.yz).rgb * weights.x +
    Texture2DSample(BORM, BORMSampler, bp.xz).rgb * weights.y +
    Texture2DSample(BORM, BORMSampler, bp.xy).rgb * weights.z;
return lerp(ao, bo, materialBlend);
)");

    UMaterialExpressionCustom* ORMKernel =
        Custom(Material, TEXT("Cubus Density ORM Kernel"), ORMCode, CMOT_Float3, -1200, 500);
    AddSharedInputs(ORMKernel, WorldPosition, PixelNormal, CameraPosition, BlendAlpha, AHeight, BHeight, AWorldScale, BWorldScale, ASharpness, BSharpness, AHeightStrength, BHeightStrength, ABlendContrast, BBlendContrast);
    AddCustomInput(ORMKernel, TEXT("AORM"), AORM);
    AddCustomInput(ORMKernel, TEXT("BORM"), BORM);

    UMaterialExpressionVectorParameter* AEmissiveColor = Vector(Material, TEXT("AEmissiveColor"), FLinearColor::Black, -700, 850);
    UMaterialExpressionScalarParameter* AEmissiveStrength = Scalar(Material, TEXT("AEmissiveStrength"), 0.0f, -700, 970);
    UMaterialExpressionVectorParameter* BEmissiveColor = Vector(Material, TEXT("BEmissiveColor"), FLinearColor::Black, -700, 1090);
    UMaterialExpressionScalarParameter* BEmissiveStrength = Scalar(Material, TEXT("BEmissiveStrength"), 0.0f, -700, 1210);

    UMaterialExpressionMultiply* AEmissive = AddExpression<UMaterialExpressionMultiply>(Material, -400, 900);
    Connect(AEmissive->A, AEmissiveColor);
    Connect(AEmissive->B, AEmissiveStrength);
    UMaterialExpressionMultiply* BEmissive = AddExpression<UMaterialExpressionMultiply>(Material, -400, 1100);
    Connect(BEmissive->A, BEmissiveColor);
    Connect(BEmissive->B, BEmissiveStrength);
    UMaterialExpressionLinearInterpolate* Emissive = AddExpression<UMaterialExpressionLinearInterpolate>(Material, -100, 1000);
    Connect(Emissive->A, AEmissive);
    Connect(Emissive->B, BEmissive);
    Connect(Emissive->Alpha, BlendAlpha);

    UMaterialEditorOnlyData* Data = Material->GetEditorOnlyData();
    Connect(Data->BaseColor, ColorKernel);
    Connect(Data->Normal, NormalKernel);
    Connect(Data->AmbientOcclusion, ORMKernel, 1);
    Connect(Data->Roughness, ORMKernel, 2);
    Connect(Data->Metallic, ORMKernel, 3);
    Connect(Data->EmissiveColor, Emissive);

    Save(Material);

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Built advanced Cubus density material with %d registered expressions."),
        Material->GetEditorOnlyData()->ExpressionCollection.Expressions.Num()
    );

    return Material;
#else
    return nullptr;
#endif
}
