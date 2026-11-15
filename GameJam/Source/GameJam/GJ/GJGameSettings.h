#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GJGameSettings.generated.h"

/**
 * Project Settings > Game > GameJam (saved to Config/DefaultGame.ini): the looks of the player and the crowd, and how the crowd
 * reacts to gunfire. Everything here is presentation: none of it reaches the agents (their senses and mechanics are the contract's).
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="GameJam"))
class GAMEJAM_API UGJGameSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UGJGameSettings();

	static const UGJGameSettings& Get() { return *GetDefault<UGJGameSettings>(); }

	/** The player's body, first-person arms and third-person shadow alike (SK_Mannequin skeleton). Empty = the Blueprint's mesh. */
	UPROPERTY(Config, EditAnywhere, Category="Player", meta=(AllowedClasses="/Script/Engine.SkeletalMesh"))
	FSoftObjectPath PlayerSkin;

	/** The mirror rifle's model: a StaticMesh or a SkeletalMesh. Empty = the template rifle. The stats never change with the model. */
	UPROPERTY(Config, EditAnywhere, Category="Player", meta=(AllowedClasses="/Script/Engine.StaticMesh,/Script/Engine.SkeletalMesh"))
	FSoftObjectPath PlayerWeapon;

	/** The gun model's offset from the hand socket (grip alignment). */
	UPROPERTY(Config, EditAnywhere, Category="Player")
	FTransform PlayerWeaponOffset;

	/** Optional animations of the gun model itself (its own skeleton): one bolt cycle per shot, and the magazine change.
	 *  They are time-scaled to the rifle's cadence and reload time, so they never change the stats. */
	UPROPERTY(Config, EditAnywhere, Category="Player", meta=(AllowedClasses="/Script/Engine.AnimSequence"))
	FSoftObjectPath PlayerWeaponFireAnim;
	UPROPERTY(Config, EditAnywhere, Category="Player", meta=(AllowedClasses="/Script/Engine.AnimSequence"))
	FSoftObjectPath PlayerWeaponReloadAnim;

	/** Club-goer skins (SK_Mannequin skeleton, feet at the origin); one at random per dancer. */
	UPROPERTY(Config, EditAnywhere, Category="Crowd", meta=(AllowedClasses="/Script/Engine.SkeletalMesh"))
	TArray<FSoftObjectPath> DancerSkins;

	/** Looping dance clips (AnimSequences on SK_Mannequin); one at random per dancer. Empty = dancers stand idle. */
	UPROPERTY(Config, EditAnywhere, Category="Crowd", meta=(AllowedClasses="/Script/Engine.AnimSequence"))
	TArray<FSoftObjectPath> DanceAnimations;

	/** Locomotion AnimBlueprint used while fleeing (its run, matched to the speed) and cowering (its idle); any SK_Mannequin
	 *  locomotion ABP. */
	UPROPERTY(Config, EditAnywhere, Category="Crowd", meta=(MetaClass="/Script/Engine.AnimInstance"))
	FSoftClassPath FleeAnimClass;

	/** The crowd dances around the first actor of this class in the level (the club's mirror ball in Lvl_ClubMap); on a
	 *  level without one, around the PlayerStart. */
	UPROPERTY(Config, EditAnywhere, Category="Crowd", meta=(MetaClass="/Script/Engine.Actor"))
	FSoftClassPath CrowdAnchorClass;

	/** Panic spreads: a calm dancer this close to a fleeing one starts running too. */
	UPROPERTY(Config, EditAnywhere, Category="Crowd", meta=(Units="cm"))
	float PanicSpreadRadiusCm = 500.f;

	/** Arms reload (AnimSequence on SK_Mannequin) played on the first-person arms through PlayerArmsSlot, stretched to the reload time. */
	UPROPERTY(Config, EditAnywhere, Category="Player", meta=(AllowedClasses="/Script/Engine.AnimSequence"))
	FSoftObjectPath PlayerReloadAnim;

	/** Material for the first-person body (every slot): the skin's look, minus the near sleeve. M_RodinCharacter_FP dithers out
	 *  the left sleeve and shoulder that sit a hand's width from the eye (parameters FadeNear / FadeFar in first-person depth,
	 *  SleeveMinX / SleeveMinZ in the bind pose); the gloves, the right arm and the gun stay. Empty = the skin's own material. */
	UPROPERTY(Config, EditAnywhere, Category="Player", meta=(AllowedClasses="/Script/Engine.MaterialInterface"))
	FSoftObjectPath PlayerFirstPersonMaterial;

	/** Montage slot of the first-person AnimBP (the template's ABP_FP_Weapon has 'Arms'). */
	UPROPERTY(Config, EditAnywhere, Category="Player")
	FName PlayerArmsSlot = TEXT("Arms");

	/** Weapon effects, player and enemies alike (presentation only: nothing here blocks bullets or agent perception). */
	UPROPERTY(Config, EditAnywhere, Category="Effects", meta=(AllowedClasses="/Script/Niagara.NiagaraSystem"))
	FSoftObjectPath MuzzleFlash;
	UPROPERTY(Config, EditAnywhere, Category="Effects", meta=(AllowedClasses="/Script/Niagara.NiagaraSystem"))
	FSoftObjectPath ShellEject;
	UPROPERTY(Config, EditAnywhere, Category="Effects", meta=(AllowedClasses="/Script/Niagara.NiagaraSystem"))
	FSoftObjectPath ImpactSparks;
	UPROPERTY(Config, EditAnywhere, Category="Effects", meta=(AllowedClasses="/Script/Engine.MaterialInterface"))
	FSoftObjectPath BulletHoleDecal;
	UPROPERTY(Config, EditAnywhere, Category="Effects", meta=(AllowedClasses="/Script/Engine.MaterialInterface"))
	FSoftObjectPath TracerMaterial;
	UPROPERTY(Config, EditAnywhere, Category="Effects", meta=(AllowedClasses="/Script/Engine.SoundBase"))
	FSoftObjectPath GunshotSound;
	UPROPERTY(Config, EditAnywhere, Category="Effects", meta=(AllowedClasses="/Script/Engine.SoundAttenuation"))
	FSoftObjectPath GunshotAttenuation;

	/** The player's bullet hitting an enemy: the exit spray lands on the floor just behind the target (a floor-splat effect,
	 *  its X axis along the bullet, scaled by CharacterHitFXScale) and, on a wall within reach behind, a splatter decal.
	 *  Optional (empty = none). */
	UPROPERTY(Config, EditAnywhere, Category="Effects", meta=(AllowedClasses="/Script/Niagara.NiagaraSystem"))
	FSoftObjectPath CharacterHitFX;
	UPROPERTY(Config, EditAnywhere, Category="Effects")
	float CharacterHitFXScale = 0.35f;
	UPROPERTY(Config, EditAnywhere, Category="Effects", meta=(AllowedClasses="/Script/Engine.MaterialInterface"))
	FSoftObjectPath CharacterHitSplatter;
	/** A pool spreading under each dead enemy once the body is down (a floor-splat effect). Optional. */
	UPROPERTY(Config, EditAnywhere, Category="Effects", meta=(AllowedClasses="/Script/Niagara.NiagaraSystem"))
	FSoftObjectPath CorpseBloodFX;
	UPROPERTY(Config, EditAnywhere, Category="Effects")
	float CorpseBloodFXScale = 0.22f;

	/** The muzzle and the ejection port in the gun model's own space (rotation: the X axis points the way the effect fires). */
	UPROPERTY(Config, EditAnywhere, Category="Effects")
	FTransform GunMuzzle;
	UPROPERTY(Config, EditAnywhere, Category="Effects")
	FTransform GunEjectionPort;

	/** Dancers within this distance of a gunshot (player's or an enemy's) flee from it. */
	UPROPERTY(Config, EditAnywhere, Category="Crowd", meta=(Units="cm"))
	float PanicRadiusCm = 2500.f;

	/** Seconds a dancer runs after the last gunshot it heard (randomised +-20%). */
	UPROPERTY(Config, EditAnywhere, Category="Crowd", meta=(Units="s"))
	float FleeSeconds = 5.f;

	/** Seconds a dancer stays put after running before it dances again. */
	UPROPERTY(Config, EditAnywhere, Category="Crowd", meta=(Units="s"))
	float CowerSeconds = 6.f;

	/** Running speed while fleeing (randomised +-10%, at most 600). The mannequin locomotion blends walk (300 cm/s) into jog
	 *  (600 cm/s), so near 540 the flight is mostly a full jog. */
	UPROPERTY(Config, EditAnywhere, Category="Crowd", meta=(Units="cm/s"))
	float FleeSpeed = 540.f;
};
