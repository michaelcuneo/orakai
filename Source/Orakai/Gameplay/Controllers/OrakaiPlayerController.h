// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "OrakaiPlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;
struct FHitResult;

/**
 * Basic PlayerController class for a third person game.
 * Manages input mappings and terrain material inspection diagnostics.
 */
UCLASS(abstract)
class AOrakaiPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AOrakaiPlayerController();

	virtual void Tick(float DeltaSeconds) override;
	
protected:

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category ="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** If true, the player will use UMG touch controls even if not playing on mobile platforms */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	/** Shows the rendered density material ID under the centre of the view. */
	UPROPERTY(EditAnywhere, Category = "Debug|Terrain Material")
	bool bShowTerrainMaterialInspector = true;

	/** Maximum camera trace distance in centimetres. */
	UPROPERTY(EditAnywhere, Category = "Debug|Terrain Material", meta = (ClampMin = "100.0", Units = "cm"))
	float TerrainMaterialTraceDistance = 10000.0f;

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Input mapping context setup */
	virtual void SetupInputComponent() override;

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;

private:
	void UpdateTerrainMaterialInspector();
	int32 ResolveRenderedTerrainMaterialId(const FHitResult& Hit) const;
};
