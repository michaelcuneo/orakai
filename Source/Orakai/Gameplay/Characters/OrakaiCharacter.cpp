// Copyright Epic Games, Inc. All Rights Reserved.

#include "OrakaiCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Orakai.h"
#include "CubusCore/Actors/CubusBlockWorldActor.h"
#include "CubusCore/Persistence/OrakaiPersistenceSubsystem.h"
#include "Gameplay/Interaction/Voxel/CubusVoxelEditLibrary.h"
#include "EngineUtils.h"
#include "InputCoreTypes.h"

AOrakaiCharacter::AOrakaiCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
}

void AOrakaiCharacter::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bGhostModeActive || GetController() == nullptr)
	{
		return;
	}

	const float VerticalInput =
		(bGhostAscendHeld ? 1.0f : 0.0f) -
		(bGhostDescendHeld ? 1.0f : 0.0f);

	if (!FMath::IsNearlyZero(VerticalInput))
	{
		AddMovementInput(FVector::UpVector, VerticalInput);
	}
}

void AOrakaiCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AOrakaiCharacter::Move);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AOrakaiCharacter::Look);
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AOrakaiCharacter::Look);

		if (bEnableSurvivalInteraction)
		{
			PlayerInputComponent->BindKey(
				EKeys::LeftMouseButton,
				IE_Pressed,
				this,
				&AOrakaiCharacter::HandleHarvestInput
			);
			PlayerInputComponent->BindKey(
				EKeys::RightMouseButton,
				IE_Pressed,
				this,
				&AOrakaiCharacter::HandlePlaceWoodInput
			);
		}

		if (bEnableGhostMode)
		{
			PlayerInputComponent->BindKey(
				EKeys::G,
				IE_Pressed,
				this,
				&AOrakaiCharacter::ToggleGhostMode
			);
			PlayerInputComponent->BindKey(
				EKeys::SpaceBar,
				IE_Pressed,
				this,
				&AOrakaiCharacter::HandleGhostAscendPressed
			);
			PlayerInputComponent->BindKey(
				EKeys::SpaceBar,
				IE_Released,
				this,
				&AOrakaiCharacter::HandleGhostAscendReleased
			);
			PlayerInputComponent->BindKey(
				EKeys::LeftControl,
				IE_Pressed,
				this,
				&AOrakaiCharacter::HandleGhostDescendPressed
			);
			PlayerInputComponent->BindKey(
				EKeys::LeftControl,
				IE_Released,
				this,
				&AOrakaiCharacter::HandleGhostDescendReleased
			);
		}
	}
	else
	{
		UE_LOG(
			LogOrakai,
			Error,
			TEXT("'%s' Failed to find an Enhanced Input component!"),
			*GetNameSafe(this)
		);
	}
}

void AOrakaiCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D MovementVector = Value.Get<FVector2D>();
	DoMove(MovementVector.X, MovementVector.Y);
}

void AOrakaiCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D LookAxisVector = Value.Get<FVector2D>();
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void AOrakaiCharacter::DoMove(float Right, float Forward)
{
	if (GetController() == nullptr)
	{
		return;
	}

	const FRotator Rotation = GetController()->GetControlRotation();

	if (bGhostModeActive)
	{
		const FVector ForwardDirection = Rotation.Vector();
		const FVector RightDirection = FRotationMatrix(Rotation).GetUnitAxis(EAxis::Y);
		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
		return;
	}

	const FRotator YawRotation(0.0f, Rotation.Yaw, 0.0f);
	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
	AddMovementInput(ForwardDirection, Forward);
	AddMovementInput(RightDirection, Right);
}

void AOrakaiCharacter::DoLook(float Yaw, float Pitch)
{
	if (GetController() != nullptr)
	{
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void AOrakaiCharacter::DoJumpStart()
{
	if (!bGhostModeActive)
	{
		Jump();
	}
}

void AOrakaiCharacter::DoJumpEnd()
{
	if (!bGhostModeActive)
	{
		StopJumping();
	}
}

void AOrakaiCharacter::ToggleGhostMode()
{
	SetGhostModeEnabled(!bGhostModeActive);
}

void AOrakaiCharacter::SetGhostModeEnabled(const bool bEnabled)
{
	if (bGhostModeActive == bEnabled)
	{
		return;
	}

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (!IsValid(Movement) || !IsValid(Capsule))
	{
		return;
	}

	if (bEnabled)
	{
		SavedCapsuleCollision = Capsule->GetCollisionEnabled();
		SavedMovementMode = Movement->MovementMode;
		SavedCustomMovementMode = Movement->CustomMovementMode;
		SavedMaxFlySpeed = Movement->MaxFlySpeed;
		bSavedOrientRotationToMovement = Movement->bOrientRotationToMovement;

		bGhostModeActive = true;
		bGhostAscendHeld = false;
		bGhostDescendHeld = false;

		StopJumping();
		Movement->StopMovementImmediately();
		Movement->MaxFlySpeed = FMath::Max(100.0f, GhostFlySpeed);
		Movement->bOrientRotationToMovement = false;
		Movement->SetMovementMode(MOVE_Flying);
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		UE_LOG(LogOrakai, Display, TEXT("Ghost mode enabled."));
		return;
	}

	bGhostModeActive = false;
	bGhostAscendHeld = false;
	bGhostDescendHeld = false;

	Movement->StopMovementImmediately();
	Movement->MaxFlySpeed = SavedMaxFlySpeed;
	Movement->bOrientRotationToMovement = bSavedOrientRotationToMovement;
	Capsule->SetCollisionEnabled(SavedCapsuleCollision);
	Movement->SetMovementMode(SavedMovementMode, SavedCustomMovementMode);

	UE_LOG(LogOrakai, Display, TEXT("Ghost mode disabled."));
}

void AOrakaiCharacter::HandleGhostAscendPressed()
{
	if (bGhostModeActive)
	{
		bGhostAscendHeld = true;
	}
}

void AOrakaiCharacter::HandleGhostAscendReleased()
{
	bGhostAscendHeld = false;
}

void AOrakaiCharacter::HandleGhostDescendPressed()
{
	if (bGhostModeActive)
	{
		bGhostDescendHeld = true;
	}
}

void AOrakaiCharacter::HandleGhostDescendReleased()
{
	bGhostDescendHeld = false;
}

ACubusBlockWorldActor* AOrakaiCharacter::FindCubusWorld() const
{
	UWorld* World = GetWorld();
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

bool AOrakaiCharacter::BuildInteractionRay(FVector& OutStart, FVector& OutEnd) const
{
	if (!IsValid(FollowCamera))
	{
		return false;
	}

	OutStart = FollowCamera->GetComponentLocation();
	OutEnd = OutStart +
		FollowCamera->GetForwardVector() * FMath::Max(100.0f, InteractionDistance);
	return true;
}

int32 AOrakaiCharacter::GetWoodCount() const
{
	const UOrakaiPersistenceSubsystem* Persistence =
		UOrakaiPersistenceSubsystem::Get(this);
	return Persistence != nullptr
		? Persistence->GetInventoryQuantity(TEXT("Wood"))
		: 0;
}

bool AOrakaiCharacter::DoHarvestTree()
{
	ACubusBlockWorldActor* CubusWorld = FindCubusWorld();
	FVector TraceStart;
	FVector TraceEnd;
	FIntVector TreeWorldVoxel;

	if (
		!IsValid(CubusWorld) ||
		!BuildInteractionRay(TraceStart, TraceEnd) ||
		!CubusWorld->HarvestTreeAlongRay(
			TraceStart,
			TraceEnd,
			TreeSelectionRadius,
			TreeWorldVoxel
		)
	)
	{
		return false;
	}

	UOrakaiPersistenceSubsystem* Persistence =
		UOrakaiPersistenceSubsystem::Get(this);
	if (Persistence == nullptr)
	{
		return false;
	}

	const int32 NewWoodCount =
		Persistence->GetInventoryQuantity(TEXT("Wood")) +
		FMath::Max(1, WoodPerTree);
	Persistence->SetInventoryQuantity(TEXT("Wood"), NewWoodCount);

	UE_LOG(
		LogOrakai,
		Display,
		TEXT("Harvested tree (%d, %d, %d): wood=%d"),
		TreeWorldVoxel.X,
		TreeWorldVoxel.Y,
		TreeWorldVoxel.Z,
		NewWoodCount
	);
	return true;
}

void AOrakaiCharacter::HandleHarvestInput()
{
	DoHarvestTree();
}

void AOrakaiCharacter::HandlePlaceWoodInput()
{
	DoPlaceWoodBlock();
}

bool AOrakaiCharacter::DoPlaceWoodBlock()
{
	UOrakaiPersistenceSubsystem* Persistence =
		UOrakaiPersistenceSubsystem::Get(this);
	if (Persistence == nullptr)
	{
		return false;
	}

	const int32 WoodCount =
		Persistence->GetInventoryQuantity(TEXT("Wood"));
	if (WoodCount <= 0)
	{
		return false;
	}

	FVector TraceStart;
	FVector TraceEnd;
	if (!BuildInteractionRay(TraceStart, TraceEnd))
	{
		return false;
	}

	FHitResult Hit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(OrakaiPlaceWood), true, this);
	if (
		!GetWorld()->LineTraceSingleByChannel(
			Hit,
			TraceStart,
			TraceEnd,
			ECC_Visibility,
			QueryParams
		) ||
		!UCubusVoxelEditLibrary::AddVoxelFromHit(Hit, WoodBlockMaterialId, false)
	)
	{
		return false;
	}

	Persistence->SetInventoryQuantity(TEXT("Wood"), WoodCount - 1);
	UE_LOG(LogOrakai, Display, TEXT("Placed wood block: wood=%d"), WoodCount - 1);
	return true;
}
