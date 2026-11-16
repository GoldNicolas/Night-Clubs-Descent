#pragma once

#include "CoreMinimal.h"

/**
 * The player's own settings from the menu: master volume, music volume, mouse sensitivity and fullscreen. Saved in
 * GameUserSettings.ini ([GJ.UserSettings]; fullscreen through UGameUserSettings) and applied at startup by UGJRunSubsystem.
 */
struct GAMEJAM_API FGJUserSettings
{
	float MasterVolume = 1.f;
	float MusicVolume = 0.7f;
	float MouseSensitivity = 1.f;
	bool bFullscreen = true;

	// Records (the results screen).
	float BestSeconds = 0.f;
	int32 BestKills = 0;

	/** The current settings (loaded from disk on first use). */
	static FGJUserSettings& Get();
	/** Writes them to disk. */
	void Save() const;
	/** Pushes them to the audio device, the music (gj.MusicVolume) and the window. Sensitivity is read by the player's aim. */
	void Apply(const UObject* WorldContext) const;

	static constexpr float MinSensitivity = 0.1f;
	static constexpr float MaxSensitivity = 3.f;
};
