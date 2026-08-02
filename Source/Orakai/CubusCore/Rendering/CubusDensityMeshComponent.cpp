#include "CubusCore/Rendering/CubusDensityMeshComponent.h"

UCubusDensityMeshComponent::UCubusDensityMeshComponent(
    const FObjectInitializer& ObjectInitializer
)
    : Super(ObjectInitializer)
{
    PrimaryComponentTick.bCanEverTick = false;
    bAutoActivate = false;
    bUseAsyncCooking = false;

    DisableLegacyRenderer();
}

void UCubusDensityMeshComponent::PostLoad()
{
    Super::PostLoad();
    DisableLegacyRenderer();
}

void UCubusDensityMeshComponent::OnRegister()
{
    Super::OnRegister();
    DisableLegacyRenderer();
}

void UCubusDensityMeshComponent::DisableLegacyRenderer()
{
    ClearAllMeshSections();
    ClearCollisionConvexMeshes();

    SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetGenerateOverlapEvents(false);
    SetCastShadow(false);
    SetVisibility(false, true);
    SetHiddenInGame(true, true);
    SetRenderInMainPass(false);
    SetRenderInDepthPass(false);
    SetVisibleInRayTracing(false);
}
