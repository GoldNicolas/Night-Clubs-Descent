#include "GJHUD.h"
#include "AgentTargetComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "GJPlayerCharacter.h"
#include "GJRifle.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

void AGJHUD::DrawText(const FString& Text, float X, float Y, const FLinearColor& Color, float Scale)
{
	if (GEngine && GEngine->GetMediumFont())
	{
		Canvas->SetDrawColor(Color.ToFColor(true));
		Canvas->DrawText(GEngine->GetMediumFont(), Text, X, Y, Scale, Scale);
	}
}

void AGJHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas || !PlayerOwner)
	{
		return;
	}
	const AGJPlayerCharacter* Player = Cast<AGJPlayerCharacter>(PlayerOwner->GetPawn());
	if (!Player)
	{
		return;
	}
	const float CX = Canvas->ClipX * 0.5f;
	const float CY = Canvas->ClipY * 0.5f;
	const float Pad = 24.f;
	const float Bottom = Canvas->ClipY - 70.f;

	// Screen washes (under the text): a red flash on every hit, and a slow fade to deep red on death.
	const double Now = GetWorld()->GetTimeSeconds();
	const double HitAge = Now - Player->GetLastDamageTime();
	if (HitAge >= 0.0 && HitAge < 0.35)
	{
		DrawRect(FLinearColor(0.6f, 0.f, 0.f, 0.18f * static_cast<float>(1.0 - HitAge / 0.35)), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
	}
	if (Player->IsDead() && Player->GetDeathTime() >= 0.0)
	{
		const float Fade = FMath::Clamp(static_cast<float>((Now - Player->GetDeathTime()) / 1.5), 0.f, 1.f);
		DrawRect(FLinearColor(0.1f, 0.f, 0.f, 0.65f * Fade), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
	}

	const AGJRifle* Rifle = Player->GetRifle();
	if (!Player->IsDead())
	{
		DrawCrosshair(Rifle ? Rifle->GetCurrentSpreadDegrees() : 0.25f, CX, CY);
		DrawHitMarker(CX, CY);
	}
	DrawDamageIndicator(CX, CY);

	// Bottom left: health / armor.
	const FLinearColor HealthColor = Player->GetHealth() > 50.f ? FLinearColor(0.9f, 0.95f, 0.9f) : FLinearColor(1.f, 0.35f, 0.3f);
	DrawText(FString::Printf(TEXT("HP %3.0f"), Player->GetHealth()), Pad, Bottom, HealthColor, 1.6f);
	DrawText(FString::Printf(TEXT("ARMOR %3.0f"), Player->GetArmor()), Pad + 130.f, Bottom, FLinearColor(0.45f, 0.75f, 1.f), 1.6f);

	// Bottom right: ammo / reload.
	if (Rifle)
	{
		const FString Ammo = Rifle->IsReloading()
			? FString::Printf(TEXT("RELOADING %.1fs"), Rifle->GetReloadSecondsLeft())
			: FString::Printf(TEXT("%2d / %d"), Rifle->GetAmmo(), Rifle->GetMagazineSize());
		DrawText(Ammo, Canvas->ClipX - 240.f, Bottom, Rifle->GetAmmo() <= 5 ? FLinearColor(1.f, 0.6f, 0.2f) : FLinearColor::White, 1.6f);
	}

	// Bottom centre: movement mode and whether agents can hear you (contract §7.4 rule, exactly what the director feeds them).
	const float Speed = Player->GetVelocity().Size2D();
	const TCHAR* Mode = Player->bIsCrouched ? TEXT("CROUCH") : Player->IsWalkHeld() ? TEXT("WALK") : TEXT("RUN");
	const UAgentTargetComponent* Target = Player->GetAgentTarget();
	const bool bLoud = Target && Target->IsMakingFootsteps();
	DrawText(FString::Printf(TEXT("%s %3.0f cm/s  %s"), Mode, Speed, bLoud ? TEXT("LOUD") : TEXT("silent")),
		CX - 120.f, Bottom, bLoud ? FLinearColor(1.f, 0.55f, 0.2f) : FLinearColor(0.6f, 0.9f, 0.6f), 1.2f);

	if (Player->IsDead())
	{
		DrawText(TEXT("YOU DIED - respawning"), CX - 150.f, CY - 40.f, FLinearColor(1.f, 0.25f, 0.2f), 2.f);
	}
}

void AGJHUD::DrawCrosshair(float SpreadDeg, float CenterX, float CenterY)
{
	// Gap = where the spread cone's edge lands on screen, so the crosshair shows the real accuracy (running ruins aim).
	const float FovDeg = PlayerOwner->PlayerCameraManager ? PlayerOwner->PlayerCameraManager->GetFOVAngle() : 90.f;
	const float HalfFov = FMath::DegreesToRadians(FMath::Clamp(FovDeg, 30.f, 170.f) * 0.5f);
	const float Gap = FMath::Max(3.f, FMath::Tan(FMath::DegreesToRadians(SpreadDeg)) / FMath::Tan(HalfFov) * (Canvas->ClipX * 0.5f));
	const float Len = 9.f;
	const FLinearColor C(1.f, 1.f, 1.f, 0.9f);
	DrawLine(CenterX - Gap - Len, CenterY, CenterX - Gap, CenterY, C, 2.f);
	DrawLine(CenterX + Gap, CenterY, CenterX + Gap + Len, CenterY, C, 2.f);
	DrawLine(CenterX, CenterY - Gap - Len, CenterX, CenterY - Gap, C, 2.f);
	DrawLine(CenterX, CenterY + Gap, CenterX, CenterY + Gap + Len, C, 2.f);
}

void AGJHUD::DrawHitMarker(float CenterX, float CenterY)
{
	const AGJPlayerCharacter* Player = Cast<AGJPlayerCharacter>(PlayerOwner->GetPawn());
	const AGJRifle* Rifle = Player ? Player->GetRifle() : nullptr;
	if (!Rifle) { return; }
	const double Age = GetWorld()->GetTimeSeconds() - Rifle->GetLastHitTime();
	const double Duration = Rifle->WasLastHitKill() ? 0.35 : 0.15;
	if (Age < 0.0 || Age > Duration) { return; }
	const FLinearColor C = Rifle->WasLastHitKill() ? FLinearColor(1.f, 0.15f, 0.1f)
		: Rifle->GetLastHitZone() == EHitZone::Head ? FLinearColor(1.f, 0.9f, 0.2f) : FLinearColor::White;
	const float In = 6.f;
	const float Out = Rifle->WasLastHitKill() ? 18.f : 13.f;
	DrawLine(CenterX - Out, CenterY - Out, CenterX - In, CenterY - In, C, 2.5f);
	DrawLine(CenterX + Out, CenterY - Out, CenterX + In, CenterY - In, C, 2.5f);
	DrawLine(CenterX - Out, CenterY + Out, CenterX - In, CenterY + In, C, 2.5f);
	DrawLine(CenterX + Out, CenterY + Out, CenterX + In, CenterY + In, C, 2.5f);
}

void AGJHUD::DrawDamageIndicator(float CenterX, float CenterY)
{
	const AGJPlayerCharacter* Player = Cast<AGJPlayerCharacter>(PlayerOwner->GetPawn());
	if (!Player) { return; }
	const double Age = GetWorld()->GetTimeSeconds() - Player->GetLastDamageTime();
	if (Age < 0.0 || Age > 0.8) { return; }
	const float Alpha = 1.f - static_cast<float>(Age / 0.8);
	// An arc segment on a ring around the crosshair, toward the shooter (relative to the view yaw).
	const FVector ToSource = (Player->GetLastDamageSource() - Player->GetActorLocation()).GetSafeNormal2D();
	const float ViewYaw = PlayerOwner->GetControlRotation().Yaw;
	const float Rel = FMath::DegreesToRadians(FRotator::NormalizeAxis(ToSource.Rotation().Yaw - ViewYaw)); // 0 = ahead, + = right
	const float Radius = 120.f;
	const FLinearColor C(1.f, 0.1f, 0.05f, Alpha);
	for (int32 i = -3; i < 3; ++i)
	{
		const float A0 = Rel + i * 0.07f;
		const float A1 = Rel + (i + 1) * 0.07f;
		DrawLine(CenterX + Radius * FMath::Sin(A0), CenterY - Radius * FMath::Cos(A0),
			CenterX + Radius * FMath::Sin(A1), CenterY - Radius * FMath::Cos(A1), C, 5.f);
	}
}
