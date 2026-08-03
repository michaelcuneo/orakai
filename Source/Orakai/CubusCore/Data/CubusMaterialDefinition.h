#pragma once

#include "CoreMinimal.h"
#include "CubusMaterialDefinition.generated.h"

class UMaterialInterface;
class UTexture2D;

UENUM(BlueprintType)
enum class ECubusMatterState : uint8
{
    Empty  UMETA(DisplayName = "Empty"),
    Solid  UMETA(DisplayName = "Solid"),
    Liquid UMETA(DisplayName = "Liquid"),
    Gas    UMETA(DisplayName = "Gas")
};

USTRUCT(BlueprintType)
struct ORAKAI_API FCubusBlockSurfaceTextures
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PBR")
    TObjectPtr<UTexture2D> BaseColor = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PBR")
    TObjectPtr<UTexture2D> Normal = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PBR")
    TObjectPtr<UTexture2D> ORM = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PBR")
    TObjectPtr<UTexture2D> Height = nullptr;

    FORCEINLINE bool HasAnyTexture() const
    {
        return IsValid(BaseColor.Get()) || IsValid(Normal.Get()) ||
            IsValid(ORM.Get()) || IsValid(Height.Get());
    }
};

/** One continuous volumetric surface used by smooth density terrain. */
USTRUCT(BlueprintType)
struct ORAKAI_API FCubusDensitySurfaceTextures
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Density|Textures")
    TObjectPtr<UTexture2D> BaseColor = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Density|Textures")
    TObjectPtr<UTexture2D> Normal = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Density|Textures")
    TObjectPtr<UTexture2D> ORM = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Density|Textures")
    TObjectPtr<UTexture2D> Height = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Density|Textures")
    TObjectPtr<UTexture2D> MacroColor = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Density|Textures")
    TObjectPtr<UTexture2D> DetailNormal = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Density|Projection", meta = (ClampMin = "0.0001"))
    float WorldScale = 0.01f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Density|Projection", meta = (ClampMin = "0.1"))
    float TriplanarSharpness = 6.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Density|Macro", meta = (ClampMin = "0.000001"))
    float MacroScale = 0.0005f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Density|Macro", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MacroStrength = 0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Density|Detail", meta = (ClampMin = "0.0001"))
    float DetailScale = 0.08f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Density|Detail", meta = (ClampMin = "0.0"))
    float DetailNormalStrength = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Density|Blend", meta = (ClampMin = "0.0"))
    float HeightStrength = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Density|Blend", meta = (ClampMin = "0.01"))
    float BlendContrast = 4.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Density|Appearance")
    FLinearColor Tint = FLinearColor::White;

    FORCEINLINE bool HasAnyTexture() const
    {
        return IsValid(BaseColor.Get()) || IsValid(Normal.Get()) ||
            IsValid(ORM.Get()) || IsValid(Height.Get()) ||
            IsValid(MacroColor.Get()) || IsValid(DetailNormal.Get());
    }
};

USTRUCT(BlueprintType)
struct ORAKAI_API FCubusMaterialDefinition
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (ClampMin = "0", ClampMax = "65535"))
    int32 MaterialId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    FName Name = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physical")
    ECubusMatterState State = ECubusMatterState::Empty;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rendering")
    bool bRenderable = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rendering")
    bool bOccludesBlockFaces = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rendering")
    TObjectPtr<UMaterialInterface> Material = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rendering|Block PBR")
    FCubusBlockSurfaceTextures SideSurface;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rendering|Block PBR")
    FCubusBlockSurfaceTextures TopSurface;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rendering|Block PBR")
    FCubusBlockSurfaceTextures BottomSurface;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rendering|Density PBR")
    FCubusDensitySurfaceTextures DensitySurface;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rendering|PBR", meta = (ClampMin = "0.01"))
    float TextureScale = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rendering|PBR", meta = (ClampMin = "0.0"))
    float HeightStrength = 0.25f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rendering|PBR")
    FLinearColor Tint = FLinearColor::White;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rendering|PBR")
    FLinearColor EmissiveColor = FLinearColor::Black;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rendering|PBR", meta = (ClampMin = "0.0"))
    float EmissiveStrength = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rendering|PBR", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float SideTopBlendStart = 0.7f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rendering|PBR", meta = (ClampMin = "0.01"))
    float SideTopBlendSharpness = 4.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physical", meta = (ClampMin = "0.0"))
    float Density = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physical", meta = (ClampMin = "0.0"))
    float Hardness = 0.0f;

    FORCEINLINE bool UsesPbrTextures() const
    {
        return SideSurface.HasAnyTexture() || TopSurface.HasAnyTexture() ||
            BottomSurface.HasAnyTexture();
    }

    FORCEINLINE bool UsesDensityTextures() const
    {
        return DensitySurface.HasAnyTexture();
    }

    FORCEINLINE bool IsEmpty() const { return State == ECubusMatterState::Empty; }
    FORCEINLINE bool IsSolid() const { return State == ECubusMatterState::Solid; }
    FORCEINLINE bool IsLiquid() const { return State == ECubusMatterState::Liquid; }
    FORCEINLINE bool IsGas() const { return State == ECubusMatterState::Gas; }
};
