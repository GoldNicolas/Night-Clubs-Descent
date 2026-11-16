#include "GJRunSubsystem.h"
#include "GameJam.h"
#include "GJEncounterSubsystem.h"
#include "GJFrontEndSettings.h"
#include "GJMusicSubsystem.h"
#include "GJPlayerCharacter.h"
#include "EnemyBean.h"
#include "GJUserSettings.h"
#include "Blueprint/UserWidget.h"
#include "Brushes/SlateColorBrush.h"
#include "Camera/CameraActor.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Camera/CameraComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/CommandLine.h"
#include "Styling/CoreStyle.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "GJFrontEnd"

namespace
{
	const FName MenuOrbitTag(TEXT("GJMenuOrbit"));
	const FName IntroTag(TEXT("GJIntro"));

	// The club's palette (MAP_DESIGN §2.1): black, cyan, magenta.
	const FLinearColor Magenta(1.f, 0.10f, 0.55f);
	const FLinearColor Cyan(0.15f, 0.75f, 1.f);
	const FLinearColor Ink(0.92f, 0.92f, 0.95f);

	UGJRunSubsystem* RunOf(UWorld* World) { return World ? World->GetSubsystem<UGJRunSubsystem>() : nullptr; }

	FAutoConsoleCommandWithWorld CmdPlay(TEXT("gj.Play"), TEXT("Front end: leave the main menu and start the intro (as the Play button)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World) { if (UGJRunSubsystem* Run = RunOf(World)) { Run->StartIntro(); } }));
	FAutoConsoleCommandWithWorld CmdSkipIntro(TEXT("gj.SkipIntro"), TEXT("Front end: skip the intro and start the run."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World) { if (UGJRunSubsystem* Run = RunOf(World)) { Run->SkipIntro(); } }));
	FAutoConsoleCommandWithWorld CmdRunInfo(TEXT("gj.RunInfo"), TEXT("Front end: print the run phase."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (const UGJRunSubsystem* Run = RunOf(World))
			{
				UE_LOG(LogGameJam, Display, TEXT("Run: phase %s"), *UEnum::GetValueAsString(Run->GetPhase()));
			}
		}));

	FAutoConsoleCommandWithWorld CmdPause(TEXT("gj.Pause"), TEXT("Front end: pause or resume the run (as Escape / P)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (UGJRunSubsystem* Run = RunOf(World)) { if (Run->IsPaused()) { Run->Resume(); } else { Run->Pause(); } }
		}));
	FAutoConsoleCommandWithWorld CmdRestart(TEXT("gj.Restart"), TEXT("Front end: reload the level straight into a new run (as Restart)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World) { if (UGJRunSubsystem* Run = RunOf(World)) { Run->RestartRun(); } }));
	FAutoConsoleCommandWithWorld CmdMainMenu(TEXT("gj.MainMenu"), TEXT("Front end: reload the level to the title screen (as Main menu)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World) { if (UGJRunSubsystem* Run = RunOf(World)) { Run->ReturnToMainMenu(); } }));
	FAutoConsoleCommandWithWorld CmdKillPlayer(TEXT("gj.KillPlayer"), TEXT("Testing: deal the player lethal damage (ends the run)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (APawn* Player = UGameplayStatics::GetPlayerPawn(World, 0))
			{
				UGameplayStatics::ApplyDamage(Player, 100000.f, nullptr, nullptr, nullptr);
			}
		}));

	// Developer helper for scripted captures: gj.DelayedExec 12 HighResShot 1 (runs the command 12 s of real time later).
	FAutoConsoleCommandWithWorldAndArgs CmdDelayedExec(TEXT("gj.DelayedExec"), TEXT("gj.DelayedExec <seconds> <command...>: run a console command later."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() < 2 || !World) { return; }
			const float Delay = FCString::Atof(*Args[0]);
			const FString Command = FString::Join(TArrayView<const FString>(Args).RightChop(1), TEXT(" "));
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Command](float)
			{
				// In the game world current when it fires (the level may have been reloaded meanwhile), through the player's console
				// so viewport commands (HighResShot) work too.
				UWorld* W = nullptr;
				if (GEngine)
				{
					for (const FWorldContext& Context : GEngine->GetWorldContexts())
					{
						if (Context.World() && (Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE)) { W = Context.World(); break; }
					}
				}
				if (W)
				{
					if (APlayerController* PC = W->GetFirstPlayerController()) { PC->ConsoleCommand(Command); }
					else if (GEngine) { GEngine->Exec(W, *Command); }
				}
				return false;
			}), Delay);
		}));

	float Smooth(float T) { T = FMath::Clamp(T, 0.f, 1.f); return T * T * T * (T * (T * 6.f - 15.f) + 10.f); }

	/** 0 before In0, rising to 1 at In1, 1 until Out0, falling to 0 at Out1. */
	float Window(float T, float In0, float In1, float Out0, float Out1)
	{
		return FMath::Min(Smooth((T - In0) / (In1 - In0)), 1.f - Smooth((T - Out0) / (Out1 - Out0)));
	}

	FSlateFontInfo Font(const char* Style, int32 Size, int32 Spacing = 0)
	{
		FSlateFontInfo Info = FCoreStyle::GetDefaultFontStyle(Style, Size);
		Info.LetterSpacing = Spacing;
		return Info;
	}

	const FButtonStyle& MenuButtonStyle()
	{
		static const FButtonStyle Style = FButtonStyle()
			.SetNormal(FSlateColorBrush(FLinearColor(1.f, 1.f, 1.f, 0.04f)))
			.SetHovered(FSlateColorBrush(FLinearColor(Magenta.R, Magenta.G, Magenta.B, 0.45f)))
			.SetPressed(FSlateColorBrush(FLinearColor(Cyan.R, Cyan.G, Cyan.B, 0.55f)))
			.SetNormalPadding(FMargin(18.f, 10.f))
			.SetPressedPadding(FMargin(18.f, 11.f, 18.f, 9.f));
		return Style;
	}

	const FSlateBrush* WhiteBrush() { return FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")); }
}

// ------------------------------------------------------------------------------------------------------------------- widgets

/** One menu button: a label and what it does. */
struct FGJMenuButton
{
	FText Label;
	TFunction<void()> OnClick;
};

/**
 * The front end's menus: the title screen (a side panel over the club), the pause menu and the results screen (centred, over a
 * dimmed game). A heading, an optional subheading, an optional body (the run's stats), buttons, and the shared settings panel.
 */
class SGJMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SGJMenu) : _bSidePanel(false), _HeadingSize(84), _SubheadingColor(Cyan) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UGJRunSubsystem>, Run)
		SLATE_ARGUMENT(bool, bSidePanel)
		SLATE_ARGUMENT(FText, Heading)
		SLATE_ARGUMENT(int32, HeadingSize)
		SLATE_ARGUMENT(FText, Subheading)
		SLATE_ARGUMENT(FLinearColor, SubheadingColor)
		SLATE_ARGUMENT(TSharedPtr<SWidget>, Body)
		SLATE_ARGUMENT(TArray<FGJMenuButton>, Buttons)
		/** Escape outside the settings panel (the pause menu resumes). */
		SLATE_ARGUMENT(TFunction<void()>, OnEscape)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args)
	{
		Run = Args._Run;
		Buttons = Args._Buttons;
		OnEscape = Args._OnEscape;
		const bool bSide = Args._bSidePanel;
		const EHorizontalAlignment Align = bSide ? HAlign_Left : HAlign_Center;

		TSharedRef<SVerticalBox> Column = SNew(SVerticalBox)
			+ SVerticalBox::Slot().FillHeight(1.f) [ SNullWidget::NullWidget ]
			+ SVerticalBox::Slot().AutoHeight().HAlign(Align)
			[
				SNew(STextBlock).Text(Args._Heading).Font(Font("Bold", Args._HeadingSize, 220)).ColorAndOpacity(Ink)
				.ShadowColorAndOpacity(FLinearColor(Magenta.R, Magenta.G, Magenta.B, 0.8f)).ShadowOffset(FVector2D(4.f, 4.f))
			];
		if (!Args._Subheading.IsEmpty())
		{
			Column->AddSlot().AutoHeight().HAlign(Align).Padding(4.f, 6.f, 0.f, 0.f)
			[
				SNew(STextBlock).Text(Args._Subheading).Font(Font("Italic", 18, 60)).ColorAndOpacity(Args._SubheadingColor)
			];
		}
		if (Args._Body.IsValid())
		{
			Column->AddSlot().AutoHeight().HAlign(Align).Padding(0.f, 28.f, 0.f, 0.f) [ Args._Body.ToSharedRef() ];
		}
		Column->AddSlot().AutoHeight().HAlign(Align).Padding(0.f, 48.f, 0.f, 0.f)
		[
			SAssignNew(Switcher, SWidgetSwitcher)
			+ SWidgetSwitcher::Slot() [ MakeButtons() ]
			+ SWidgetSwitcher::Slot() [ MakeSettingsPanel() ]
		];
		Column->AddSlot().FillHeight(1.2f) [ SNullWidget::NullWidget ];

		if (bSide)
		{
			ChildSlot
			[
				SNew(SBox).HAlign(HAlign_Left)
				[
					SNew(SBorder).BorderImage(WhiteBrush()).BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.02f, 0.62f)).Padding(FMargin(90.f, 0.f, 120.f, 0.f))
					[ Column ]
				]
			];
		}
		else
		{
			ChildSlot
			[
				SNew(SBorder).BorderImage(WhiteBrush()).BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.02f, 0.72f)).HAlign(HAlign_Center)
				[ Column ]
			];
		}
	}

	/** The first button (keyboard focus goes there). */
	TSharedPtr<SButton> FirstButton;

	/** Opens the settings panel (the Settings button). */
	void ShowSettings() { Switcher->SetActiveWidgetIndex(1); }

private:
	TSharedRef<SWidget> MakeButton(const FText& Label, FOnClicked OnClicked, TSharedPtr<SButton>* Out = nullptr)
	{
		TSharedPtr<SButton> Button;
		TSharedRef<SWidget> Box = SNew(SBox).WidthOverride(340.f).Padding(FMargin(0.f, 0.f, 0.f, 10.f))
		[
			SAssignNew(Button, SButton).ButtonStyle(&MenuButtonStyle()).OnClicked(OnClicked)
			[
				SNew(STextBlock).Text(Label).Font(Font("Bold", 24, 160)).ColorAndOpacity(Ink)
			]
		];
		if (Out) { *Out = Button; }
		return Box;
	}

	TSharedRef<SWidget> MakeButtons()
	{
		TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
		for (int32 I = 0; I < Buttons.Num(); ++I)
		{
			TFunction<void()> Click = Buttons[I].OnClick;
			Box->AddSlot().AutoHeight()
			[
				MakeButton(Buttons[I].Label, FOnClicked::CreateLambda([Click]() { if (Click) { Click(); } return FReply::Handled(); }),
					I == 0 ? &FirstButton : nullptr)
			];
		}
		return Box;
	}

	TSharedRef<SWidget> MakeSlider(const FText& Label, TFunction<float()> Get, TFunction<void(float)> Set)
	{
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Text(Label).Font(Font("Regular", 14, 120)).ColorAndOpacity(Cyan)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 18.f)
			[
				SNew(SBox).WidthOverride(340.f)
				[
					SNew(SSlider).Value_Lambda([Get]() { return Get(); }).OnValueChanged_Lambda([Set](float V) { Set(V); })
					.SliderBarColor(FLinearColor(1.f, 1.f, 1.f, 0.25f)).SliderHandleColor(Magenta)
				]
			];
	}

	TSharedRef<SWidget> MakeSettingsPanel()
	{
		auto Changed = [this]() { FGJUserSettings::Get().Apply(Run.Get()); };
		const float MinS = FGJUserSettings::MinSensitivity;
		const float MaxS = FGJUserSettings::MaxSensitivity;
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				MakeSlider(LOCTEXT("Master", "MASTER VOLUME"), []() { return FGJUserSettings::Get().MasterVolume; },
					[Changed](float V) { FGJUserSettings::Get().MasterVolume = V; Changed(); })
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				MakeSlider(LOCTEXT("Music", "MUSIC VOLUME"), []() { return FGJUserSettings::Get().MusicVolume; },
					[Changed](float V) { FGJUserSettings::Get().MusicVolume = V; Changed(); })
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				MakeSlider(LOCTEXT("Sensitivity", "MOUSE SENSITIVITY"),
					[MinS, MaxS]() { return (FGJUserSettings::Get().MouseSensitivity - MinS) / (MaxS - MinS); },
					[MinS, MaxS](float V) { FGJUserSettings::Get().MouseSensitivity = FMath::Lerp(MinS, MaxS, V); })
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 26.f)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([]() { return FGJUserSettings::Get().bFullscreen ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([Changed](ECheckBoxState State) { FGJUserSettings::Get().bFullscreen = State == ECheckBoxState::Checked; Changed(); })
				[
					SNew(STextBlock).Text(LOCTEXT("Fullscreen", "FULLSCREEN")).Font(Font("Regular", 14, 120)).ColorAndOpacity(Cyan)
				]
			]
			+ SVerticalBox::Slot().AutoHeight() [ MakeButton(LOCTEXT("Back", "BACK"), FOnClicked::CreateSP(this, &SGJMenu::OnBack)) ];
	}

	FReply OnBack() { FGJUserSettings::Get().Save(); Switcher->SetActiveWidgetIndex(0); return FReply::Handled(); }

	virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override
	{
		if (Event.GetKey() == EKeys::Escape || Event.GetKey() == EKeys::P)
		{
			if (Switcher->GetActiveWidgetIndex() == 1) { return OnBack(); }
			if (OnEscape) { OnEscape(); return FReply::Handled(); }
		}
		return FReply::Unhandled();
	}
	virtual bool SupportsKeyboardFocus() const override { return true; }

	TWeakObjectPtr<UGJRunSubsystem> Run;
	TArray<FGJMenuButton> Buttons;
	TFunction<void()> OnEscape;
	TSharedPtr<SWidgetSwitcher> Switcher;
};

/** The results screen's numbers: big values over small labels. */
static TSharedRef<SWidget> MakeStats(const TArray<TPair<FText, FText>>& Stats)
{
	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
	for (const TPair<FText, FText>& S : Stats)
	{
		Row->AddSlot().AutoWidth().Padding(28.f, 0.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(STextBlock).Text(S.Value).Font(Font("Bold", 44, 60)).ColorAndOpacity(Ink)
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(STextBlock).Text(S.Key).Font(Font("Regular", 13, 200)).ColorAndOpacity(Cyan)
			]
		];
	}
	return Row;
}

/** The intro's title card and skip hint; any key or click skips. */
class SGJIntroOverlay : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SGJIntroOverlay) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UGJRunSubsystem>, Run)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args)
	{
		Run = Args._Run;
		const UGJFrontEndSettings& Settings = UGJFrontEndSettings::Get();
		auto Alpha = [this](bool bTagline, FLinearColor Color)
		{
			return TAttribute<FSlateColor>::CreateLambda([this, bTagline, Color]()
			{
				const UGJRunSubsystem* R = Run.Get();
				FLinearColor C = Color;
				C.A = R ? (bTagline ? R->GetTaglineAlpha() : R->GetTitleAlpha()) : 0.f;
				return FSlateColor(C);
			});
		};
		ChildSlot
		[
			SNew(SOverlay)
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(STextBlock).Text(Settings.Title).Font(Font("Bold", 110, 300)).ColorAndOpacity(Alpha(false, Ink))
					.ShadowColorAndOpacity_Lambda([this]()
					{
						const UGJRunSubsystem* R = Run.Get();
						return FLinearColor(Magenta.R, Magenta.G, Magenta.B, R ? 0.8f * R->GetTitleAlpha() : 0.f);
					})
					.ShadowOffset(FVector2D(5.f, 5.f))
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 10.f, 0.f, 0.f)
				[
					SNew(STextBlock).Text(Settings.Tagline).Font(Font("Italic", 22, 80)).ColorAndOpacity(Alpha(true, Cyan))
				]
			]
			+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(32.f)
			[
				SNew(STextBlock).Text(LOCTEXT("Skip", "PRESS ANY KEY TO SKIP")).Font(Font("Regular", 12, 140))
				.ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.45f))
			]
		];
	}

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry&, const FKeyEvent&) override { return Skip(); }
	virtual FReply OnMouseButtonDown(const FGeometry&, const FPointerEvent&) override { return Skip(); }

private:
	FReply Skip() { if (UGJRunSubsystem* R = Run.Get()) { R->SkipIntro(); } return FReply::Handled(); }
	TWeakObjectPtr<UGJRunSubsystem> Run;
};

// ------------------------------------------------------------------------------------------------------------------- subsystem

bool UGJRunSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

TStatId UGJRunSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGJRunSubsystem, STATGROUP_Tickables);
}

bool UGJRunSubsystem::IsTickable() const
{
	const UWorld* World = GetWorld();
	return World && World->HasBegunPlay() && !World->bIsTearingDown && (bFrontEnd || Phase != EGJRunPhase::Playing);
}

bool UGJRunSubsystem::HidesGameHud(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGJRunSubsystem* Run = World ? World->GetSubsystem<UGJRunSubsystem>() : nullptr;
	return Run && (Run->Phase == EGJRunPhase::Waiting || Run->Phase == EGJRunPhase::Menu || Run->Phase == EGJRunPhase::Intro
		|| (Run->Phase == EGJRunPhase::Dead && Run->bResultsShown));
}

bool UGJRunSubsystem::EndsRunOnDeath(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGJRunSubsystem* Run = World ? World->GetSubsystem<UGJRunSubsystem>() : nullptr;
	return Run && Run->bFrontEnd;
}

bool UGJRunSubsystem::IsPlaying(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGJRunSubsystem* Run = World ? World->GetSubsystem<UGJRunSubsystem>() : nullptr;
	return !Run || Run->Phase == EGJRunPhase::Playing;
}

void UGJRunSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	FGJUserSettings::Get().Apply(&InWorld);
	for (TActorIterator<AActor> It(&InWorld); It; ++It)
	{
		if (It->ActorHasTag(MenuOrbitTag)) { OrbitMarker = *It; break; }
	}
	bFrontEnd = UGJFrontEndSettings::Get().bEnabled && OrbitMarker.IsValid() && !FParse::Param(FCommandLine::Get(), TEXT("nomenu"));
	// Restart reloads the level with ?GJStart=play: straight into a new run, no menu or intro.
	bSkipToPlay = InWorld.URL.HasOption(TEXT("GJStart")) && FCString::Stricmp(InWorld.URL.GetOption(TEXT("GJStart="), TEXT("")), TEXT("play")) == 0;
	if (!bFrontEnd)
	{
		SetPhase(EGJRunPhase::Playing);
		RunStartTime = InWorld.GetTimeSeconds();
	}
	EnemyDiedHandle = AEnemyBean::OnAnyEnemyDied.AddUObject(this, &UGJRunSubsystem::OnEnemyDied);
}

void UGJRunSubsystem::OnEnemyDied(AEnemyBean* Enemy)
{
	if (Enemy && Enemy->GetWorld() == GetWorld() && Phase == EGJRunPhase::Playing) { ++Kills; }
}

void UGJRunSubsystem::Deinitialize()
{
	AEnemyBean::OnAnyEnemyDied.Remove(EnemyDiedHandle);
	if (bPaused && GetWorld()) { UGameplayStatics::SetGamePaused(GetWorld(), false); }
	RemoveWidget();
	SetDebugMessagesHidden(false);
	Super::Deinitialize();
}

void UGJRunSubsystem::SetPhase(EGJRunPhase NewPhase)
{
	Phase = NewPhase;
	PhaseTime = 0.f;
	UE_LOG(LogGameJam, Log, TEXT("Run: %s"), *UEnum::GetValueAsString(NewPhase));
}

void UGJRunSubsystem::Tick(float DeltaTime)
{
	PhaseTime += DeltaTime;
	switch (Phase)
	{
	case EGJRunPhase::Waiting:
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0); PC && PC->GetPawn())
		{
			if (bSkipToPlay) { EnterPlaying(); }
			else { EnterMenu(PC); }
		}
		break;
	case EGJRunPhase::Menu: TickMenu(DeltaTime); break;
	case EGJRunPhase::Intro: TickIntro(DeltaTime); break;
	case EGJRunPhase::Playing: TickPlaying(); break;
	case EGJRunPhase::Dead:
		if (!bResultsShown && PhaseTime >= ResultsDelaySeconds) { ShowResults(); }
		break;
	}
	if (Phase == EGJRunPhase::Menu || Phase == EGJRunPhase::Intro)
	{
		SetGameWidgetsHidden(true); // widgets the template adds after possession, too
	}
	if (Phase == EGJRunPhase::Menu || Phase == EGJRunPhase::Intro)
	{
		SetPlayerHidden(true); // again each frame: the weapon is attached after possession
	}
}

ACameraActor* UGJRunSubsystem::EnsureCamera()
{
	if (!Camera)
	{
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		Camera = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, Params);
		Camera->GetCameraComponent()->SetConstraintAspectRatio(false);
		Camera->GetCameraComponent()->SetFieldOfView(90.f);
	}
	return Camera;
}

void UGJRunSubsystem::EnterMenu(APlayerController* PC)
{
	SetPhase(EGJRunPhase::Menu);
	OrbitAngle = 0.f;
	TickMenu(0.f);
	PC->SetViewTarget(EnsureCamera());
	SetMusicMuffled(true);
	const UGJFrontEndSettings& S = UGJFrontEndSettings::Get();
	ShowMenu(/*bSidePanel*/ true, S.Title, 84, S.Tagline, Cyan, nullptr, {
		{ LOCTEXT("Play", "PLAY"), [this]() { StartIntro(); } },
		{ LOCTEXT("Settings", "SETTINGS"), nullptr }, // opens the settings panel (ShowMenu wires it)
		{ LOCTEXT("Quit", "QUIT"), [this]() { QuitGame(); } },
	}, nullptr);
	if (bPlayRequested) { StartIntro(); }
}

void UGJRunSubsystem::TickMenu(float DeltaTime)
{
	const AActor* Marker = OrbitMarker.Get();
	if (!Marker) { return; }
	const UGJFrontEndSettings& S = UGJFrontEndSettings::Get();
	OrbitAngle = FMath::Fmod(OrbitAngle + DeltaTime * 2.f * PI / FMath::Max(10.f, S.MenuOrbitSeconds), 2.f * PI);
	const FVector Center = Marker->GetActorLocation();
	const FVector Eye = Center + FVector(FMath::Cos(OrbitAngle) * S.MenuOrbitRadiusCm, FMath::Sin(OrbitAngle) * S.MenuOrbitRadiusCm, S.MenuOrbitHeightCm);
	const FVector Look = Center + FVector(0.f, 0.f, 120.f);
	EnsureCamera()->SetActorLocationAndRotation(Eye, (Look - Eye).Rotation());
}

void UGJRunSubsystem::StartIntro()
{
	if (Phase == EGJRunPhase::Waiting) { bPlayRequested = true; return; } // gj.Play before the menu is up (e.g. -ExecCmds)
	if (Phase != EGJRunPhase::Menu) { return; }
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC || !BuildIntroPath(PC))
	{
		EnterPlaying();
		return;
	}
	FGJUserSettings::Get().Save();
	SetPhase(EGJRunPhase::Intro);
	SetMusicMuffled(false); // the music opens up as the descent begins
	TickIntro(0.f);
	ShowWidget(SNew(SGJIntroOverlay).Run(this), /*bShowCursor*/ false);
}

bool UGJRunSubsystem::BuildIntroPath(APlayerController* PC)
{
	// The level's GJIntro markers in order (each also carries its number as a tag), then the player's eyes.
	TArray<TPair<int32, FTransform>> Marks;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (!It->ActorHasTag(IntroTag)) { continue; }
		int32 Order = 0;
		for (const FName& Tag : It->Tags)
		{
			if (Tag.ToString().IsNumeric()) { Order = FCString::Atoi(*Tag.ToString()); }
		}
		Marks.Emplace(Order, It->GetActorTransform());
	}
	Marks.Sort([](const TPair<int32, FTransform>& A, const TPair<int32, FTransform>& B) { return A.Key < B.Key; });
	const APawn* Pawn = PC->GetPawn();
	if (Marks.IsEmpty() || !Pawn)
	{
		UE_LOG(LogGameJam, Warning, TEXT("Run: no %s markers (or no pawn); starting the run without the intro"), *IntroTag.ToString());
		return false;
	}
	IntroKeys.Reset();
	for (const TPair<int32, FTransform>& M : Marks) { IntroKeys.Add(FTransform(M.Value.GetRotation(), M.Value.GetLocation())); }
	IntroKeys.Add(FTransform(PC->GetControlRotation().Quaternion(), Pawn->GetPawnViewLocation()));
	IntroDistance.Reset();
	float D = 0.f;
	for (int32 I = 0; I < IntroKeys.Num(); ++I)
	{
		if (I > 0) { D += FVector::Dist(IntroKeys[I - 1].GetLocation(), IntroKeys[I].GetLocation()); }
		IntroDistance.Add(D);
	}
	return true;
}

FTransform UGJRunSubsystem::SampleIntro(float Alpha) const
{
	// Ease in and out over the whole path, at constant speed along it in between; Catmull-Rom through the markers.
	const float Total = IntroDistance.Last();
	if (IntroKeys.Num() < 2 || Total <= KINDA_SMALL_NUMBER) { return IntroKeys.Last(); }
	const float S = Smooth(Alpha) * Total;
	int32 I = 0;
	while (I < IntroKeys.Num() - 2 && IntroDistance[I + 1] < S) { ++I; }
	const float Len = FMath::Max(KINDA_SMALL_NUMBER, IntroDistance[I + 1] - IntroDistance[I]);
	const float T = FMath::Clamp((S - IntroDistance[I]) / Len, 0.f, 1.f);
	const FVector P0 = IntroKeys[FMath::Max(0, I - 1)].GetLocation();
	const FVector P1 = IntroKeys[I].GetLocation();
	const FVector P2 = IntroKeys[I + 1].GetLocation();
	const FVector P3 = IntroKeys[FMath::Min(IntroKeys.Num() - 1, I + 2)].GetLocation();
	const float T2 = T * T;
	const float T3 = T2 * T;
	const FVector P = 0.5f * ((2.f * P1) + (-P0 + P2) * T + (2.f * P0 - 5.f * P1 + 4.f * P2 - P3) * T2 + (-P0 + 3.f * P1 - 3.f * P2 + P3) * T3);
	const FQuat Q = FQuat::Slerp(IntroKeys[I].GetRotation(), IntroKeys[I + 1].GetRotation(), Smooth(T));
	return FTransform(Q, P);
}

void UGJRunSubsystem::TickIntro(float DeltaTime)
{
	const float Seconds = FMath::Max(2.f, UGJFrontEndSettings::Get().IntroSeconds);
	const float Alpha = FMath::Clamp(PhaseTime / Seconds, 0.f, 1.f);
	const FTransform View = SampleIntro(Alpha);
	EnsureCamera()->SetActorLocationAndRotation(View.GetLocation(), View.GetRotation());
	if (PhaseTime >= Seconds) { EnterPlaying(); }
}

float UGJRunSubsystem::GetTitleAlpha() const
{
	if (Phase != EGJRunPhase::Intro) { return 0.f; }
	const float T = PhaseTime / FMath::Max(2.f, UGJFrontEndSettings::Get().IntroSeconds);
	return Window(T, 0.10f, 0.25f, 0.70f, 0.85f);
}

float UGJRunSubsystem::GetTaglineAlpha() const
{
	if (Phase != EGJRunPhase::Intro) { return 0.f; }
	const float T = PhaseTime / FMath::Max(2.f, UGJFrontEndSettings::Get().IntroSeconds);
	return Window(T, 0.20f, 0.35f, 0.70f, 0.85f);
}

void UGJRunSubsystem::SkipIntro()
{
	if (Phase == EGJRunPhase::Intro || Phase == EGJRunPhase::Menu) { EnterPlaying(); }
}

void UGJRunSubsystem::EnterPlaying()
{
	RemoveWidget();
	SetMusicMuffled(false);
	SetGameWidgetsHidden(false);
	SetPlayerHidden(false);
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			PC->SetViewTargetWithBlend(Pawn, Phase == EGJRunPhase::Intro ? UGJFrontEndSettings::Get().IntroBlendSeconds : 0.f, VTBlend_EaseInOut, 2.f);
		}
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
	SetPhase(EGJRunPhase::Playing);
	RunStartTime = GetWorld()->GetTimeSeconds();
	Kills = 0;
	// Encounter timers start from now: the first enemy arrives one spawn interval into the run, not the moment the menu closes.
	if (UGJEncounterSubsystem* Encounter = GetWorld()->GetSubsystem<UGJEncounterSubsystem>()) { Encounter->KillAll(); }
}

void UGJRunSubsystem::TickPlaying()
{
	if (!bFrontEnd) { return; }
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	AGJPlayerCharacter* Player = PC ? Cast<AGJPlayerCharacter>(PC->GetPawn()) : nullptr;
	if (Player && Player->IsDead())
	{
		EnterDead(Player);
		return;
	}
	// Escape, or P (in the editor's play-in-editor, Escape stops the session before the game sees it).
	if (!bPaused && PC && (PC->WasInputKeyJustPressed(EKeys::Escape) || PC->WasInputKeyJustPressed(EKeys::P)))
	{
		Pause();
	}
}

float UGJRunSubsystem::GetRunSeconds() const
{
	if (Phase == EGJRunPhase::Dead) { return FinalSeconds; }
	return Phase == EGJRunPhase::Playing && GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds() - RunStartTime) : 0.f;
}

int32 UGJRunSubsystem::GetWave() const
{
	const UGJEncounterSubsystem* Encounter = GetWorld() ? GetWorld()->GetSubsystem<UGJEncounterSubsystem>() : nullptr;
	return Encounter ? Encounter->GetCurrentWave() : 1;
}

FText UGJRunSubsystem::FormatTime(float Seconds)
{
	const int32 Total = FMath::FloorToInt(FMath::Max(0.f, Seconds));
	return FText::FromString(FString::Printf(TEXT("%d:%02d"), Total / 60, Total % 60));
}

void UGJRunSubsystem::Pause()
{
	if (Phase != EGJRunPhase::Playing || bPaused) { return; }
	bPaused = true;
	UGameplayStatics::SetGamePaused(GetWorld(), true);
	SetMusicMuffled(true);
	const FText Sub = FText::Format(LOCTEXT("PauseSub", "{0}   \u00B7   {1} KILLS   \u00B7   WAVE {2}"),
		FormatTime(GetRunSeconds()), FText::AsNumber(Kills), FText::AsNumber(GetWave()));
	ShowMenu(/*bSidePanel*/ false, LOCTEXT("Paused", "PAUSED"), 72, Sub, Cyan, nullptr, {
		{ LOCTEXT("Resume", "RESUME"), [this]() { Resume(); } },
		{ LOCTEXT("Settings", "SETTINGS"), nullptr },
		{ LOCTEXT("Restart", "RESTART"), [this]() { RestartRun(); } },
		{ LOCTEXT("MainMenu", "MAIN MENU"), [this]() { ReturnToMainMenu(); } },
		{ LOCTEXT("Quit", "QUIT"), [this]() { QuitGame(); } },
	}, [this]() { Resume(); });
	UE_LOG(LogGameJam, Log, TEXT("Run: paused"));
}

void UGJRunSubsystem::Resume()
{
	if (!bPaused) { return; }
	bPaused = false;
	FGJUserSettings::Get().Save();
	RemoveWidget();
	UGameplayStatics::SetGamePaused(GetWorld(), false);
	SetMusicMuffled(false);
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
	UE_LOG(LogGameJam, Log, TEXT("Run: resumed"));
}

void UGJRunSubsystem::EnterDead(AGJPlayerCharacter* Player)
{
	FinalSeconds = GetRunSeconds();
	FinalWave = GetWave();
	SetPhase(EGJRunPhase::Dead);
	bResultsShown = false;
	// The run is over: no respawn (the template respawns the player after a few seconds).
	Player->CancelRespawn();
	FGJUserSettings& Records = FGJUserSettings::Get();
	bNewBest = FinalSeconds > Records.BestSeconds;
	if (bNewBest) { Records.BestSeconds = FinalSeconds; }
	Records.BestKills = FMath::Max(Records.BestKills, Kills);
	Records.Save();
	UE_LOG(LogGameJam, Display, TEXT("Run: died after %.1f s, %d kills, wave %d%s"), FinalSeconds, Kills, FinalWave, bNewBest ? TEXT(" (new best)") : TEXT(""));
}

void UGJRunSubsystem::ShowResults()
{
	bResultsShown = true;
	SetDebugMessagesHidden(true);
	SetGameWidgetsHidden(true); // the template's health bar and ammo counter (a restart or the main menu reloads the level)
	const FGJUserSettings& Records = FGJUserSettings::Get();
	const FText Sub = bNewBest ? LOCTEXT("NewBest", "NEW BEST TIME")
		: FText::Format(LOCTEXT("Best", "BEST  {0}   \u00B7   MOST KILLS  {1}"), FormatTime(Records.BestSeconds), FText::AsNumber(Records.BestKills));
	const TSharedRef<SWidget> Stats = MakeStats({
		{ LOCTEXT("Time", "TIME SURVIVED"), FormatTime(FinalSeconds) },
		{ LOCTEXT("Kills", "KILLS"), FText::AsNumber(Kills) },
		{ LOCTEXT("Wave", "WAVE"), FText::AsNumber(FinalWave) },
	});
	ShowMenu(/*bSidePanel*/ false, LOCTEXT("Dead", "YOU DIED"), 96, Sub, bNewBest ? Magenta : Cyan, Stats, {
		{ LOCTEXT("Restart", "RESTART"), [this]() { RestartRun(); } },
		{ LOCTEXT("MainMenu", "MAIN MENU"), [this]() { ReturnToMainMenu(); } },
		{ LOCTEXT("Quit", "QUIT"), [this]() { QuitGame(); } },
	}, nullptr);
	UE_LOG(LogGameJam, Log, TEXT("Run: results shown"));
}

void UGJRunSubsystem::RestartRun()
{
	TravelToLevel(TEXT("GJStart=play"));
}

void UGJRunSubsystem::ReturnToMainMenu()
{
	TravelToLevel(TEXT(""));
}

void UGJRunSubsystem::TravelToLevel(const FString& Options)
{
	UWorld* World = GetWorld();
	FGJUserSettings::Get().Save();
	if (bPaused) { UGameplayStatics::SetGamePaused(World, false); bPaused = false; }
	RemoveWidget();
	// A fresh load of the same level: new tunnel seeds, new encounter, new run.
	UE_LOG(LogGameJam, Log, TEXT("Run: reloading the level (%s)"), Options.IsEmpty() ? TEXT("main menu") : *Options);
	UGameplayStatics::OpenLevel(World, FName(*UGameplayStatics::GetCurrentLevelName(World, /*bRemovePrefixString*/ true)), true, Options);
}

void UGJRunSubsystem::ShowMenu(bool bSidePanel, const FText& Heading, int32 HeadingSize, const FText& Subheading, const FLinearColor& SubColor,
	TSharedPtr<SWidget> Body, TArray<FGJMenuButton> Buttons, TFunction<void()> OnEscape)
{
	// A button with no action opens the settings panel.
	TSharedRef<TWeakPtr<SGJMenu>> Self = MakeShared<TWeakPtr<SGJMenu>>();
	for (FGJMenuButton& Button : Buttons)
	{
		if (!Button.OnClick) { Button.OnClick = [Self]() { if (TSharedPtr<SGJMenu> M = Self->Pin()) { M->ShowSettings(); } }; }
	}
	TSharedRef<SGJMenu> Menu = SNew(SGJMenu).Run(this).bSidePanel(bSidePanel).Heading(Heading).HeadingSize(HeadingSize)
		.Subheading(Subheading).SubheadingColor(SubColor).Body(Body).Buttons(Buttons).OnEscape(OnEscape);
	*Self = Menu;
	ShowWidget(Menu, /*bShowCursor*/ true);
	if (FSlateApplication::IsInitialized() && Menu->FirstButton.IsValid())
	{
		FSlateApplication::Get().SetKeyboardFocus(Menu->FirstButton);
	}
}

void UGJRunSubsystem::QuitGame()
{
	FGJUserSettings::Get().Save();
	UKismetSystemLibrary::QuitGame(GetWorld(), UGameplayStatics::GetPlayerController(GetWorld(), 0), EQuitPreference::Quit, false);
}

void UGJRunSubsystem::ShowWidget(const TSharedRef<SWidget>& NewWidget, bool bShowCursor)
{
	RemoveWidget();
	UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!Viewport || !PC) { return; } // headless
	Widget = NewWidget;
	Viewport->AddViewportWidgetContent(NewWidget, 100);
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(NewWidget);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(Mode);
	PC->SetShowMouseCursor(bShowCursor);
}

void UGJRunSubsystem::RemoveWidget()
{
	if (Widget.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			if (UGameViewportClient* Viewport = World->GetGameViewport()) { Viewport->RemoveViewportWidgetContent(Widget.ToSharedRef()); }
		}
		Widget.Reset();
	}
}

void UGJRunSubsystem::SetGameWidgetsHidden(bool bHidden)
{
	if (bHidden)
	{
		for (TObjectIterator<UUserWidget> It; It; ++It)
		{
			if (It->GetWorld() == GetWorld() && It->IsInViewport() && It->GetVisibility() != ESlateVisibility::Hidden)
			{
				HiddenWidgets.Add(*It, It->GetVisibility());
				It->SetVisibility(ESlateVisibility::Hidden);
			}
		}
		return;
	}
	for (const TPair<TWeakObjectPtr<UUserWidget>, ESlateVisibility>& W : HiddenWidgets)
	{
		if (UUserWidget* Live = W.Key.Get()) { Live->SetVisibility(W.Value); }
	}
	HiddenWidgets.Reset();
}

void UGJRunSubsystem::SetPlayerHidden(bool bHidden)
{
	// The intro camera flies into the player's eyes: their body and rifle would be in the shot. They come back when the run starts.
	SetDebugMessagesHidden(bHidden);
	bPlayerHidden = bHidden;
	APawn* Pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!Pawn) { return; }
	TArray<AActor*> Attached;
	Pawn->GetAttachedActors(Attached, /*bResetArray*/ true, /*bRecursivelyIncludeAttachedActors*/ true);
	Attached.Add(Pawn);
	for (AActor* Actor : Attached)
	{
		if (Actor && Actor->IsHidden() != bHidden) { Actor->SetActorHiddenInGame(bHidden); }
	}
}

void UGJRunSubsystem::SetDebugMessagesHidden(bool bHidden)
{
	if (!GEngine || bHidden == bDebugMessagesHidden) { return; }
	if (bHidden) { bDebugMessagesWere = GEngine->bEnableOnScreenDebugMessages; GEngine->bEnableOnScreenDebugMessages = false; }
	else { GEngine->bEnableOnScreenDebugMessages = bDebugMessagesWere; }
	bDebugMessagesHidden = bHidden;
}

void UGJRunSubsystem::SetMusicMuffled(bool bMuffled)
{
	if (UGJMusicSubsystem* Music = GetWorld()->GetSubsystem<UGJMusicSubsystem>())
	{
		Music->SetMenuCutoff(bMuffled ? UGJFrontEndSettings::Get().MenuMusicCutoffHz : 0.f);
	}
}

#undef LOCTEXT_NAMESPACE
