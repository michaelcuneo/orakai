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

/**
 *  A simple player-controllable third person character
 *  Implements a controllable orbiting camera
 */
UCLASS(abstract)
class AOrakaiCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;
	
protected:

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;

	/** Source-only defaults: left click harvests, right click places wood. */
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

public:

	/** Constructor */
	AOrakaiCharacter();	

protected:

	/** Initialize input action bindings */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

public:

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles look inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

	UFUNCTION(BlueprintCallable, Category="Survival")
	bool DoHarvestTree();

	UFUNCTION(BlueprintCallable, Category="Survival")
	bool DoPlaceWoodBlock();

	UFUNCTION(BlueprintPure, Category="Survival")
	int32 GetWoodCount() const;

public:

	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }

private:
	ACubusBlockWorldActor* FindCubusWorld() const;
	bool BuildInteractionRay(FVector& OutStart, FVector& OutEnd) const;
	void HandleHarvestInput();
	void HandlePlaceWoodInput();
};
