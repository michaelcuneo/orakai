#include "CubusCore/UI/OrakaiWorldLoadingWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "CubusCore/Generation/CubusGeneratedTerrainRuntime.h"
#include "Engine/Texture2D.h"
#include "InputCoreTypes.h"

void UOrakaiWorldLoadingWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (!IsValid(WidgetTree) || IsValid(WidgetTree->RootWidget))
	{
		return;
	}

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
	WidgetTree->RootWidget = Root;

	UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
	Background->SetBrushColor(FLinearColor(0.012f, 0.018f, 0.016f, 1.0f));
	UOverlaySlot* BackgroundSlot = Root->AddChildToOverlay(Background);
	BackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
	BackgroundSlot->SetVerticalAlignment(VAlign_Fill);

	USizeBox* ContentWidth = WidgetTree->ConstructWidget<USizeBox>();
	ContentWidth->SetWidthOverride(720.0f);
	UOverlaySlot* ContentSlot = Root->AddChildToOverlay(ContentWidth);
	ContentSlot->SetHorizontalAlignment(HAlign_Center);
	ContentSlot->SetVerticalAlignment(VAlign_Center);

	UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
	ContentWidth->AddChild(Content);

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>();
	Title->SetText(FText::FromString(TEXT("ORAKAI")));
	Title->SetJustification(ETextJustify::Center);
	Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.91f, 0.94f, 0.89f, 1.0f)));
	FSlateFontInfo TitleFont = Title->GetFont();
	TitleFont.Size = 52;
	TitleFont.TypefaceFontName = TEXT("Bold");
	Title->SetFont(TitleFont);
	UVerticalBoxSlot* TitleSlot = Content->AddChildToVerticalBox(Title);
	TitleSlot->SetHorizontalAlignment(HAlign_Fill);
	TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 24.0f));

	StatusText = WidgetTree->ConstructWidget<UTextBlock>();
	StatusText->SetText(FText::FromString(TEXT("Building generated world chunks")));
	StatusText->SetJustification(ETextJustify::Center);
	StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.63f, 0.70f, 0.64f, 1.0f)));
	FSlateFontInfo StatusFont = StatusText->GetFont();
	StatusFont.Size = 18;
	StatusText->SetFont(StatusFont);
	UVerticalBoxSlot* StatusSlot = Content->AddChildToVerticalBox(StatusText);
	StatusSlot->SetHorizontalAlignment(HAlign_Fill);
	StatusSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));

	USizeBox* ProgressHeight = WidgetTree->ConstructWidget<USizeBox>();
	ProgressHeight->SetHeightOverride(6.0f);
	ProgressBar = WidgetTree->ConstructWidget<UProgressBar>();
	ProgressBar->SetPercent(0.0f);
	ProgressBar->SetFillColorAndOpacity(FLinearColor(0.47f, 0.67f, 0.42f, 1.0f));
	ProgressHeight->AddChild(ProgressBar);
	UVerticalBoxSlot* ProgressSlot = Content->AddChildToVerticalBox(ProgressHeight);
	ProgressSlot->SetHorizontalAlignment(HAlign_Fill);
	ProgressSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 18.0f));

	SpawnInstructionText = WidgetTree->ConstructWidget<UTextBlock>();
	SpawnInstructionText->SetText(FText::FromString(TEXT("Building DEM-derived terrain coverage...")));
	SpawnInstructionText->SetJustification(ETextJustify::Center);
	SpawnInstructionText->SetAutoWrapText(true);
	FSlateFontInfo InstructionFont = SpawnInstructionText->GetFont();
	InstructionFont.Size = 16;
	SpawnInstructionText->SetFont(InstructionFont);
	UVerticalBoxSlot* InstructionSlot = Content->AddChildToVerticalBox(SpawnInstructionText);
	InstructionSlot->SetHorizontalAlignment(HAlign_Fill);
	InstructionSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));

	USizeBox* PreviewSize = WidgetTree->ConstructWidget<USizeBox>();
	PreviewSize->SetWidthOverride(640.0f);
	PreviewSize->SetHeightOverride(480.0f);
	UVerticalBoxSlot* PreviewOuterSlot = Content->AddChildToVerticalBox(PreviewSize);
	PreviewOuterSlot->SetHorizontalAlignment(HAlign_Center);
	PreviewOuterSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));

	PreviewCanvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	PreviewSize->AddChild(PreviewCanvas);

	PreviewImage = WidgetTree->ConstructWidget<UImage>();
	UCanvasPanelSlot* ImageSlot = PreviewCanvas->AddChildToCanvas(PreviewImage);
	ImageSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	ImageSlot->SetOffsets(FMargin(0.0f));

	SpawnMarker = WidgetTree->ConstructWidget<UBorder>();
	SpawnMarker->SetBrushColor(FLinearColor(0.95f, 0.78f, 0.18f, 1.0f));
	SpawnMarker->SetVisibility(ESlateVisibility::Collapsed);
	UCanvasPanelSlot* MarkerSlot = PreviewCanvas->AddChildToCanvas(SpawnMarker);
	MarkerSlot->SetAnchors(FAnchors(0.0f, 0.0f));
	MarkerSlot->SetSize(FVector2D(16.0, 16.0));

	SpawnCoordinateText = WidgetTree->ConstructWidget<UTextBlock>();
	SpawnCoordinateText->SetText(FText::FromString(TEXT("Select a location on the generated terrain")));
	SpawnCoordinateText->SetJustification(ETextJustify::Center);
	FSlateFontInfo CoordinateFont = SpawnCoordinateText->GetFont();
	CoordinateFont.Size = 14;
	SpawnCoordinateText->SetFont(CoordinateFont);
	UVerticalBoxSlot* CoordinateSlot = Content->AddChildToVerticalBox(SpawnCoordinateText);
	CoordinateSlot->SetHorizontalAlignment(HAlign_Fill);
	CoordinateSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));

	SpawnButton = WidgetTree->ConstructWidget<UButton>();
	SpawnButtonText = WidgetTree->ConstructWidget<UTextBlock>();
	SpawnButtonText->SetText(FText::FromString(TEXT("SPAWN")));
	SpawnButtonText->SetJustification(ETextJustify::Center);
	FSlateFontInfo ButtonFont = SpawnButtonText->GetFont();
	ButtonFont.Size = 18;
	ButtonFont.TypefaceFontName = TEXT("Bold");
	SpawnButtonText->SetFont(ButtonFont);
	SpawnButton->AddChild(SpawnButtonText);
	SpawnButton->OnClicked.AddDynamic(this, &UOrakaiWorldLoadingWidget::HandleSpawnClicked);
	UVerticalBoxSlot* ButtonSlot = Content->AddChildToVerticalBox(SpawnButton);
	ButtonSlot->SetHorizontalAlignment(HAlign_Center);

	BuildGeneratedPreviewTexture();
	RefreshSpawnUi();
}

void UOrakaiWorldLoadingWidget::SetLoadingState(const float Progress, const FText& Status)
{
	if (IsValid(ProgressBar))
	{
		ProgressBar->SetPercent(FMath::Clamp(Progress, 0.0f, 1.0f));
	}
	if (IsValid(StatusText))
	{
		StatusText->SetText(Status);
	}
}

void UOrakaiWorldLoadingWidget::SetSpawnSelectionAvailable(const bool bAvailable)
{
	bSpawnSelectionAvailable = bAvailable && !bSpawnPromotionActive;
	RefreshSpawnUi();
}

void UOrakaiWorldLoadingWidget::SetSpawnPromotionActive(const bool bActive)
{
	bSpawnPromotionActive = bActive;
	if (bActive)
	{
		bSpawnSelectionAvailable = false;
	}
	RefreshSpawnUi();
}

FReply UOrakaiWorldLoadingWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bSpawnSelectionAvailable || bSpawnPromotionActive || !IsValid(PreviewImage) ||
		InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	const FGeometry PreviewGeometry = PreviewImage->GetCachedGeometry();
	const FVector2D LocalSize = PreviewGeometry.GetLocalSize();
	if (LocalSize.X <= 1.0 || LocalSize.Y <= 1.0)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	const FVector2D LocalPosition = PreviewGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
	if (LocalPosition.X < 0.0 || LocalPosition.Y < 0.0 ||
		LocalPosition.X > LocalSize.X || LocalPosition.Y > LocalSize.Y)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	SelectedPreviewUV = FVector2D(
		FMath::Clamp(LocalPosition.X / LocalSize.X, 0.0, 1.0),
		FMath::Clamp(LocalPosition.Y / LocalSize.Y, 0.0, 1.0)
	);
	FCubusGeneratedTerrainRuntime::SetProposedSpawnFromPreviewUV(SelectedPreviewUV);
	bHasSpawnSelection = true;
	UpdateSelectionMarker(SelectedPreviewUV);

	FVector2D WorldMeters;
	if (FCubusGeneratedTerrainRuntime::GetProposedSpawnWorldMeters(WorldMeters) && IsValid(SpawnCoordinateText))
	{
		SpawnCoordinateText->SetText(FText::FromString(FString::Printf(
			TEXT("Selected spawn: %.0f m, %.0f m"),
			WorldMeters.X,
			WorldMeters.Y
		)));
	}

	RefreshSpawnUi();
	return FReply::Handled();
}

void UOrakaiWorldLoadingWidget::HandleSpawnClicked()
{
	if (!bSpawnSelectionAvailable || !bHasSpawnSelection || bSpawnPromotionActive)
	{
		return;
	}

	if (!FCubusGeneratedTerrainRuntime::ConfirmProposedSpawn())
	{
		if (IsValid(SpawnCoordinateText))
		{
			SpawnCoordinateText->SetText(FText::FromString(TEXT("That location is outside the generated terrain. Choose another point.")));
		}
		return;
	}

	bSpawnPromotionActive = true;
	bSpawnSelectionAvailable = false;
	RefreshSpawnUi();
}

void UOrakaiWorldLoadingWidget::BuildGeneratedPreviewTexture()
{
	if (!IsValid(PreviewImage))
	{
		return;
	}

	int32 Resolution = 0;
	FBox2D BoundsMeters;
	TArray<FColor> Pixels;
	if (!FCubusGeneratedTerrainRuntime::GetPreviewSnapshot(Resolution, BoundsMeters, Pixels) ||
		Resolution <= 0 || Pixels.Num() != Resolution * Resolution)
	{
		return;
	}

	RuntimePreviewTexture = UTexture2D::CreateTransient(Resolution, Resolution, PF_B8G8R8A8);
	if (!IsValid(RuntimePreviewTexture) || RuntimePreviewTexture->GetPlatformData() == nullptr ||
		RuntimePreviewTexture->GetPlatformData()->Mips.IsEmpty())
	{
		RuntimePreviewTexture = nullptr;
		return;
	}

	RuntimePreviewTexture->SRGB = true;
	FTexture2DMipMap& Mip = RuntimePreviewTexture->GetPlatformData()->Mips[0];
	void* Destination = Mip.BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(Destination, Pixels.GetData(), static_cast<SIZE_T>(Pixels.Num()) * sizeof(FColor));
	Mip.BulkData.Unlock();
	RuntimePreviewTexture->UpdateResource();

	PreviewImage->SetBrushFromTexture(RuntimePreviewTexture, true);
}

void UOrakaiWorldLoadingWidget::UpdateSelectionMarker(const FVector2D& PreviewUV)
{
	if (!IsValid(SpawnMarker) || !IsValid(PreviewImage))
	{
		return;
	}

	const FVector2D Size = PreviewImage->GetCachedGeometry().GetLocalSize();
	if (Size.X <= 1.0 || Size.Y <= 1.0)
	{
		return;
	}

	if (UCanvasPanelSlot* MarkerSlot = Cast<UCanvasPanelSlot>(SpawnMarker->Slot))
	{
		MarkerSlot->SetPosition(FVector2D(
			PreviewUV.X * Size.X - 8.0,
			PreviewUV.Y * Size.Y - 8.0
		));
	}
	SpawnMarker->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UOrakaiWorldLoadingWidget::RefreshSpawnUi()
{
	const bool bShowPreview = bSpawnSelectionAvailable || bSpawnPromotionActive;
	if (IsValid(PreviewCanvas))
	{
		PreviewCanvas->SetVisibility(bShowPreview ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (IsValid(SpawnCoordinateText))
	{
		SpawnCoordinateText->SetVisibility(bShowPreview ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (IsValid(SpawnButton))
	{
		SpawnButton->SetVisibility(bSpawnSelectionAvailable ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		SpawnButton->SetIsEnabled(bSpawnSelectionAvailable && bHasSpawnSelection);
	}
	if (IsValid(SpawnInstructionText))
	{
		if (bSpawnPromotionActive)
		{
			SpawnInstructionText->SetText(FText::FromString(TEXT("Promoting the selected DEM-derived chunks to gameplay LOD...")));
		}
		else if (bSpawnSelectionAvailable)
		{
			SpawnInstructionText->SetText(FText::FromString(TEXT("Choose a location on the generated terrain, then press SPAWN.")));
		}
		else
		{
			SpawnInstructionText->SetText(FText::FromString(TEXT("Building DEM-derived terrain coverage...")));
		}
	}
}
