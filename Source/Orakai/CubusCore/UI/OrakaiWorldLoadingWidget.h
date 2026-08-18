#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OrakaiWorldLoadingWidget.generated.h"

class UBorder;
class UButton;
class UCanvasPanel;
class UImage;
class UProgressBar;
class USizeBox;
class UTextBlock;
class UTexture2D;

UCLASS()
class ORAKAI_API UOrakaiWorldLoadingWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetLoadingState(float Progress, const FText& Status);
	void SetSpawnSelectionAvailable(bool bAvailable);
	void SetSpawnReady(bool bReady);
	void SetSpawnPromotionActive(bool bActive);
	bool IsSpawnSelectionAvailable() const { return bSpawnSelectionAvailable; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void				NativeConstruct() override;
	virtual void				NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply				NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	UFUNCTION()
	void HandleSpawnClicked();

	void BuildWidgetTree();
	void BuildGeneratedPreviewTexture();
	void UpdateSelectionMarker(const FVector2D& PreviewUV);
	void RefreshSpawnUi();

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> ProgressBar = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SpawnInstructionText = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SpawnCoordinateText = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> PreviewCanvas = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> PreviewSizeBox = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UImage> PreviewImage = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> SpawnMarker = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UButton> SpawnButton = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SpawnButtonText = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> RuntimePreviewTexture = nullptr;

	FVector2D SelectedPreviewUV		   = FVector2D(0.5, 0.5);
	bool	  bHasSpawnSelection	   = false;
	bool	  bSpawnSelectionAvailable = false;
	bool	  bSpawnReady			   = false;
	bool	  bSpawnPromotionActive	   = false;
};
