// Copyright Epic Games, Inc. All Rights Reserved.

#include "OrakaiCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"
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
	DrawDensityToolHud();

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

		PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AOrakaiCharacter::HandleHarvestInput);
		PlayerInputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &AOrakaiCharacter::HandlePlaceWoodInput);

		if (bEnableGhostMode)
		{
			PlayerInputComponent->BindKey(EKeys::G, IE_Pressed, this, &AOrakaiCharacter::ToggleGhostMode);
			PlayerInputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &AOrakaiCharacter::HandleGhostAscendPressed);
			PlayerInputComponent->BindKey(EKeys::SpaceBar, IE_Released, this, &AOrakaiCharacter::HandleGhostAscendReleased);
			PlayerInputComponent->BindKey(EKeys::LeftControl, IE_Pressed, this, &AOrakaiCharacter::HandleGhostDescendPressed);
			PlayerInputComponent->BindKey(EKeys::LeftControl, IE_Released, this, &AOrakaiCharacter::HandleGhostDescendReleased);
		}

		if (bEnableVoxelEditTestMode)
		{
			PlayerInputComponent->BindKey(EKeys::Zero, IE_Pressed, this, &AOrakaiCharacter::SelectDensityTool0);
			PlayerInputComponent->BindKey(EKeys::One, IE_Pressed, this, &AOrakaiCharacter::SelectDensityTool1);
			PlayerInputComponent->BindKey(EKeys::Two, IE_Pressed, this, &AOrakaiCharacter::SelectDensityTool2);
			PlayerInputComponent->BindKey(EKeys::Three, IE_Pressed, this, &AOrakaiCharacter::SelectDensityTool3);
			PlayerInputComponent->BindKey(EKeys::Four, IE_Pressed, this, &AOrakaiCharacter::SelectDensityTool4);
			PlayerInputComponent->BindKey(EKeys::Five, IE_Pressed, this, &AOrakaiCharacter::SelectDensityTool5);
		}
	}
	else
	{
		UE_LOG(LogOrakai, Error, TEXT("'%s' Failed to find an Enhanced Input component!"), *GetNameSafe(this));
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
		AddMovementInput(Rotation.Vector(), Forward);
		AddMovementInput(FRotationMatrix(Rotation).GetUnitAxis(EAxis::Y), Right);
		return;
	}

	const FRotator YawRotation(0.0f, Rotation.Yaw, 0.0f);
	AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), Forward);
	AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), Right);
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

void AOrakaiCharacter::SetDensityTool(const EOrakaiDensityTool Tool)
{
	ActiveDensityTool = bEnableVoxelEditTestMode ? Tool : EOrakaiDensityTool::Off;
	UE_LOG(LogOrakai, Display, TEXT("Density tool %d: %s"), static_cast<int32>(ActiveDensityTool), GetDensityToolName());
}

void AOrakaiCharacter::SelectDensityTool0() { SetDensityTool(EOrakaiDensityTool::Off); }
void AOrakaiCharacter::SelectDensityTool1() { SetDensityTool(EOrakaiDensityTool::Add); }
void AOrakaiCharacter::SelectDensityTool2() { SetDensityTool(EOrakaiDensityTool::Remove); }
void AOrakaiCharacter::SelectDensityTool3() { SetDensityTool(EOrakaiDensityTool::Smooth); }
void AOrakaiCharacter::SelectDensityTool4() { SetDensityTool(EOrakaiDensityTool::Level); }
void AOrakaiCharacter::SelectDensityTool5() { SetDensityTool(EOrakaiDensityTool::Restore); }

const TCHAR* AOrakaiCharacter::GetDensityToolName() const
{
	switch (ActiveDensityTool)
	{
	case EOrakaiDensityTool::Add: return TEXT("ADD DENSITY");
	case EOrakaiDensityTool::Remove: return TEXT("REMOVE DENSITY");
	case EOrakaiDensityTool::Smooth: return TEXT("SMOOTH DENSITY");
	case EOrakaiDensityTool::Level: return TEXT("LEVEL DENSITY");
	case EOrakaiDensityTool::Restore: return TEXT("RESTORE GENERATED TERRAIN");
	default: return TEXT("OFF");
	}
}

void AOrakaiCharacter::DrawDensityToolHud() const
{
	if (!bEnableVoxelEditTestMode || GEngine == nullptr)
	{
		return;
	}

	const FString Message = FString::Printf(
		TEXT("TERRAIN TOOL: %d - %s\nRadius: %d | Strength: %.2f | Material: %d\nLMB: Apply | 0-5: Select Tool"),
		static_cast<int32>(ActiveDensityTool),
		GetDensityToolName(),
		FMath::Max(0, VoxelEditBrushRadius),
		VoxelEditStrength,
		FMath::Max(1, VoxelEditMaterialId)
	);

	GEngine->AddOnScreenDebugMessage(
		731942,
		0.0f,
		ActiveDensityTool == EOrakaiDensityTool::Off ? FColor::Silver : FColor::Green,
		Message,
		false,
		FVector2D(1.15f, 1.15f)
	);
}

bool AOrakaiCharacter::TraceInteractionHit(FHitResult& OutHit) const
{
	FVector TraceStart;
	FVector TraceEnd;
	if (!BuildInteractionRay(TraceStart, TraceEnd) || !IsValid(GetWorld()))
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(OrakaiInteractionTrace), true, this);
	return GetWorld()->LineTraceSingleByChannel(OutHit, TraceStart, TraceEnd, ECC_Visibility, QueryParams);
}

bool AOrakaiCharacter::ApplyDensityToolAtCrosshair()
{
	if (ActiveDensityTool == EOrakaiDensityTool::Off)
	{
		return false;
	}

	FHitResult Hit;
	if (!TraceInteractionHit(Hit))
	{
		return false;
	}

	const int32 Radius = FMath::Max(0, VoxelEditBrushRadius);
	const float Strength = FMath::Max(0.01f, VoxelEditStrength);
	int32 EditedCount = 0;

	switch (ActiveDensityTool)
	{
	case EOrakaiDensityTool::Add:
		EditedCount = UCubusVoxelEditLibrary::AddDensityFromHit(Hit, Radius, Strength, FMath::Max(1, VoxelEditMaterialId));
		break;
	case EOrakaiDensityTool::Remove:
		EditedCount = UCubusVoxelEditLibrary::RemoveDensityFromHit(Hit, Radius, Strength);
		break;
	case EOrakaiDensityTool::Smooth:
		EditedCount = UCubusVoxelEditLibrary::SmoothDensityFromHit(Hit, Radius, FMath::Clamp(Strength, 0.0f, 1.0f));
		break;
	case EOrakaiDensityTool::Level:
		EditedCount = UCubusVoxelEditLibrary::LevelDensityFromHit(Hit, Radius, FMath::Clamp(Strength, 0.0f, 1.0f), FMath::Max(1, VoxelEditMaterialId));
		break;
	case EOrakaiDensityTool::Restore:
		EditedCount = UCubusVoxelEditLibrary::RestoreDensityFromHit(Hit, Radius, FMath::Clamp(Strength, 0.0f, 1.0f));
		break;
	default:
		break;
	}

	UE_LOG(LogOrakai, Display, TEXT("Density tool %s edited %d samples."), GetDensityToolName(), EditedCount);
	return EditedCount > 0;
}

void AOrakaiCharacter::HandleGhostAscendPressed()
{
	if (bGhostModeActive)
	{
		bGhostAscendHeld = true;
	}
}

void AOrakaiCharacter::HandleGhostAscendReleased() { bGhostAscendHeld = false; }

void AOrakaiCharacter::HandleGhostDescendPressed()
{
	if (bGhostModeActive)
	{
		bGhostDescendHeld = true;
	}
}

void AOrakaiCharacter::HandleGhostDescendReleased() { bGhostDescendHeld = false; }

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
	OutEnd = OutStart + FollowCamera->GetForwardVector() * FMath::Max(100.0f, InteractionDistance);
	return true;
}

int32 AOrakaiCharacter::GetWoodCount() const
{
	const UOrakaiPersistenceSubsystem* Persistence = UOrakaiPersistenceSubsystem::Get(this);
	return Persistence != nullptr ? Persistence->GetInventoryQuantity(TEXT("Wood")) : 0;
}

bool AOrakaiCharacter::DoHarvestTree()
{
	ACubusBlockWorldActor* CubusWorld = FindCubusWorld();
	FVector TraceStart;
	FVector TraceEnd;
	FIntVector TreeWorldVoxel;
	if (!IsValid(CubusWorld) || !BuildInteractionRay(TraceStart, TraceEnd) || !CubusWorld->HarvestTreeAlongRay(TraceStart, TraceEnd, TreeSelectionRadius, TreeWorldVoxel))
	{
		return false;
	}

	UOrakaiPersistenceSubsystem* Persistence = UOrakaiPersistenceSubsystem::Get(this);
	if (Persistence == nullptr)
	{
		return false;
	}

	const int32 NewWoodCount = Persistence->GetInventoryQuantity(TEXT("Wood")) + FMath::Max(1, WoodPerTree);
	Persistence->SetInventoryQuantity(TEXT("Wood"), NewWoodCount);
	UE_LOG(LogOrakai, Display, TEXT("Harvested tree (%d, %d, %d): wood=%d"), TreeWorldVoxel.X, TreeWorldVoxel.Y, TreeWorldVoxel.Z, NewWoodCount);
	return true;
}

void AOrakaiCharacter::HandleHarvestInput()
{
	if (ActiveDensityTool != EOrakaiDensityTool::Off)
	{
		ApplyDensityToolAtCrosshair();
		return;
	}

	if (bEnableSurvivalInteraction)
	{
		DoHarvestTree();
	}
}

void AOrakaiCharacter::HandlePlaceWoodInput()
{
	if (ActiveDensityTool != EOrakaiDensityTool::Off)
	{
		return;
	}

	if (bEnableSurvivalInteraction)
	{
		DoPlaceWoodBlock();
	}
}

bool AOrakaiCharacter::DoPlaceWoodBlock()
{
	UOrakaiPersistenceSubsystem* Persistence = UOrakaiPersistenceSubsystem::Get(this);
	if (Persistence == nullptr)
	{
		return false;
	}

	const int32 WoodCount = Persistence->GetInventoryQuantity(TEXT("Wood"));
	if (WoodCount <= 0)
	{
		return false;
	}

	FHitResult Hit;
	if (!TraceInteractionHit(Hit) || !UCubusVoxelEditLibrary::AddVoxelFromHit(Hit, WoodBlockMaterialId, false))
	{
		return false;
	}

	Persistence->SetInventoryQuantity(TEXT("Wood"), WoodCount - 1);
	UE_LOG(LogOrakai, Display, TEXT("Placed wood block: wood=%d"), WoodCount - 1);
	return true;
}
