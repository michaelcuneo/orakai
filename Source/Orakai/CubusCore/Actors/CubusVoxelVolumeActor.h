#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "CubusCore/Chunks/CubusBlockChunkData.h"

#include "CubusVoxelVolumeActor.generated.h"

class ACubusBlockWorldActor;
class UCubusMaterialRegistry;
class UMaterialInterface;
class UProceduralMeshComponent;
class UCubusGeologyProfile;

struct FCubusBlockChunkNeighborhood;

UENUM(BlueprintType)
enum class ECubusVolumeTestShape : uint