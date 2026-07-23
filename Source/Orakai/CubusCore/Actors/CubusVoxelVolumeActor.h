#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CubusCore/Chunks/CubusBlockChunkData.h"
#include "CubusVoxelVolumeActor.generated.h"

class ACubusBlockWorldActor;
class UCubusMaterialRegistry;
class UCubusGeologyProfile;
class UMaterialInterface;
class UProceduralMeshComponent;
struct FCubusBlockChunkNeighborhood;

UENUM(BlueprintType)
enum class ECubusVolumeTestShape : uint8
{
    FlatTerrain UMETA(DisplayName = "Flat Terrain"),
    HeightTerrain UMETA(DisplayName = "Height Terrain"),
    SolidBlock UMETA(DisplayName = "Solid Block"),
    Platform UMETA(DisplayName = "Platform"),
    Steps UMETA(DisplayName = "Steps"),
    HollowRoom UMETA(DisplayName = "Hollow Room"),
    Pillars UMETA(DisplayName = "Pillars"),
    MixedMaterials UMETA(DisplayName = "Mixed Materials")
};

UCLASS(
    BlueprintType,
    Blueprintable,
    ClassGroup = "Cubus",
    meta = (DisplayName = "Cubus Block Chunk")
)
class ORAKAI_API ACubusVoxelVolumeActor : public AActor
{
    GENERATED_BODY()

public:
    ACubusVoxelVolumeActor();

    virtual void OnConstruction(const FTransform& Transform) override;

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cubus|Chunk")
    void GenerateTestShape();

    void GenerateTestShapeData();

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cubus|Chunk")
    void FillVolume();

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cubus|Chunk")
    void ClearVolume();

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cubus|Rendering")
    void RebuildVolume();

    const FIntVector& GetChunkCoordinate() const
    {
        return ChunkCoordinate;
    }

    float GetVoxelSize() const
    {
        return VoxelSize;
    }

    const FCubusBlockChunkData* GetChunkData() const
    {
        return ChunkData.Get();
    }

    void SetOwningBlockWorld(ACubusBlockWorldActor* InBlockWorld)
    {
        OwningBlockWorld = InBlockWorld;
    }

    void SetGenerateCollision(const bool bEnabled)
    {
        bGenerateCollision = bEnabled;
    }

    void ConfigureGeneratedChunk(
        const FIntVector& InChunkCoordinate,
        float InVoxelSize,
        ACubusBlockWorldActor* InBlockWorld
    );

    void ConfigureRendering(
        UCubusMaterialRegistry* InMaterialRegistry,
        UMaterialInterface* InFallbackVoxelMaterial
    );

    void ConfigureGeology(UCubusGeologyProfile* InGeologyProfile);

    void ConfigureTerrain(
        bool bInUseHeightTerrain,
        int32 InTerrainSurfaceWorldZ,
        int32 InTerrainBaseHeight,
        float InTerrainContinentAmplitude,
        float InTerrainContinentFrequency,
        float InTerrainHillAmplitude,
        float InTerrainHillFrequency,
        float InTerrainDetailAmplitude,
        float InTerrainDetailFrequency,
        float InTerrainRidgeAmplitude,
        float InTerrainRidgeFrequency,
        float InTerrainValleyDepth,
        float InTerrainValleyFrequency,
        float InTerrainValleyWidth,
        float InTerrainValleyFalloff,
        float InTerrainValleyWarpAmplitude,
        float InTerrainValleyWarpFrequency,
        float InTerrainRegionFrequency,
        float InTerrainPlainsThreshold,
        float InTerrainPlainsBlend,
        float InTerrainMountainThreshold,
        float InTerrainMountainBlend,
        int32 InTerrainSurfaceMaterialId,
        int32 InTerrainSubsurfaceMaterialId,
        int32 InTerrainRockMaterialId,
        int32 InTerrainSnowMaterialId,
        float InTerrainRockSlopeThreshold,
        int32 InTerrainSnowMinimumHeight,
        bool bInGenerateWater,
        int32 InTerrainWaterLevel,
        int32 InTerrainWaterMaterialId
    );

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cubus|Components")
    TObjectPtr<UProceduralMeshComponent> ProceduralMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cubus|Chunk")
    FIntVector Dimensions = FIntVector(32, 32, 32);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Chunk")
    FIntVector ChunkCoordinate = FIntVector::ZeroValue;

    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Cubus|Chunk")
    TObjectPtr<ACubusBlockWorldActor> OwningBlockWorld;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Chunk",
        meta = (ClampMin = "1.0", UIMin = "1.0", UIMax = "500.0", Units = "cm")
    )
    float VoxelSize = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Chunk")
    ECubusVolumeTestShape TestShape = ECubusVolumeTestShape::FlatTerrain;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain")
    int32 TerrainSurfaceWorldZ = 8;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain", meta = (ClampMin = "1"))
    int32 TerrainSurfaceMaterialId = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain", meta = (ClampMin = "1"))
    int32 TerrainSubsurfaceMaterialId = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Rendering")
    TObjectPtr<UMaterialInterface> VoxelMaterial;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Collision")
    bool bGenerateCollision = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Editor")
    bool bRebuildAutomatically = true;

    UPROPERTY(
        VisibleInstanceOnly,
        BlueprintReadOnly,
        Category = "Cubus|Rendering",
        meta = (AllowPrivateAccess = "true")
    )
    TObjectPtr<UCubusMaterialRegistry> MaterialRegistry = nullptr;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Geology",
        meta = (AllowPrivateAccess = "true")
    )
    TObjectPtr<UCubusGeologyProfile> GeologyProfile = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain")
    int32 TerrainBaseHeight = 8;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Shape", meta = (ClampMin = "0.0"))
    float TerrainContinentAmplitude = 18.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Shape", meta = (ClampMin = "0.000001"))
    float TerrainContinentFrequency = 0.003f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Shape", meta = (ClampMin = "0.0"))
    float TerrainHillAmplitude = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Shape", meta = (ClampMin = "0.000001"))
    float TerrainHillFrequency = 0.015f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Shape", meta = (ClampMin = "0.0"))
    float TerrainDetailAmplitude = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Shape", meta = (ClampMin = "0.000001"))
    float TerrainDetailFrequency = 0.08f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Shape", meta = (ClampMin = "0.0"))
    float TerrainRidgeAmplitude = 16.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Shape", meta = (ClampMin = "0.000001"))
    float TerrainRidgeFrequency = 0.012f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Shape", meta = (ClampMin = "0.0"))
    float TerrainValleyDepth = 14.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Shape", meta = (ClampMin = "0.000001"))
    float TerrainValleyFrequency = 0.006f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Shape", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float TerrainValleyWidth = 0.08f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Shape", meta = (ClampMin = "0.001", ClampMax = "1.0"))
    float TerrainValleyFalloff = 0.22f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Shape", meta = (ClampMin = "0.0"))
    float TerrainValleyWarpAmplitude = 24.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Shape", meta = (ClampMin = "0.000001"))
    float TerrainValleyWarpFrequency = 0.004f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Regions", meta = (ClampMin = "0.000001"))
    float TerrainRegionFrequency = 0.0025f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Regions", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
    float TerrainPlainsThreshold = -0.25f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Regions", meta = (ClampMin = "0.001", ClampMax = "1.0"))
    float TerrainPlainsBlend = 0.18f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Regions", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
    float TerrainMountainThreshold = 0.30f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Regions", meta = (ClampMin = "0.001", ClampMax = "1.0"))
    float TerrainMountainBlend = 0.20f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Materials", meta = (ClampMin = "1", ClampMax = "65535"))
    int32 TestSolidMaterialId = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Materials", meta = (ClampMin = "1"))
    int32 TerrainRockMaterialId = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Materials", meta = (ClampMin = "1"))
    int32 TerrainSnowMaterialId = 4;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Materials", meta = (ClampMin = "0.0"))
    float TerrainRockSlopeThreshold = 1.25f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Materials")
    int32 TerrainSnowMinimumHeight = 34;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Water")
    bool bGenerateWater = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Water")
    int32 TerrainWaterLevel = 8;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Water", meta = (ClampMin = "1"))
    int32 TerrainWaterMaterialId = 5;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Diagnostics")
    int32 TotalVoxelCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Diagnostics")
    int32 SolidVoxelCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Diagnostics")
    int32 GeneratedFaceCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Diagnostics")
    int32 GeneratedVertexCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Diagnostics")
    int32 GeneratedTriangleCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Diagnostics")
    int32 GeneratedMaterialSectionCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Diagnostics", meta = (Units = "ms"))
    float LastBuildTimeMilliseconds = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Diagnostics")
    int32 RebuildCount = 0;

private:
    TUniquePtr<FCubusBlockChunkData> ChunkData;

    void EnsureChunkData();
    void SynchronizeChunkState();
    void ResolveOwningBlockWorld();

    const FCubusBlockChunkData* FindNeighbourChunkData(
        const FIntVector& CoordinateOffset
    ) const;

    FCubusBlockChunkNeighborhood BuildNeighborhood() const;

    void RebuildAffectedChunks();
    void GenerateHeightTerrain();
    void GenerateFlatTerrain();
    void GenerateSolidBlock();
    void GeneratePlatform();
    void GenerateSteps();
    void GenerateHollowRoom();
    void GeneratePillars();
    void GenerateMixedMaterials();
    void ResetDiagnostics();
};
