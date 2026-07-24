#include "CubusCore/Actors/CubusBlockWorldActor.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"

namespace CubusClientStreamingSettings
{
    ACubusBlockWorldActor* FindBlockWorld(UWorld* World)
    {
        if (!IsValid(World))
        {
            return nullptr;
        }

        for (TActorIterator<ACubusBlockWorldActor> Iterator(World); Iterator; ++Iterator)
        {
            if (IsValid(*Iterator))
            {
                return *Iterator;
            }
        }

        return nullptr;
    }

    void SetViewDistance(
        const TArray<FString>& Arguments,
        UWorld* World
    )
    {
        ACubusBlockWorldActor* BlockWorld = FindBlockWorld(World);

        if (!IsValid(BlockWorld))
        {
            UE_LOG(LogTemp, Warning, TEXT("Cubus.ViewDistance could not find a Cubus block world"));
            return;
        }

        if (Arguments.IsEmpty())
        {
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Usage: Cubus.ViewDistance <horizontal 0-16> [vertical 0-4]")
            );
            return;
        }

        const int32 Horizontal = FCString::Atoi(*Arguments[0]);
        const int32 Vertical = Arguments.Num() > 1
            ? FCString::Atoi(*Arguments[1])
            : BlockWorld->GetClientVerticalViewDistance();

        BlockWorld->SetClientViewDistance(Horizontal, Vertical, true);
    }

    void SetChunkLoadRate(
        const TArray<FString>& Arguments,
        UWorld* World
    )
    {
        ACubusBlockWorldActor* BlockWorld = FindBlockWorld(World);

        if (!IsValid(BlockWorld))
        {
            UE_LOG(LogTemp, Warning, TEXT("Cubus.ChunkLoadRate could not find a Cubus block world"));
            return;
        }

        if (Arguments.IsEmpty())
        {
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Usage: Cubus.ChunkLoadRate <chunks per tick 1-16>")
            );
            return;
        }

        BlockWorld->SetClientChunkLoadRate(
            FCString::Atoi(*Arguments[0]),
            true
        );
    }

    FAutoConsoleCommandWithWorldAndArgs ViewDistanceCommand(
        TEXT("Cubus.ViewDistance"),
        TEXT("Sets persistent Cubus client view distance. Usage: Cubus.ViewDistance <horizontal> [vertical]"),
        FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SetViewDistance)
    );

    FAutoConsoleCommandWithWorldAndArgs ChunkLoadRateCommand(
        TEXT("Cubus.ChunkLoadRate"),
        TEXT("Sets persistent Cubus chunks loaded per tick. Usage: Cubus.ChunkLoadRate <1-16>"),
        FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SetChunkLoadRate)
    );
}

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
