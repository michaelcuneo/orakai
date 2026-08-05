// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "OrakaiCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class ACubusBlockWorldActor;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

UCLASS(abstract)
class AOrakaiCharacter : public ACharacter
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	UCameraComponent* FollowCamera;

protected:
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* JumpAction;

	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;

	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;

	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Survival")
	bool bEnableSurvivalInteraction = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Survival", meta=(ClampMin="100.0", Units="cm"))
	float InteractionDistance = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Survival", meta=(ClampMin="1.0", Units="cm"))
	float TreeSelectionRadius = 225.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Survival", meta=(ClampMin="1"))
	int32 WoodPerTree = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Survival", meta=(ClampMin="1"))
	int32 WoodBlockMaterialId = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug|Ghost Mode")
	bool bEnableGhostMode = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug|Ghost Mode", meta=(ClampMin="100.0", Units="cm/s"))
	float GhostFlySpeed = 2400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug|Voxel Editing")
	bool bEnableVoxelEditTestMode = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug|Voxel Editing", meta=(ClampMin="0"))
	int32 VoxelEditBrushRadius = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug|Voxel Editing", meta=(ClampMin="0.01"))
	float VoxelEditStrength = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug|Voxel Editing", meta=(ClampMin="1"))
	int32 VoxelEditMaterialId = 1;

public:
	AOrakaiCharacter();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);

public:
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

	UFUNCTION(BlueprintCallable, Category="Survival")
	bool DoHarvestTree();

	UFUNCTION(BlueprintCallable, Category="Survival")
	bool DoPlaceWoodBlock();

	UFUNCTION(BlueprintPure, Category="Survival")
	int32 GetWoodCount() const;

	UFUNCTION(BlueprintCallable, Category="Debug|Ghost Mode")
	void ToggleGhostMode();

	UFUNCTION(BlueprintCallable, Category="Debug|Ghost Mode")
	void SetGhostModeEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category="Debug|Ghost Mode")
	bool IsGhostModeEnabled() const { return bGhostModeActive; }

	UFUNCTION(BlueprintCallable, Category="Debug|Voxel Editing")
	void SetVoxelEditTestModeEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category="Debug|Voxel Editing")
	bool IsVoxelEditTestModeEnabled() const { return bVoxelEditTestModeActive; }

	UFUNCTION(BlueprintCallable, Category="Debug|Voxel Editing")
	bool RemoveDensityVoxelAtCrosshair();

	UFUNCTION(BlueprintCallable, Category="Debug|Voxel Editing")
	bool AddDensityVoxelAtCrosshair();

	FORCEINLINE USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	FORCEINLINE UCameraComponent* GetFollowCamera() const { return FollowCamera; }

private:
	ACubusBlockWorldActor* FindCubusWorld() const;
	bool BuildInteractionRay(FVector& OutStart, FVector& OutEnd) const;
	bool TraceInteractionHit(FHitResult& OutHit) const;
	void HandleHarvestInput();
	void HandlePlaceWoodInput();
	void HandleEnableVoxelEditTestMode();
	void HandleDisableVoxelEditTestMode();
	void HandleGhostAscendPressed();
	void HandleGhostAscendReleased();
	void HandleGhostDescendPressed();
	void HandleGhostDescendReleased();

	bool bGhostModeActive = false;
	bool bGhostAscendHeld = false;
	bool bGhostDescendHeld = false;
	bool bVoxelEditTestModeActive = false;
	TEnumAsByte<ECollisionEnabled::Type> SavedCapsuleCollision = ECollisionEnabled::QueryAndPhysics;
	TEnumAsByte<EMovementMode> SavedMovementMode = MOVE_Walking;
	uint8 SavedCustomMovementMode = 0;
	float SavedMaxFlySpeed = 600.0f;
	bool bSavedOrientRotationToMovement = true;
};
