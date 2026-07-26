#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OrakaiMainMenuWidget.generated.h"

class SWidget;
class UButton;

UCLASS()
class ORAKAI_API UOrakaiMainMenuWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;

private:
    void BuildWidgetTree();

    UFUNCTION()
    void HandleStartGame();

    UFUNCTION()
    void HandleQuitGame();

    UPROPERTY(Transient)
    TObjectPtr<UButton> StartButton = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UButton> QuitButton = nullptr;
};
