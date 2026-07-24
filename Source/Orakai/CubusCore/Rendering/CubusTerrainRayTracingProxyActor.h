#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "CubusTerrainRayTracingProxyActor.generated.h"

class ACubusPCGVoxelVolumeActor;
class UProceduralMeshComponent;

/**
 * Stable ray-tracing-only copy of one completed Cubus chunk mesh.
 *
 * The streamed source chunk remains raster/collision only. This actor owns a
 * separate procedural mesh that is never cleared or rebuilt while visible in
 * the ray-tracing scene. Replacements are created as new actors and old
 * proxies are retired after first being removed from ray tracing.
 */
UCLASS(Transient, NotBlueprintable)
class ORAKAI_API ACubusTerrainRayTracingProxyActor : public AActor
{
    GENERATED_BODY()

public:
    ACubusTerrainRayTracingProxyActor();

    bool BuildFromSource(
        ACubusPCGVoxelVolumeActor* InSourceChunk,
        int32 InSourceRevision
    );

    void BeginRetire(float DelaySeconds = 0.5f);

    ACubusPCGVoxelVolumeActor* GetSourceChunk() const
    {
        return SourceChunk.Get();
    }

    int32 GetSourceRevision() const
    {
        return SourceRevision;
    }

    bool IsRetiring() const
    {
        return bRetiring;
    }

protected:
    virtual void EndPlay(
        const EEndPlayReason::Type EndPlayReason
    ) override;

private:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UProceduralMeshComponent> ProxyMesh;

    TWeakObjectPtr<ACubusPCGVoxelVolumeActor> SourceChunk;
    int32 SourceRevision = INDEX_NONE;
    bool bRetiring = false;
};
