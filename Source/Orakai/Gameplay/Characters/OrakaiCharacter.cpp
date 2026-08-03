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
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

void AOrakaiCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AOrakaiCharacter::Move);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AOrakaiCharacter::Look);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AOrakaiCharacter::Look);

		if (bEnableSurvivalInteraction)
		{
			// UE 5.8 deliberately deletes UEnhancedInputComponent::BindKey. These
			// source-default mouse bindings still use the legacy key-binding path,
			// so bind them through the UInputComponent base instead.
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
	}
	else
	{
		UE_LOG(LogOrakai, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void AOrakaiCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	// route the input
	DoMove(MovementVector.X, MovementVector.Y);
}

void AOrakaiCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// route the input
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void AOrakaiCharacter::DoMove(float Right, float Forward)
{
	if (GetController() != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void AOrakaiCharacter::DoLook(float Yaw, float Pitch)
{
	if (GetController() != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void AOrakaiCharacter::DoJumpStart()
{
	// signal the character to jump
	Jump();
}

void AOrakaiCharacter::DoJumpEnd()
{
	// signal the character to stop jumping
	StopJumping();
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

bool AOrakaiCharacter::BuildInteractionRay(
	FVector& OutStart,
	FVector& OutEnd
) const
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
