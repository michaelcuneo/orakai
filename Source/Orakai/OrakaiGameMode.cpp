// Copyright Epic Games, Inc. All Rights Reserved.

#include "OrakaiGameMode.h"

#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "UI/OrakaiMainMenuWidget.h"

AOrakaiGameMode::AOrakaiGameMode()
{
}

void AOrakaiGameMode::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PlayerController =
		UGameplayStatics::GetPlayerController(this, 0);

	if (!IsValid(PlayerController) || !PlayerController->IsLocalController())
	{
		return;
	}

	MainMenuWidget = CreateWidget<UOrakaiMainMenuWidget>(
		PlayerController,
		UOrakaiMainMenuWidget::StaticClass()
	);

	if (!IsValid(MainMenuWidget))
	{
		return;
	}

	MainMenuWidget->AddToPlayerScreen(100);

	PlayerController->SetIgnoreMoveInput(true);
	PlayerController->SetIgnoreLookInput(true);
	PlayerController->SetShowMouseCursor(true);

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(MainMenuWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PlayerController->SetInputMode(InputMode);
}
