#include "CubusCore/UI/OrakaiWorldLoadingWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

void UOrakaiWorldLoadingWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (!IsValid(WidgetTree) || IsValid(WidgetTree->RootWidget))
	{
		return;
	}

	UOverlay* Root		   = WidgetTree->ConstructWidget<UOverlay>();
	WidgetTree->RootWidget = Root;

	UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
	Background->SetBrushColor(FLinearColor(0.012f, 0.018f, 0.016f, 1.0f));
	UOverlaySlot* BackgroundSlot = Root->AddChildToOverlay(Background);
	BackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
	BackgroundSlot->SetVerticalAlignment(VAlign_Fill);

	USizeBox* ContentWidth = WidgetTree->ConstructWidget<USizeBox>();
	ContentWidth->SetWidthOverride(520.0f);
	UOverlaySlot* ContentSlot = Root->AddChildToOverlay(ContentWidth);
	ContentSlot->SetHorizontalAlignment(HAlign_Center);
	ContentSlot->SetVerticalAlignment(VAlign_Center);

	UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
	ContentWidth->AddChild(Content);

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>();
	Title->SetText(FText::FromString(TEXT("ORAKAI")));
	Title->SetJustification(ETextJustify::Center);
	Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.91f, 0.94f, 0.89f, 1.0f)));
	FSlateFontInfo TitleFont   = Title->GetFont();
	TitleFont.Size			   = 58;
	TitleFont.TypefaceFontName = TEXT("Bold");
	Title->SetFont(TitleFont);
	UVerticalBoxSlot* TitleSlot = Content->AddChildToVerticalBox(Title);
	TitleSlot->SetHorizontalAlignment(HAlign_Fill);
	TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 46.0f));

	StatusText = WidgetTree->ConstructWidget<UTextBlock>();
	StatusText->SetText(FText::FromString(TEXT("Preparing terrain")));
	StatusText->SetJustification(ETextJustify::Center);
	StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.63f, 0.70f, 0.64f, 1.0f)));
	FSlateFontInfo StatusFont = StatusText->GetFont();
	StatusFont.Size			  = 18;
	StatusText->SetFont(StatusFont);
	UVerticalBoxSlot* StatusSlot = Content->AddChildToVerticalBox(StatusText);
	StatusSlot->SetHorizontalAlignment(HAlign_Fill);
	StatusSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 16.0f));

	USizeBox* ProgressHeight = WidgetTree->ConstructWidget<USizeBox>();
	ProgressHeight->SetHeightOverride(6.0f);
	ProgressBar = WidgetTree->ConstructWidget<UProgressBar>();
	ProgressBar->SetPercent(0.0f);
	ProgressBar->SetFillColorAndOpacity(FLinearColor(0.47f, 0.67f, 0.42f, 1.0f));
	ProgressHeight->AddChild(ProgressBar);
	UVerticalBoxSlot* ProgressSlot = Content->AddChildToVerticalBox(ProgressHeight);
	ProgressSlot->SetHorizontalAlignment(HAlign_Fill);
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
