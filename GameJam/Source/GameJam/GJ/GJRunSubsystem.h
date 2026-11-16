#pragma once

#include "CoreMinimal.h"
#include "Components/SlateWrapperTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "GJRunSubsystem.generated.h"

class ACameraActor;
class AEnemyBean;
class AGJPlayerCharacter;
class APlayerController;
class SGJMenu;
class SWidget;
class UUserWidget;
struct FGJMenuButton;

UENUM()
enum class EGJRunPhase : uint8
{
	/** Waiting for the player controller and pawn. */
	Waiting,
	/** Title screen: the camera circles the dance hall, the music is muffled, nothing spawns. */
	Menu,
	/** The starting sequence: the camera descends from the drains to the player's eyes under the title card. */
	Intro,
	/** The run: the player has control, enemies spawn, the HUD draws. Escape (or P) pauses it. */
	Playing,
	/** The player died: the death view plays, then the results screen (Restart / Main menu / Quit). The run is over. */
	Dead,
};

/**
 * Front end and run state (FRONTEND.md). In a level with a menu camera marker (an actor tagged GJMenuOrbit, as in Lvl_Club) the
 * game opens on the main menu (Play / Settings / Quit) over a slow orbit of the club. Play starts the intro: a skippable camera
 * descent through the actors tagged GJIntro (+ an order number) into the player's view. Then the run begins.
 * Levels without the marker (or with -nomenu on the command line) go straight to Playing, as before.
 * Gameplay systems ask IsPlaying(): the encounter spawns nothing and the HUD draws nothing before the run starts.
 * Presentation only: nothing here touches agents.
 * During the run, Escape or P pauses the game (Resume / Settings / Restart / Main menu / Quit). When the player dies the run ends
 * (no respawn) and a results screen shows the time survived, kills and wave, and the best run. Restart reloads the level straight
 * into a new run (?GJStart=play); Main menu reloads it to the title screen.
 * Console: gj.Play (menu -> intro), gj.SkipIntro, gj.RunInfo.
 */
UCLASS()
class GAMEJAM_API UGJRunSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

	/** True while the run is on (not before it starts, not after death), or always in a world without a front end. */
	static bool IsPlaying(const UObject* WorldContext);
	/** True while the title screen, the intro or the results screen owns the screen (the HUD stays hidden). */
	static bool HidesGameHud(const UObject* WorldContext);
	/** True where death ends the run with the results screen (levels with the front end); elsewhere the player respawns. */
	static bool EndsRunOnDeath(const UObject* WorldContext);

	EGJRunPhase GetPhase() const { return Phase; }
	/** Menu -> intro (the Play button). */
	void StartIntro();
	/** Intro -> playing at once. */
	void SkipIntro();
	void QuitGame();
	void Pause();
	void Resume();
	/** Reloads the level straight into a new run. */
	void RestartRun();
	/** Reloads the level to the title screen. */
	void ReturnToMainMenu();
	bool IsPaused() const { return bPaused; }
	float GetRunSeconds() const;
	int32 GetKills() const { return Kills; }

	/** Title card opacity during the intro (0-1). */
	float GetTitleAlpha() const;
	float GetTaglineAlpha() const;

private:
	void EnterMenu(APlayerController* PC);
	void EnterPlaying();
	void TickPlaying();
	void EnterDead(AGJPlayerCharacter* Player);
	void ShowResults();
	void TravelToLevel(const FString& Options);
	void OnEnemyDied(AEnemyBean* Enemy);
	int32 GetWave() const;
	static FText FormatTime(float Seconds);
	/** Shows one of the front end's menus (a button with no action opens the settings panel). */
	void ShowMenu(bool bSidePanel, const FText& Heading, int32 HeadingSize, const FText& Subheading, const FLinearColor& SubColor,
		TSharedPtr<SWidget> Body, TArray<FGJMenuButton> Buttons, TFunction<void()> OnEscape);
	void TickMenu(float DeltaTime);
	void TickIntro(float DeltaTime);
	bool BuildIntroPath(APlayerController* PC);
	FTransform SampleIntro(float Alpha) const;
	ACameraActor* EnsureCamera();
	void ShowWidget(const TSharedRef<SWidget>& NewWidget, bool bShowCursor);
	void RemoveWidget();
	/** Hides the template's UMG widgets (ammo counter etc.) until the run starts; restores them after. */
	void SetGameWidgetsHidden(bool bHidden);
	void SetMusicMuffled(bool bMuffled);
	/** Hides the player's body and weapon (and the engine's on-screen debug text) under the menu and during the intro. */
	void SetPlayerHidden(bool bHidden);
	/** Hides the engine's on-screen debug text (the agent runtime's status line) while a front-end screen is up. */
	void SetDebugMessagesHidden(bool bHidden);
	void SetPhase(EGJRunPhase NewPhase);

	EGJRunPhase Phase = EGJRunPhase::Waiting;
	bool bFrontEnd = false;
	bool bPlayRequested = false;
	bool bSkipToPlay = false;
	bool bPaused = false;
	bool bResultsShown = false;
	bool bNewBest = false;
	double RunStartTime = 0.0;
	int32 Kills = 0;
	float FinalSeconds = 0.f;
	int32 FinalWave = 1;
	/** The death view plays this long before the results screen. */
	static constexpr float ResultsDelaySeconds = 2.5f;
	FDelegateHandle EnemyDiedHandle;
	bool bPlayerHidden = false;
	bool bDebugMessagesHidden = false;
	bool bDebugMessagesWere = true;
	float PhaseTime = 0.f;
	float OrbitAngle = 0.f;

	UPROPERTY() TObjectPtr<ACameraActor> Camera;
	TWeakObjectPtr<AActor> OrbitMarker;
	TArray<FTransform> IntroKeys;
	TArray<float> IntroDistance;

	TSharedPtr<SWidget> Widget;
	/** Template widgets hidden until the run starts, with the visibility to restore. */
	TMap<TWeakObjectPtr<UUserWidget>, ESlateVisibility> HiddenWidgets;
};
