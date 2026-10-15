#include "Vehicle/DNCarEffectsComponent.h"
#include "Vehicle/DNVehicle.h"
#include "Effects/DNEffectsSubsystem.h"
#include "Detonauto.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

namespace
{
	/** Luz do fogo: laranja, sem sombra, tremulando entre 60% e 100% (lumens). */
	constexpr float FireLightLumens = 6000.0f;
	constexpr float FireLightRadiusCm = 600.0f;
	/** Abaixo disto a roda não solta fumaça (evita ligar e desligar o sistema a cada quadro). */
	constexpr float TireRateOn = 0.02f;
}

UDNCarEffectsComponent::UDNCarEffectsComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics; // o estado das rodas e o dano do quadro já estão prontos
}

UNiagaraComponent* UDNCarEffectsComponent::Ensure(TObjectPtr<UNiagaraComponent>& Slot, UDNEffectsSubsystem& Fx, EDNEffect Effect, const FDNEffectSpec& Spec)
{
	if (Slot)
	{
		return Slot;
	}
	UNiagaraSystem* System = Fx.System(Effect);
	if (!System || !GetOwner())
	{
		return nullptr;
	}
	Slot = UNiagaraFunctionLibrary::SpawnSystemAttached(System, GetOwner()->GetRootComponent(), NAME_None, FVector::ZeroVector, FRotator::ZeroRotator,
		EAttachLocation::KeepRelativeOffset, false, false, ENCPoolMethod::None);
	if (Slot)
	{
		Slot->SetUsingAbsoluteLocation(true); // a posição vem do contato da roda ou do vão do motor, a cada quadro
		Slot->SetVariableFloat(TEXT("SpawnPerS"), static_cast<float>(Spec.CountMax));
		Slot->SetVariableFloat(TEXT("LifeMin"), Spec.LifeMinS);
		Slot->SetVariableFloat(TEXT("LifeMax"), Spec.LifeMaxS);
		Slot->SetVariableFloat(TEXT("SpeedMin"), Spec.SpeedMinMs * 100.0f);
		Slot->SetVariableFloat(TEXT("SpeedMax"), Spec.SpeedMaxMs * 100.0f);
		Slot->SetVariableFloat(TEXT("FadeNear"), Fx.Settings().SmokeFadeNearCm);
		Slot->SetVariableFloat(TEXT("FadeFar"), Fx.Settings().SmokeFadeFarCm);
	}
	return Slot;
}

void UDNCarEffectsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	ADNVehicle* Car = Cast<ADNVehicle>(GetOwner());
	UDNEffectsSubsystem* Fx = GetWorld() ? GetWorld()->GetSubsystem<UDNEffectsSubsystem>() : nullptr;
	if (!Car || !Fx)
	{
		return;
	}
	// E11: a fumaça pedida neste quadro (pneus e motor, taxa x sprites/s x vida) e a escala do teto
	float Requested = 0.0f;
	FDNEffectSpec Tire, Smoke;
	const bool bTire = Fx->Spec(EDNEffect::TireSmoke, Tire);
	const bool bSmoke = Fx->Spec(EDNEffect::EngineSmoke, Smoke);
	for (const float Rate : TireRates)
	{
		Requested += bTire ? Rate * Tire.CountMax * Tire.LifeMaxS : 0.0f;
	}
	Requested += bSmoke && EngineState.bSmoke ? FMath::Lerp(0.3f, 1.0f, EngineState.Intensity) * Smoke.CountMax * Smoke.LifeMaxS : 0.0f;
	const float Scale = Fx->ReportSmoke(this, Requested);
	UpdateTires(*Car, *Fx, Scale);
	UpdateEngine(*Car, *Fx, Scale, DeltaTime);
}

void UDNCarEffectsComponent::UpdateTires(ADNVehicle& Car, UDNEffectsSubsystem& Fx, float Scale)
{
	FDNEffectSpec Tire;
	if (!Fx.Spec(EDNEffect::TireSmoke, Tire))
	{
		return;
	}
	const int32 Wheels = Car.WheelCount();
	TireRates.SetNumZeroed(Wheels);
	TireFx.SetNum(Wheels);
	const float SpeedKmh = Car.SpeedKmh();
	bool bAny = false;
	float Strongest = 0.0f;
	for (int32 i = 0; i < Wheels; ++i)
	{
		const FWheelStatus* W = Car.WheelStatus(i);
		const bool bGrounded = W && W->bInContact;
		TireRates[i] = bGrounded ? FDNEffects::TireSmokeRate(Tire, FMath::RadiansToDegrees(W->SlipAngle), SpeedKmh, W->bIsSlipping) : 0.0f;
		bAny |= TireRates[i] >= TireRateOn;
		Strongest = FMath::Max(Strongest, TireRates[i]);
	}
	// a vaga dos pneus (até PoolMax carros): pega quando algum pneu fuma; devolve quando todos param
	if (bAny && !bTireClaimed)
	{
		bTireClaimed = Fx.ClaimContinuous(EDNEffect::TireSmoke, this);
	}
	if (bAny != bTiresWereSmoking)
	{
		UE_LOG(LogDetonauto, Display, TEXT("DNFx: fumaça de pneu %s em '%s' a %.0f km/h (maior taxa %.2f, escala %.2f%s)"), bAny ? TEXT("liga") : TEXT("desliga"),
			*GetNameSafe(GetOwner()), SpeedKmh, Strongest, Scale, bAny && !bTireClaimed ? TEXT(", sem vaga") : TEXT(""));
		bTiresWereSmoking = bAny;
	}
	for (int32 i = 0; i < Wheels; ++i)
	{
		const bool bOn = bTireClaimed && TireRates[i] >= TireRateOn;
		UNiagaraComponent* C = bOn ? Ensure(TireFx[i], Fx, EDNEffect::TireSmoke, Tire) : TireFx[i].Get();
		if (!C)
		{
			continue;
		}
		if (const FWheelStatus* W = Car.WheelStatus(i))
		{
			C->SetWorldLocation(W->ContactPoint);
		}
		C->SetVariableFloat(TEXT("Rate"), bOn ? TireRates[i] * Scale : 0.0f);
		if (bOn && !C->IsActive())
		{
			C->Activate();
		}
		else if (!bOn && C->IsActive())
		{
			C->Deactivate(); // as partículas vivas terminam a vida
		}
	}
	if (!bAny && bTireClaimed)
	{
		Fx.ReleaseContinuous(EDNEffect::TireSmoke, this);
		bTireClaimed = false;
	}
}

void UDNCarEffectsComponent::UpdateEngine(ADNVehicle& Car, UDNEffectsSubsystem& Fx, float Scale, float DeltaTime)
{
	FDNEffectSpec Smoke, Fire;
	if (!Fx.Spec(EDNEffect::EngineSmoke, Smoke) || !Fx.Spec(EDNEffect::EngineFire, Fire))
	{
		return;
	}
	const FDNDamageState& Damage = Car.GetDamage();
	const FDNEngineSmoke Now = FDNEffects::EngineSmoke(Smoke, Fire, Damage.ZoneDamage[static_cast<int32>(EDNDamageZone::Front)], Damage.bDestroyed);
	// E7: só sobe (o dano não desce; um dano maior não reinicia a fumaça)
	const FDNEngineSmoke Before = EngineState;
	EngineState.bSmoke |= Now.bSmoke;
	EngineState.bDark |= Now.bDark;
	EngineState.bFire |= Now.bFire;
	EngineState.Intensity = FMath::Max(EngineState.Intensity, Now.Intensity);
	if (EngineState.bSmoke != Before.bSmoke || EngineState.bDark != Before.bDark || EngineState.bFire != Before.bFire)
	{
		UE_LOG(LogDetonauto, Display, TEXT("DNFx: motor de '%s': %s (frente %.2f, intensidade %.2f)"), *GetNameSafe(GetOwner()),
			EngineState.bFire ? TEXT("fogo") : (EngineState.bDark ? TEXT("fumaça escura") : TEXT("fumaça clara")),
			Damage.ZoneDamage[static_cast<int32>(EDNDamageZone::Front)], EngineState.Intensity);
	}
	const FVector Bay = Car.EngineBayWorld();
	if (EngineState.bSmoke && !bSmokeClaimed)
	{
		bSmokeClaimed = Fx.ClaimContinuous(EDNEffect::EngineSmoke, this);
		if (!bSmokeClaimed)
		{
			UE_LOG(LogDetonauto, Display, TEXT("DNFx: fumaça do motor de '%s' sem vaga (%d carros já soltam)"), *GetNameSafe(GetOwner()), Smoke.PoolMax);
		}
	}
	if (bSmokeClaimed)
	{
		if (UNiagaraComponent* C = Ensure(SmokeFx, Fx, EDNEffect::EngineSmoke, Smoke))
		{
			C->SetWorldLocation(Bay);
			// a clara começa com 30% da taxa e chega a 1 na escura
			C->SetVariableFloat(TEXT("Rate"), FMath::Lerp(0.3f, 1.0f, EngineState.Intensity) * Scale);
			C->SetVariableFloat(TEXT("Dark"), EngineState.bDark ? 1.0f : 0.0f);
			if (!C->IsActive())
			{
				C->Activate();
			}
		}
	}
	if (EngineState.bFire && !bFireClaimed)
	{
		bFireClaimed = Fx.ClaimContinuous(EDNEffect::EngineFire, this);
	}
	if (bFireClaimed)
	{
		if (UNiagaraComponent* C = Ensure(FireFx, Fx, EDNEffect::EngineFire, Fire))
		{
			C->SetWorldLocation(Bay);
			C->SetVariableFloat(TEXT("Rate"), 1.0f);
			if (!C->IsActive())
			{
				C->Activate();
			}
		}
		// a luz vem mesmo sem o sistema (dá para ver o fogo no log e na captura antes do asset)
		if (!FireLight && GetOwner())
		{
			FireLight = NewObject<UPointLightComponent>(GetOwner(), TEXT("DN_FireLight"));
			FireLight->SetCastShadows(false);
			FireLight->SetLightColor(FLinearColor(1.0f, 0.45f, 0.12f));
			FireLight->SetIntensityUnits(ELightUnits::Lumens);
			FireLight->SetAttenuationRadius(FireLightRadiusCm);
			FireLight->SetupAttachment(GetOwner()->GetRootComponent());
			FireLight->RegisterComponent();
		}
		if (FireLight)
		{
			// tremulação: duas senoides fora de fase, entre 60% e 100%
			FlickerClock += DeltaTime;
			const float Flicker = 0.8f + 0.12f * FMath::Sin(FlickerClock * 17.0f) + 0.08f * FMath::Sin(FlickerClock * 41.0f + 1.3f);
			FireLight->SetWorldLocation(Bay + FVector(0.0f, 0.0f, 30.0f));
			FireLight->SetIntensity(FireLightLumens * Flicker);
		}
	}
}

void UDNCarEffectsComponent::ResetEffects()
{
	UDNEffectsSubsystem* Fx = GetWorld() ? GetWorld()->GetSubsystem<UDNEffectsSubsystem>() : nullptr;
	for (TObjectPtr<UNiagaraComponent>& C : TireFx)
	{
		if (C)
		{
			C->DestroyComponent();
		}
	}
	TireFx.Reset();
	for (UNiagaraComponent* C : { SmokeFx.Get(), FireFx.Get() })
	{
		if (C)
		{
			C->DestroyComponent();
		}
	}
	SmokeFx = nullptr;
	FireFx = nullptr;
	if (FireLight)
	{
		FireLight->DestroyComponent();
		FireLight = nullptr;
	}
	if (Fx)
	{
		for (const EDNEffect E : { EDNEffect::TireSmoke, EDNEffect::EngineSmoke, EDNEffect::EngineFire })
		{
			Fx->ReleaseContinuous(E, this);
		}
	}
	bTireClaimed = bSmokeClaimed = bFireClaimed = false;
	bTiresWereSmoking = false;
	EngineState = FDNEngineSmoke();
	TireRates.Reset();
}

void UDNCarEffectsComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ResetEffects();
	Super::EndPlay(EndPlayReason);
}
