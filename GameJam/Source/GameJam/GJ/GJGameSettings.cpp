#include "GJGameSettings.h"

UGJGameSettings::UGJGameSettings()
{
	CategoryName = TEXT("Game");

	// Rodin characters imported 2026-11-15 onto SK_Mannequin (heights fitted at import; feet at the origin).
	PlayerSkin = FSoftObjectPath(TEXT("/Game/Characters/Rodin/Player_CleanerHD/SKM_Player_CleanerHD.SKM_Player_CleanerHD"));
	DancerSkins = {
		FSoftObjectPath(TEXT("/Game/Characters/Rodin/Dancer_01/SKM_Dancer_01.SKM_Dancer_01")),
		FSoftObjectPath(TEXT("/Game/Characters/Rodin/Dancer_02/SKM_Dancer_02.SKM_Dancer_02")),
		FSoftObjectPath(TEXT("/Game/Characters/Rodin/Dancer_04/SKM_Dancer_04.SKM_Dancer_04")),
		FSoftObjectPath(TEXT("/Game/Characters/Rodin/Dancer_06/SKM_Dancer_06.SKM_Dancer_06")),
	};
	FleeAnimClass = FSoftClassPath(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.ABP_Unarmed_C"));
	CrowdAnchorClass = FSoftClassPath(TEXT("/Game/ClubMap/Blueprints/BP_CM_MirrorBall.BP_CM_MirrorBall_C"));

	// The M16A1 pack's own gun animations (SK_M16_Skeleton); ignored for a gun model on another skeleton.
	PlayerWeaponFireAnim = FSoftObjectPath(TEXT("/Game/M16A1/Animation/Anim_Shoot.Anim_Shoot"));
	PlayerWeaponReloadAnim = FSoftObjectPath(TEXT("/Game/M16A1/Animation/Anim_Reload.Anim_Reload"));

	// Arms reload: the free "First Person Rifle Animation" pack, retargeted onto SK_Mannequin (RTG_FPArms_to_UE5).
	PlayerReloadAnim = FSoftObjectPath(TEXT("/Game/Characters/FPAnims/FP_Rifle_Reload.FP_Rifle_Reload"));
	// Panic sprint: the free "Run like hell" clip, retargeted onto SK_Mannequin (RTG_RunLikeHell_to_UE5), root-locked.

	// Effects: the free NEON WEX muzzle flash pack, a Megascans bullet-hole decal, and the game's own tracer material.
	MuzzleFlash = FSoftObjectPath(TEXT("/Game/NW_MuzzleFX/Particle_FX/FXS_NS_MuzzleFlash_04.FXS_NS_MuzzleFlash_04"));
	ShellEject = FSoftObjectPath(TEXT("/Game/NW_MuzzleFX/Particle_FX/Shell/FXS_NS_NS3A1_ShellDrop_FXpack.FXS_NS_NS3A1_ShellDrop_FXpack"));
	ImpactSparks = FSoftObjectPath(TEXT("/Game/NW_MuzzleFX/Particle_FX/FXS_NS_ShotBurst_01.FXS_NS_ShotBurst_01"));
	PlayerFirstPersonMaterial = FSoftObjectPath(TEXT("/Game/Characters/Rodin/Player_CleanerHD/MI_Player_CleanerHD_FP.MI_Player_CleanerHD_FP"));
	// A darker, punchier child of the Megascans concrete-hole decal (the stock one reads as a faint smudge on light floors).
	BulletHoleDecal = FSoftObjectPath(TEXT("/Game/GJ/FX/MI_GJ_BulletHole.MI_GJ_BulletHole"));
	// Blood: Vefects' Free Blood floor splats (one-shot versions) and a Megascans high-velocity spatter decal.
	CharacterHitFX = FSoftObjectPath(TEXT("/Game/Vefects/VFX_Free_Blood/VFX/Particles/Once/NS_Blood_Splat_Directional_01_Floor_Once.NS_Blood_Splat_Directional_01_Floor_Once"));
	CharacterHitSplatter = FSoftObjectPath(TEXT("/Game/GJ/FX/MI_GJ_BloodSpatter.MI_GJ_BloodSpatter"));
	CorpseBloodFX = FSoftObjectPath(TEXT("/Game/Vefects/VFX_Free_Blood/VFX/Particles/Once/NS_Blood_Splat_01_Floor_Once.NS_Blood_Splat_01_Floor_Once"));
	TracerMaterial = FSoftObjectPath(TEXT("/Game/GJ/FX/M_GJ_Tracer.M_GJ_Tracer"));
	GunshotSound = FSoftObjectPath(TEXT("/Game/NW_MuzzleFX/Sound/Sfx/SC_Shot_001.SC_Shot_001"));
	GunshotAttenuation = FSoftObjectPath(TEXT("/Game/GJ/FX/SA_GJ_Gunshot.SA_GJ_Gunshot"));
	// The M16 model: 100 cm long along Y with the muzzle at -Y; its right side (ejection port) is +X.
	GunMuzzle = FTransform(FRotator(0.f, -90.f, 0.f), FVector(0.f, -50.f, 1.5f));
	GunEjectionPort = FTransform(FRotator(30.f, 0.f, 0.f), FVector(3.5f, 8.f, 2.f));
}
