#include "GJPlayerCharacter.h"
#include "AgentRuntime.h"
#include "AgentTargetComponent.h"
#include "Character/BeanHitboxLayout.h"
#include "Common/FPSAgentSettings.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInterface.h"
#include "GJGameSettings.h"
#include "GJViewModifier.h"
#include "GJRifle.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Weapons/CombatMath.h"

namespace
{
	// Exactly the bean's hitbox setup (vendored BeanCharacter.cpp ConfigureHitbox): only the Weapon channel can hit it.
	void ConfigureHitbox(UShapeComponent* Shape)
	{
		Shape->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Shape->SetCollisionObjectType(ECC_Pawn);
		Shape->SetCollisionResponseToAllChannels(ECR_Ignore);
		Shape->SetCollisionResponseToChannel(ECC_Weapon, ECR_Block);
		Shape->SetGenerateOverlapEvents(false);
		Shape->SetCanEverAffectNavigation(false);
		Shape->SetHiddenInGame(true);
	}
}

AGJPlayerCharacter::AGJPlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	const UFPSAgentSettings& S = UFPSAgentSettings::Get();

	// Body (contract §7.5): the bean's capsule, so the head is at 158 cm standing and 102 cm crouched.
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	Capsule->InitCapsuleSize(S.Body.CapsuleRadius, S.Body.StandingHalfHeight);
	Capsule->SetCollisionResponseToChannel(ECC_Weapon, ECR_Ignore); // agent bullets hit the hitboxes, not the capsule
	GetMesh()->SetCollisionResponseToChannel(ECC_Weapon, ECR_Ignore);
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->GetNavAgentPropertiesRef().bCanCrouch = true;
	Move->SetCrouchedHalfHeight(S.Body.CrouchedHalfHeight);
	BaseEyeHeight = ComputeBeanHitboxLayout(S.Body, S.Body.StandingHalfHeight).EyeZ;
	CrouchedEyeHeight = ComputeBeanHitboxLayout(S.Body, S.Body.CrouchedHalfHeight).EyeZ;

	HeadHitbox = CreateDefaultSubobject<USphereComponent>(TEXT("HeadHitbox"));
	BodyHitbox = CreateDefaultSubobject<UBoxComponent>(TEXT("BodyHitbox"));
	LegsHitbox = CreateDefaultSubobject<UBoxComponent>(TEXT("LegsHitbox"));
	UShapeComponent* Hitboxes[] = { HeadHitbox.Get(), BodyHitbox.Get(), LegsHitbox.Get() };
	for (UShapeComponent* Shape : Hitboxes)
	{
		Shape->SetupAttachment(Capsule);
		ConfigureHitbox(Shape);
	}
	ApplyBodyLayout();

	AgentTarget = CreateDefaultSubobject<UAgentTargetComponent>(TEXT("AgentTarget"));

	// Extra controls, in code so no input assets are needed: Shift = walk (silent), Ctrl / C = crouch (hold), R = reload.
	WalkAction = CreateDefaultSubobject<UInputAction>(TEXT("IA_GJ_Walk"));
	CrouchAction = CreateDefaultSubobject<UInputAction>(TEXT("IA_GJ_Crouch"));
	ReloadAction = CreateDefaultSubobject<UInputAction>(TEXT("IA_GJ_Reload"));
	AgentRulesContext = CreateDefaultSubobject<UInputMappingContext>(TEXT("IMC_GJ_Player"));
	AgentRulesContext->MapKey(WalkAction, EKeys::LeftShift);
	AgentRulesContext->MapKey(CrouchAction, EKeys::LeftControl);
	AgentRulesContext->MapKey(CrouchAction, EKeys::C);
	AgentRulesContext->MapKey(ReloadAction, EKeys::R);

	DefaultWeaponClass = AGJRifle::StaticClass();
}

void AGJPlayerCharacter::PostInitializeComponents()
{
	// Blueprint children may carry the template's 96 cm capsule: enforce the contract body before ACharacter caches the mesh offset.
	const UFPSAgentSettings& S = UFPSAgentSettings::Get();
	// GameJam settings PlayerSkin: the same body for the first-person arms and the third-person shadow/body (SK_Mannequin skeleton,
	// so the template's weapon AnimBPs drive it). Looks only: the hitboxes are the bean's whatever the skin.
	if (USkeletalMesh* Skin = Cast<USkeletalMesh>(UGJGameSettings::Get().PlayerSkin.TryLoad()))
	{
		GetMesh()->SetSkeletalMeshAsset(Skin);
		GetFirstPersonMesh()->SetSkeletalMeshAsset(Skin);
	}
	// The first-person body hides its near sleeve (see GJGameSettings::PlayerFirstPersonMaterial).
	const FSoftObjectPath& FPMaterialPath = UGJGameSettings::Get().PlayerFirstPersonMaterial;
	if (UMaterialInterface* FPMaterial = FPMaterialPath.IsValid() ? Cast<UMaterialInterface>(FPMaterialPath.TryLoad()) : nullptr)
	{
		for (int32 Slot = 0; Slot < GetFirstPersonMesh()->GetNumMaterials(); ++Slot)
		{
			GetFirstPersonMesh()->SetMaterial(Slot, FPMaterial);
		}
	}
	GetCapsuleComponent()->SetCapsuleSize(S.Body.CapsuleRadius, S.Body.StandingHalfHeight);
	FVector MeshLocation = GetMesh()->GetRelativeLocation();
	MeshLocation.Z = -S.Body.StandingHalfHeight; // the mannequin's origin is at its feet
	GetMesh()->SetRelativeLocation(MeshLocation);
	Super::PostInitializeComponents();
}

void AGJPlayerCharacter::BeginPlay()
{
	MaxHP = PlayerMaxHealth; // before the template's BeginPlay fills CurrentHP from it
	Super::BeginPlay();
	CurrentArmor = MaxArmor;
	ApplyMovementTuning();
	ApplyBodyLayout();
	AgentTarget->SetHitboxes(HeadHitbox, BodyHitbox, LegsHitbox);
	AgentTarget->HitHandler.BindUObject(this, &AGJPlayerCharacter::HandleAgentHit);
	AgentTarget->SetAlive(true);
	AgentTarget->SetWalking(false);
	GiveDefaultWeapon();
}

void AGJPlayerCharacter::GiveDefaultWeapon()
{
	// Only once possessed AND begun play. Possessed: the template's first-person weapon AnimBP reads the controller as soon as the
	// weapon activates. Begun play: a weapon spawned before the world begins play has no owner yet (AShooterWeapon::BeginPlay sets it),
	// and activating it would crash. First spawn: possession happens before BeginPlay (BeginPlay gives it); respawn: the reverse
	// (NotifyControllerChanged gives it).
	if (DefaultWeaponClass && Controller && (HasActorBegunPlay() || IsActorBeginningPlay()) && !CurrentWeapon)
	{
		AddWeaponClass(DefaultWeaponClass);
	}
}

void AGJPlayerCharacter::AddPlayerInputContext()
{
	const APlayerController* PC = Cast<APlayerController>(Controller);
	if (PC && PC->GetPawn() == this && PC->GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (!Subsystem->HasMappingContext(AgentRulesContext))
			{
				Subsystem->AddMappingContext(AgentRulesContext, 1);
			}
		}
	}
}

void AGJPlayerCharacter::RemovePlayerInputContext()
{
	// Only while this pawn is still the controller's pawn: a dying pawn's cleanup must never strip the respawned pawn's input.
	const APlayerController* PC = Cast<APlayerController>(Controller);
	if (PC && PC->GetPawn() == this && PC->GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->RemoveMappingContext(AgentRulesContext);
		}
	}
}

void AGJPlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AgentTarget)
	{
		AgentTarget->HitHandler.Unbind();
	}
	RemovePlayerInputContext();
	Super::EndPlay(EndPlayReason);
}

void AGJPlayerCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();
	AddPlayerInputContext();
	GiveDefaultWeapon();
}

void AGJPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		Input->BindAction(WalkAction, ETriggerEvent::Started, this, &AGJPlayerCharacter::OnWalkStarted);
		Input->BindAction(WalkAction, ETriggerEvent::Completed, this, &AGJPlayerCharacter::OnWalkEnded);
		Input->BindAction(CrouchAction, ETriggerEvent::Started, this, &AGJPlayerCharacter::OnCrouchStarted);
		Input->BindAction(CrouchAction, ETriggerEvent::Completed, this, &AGJPlayerCharacter::OnCrouchEnded);
		Input->BindAction(ReloadAction, ETriggerEvent::Started, this, &AGJPlayerCharacter::OnReload);
	}
}

AGJRifle* AGJPlayerCharacter::GetRifle() const
{
	return Cast<AGJRifle>(CurrentWeapon);
}

void AGJPlayerCharacter::ApplyMovementTuning()
{
	// The bean's movement (contract §6.2 / §7.5): in the agents' trained envelope (<= 540 cm/s, snappy, ~90 cm jump).
	const FMovementTuning& M = UFPSAgentSettings::Get().Movement;
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->MaxWalkSpeed = bWalkHeld ? M.WalkSpeed : M.RunSpeed;
	Move->MaxWalkSpeedCrouched = M.CrouchSpeed;
	Move->MaxAcceleration = M.MaxAcceleration;
	Move->BrakingDecelerationWalking = M.BrakingDeceleration;
	Move->GroundFriction = M.GroundFriction;
	Move->JumpZVelocity = M.JumpZVelocity;
	Move->AirControl = M.AirControl;
}

void AGJPlayerCharacter::ApplyBodyLayout()
{
	const FBeanBody& B = UFPSAgentSettings::Get().Body;
	const FBeanHitboxLayout L = ComputeBeanHitboxLayout(B, GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight());
	HeadHitbox->SetSphereRadius(L.HeadRadius);
	HeadHitbox->SetRelativeLocation(FVector(0.f, 0.f, L.HeadCenterZ));
	BodyHitbox->SetBoxExtent(L.BodyExtent);
	BodyHitbox->SetRelativeLocation(FVector(0.f, 0.f, L.BodyCenterZ));
	LegsHitbox->SetBoxExtent(L.LegsExtent);
	LegsHitbox->SetRelativeLocation(FVector(0.f, 0.f, L.LegsCenterZ));
}

void AGJPlayerCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	ApplyBodyLayout();
}

void AGJPlayerCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	ApplyBodyLayout();
}

void AGJPlayerCharacter::OnWalkStarted()
{
	bWalkHeld = true;
	ApplyMovementTuning();
	AgentTarget->SetWalking(true);
}

void AGJPlayerCharacter::OnWalkEnded()
{
	bWalkHeld = false;
	ApplyMovementTuning();
	AgentTarget->SetWalking(false);
}

void AGJPlayerCharacter::OnCrouchStarted()
{
	if (!IsDead()) { Crouch(); }
}

void AGJPlayerCharacter::OnCrouchEnded()
{
	UnCrouch();
}

void AGJPlayerCharacter::OnReload()
{
	if (AGJRifle* Rifle = GetRifle(); Rifle && !IsDead())
	{
		Rifle->RequestReload();
	}
}

bool AGJPlayerCharacter::HandleAgentHit(float Damage, EHitZone Zone, AActor* DamageInstigator)
{
	ApplyPlayerDamage(Damage * IncomingDamageScale, DamageInstigator);
	return IsDead();
}

float AGJPlayerCharacter::TakeDamage(float Damage, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	// Template damage (projectiles, hazards) goes through the same health + armor model.
	ApplyPlayerDamage(Damage, DamageCauser);
	return Damage;
}

void AGJPlayerCharacter::ApplyPlayerDamage(float Damage, AActor* DamageInstigator)
{
	if (IsDead() || Damage <= 0.f)
	{
		return;
	}
	const FPSCombat::FDamageResult R = FPSCombat::ApplyDamage(CurrentHP, CurrentArmor, Damage, ArmorAbsorbFraction);
	CurrentHP = R.Health;
	CurrentArmor = R.Armor;
	LastDamageTime = GetWorld()->GetTimeSeconds();
	LastDamageSource = DamageInstigator ? DamageInstigator->GetActorLocation() : GetActorLocation();
	// View flinch: the camera rolls away from the shooter and springs back (heavier hits flinch harder).
	const float Side = FVector::DotProduct(GetActorRightVector(), (LastDamageSource - GetActorLocation()).GetSafeNormal2D());
	HitRollVel += (Side >= 0.f ? -1.f : 1.f) * HitFlinchRollDeg * 31.f * FMath::Clamp(Damage / 34.f, 0.4f, 1.5f);
	OnDamaged.Broadcast(FMath::Max(0.f, CurrentHP / MaxHP));
	UE_LOG(LogAgentRuntime, Log, TEXT("Player hit: %.0f damage from %s -> health %.0f, armor %.0f"),
		Damage, DamageInstigator ? *DamageInstigator->GetName() : TEXT("?"), CurrentHP, CurrentArmor);
	if (CurrentHP <= 0.f)
	{
		UE_LOG(LogAgentRuntime, Display, TEXT("Player killed by %s"), DamageInstigator ? *DamageInstigator->GetName() : TEXT("?"));
		AgentTarget->SetAlive(false); // agents drop the player from their tracks at once
		// The death view falls from the true eye to a little above the floor.
		DeathTime = GetWorld()->GetTimeSeconds();
		const float FloorZ = GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const float EyeZ = GetFirstPersonCameraComponent()
			? GetFirstPersonCameraComponent()->GetComponentLocation().Z - ArmsWorldShift.Z : GetPawnViewLocation().Z;
		DeathDropCm = FMath::Max(0.f, EyeZ - FloorZ - 35.f);
		// The Blueprint's On Death ragdolls the body and cuts to its Death Camera: show that body and its gun to the player,
		// and hide the first-person body and gun, which would otherwise stand frozen in that view.
		GetFirstPersonMesh()->SetVisibility(false, /*bPropagateToChildren=*/true);
		GetMesh()->SetOwnerNoSee(false);
		if (AGJRifle* Rifle = GetRifle())
		{
			if (USkeletalMeshComponent* Gun = Rifle->GetFirstPersonMesh()) { Gun->SetVisibility(false); }
			if (USkeletalMeshComponent* Gun = Rifle->GetThirdPersonMesh()) { Gun->SetOwnerNoSee(false); }
		}
		RemovePlayerInputContext();
		UnCrouch();
		Die();
	}
}

void AGJPlayerCharacter::EnsureViewModifier()
{
	APlayerController* PC = Cast<APlayerController>(Controller);
	if (PC && PC->PlayerCameraManager && IsLocallyControlled()
		&& !PC->PlayerCameraManager->FindCameraModifierByClass(UGJViewModifier::StaticClass()))
	{
		PC->PlayerCameraManager->AddNewCameraModifier(UGJViewModifier::StaticClass());
	}
}

void AGJPlayerCharacter::OnRifleShot()
{
	KickBackVel += KickBackCm * 22.f;
	KickPitchVel += KickPitchDeg * 22.f;
}

void AGJPlayerCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	LandDipVel += FMath::Clamp(FallSpeed / 100.f, 0.f, 9.f) * 9.f; // the view and the gun dip on landing, harder from higher
	FallSpeed = 0.f;
}

void AGJPlayerCharacter::GetViewEffects(FVector& OutViewOffset, FRotator& OutViewRoll, FVector& OutArmsWorldShift, FVector& OutViewWorldOffset) const
{
	OutViewOffset = ViewOffset;
	OutViewRoll = ViewRoll;
	OutArmsWorldShift = ArmsWorldShift;
	OutViewWorldOffset = ViewWorldOffset;
}

void AGJPlayerCharacter::UpdateWeaponFeel(float DeltaSeconds)
{
	const float Dt = FMath::Clamp(DeltaSeconds, 0.f, 0.05f);
	if (Dt <= 0.f)
	{
		return;
	}
	APlayerController* PC = Cast<APlayerController>(Controller);
	const bool bActive = bWeaponFeel && PC && IsLocallyControlled() && !IsDead();
	USkeletalMeshComponent* FP = GetFirstPersonMesh();
	if (FP && !bArmsBaseCaptured)
	{
		ArmsBaseLocation = FP->GetRelativeLocation();
		bArmsBaseCaptured = true;
	}
	const UCharacterMovementComponent* Move = GetCharacterMovement();
	if (Move && Move->IsFalling())
	{
		FallSpeed = FMath::Max(FallSpeed, static_cast<float>(-GetVelocity().Z));
	}

	// 1. Recoil. The aim punch follows the rifle's own bullet recoil pattern (it climbs while spraying and settles as the spray
	//    index recovers) plus a sharp per-shot kick; the rifle removes the punch from its aim, so the bullets keep the mirror rifle's
	//    exact path and the crosshair shows where they go.
	auto Spring = [Dt](float& X, float& V, float Stiffness, float Damping)
	{
		V += (-Stiffness * X - Damping * V) * Dt;
		X += V * Dt;
	};
	Spring(KickBack, KickBackVel, 420.f, 30.f);
	Spring(KickPitch, KickPitchVel, 380.f, 30.f);
	Spring(LandDip, LandDipVel, 240.f, 24.f);
	Spring(HitRoll, HitRollVel, 260.f, 18.f);
	FVector2D PatternTarget = FVector2D::ZeroVector;
	if (const AGJRifle* Rifle = GetRifle(); Rifle && bActive)
	{
		PatternTarget = Rifle->GetViewRecoilDegrees();
	}
	PatternPunch = FMath::Vector2DInterpTo(PatternPunch, PatternTarget, Dt, 20.f);
	const FRotator WantedPunch = bActive ? FRotator(PatternPunch.Y + KickPitch, PatternPunch.X, 0.f) : FRotator::ZeroRotator;
	if (PC)
	{
		const FRotator Delta = WantedPunch - AimPunchApplied;
		if (!Delta.IsNearlyZero(1e-4f))
		{
			FRotator Control = PC->GetControlRotation();
			Control.Pitch += Delta.Pitch;
			Control.Yaw += Delta.Yaw;
			PC->SetControlRotation(Control);
		}
		AimPunchApplied = WantedPunch;
	}

	// 2. Sway: the gun lags behind fast turns (the player's own aim, not the recoil punch).
	const FRotator Aim = (PC ? PC->GetControlRotation() : GetActorRotation()) - AimPunchApplied;
	if (!bHasLastAim)
	{
		LastAim = Aim;
		bHasLastAim = true;
	}
	const float YawRate = FRotator::NormalizeAxis(Aim.Yaw - LastAim.Yaw) / Dt;
	const float PitchRate = FRotator::NormalizeAxis(Aim.Pitch - LastAim.Pitch) / Dt;
	LastAim = Aim;
	const FVector2D SwayTarget = bActive
		? FVector2D(FMath::Clamp(-YawRate * SwayCmPerDegPerSec, -SwayMaxCm, SwayMaxCm), FMath::Clamp(-PitchRate * SwayCmPerDegPerSec, -SwayMaxCm, SwayMaxCm))
		: FVector2D::ZeroVector;
	Sway = FMath::Vector2DInterpTo(Sway, SwayTarget, Dt, 9.f);

	// 3. Bob: walking and running swing the gun in a figure eight and bob the view, scaled by speed; crouching softens it.
	const float Speed = GetVelocity().Size2D();
	const float SpeedAlpha = (bActive && Move && !Move->IsFalling()) ? FMath::Clamp(Speed / 540.f, 0.f, 1.f) : 0.f;
	BobBlend = FMath::FInterpTo(BobBlend, SpeedAlpha, Dt, 7.f);
	BobPhase = FMath::Fmod(BobPhase + Dt * FMath::Lerp(1.f, 1.75f, SpeedAlpha) * 2.f * PI, 2.f * PI);
	const float Amp = BobBlend * (bIsCrouched ? 0.55f : 1.f);
	const FVector LocalVelocity = FRotator(0.f, Aim.Yaw, 0.f).UnrotateVector(GetVelocity());
	const float Strafe = FMath::Clamp(static_cast<float>(LocalVelocity.Y) / 540.f, -1.f, 1.f);

	// 4. Death: the gun drops out of sight and the view collapses to the floor, tipping onto its side.
	float Collapse = 0.f;
	float Tip = 0.f;
	if (DeathTime >= 0.0)
	{
		const float T = FMath::Clamp(static_cast<float>(GetWorld()->GetTimeSeconds() - DeathTime) / 0.85f, 0.f, 1.f);
		Collapse = T * T; // knees give way: slow, then fast
		Tip = FMath::SmoothStep(0.f, 1.f, T);
	}

	// Compose. The gun (arms) offset in view space: X forward, Y right, Z up; the rest offset frames the arms even when the
	// feel is off.
	const FVector ArmsLocal = ArmsRestOffsetCm + FVector(
		-KickBack,
		FMath::Sin(BobPhase) * GunBobCm * Amp + Sway.X - Strafe * 0.8f,
		FMath::Cos(2.f * BobPhase) * 0.5f * GunBobCm * Amp + Sway.Y - LandDip * 0.5f - 50.f * Tip);
	const FRotator ViewRotation = PC ? PC->GetControlRotation() : GetActorRotation();
	ArmsWorldShift = ViewRotation.RotateVector(ArmsLocal);
	if (FP && bArmsBaseCaptured)
	{
		const USceneComponent* Parent = FP->GetAttachParent();
		const FVector InParent = Parent ? Parent->GetComponentQuat().UnrotateVector(ArmsWorldShift) / Parent->GetComponentScale() : ArmsWorldShift;
		FP->SetRelativeLocation(ArmsBaseLocation + InParent);
	}
	ViewOffset = FVector(-KickBack * 0.25f, 0.f, (0.5f - FMath::Abs(FMath::Sin(BobPhase))) * 2.f * CameraBobCm * Amp - LandDip);
	// Roll only while alive (pitch or yaw here would pull the view off the crosshair); the dead view also looks down.
	ViewRoll = FRotator(-20.f * Tip, 0.f, FMath::Sin(BobPhase) * 0.3f * Amp + Strafe * 0.8f + HitRoll + 70.f * Tip);
	ViewWorldOffset = FVector(0.f, 0.f, -DeathDropCm * Collapse);
}

void AGJPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateWeaponFeel(DeltaSeconds);
	if (!IsDead())
	{
		AddPlayerInputContext(); // self-heal: cheap check, guarantees Shift / Ctrl / C / R after any possession order
		EnsureViewModifier();
	}
	// Health regenerates after HealthRegenDelay seconds without damage; armor doesn't (items will refill it).
	if (!IsDead() && CurrentHP < MaxHP && GetWorld()->GetTimeSeconds() - LastDamageTime >= HealthRegenDelay)
	{
		CurrentHP = FMath::Min(MaxHP, CurrentHP + HealthRegenPerSecond * DeltaSeconds);
		OnDamaged.Broadcast(CurrentHP / MaxHP);
	}
}
