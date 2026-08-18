#include "CubusCore/Generation/CubusGeneratedLoadingWorldActor.h"

#include "CubusCore/Actors/CubusWorldVegetationActor.h"
#include "CubusCore/Data/CubusGeologyProfile.h"
#include "CubusCore/Data/CubusMaterialRegistry.h"
#include "UObject/ConstructorHelpers.h"

ACubusGeneratedLoadingWorldActor::ACubusGeneratedLoadingWorldActor()
{
    static ConstructorHelpers::FClassFinder<ACubusVoxelVolumeActor> ChunkClass(
        TEXT("/Game/Cubus/Blueprints/BP_CubusVoxelPCGChunk"));
    if (ChunkClass.Succeeded())
    {
        ChunkActorClass = ChunkClass.Class;
    }

    static ConstructorHelpers::FObjectFinder<UCubusMaterialRegistry> MaterialLibrary(
        TEXT("/Game/Cubus/DataAssets/OrakaiMaterialLibrary.OrakaiMaterialLibrary"));
    if (MaterialLibrary.Succeeded())
    {
        MaterialRegistry = MaterialLibrary.Object;
    }

    static ConstructorHelpers::FObjectFinder<UCubusGeologyProfile> DefaultGeology(
        TEXT("/Game/Cubus/DataAssets/DA_CubusGeologyDefault.DA_CubusGeologyDefault"));
    if (DefaultGeology.Succeeded())
    {
        GeologyProfile = DefaultGeology.Object;
    }

    static ConstructorHelpers::FClassFinder<ACubusWorldVegetationActor> VegetationClass(
        TEXT("/Game/Cubus/Blueprints/BP_CubusWorldVegetationActor"));
    if (VegetationClass.Succeeded())
    {
        WorldVegetationActorClass = VegetationClass.Class;
    }
}
