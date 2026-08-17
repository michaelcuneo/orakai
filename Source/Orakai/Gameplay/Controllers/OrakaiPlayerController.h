// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "OrakaiPlayerController.generated.h"

class UInputMappingContext;
class UOrakaiWorldLoadingWidget;
class UUserWidget;
class ACubusBlockWorldActor;
struct FHitResult;

UCLASS(abstract)
class AOrakaiPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AOrakaiPlayerController();
	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(EditAnywhere, Category = "Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	UPROPERTY(EditAnywhere, Category = "Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	UPROPERTY(EditAnywhere, Category = "Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	UPROPERTY(EditAnywhere, Category = "Debug|Terrain Material")
	bool bShowTerrainMaterialInspector = true;

	UPROPERTY(EditAnywhere, Category = "Debug|Terrain Material", meta = (ClampMin = "100.0", Units = "cm"))
	float TerrainMaterialTraceDistance = 10000.0f;

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	bool ShouldUseTouchControls() const;

private:
	void InitializeWorldLoadingScreen();
	void UpdateWorldLoadingScreen(float DeltaSeconds);
	void EnterWorldLoadingInputMode();
	void EnterGameplayInputMode();
	void CreateMobileControlsIfNeeded();
	void UpdateTerrainMaterialInspector();
	int32 ResolveRenderedTerrainMaterialId(const FHitResult& Hit) const;

	UPROPERTY(Transient)
	TObjectPtr<UOrakaiWorldLoadingWidget> WorldLoadingWidget;

	UPROPERTY(Transient)
	TObjectPtr<ACubusBlockWorldActor> LoadingBlockWorld;

	float LoadingFadeElapsedSeconds = 0.0f;
};
