#include "CubusCore/Vegetation/CubusVegetationRepresentationSelector.h"

void FCubusVegetationRepresentationSelector::RouteCandidates(
    TArray<FCubusVegetationRepresentationCandidate>& Candidates,
    const int32 HeroLimit,
    const float HeroMaxDistance,
    TMap<int64, TArray<FTransform>>& RegularTransformsByBatchKey,
    TMap<int64, TArray<FTransform>>& HeroTransformsByBatchKey
)
{
    Candidates.Sort(
        [](
            const FCubusVegetationRepresentationCandidate& A,
            const FCubusVegetationRepresentationCandidate& B
        )
        {
            return A.DistanceSquared < B.DistanceSquared;
        }
    );

    const int32 SafeHeroLimit =
        FMath::Clamp(HeroLimit, 0, 64);

    const float SafeHeroDistance =
        FMath::Max(0.0f, HeroMaxDistance);

    const float HeroDistanceSquared =
        SafeHeroDistance * SafeHeroDistance;

    int32 SelectedHeroCount = 0;

    for (
        const FCubusVegetationRepresentationCandidate& Candidate :
        Candidates
    )
    {
        const bool bUseHero =
            SelectedHeroCount < SafeHeroLimit &&
            Candidate.DistanceSquared <= HeroDistanceSquared;

        if (bUseHero)
        {
            HeroTransformsByBatchKey
                .FindOrAdd(Candidate.PrimaryBatchKey)
                .Add(Candidate.LocalTransform);

            ++SelectedHeroCount;
            continue;
        }

        if (Candidate.bHasStaticFallback)
        {
            RegularTransformsByBatchKey
                .FindOrAdd(Candidate.StaticFallbackBatchKey)
                .Add(Candidate.LocalTransform);

            continue;
        }

        RegularTransformsByBatchKey
            .FindOrAdd(Candidate.PrimaryBatchKey)
            .Add(Candidate.LocalTransform);
    }
}
