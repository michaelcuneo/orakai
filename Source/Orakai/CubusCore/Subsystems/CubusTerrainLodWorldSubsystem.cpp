#include "CubusCore/Subsystems/CubusTerrainLodWorldSubsystem.h"

#include "CubusCore/Actors/CubusTerrainLodWorldActor.h"

#include "Engine/World.h"
#include "EngineUtils.h"

void UCubusTerrainLodWorldSubsystem::OnWorldBeginPlay(
    UWorld& InWorld
)
{
    Super::OnWorldBeginPlay(InWorld);

    if (!InWorld.IsGameWorld())
    {
        return;
    }

    for (
        TActorIterator<ACubusTerrainLodWorldActor> It(&InWorld);
        It;
        ++It
    )
    {
        if (IsValid(*It))
        {
            return;
        }
    }

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnParameters.ObjectFlags |= RF_Transient;

    ACubusTerrainLodWorldActor* LodActor =
        InWorld.SpawnActor<ACubusTerrainLodWorldActor>(
            ACubusTerrainLodWorldActor::StaticClass(),
            FVector::ZeroVector,
            FRotator::ZeroRotator,
            SpawnParameters
        );

    if (IsValid(LodActor))
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Cubus spawned terrain LOD world actor: %s"),
            *LodActor->GetName()
        );
    }
}
