#include "CubusCore/UI/OrakaiMainMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetSystemLibrary.h"

namespace OrakaiMainMenu
{
    UTextBlock* CreateButtonLabel(
        UWidgetTree* WidgetTree,
        const FText& Label
    )
    {
        UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
        Text->SetText(Label);
        Text->SetJustification(ETextJustify::Center);
        Text->SetColorAndOpacity(FSlateColor(FLinearColor::White));

        FSlateFontInfo Font = Text->GetFont();
        Font.Size = 22;
        Font.TypefaceFontName = TEXT("Bold");
        Text->SetFont(Font);

        return Text;
    }
}

void UOrakaiMainMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (!IsValid(WidgetTree) || IsValid(WidgetTree->RootWidget))
    {
        return;
    }

    UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
    WidgetTree->RootWidget = Root;

    UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
    Background->SetBrushColor(FLinearColor(0.015f, 0.02f, 0.03f, 0.94f));

    UOverlaySlot* BackgroundSlot = Root->AddChildToOverlay(Background);
    BackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
    BackgroundSlot->SetVerticalAlignment(VAlign_Fill);

    USizeBox* MenuWidth = WidgetTree->ConstructWidget<USizeBox>();
    MenuWidth->SetWidthOverride(420.0f);

    UOverlaySlot* MenuSlot = Root->AddChildToOverlay(MenuWidth);
    MenuSlot->SetHorizontalAlignment(HAlign_Center);
    MenuSlot->SetVerticalAlignment(VAlign_Center);

    UVerticalBox* Menu = WidgetTree->ConstructWidget<UVerticalBox>();
    MenuWidth->AddChild(Menu);

    UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>();
    Title->SetText(FText::FromString(TEXT("ORAKAI")));
    Title->SetJustification(ETextJustify::Center);
    Title->SetColorAndOpacity(FSlateColor(FLinearColor::White));

    FSlateFontInfo TitleFont = Title->GetFont();
    TitleFont.Size = 52;
    TitleFont.TypefaceFontName = TEXT("Bold");
    Title->SetFont(TitleFont);

    UVerticalBoxSlot* TitleSlot = Menu->AddChildToVerticalBox(Title);
    TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
    TitleSlot->SetHorizontalAlignment(HAlign_Fill);

    UTextBlock* Subtitle = WidgetTree->ConstructWidget<UTextBlock>();
    Subtitle->SetText(FText::FromString(TEXT("A procedural world awaits")));
    Subtitle->SetJustification(ETextJustify::Center);
    Subtitle->SetColorAndOpacity(
        FSlateColor(FLinearColor(0.72f, 0.76f, 0.82f, 1.0f))
    );

    FSlateFontInfo SubtitleFont = Subtitle->GetFont();
    SubtitleFont.Size = 18;
    Subtitle->SetFont(SubtitleFont);

    UVerticalBoxSlot* SubtitleSlot = Menu->AddChildToVerticalBox(Subtitle);
    SubtitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 42.0f));
    SubtitleSlot->SetHorizontalAlignment(HAlign_Fill);

    USizeBox* StartButtonSize = WidgetTree->ConstructWidget<USizeBox>();
    StartButtonSize->SetHeightOverride(64.0f);

    StartButton = WidgetTree->ConstructWidget<UButton>();
    StartButton->SetBackgroundColor(
        FLinearColor(0.15f, 0.42f, 0.72f, 1.0f)
    );
    StartButton->AddChild(
        OrakaiMainMenu::CreateButtonLabel(
            WidgetTree,
            FText::FromString(TEXT("START GAME"))
        )
    );
    StartButton->OnClicked.AddDynamic(
        this,
        &UOrakaiMainMenuWidget::HandleStartGame
    );
    StartButtonSize->AddChild(StartButton);

    UVerticalBoxSlot* StartSlot = Menu->AddChildToVerticalBox(StartButtonSize);
    StartSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 14.0f));
    StartSlot->SetHorizontalAlignment(HAlign_Fill);

    USizeBox* QuitButtonSize = WidgetTree->ConstructWidget<USizeBox>();
    QuitButtonSize->SetHeightOverride(56.0f);

    QuitButton = WidgetTree->ConstructWidget<UButton>();
    QuitButton->SetBackgroundColor(
        FLinearColor(0.11f, 0.13f, 0.17f, 1.0f)
    );
    QuitButton->AddChild(
        OrakaiMainMenu::CreateButtonLabel(
            WidgetTree,
            FText::FromString(TEXT("QUIT"))
        )
    );
    QuitButton->OnClicked.AddDynamic(
        this,
        &UOrakaiMainMenuWidget::HandleQuitGame
    );
    QuitButtonSize->AddChild(QuitButton);

    UVerticalBoxSlot* QuitSlot = Menu->AddChildToVerticalBox(QuitButtonSize);
    QuitSlot->SetHorizontalAlignment(HAlign_Fill);

    if (IsValid(StartButton))
    {
        StartButton->SetKeyboardFocus();
    }
}

void UOrakaiMainMenuWidget::HandleStartGame()
{
    APlayerController* PlayerController = GetOwningPlayer();

    RemoveFromParent();

    if (!IsValid(PlayerController))
    {
        return;
    }

    PlayerController->SetIgnoreMoveInput(false);
    PlayerController->SetIgnoreLookInput(false);
    PlayerController->SetInputMode(FInputModeGameOnly());
    PlayerController->SetShowMouseCursor(false);
}

void UOrakaiMainMenuWidget::HandleQuitGame()
{
    APlayerController* PlayerController = GetOwningPlayer();

    if (!IsValid(PlayerController))
    {
        return;
    }

    UKismetSystemLibrary::QuitGame(
        this,
        PlayerController,
        EQuitPreference::Quit,
        false
    );
}
