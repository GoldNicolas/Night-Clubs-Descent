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
	OnDamaged.Broadcast(FMath::Max(0.f, CurrentHP / MaxHP));
	UE_LOG(LogAgentRuntime, Log, TEXT("Player hit: %.0f damage from %s -> health %.0f, armor %.0f"),
		Damage, DamageInstigator ? *DamageInstigator->GetName() : TEXT("?"), CurrentHP, CurrentArmor);
	if (CurrentHP <= 0.f)
	{
		UE_LOG(LogAgentRuntime, Display, TEXT("Player killed by %s"), DamageInstigator ? *DamageInstigator->GetName() : TEXT("?"));
		AgentTarget->SetAlive(false); // agents drop the player from their tracks at once
		RemovePlayerInputContext();
		UnCrouch();
		Die();
	}
}

void AGJPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!IsDead())
	{
		AddPlayerInputContext(); // self-heal: cheap check, guarantees Shift / Ctrl / C / R after any possession order
	}
	// Health regenerates after HealthRegenDelay seconds without damage; armor doesn't (items will refill it).
	if (!IsDead() && CurrentHP < MaxHP && GetWorld()->GetTimeSeconds() - LastDamageTime >= HealthRegenDelay)
	{
		CurrentHP = FMath::Min(MaxHP, CurrentHP + HealthRegenPerSecond * DeltaSeconds);
		OnDamaged.Broadcast(CurrentHP / MaxHP);
	}
}
