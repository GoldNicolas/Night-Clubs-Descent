#include "GJUserSettings.h"
#include "AudioDevice.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ConfigCacheIni.h"

namespace
{
	const TCHAR* Section = TEXT("GJ.UserSettings");
}

FGJUserSettings& FGJUserSettings::Get()
{
	static FGJUserSettings Settings;
	static bool bLoaded = false;
	if (!bLoaded)
	{
		bLoaded = true;
		if (GConfig)
		{
			GConfig->GetFloat(Section, TEXT("MasterVolume"), Settings.MasterVolume, GGameUserSettingsIni);
			GConfig->GetFloat(Section, TEXT("MusicVolume"), Settings.MusicVolume, GGameUserSettingsIni);
			GConfig->GetFloat(Section, TEXT("MouseSensitivity"), Settings.MouseSensitivity, GGameUserSettingsIni);
			GConfig->GetFloat(Section, TEXT("BestSeconds"), Settings.BestSeconds, GGameUserSettingsIni);
			GConfig->GetInt(Section, TEXT("BestKills"), Settings.BestKills, GGameUserSettingsIni);
		}
		if (GEngine && GEngine->GetGameUserSettings())
		{
			Settings.bFullscreen = GEngine->GetGameUserSettings()->GetFullscreenMode() != EWindowMode::Windowed;
		}
		Settings.MasterVolume = FMath::Clamp(Settings.MasterVolume, 0.f, 1.f);
		Settings.MusicVolume = FMath::Clamp(Settings.MusicVolume, 0.f, 1.f);
		Settings.MouseSensitivity = FMath::Clamp(Settings.MouseSensitivity, MinSensitivity, MaxSensitivity);
	}
	return Settings;
}

void FGJUserSettings::Save() const
{
	if (GConfig)
	{
		GConfig->SetFloat(Section, TEXT("MasterVolume"), MasterVolume, GGameUserSettingsIni);
		GConfig->SetFloat(Section, TEXT("MusicVolume"), MusicVolume, GGameUserSettingsIni);
		GConfig->SetFloat(Section, TEXT("MouseSensitivity"), MouseSensitivity, GGameUserSettingsIni);
		GConfig->SetFloat(Section, TEXT("BestSeconds"), BestSeconds, GGameUserSettingsIni);
		GConfig->SetInt(Section, TEXT("BestKills"), BestKills, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
	if (UGameUserSettings* Game = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		Game->SaveSettings();
	}
}

void FGJUserSettings::Apply(const UObject* WorldContext) const
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (World)
	{
		if (FAudioDeviceHandle Audio = World->GetAudioDevice())
		{
			Audio->SetTransientPrimaryVolume(MasterVolume);
		}
	}
	if (IConsoleVariable* Music = IConsoleManager::Get().FindConsoleVariable(TEXT("gj.MusicVolume")))
	{
		Music->Set(MusicVolume, ECVF_SetByGameSetting);
	}
	// The window only in a real game window (not in the editor's play-in-editor viewport).
	if (UGameUserSettings* Game = GEngine ? GEngine->GetGameUserSettings() : nullptr; Game && !GIsEditor)
	{
		const EWindowMode::Type Wanted = bFullscreen ? EWindowMode::WindowedFullscreen : EWindowMode::Windowed;
		if (Game->GetFullscreenMode() != Wanted)
		{
			Game->SetFullscreenMode(Wanted);
			Game->ApplyResolutionSettings(false);
		}
	}
}
