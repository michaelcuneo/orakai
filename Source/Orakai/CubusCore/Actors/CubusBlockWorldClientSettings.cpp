#include "CubusCore/Actors/CubusBlockWorldActor.h"

void ACubusBlockWorldActor::SetClientViewDistance(
    const int32 InHorizontalViewRadius,
    const int32 InVerticalViewRadius,
    const bool bSaveSetting
)
{
    HorizontalViewRadius = FMath::Clamp(
        InHorizontalViewRadius,
        FMath::Max(0, InitialLoadRadius),
        16
    );

    VerticalViewRadius = FMath::Clamp(
        InVerticalViewRadius,
        0,
        4
    );

    if (bSaveSetting)
    {
        ACubusBlockWorldActor* Defaults =
            GetMutableDefault<ACubusBlockWorldActor>();

        Defaults->HorizontalViewRadius = HorizontalViewRadius;
        Defaults->VerticalViewRadius = VerticalViewRadius;
        Defaults->SaveConfig();
    }

    if (bEnableRuntimeStreaming && HasActorBegunPlay())
    {
        LastTrackedChunk = FIntVector(MAX_int32, MAX_int32, MAX_int32);
        UpdateRuntimeStreaming(true);
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Cubus client view distance set to horizontal %d, vertical %d chunks"),
        HorizontalViewRadius,
        VerticalViewRadius
    );
}

void ACubusBlockWorldActor::SetClientChunkLoadRate(
    const int32 InMaxChunksGeneratedPerTick,
    const bool bSaveSetting
)
{
    MaxChunksGeneratedPerTick = FMath::Clamp(
        InMaxChunksGeneratedPerTick,
        1,
        16
    );

    if (bSaveSetting)
    {
        ACubusBlockWorldActor* Defaults =
            GetMutableDefault<ACubusBlockWorldActor>();

        Defaults->MaxChunksGeneratedPerTick = MaxChunksGeneratedPerTick;
        Defaults->SaveConfig();
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Cubus client chunk load rate set to %d chunks per tick"),
        MaxChunksGeneratedPerTick
    );
}
