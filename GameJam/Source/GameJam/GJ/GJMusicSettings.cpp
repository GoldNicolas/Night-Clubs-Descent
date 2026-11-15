#include "GJMusicSettings.h"
#include "Sound/SoundBase.h"

UGJMusicSettings::UGJMusicSettings()
{
	// The stems Tools/Music/clubsynth renders and Tools/Music/import_music.py imports (MUSIC.md).
	const TPair<EGJMusicRole, const TCHAR*> Defaults[] = {
		{EGJMusicRole::Kick, TEXT("Kick")},
		{EGJMusicRole::Hats, TEXT("Hats")},
		{EGJMusicRole::Perc, TEXT("Perc")},
		{EGJMusicRole::BassA, TEXT("BassA")},
		{EGJMusicRole::BassB, TEXT("BassB")},
		{EGJMusicRole::Sub, TEXT("Sub")},
		{EGJMusicRole::Pad, TEXT("Pad")},
		{EGJMusicRole::Drone, TEXT("Drone")},
		{EGJMusicRole::Arp, TEXT("Arp")},
		{EGJMusicRole::LeadA, TEXT("LeadA")},
		{EGJMusicRole::LeadB, TEXT("LeadB")},
	};
	for (const TPair<EGJMusicRole, const TCHAR*>& D : Defaults)
	{
		FGJMusicStem& Stem = Stems.AddDefaulted_GetRef();
		Stem.Role = D.Key;
		Stem.Sound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(FString::Printf(TEXT("/Game/GJ/Audio/Music/MUS_Club_%s.MUS_Club_%s"), D.Value, D.Value)));
	}
}
