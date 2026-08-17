#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OrakaiWorldLoadingWidget.generated.h"

class UProgressBar;
class UTextBlock;

UCLASS()
class ORAKAI_API UOrakaiWorldLoadingWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetLoadingState(float Progress, const FText& Status);

protected:
	virtual void NativeConstruct() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> ProgressBar = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText = nullptr;
};
