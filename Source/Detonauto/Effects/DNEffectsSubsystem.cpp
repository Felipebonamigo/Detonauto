#include "Effects/DNEffectsSubsystem.h"
#include "Core/DNGameDataSubsystem.h"
#include "Crash/DNBreakableProp.h"
#include "Crash/DNBreakableSubsystem.h"
#include "Detonauto.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

namespace
{
	/** Onde ficam os sistemas: NS_DN + o nome do efeito. */
	constexpr const TCHAR* EffectsDir = TEXT("/Game/Detonauto/Effects");
}

void UDNEffectsSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	LoadData();
	if (UDNBreakableSubsystem* Props = InWorld.GetSubsystem<UDNBreakableSubsystem>())
	{
		Props->OnBroken.AddUObject(this, &UDNEffectsSubsystem::OnPropBroken);
	}
}

void UDNEffectsSubsystem::LoadData()
{
	if (bDataLoaded)
	{
		return;
	}
	bDataLoaded = true;
	Specs.SetNum(EffectCount);
	SpecValid.Init(false, EffectCount);
	Systems.SetNum(EffectCount);
	SystemLooked.Init(false, EffectCount);
	const UGameInstance* Game = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UDNGameDataSubsystem* Data = Game ? Game->GetSubsystem<UDNGameDataSubsystem>() : nullptr;
	int32 Valid = 0;
	for (int32 i = 0; i < EffectCount && Data; ++i)
	{
		SpecValid[i] = Data->GetEffect(static_cast<EDNEffect>(i), Specs[i]);
		Valid += SpecValid[i] ? 1 : 0;
	}
	EffectsSettings = Data ? Data->GetEffectsSettings() : FDNEffectsSettings();
	UE_LOG(LogDetonauto, Display, TEXT("DNFx: %d de %d efeitos de Effects.csv; tetos: %d partículas de GPU, %d sprites de fumaça"),
		Valid, EffectCount, EffectsSettings.MaxGpuParticles, EffectsSettings.MaxSmokeSprites);
}

bool UDNEffectsSubsystem::Spec(EDNEffect Effect, FDNEffectSpec& Out) const
{
	const int32 Index = static_cast<int32>(Effect);
	if (!SpecValid.IsValidIndex(Index) || !SpecValid[Index])
	{
		return false;
	}
	Out = Specs[Index];
	return true;
}

UNiagaraSystem* UDNEffectsSubsystem::System(EDNEffect Effect)
{
	LoadData();
	const int32 Index = static_cast<int32>(Effect);
	if (!SystemLooked.IsValidIndex(Index))
	{
		return nullptr;
	}
	if (!SystemLooked[Index])
	{
		SystemLooked[Index] = true;
		const FString Name = FString::Printf(TEXT("NS_DN%s"), FDNEffects::EffectName(Effect));
		Systems[Index] = LoadObject<UNiagaraSystem>(nullptr, *FString::Printf(TEXT("%s/%s.%s"), EffectsDir, *Name, *Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Systems[Index])
		{
			UE_LOG(LogDetonauto, Display, TEXT("DNFx: sem o sistema %s/%s; o efeito %s fica só no log até ser montado (lote 19)"), EffectsDir, *Name,
				FDNEffects::EffectName(Effect));
		}
	}
	return Systems[Index];
}

int32 UDNEffectsSubsystem::Burst(EDNEffect Effect, int32 Count, const FVector& Location, const FVector& Direction, const FVector& VelocityCmS, const FVector& Extent)
{
	FDNEffectSpec S;
	if (Count <= 0 || !Spec(Effect, S))
	{
		return 0;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	const int32 Granted = GpuBudget.Request(Count, EffectsSettings.MaxGpuParticles, Now, S.LifeMaxS);
	const int32 Index = static_cast<int32>(Effect);
	if (Granted <= 0)
	{
		UE_LOG(LogDetonauto, Display, TEXT("DNFx: %s cortado pelo teto (%d partículas vivas de %d)"), FDNEffects::EffectName(Effect),
			GpuBudget.Active(Now), EffectsSettings.MaxGpuParticles);
		return 0;
	}
	bool bRecycled = false;
	const int32 Slot = Pools[Index].Acquire(S.PoolMax, Now, S.LifeMaxS, bRecycled);
	TArray<TWeakObjectPtr<UNiagaraComponent>>& Live = Slots[Index];
	if (Live.Num() <= Slot)
	{
		Live.SetNum(Slot + 1);
	}
	if (UNiagaraComponent* Old = Live[Slot].Get())
	{
		Old->DestroyComponent(); // E9: a vaga mais antiga dá lugar ao efeito novo
	}
	Live[Slot] = nullptr;
	++Bursts[Index];
	UNiagaraSystem* Asset = System(Effect);
	if (Asset)
	{
		UNiagaraComponent* Fx = UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), Asset, Location, Direction.Rotation(), FVector(1.0f),
			true, false, ENCPoolMethod::None);
		if (Fx)
		{
			Fx->SetVariableInt(TEXT("Count"), Granted);
			Fx->SetVariableFloat(TEXT("LifeMin"), S.LifeMinS);
			Fx->SetVariableFloat(TEXT("LifeMax"), S.LifeMaxS);
			Fx->SetVariableFloat(TEXT("SpeedMin"), S.SpeedMinMs * 100.0f);
			Fx->SetVariableFloat(TEXT("SpeedMax"), S.SpeedMaxMs * 100.0f);
			Fx->SetVariableFloat(TEXT("Extra"), S.Extra);
			Fx->SetVariableVec3(TEXT("Direction"), Direction.GetSafeNormal());
			Fx->SetVariableVec3(TEXT("Velocity"), VelocityCmS);
			Fx->SetVariableVec3(TEXT("Extent"), Extent);
			Fx->Activate(true);
			Live[Slot] = Fx;
		}
	}
	// o raspão chega várias vezes por segundo: registra o 1º e depois um a cada 20
	if (Effect != EDNEffect::SparkScrape || Bursts[Index] % 20 == 1)
	{
		UE_LOG(LogDetonauto, Display, TEXT("DNFx: %s %d de %d partículas em %s (vaga %d%s)%s"), FDNEffects::EffectName(Effect), Granted, Count,
			*Location.ToCompactString(), Slot, bRecycled ? TEXT(", reaproveitada") : TEXT(""), Asset ? TEXT("") : TEXT(" [sem asset]"));
	}
	return Granted;
}

int32 UDNEffectsSubsystem::OnImpact(const FVector& Point, const FVector& Normal, const FVector& CarVelocityCmS, float DeltaVMs)
{
	FDNEffectSpec S;
	int32 Sparks = 0;
	if (Spec(EDNEffect::SparkImpact, S))
	{
		Sparks = Burst(EDNEffect::SparkImpact, FDNEffects::ImpactSparks(S, DeltaVMs), Point, Normal, CarVelocityCmS, FVector::ZeroVector);
	}
	if (Spec(EDNEffect::PlasticChips, S))
	{
		Burst(EDNEffect::PlasticChips, FDNEffects::PlasticChips(S, DeltaVMs), Point, Normal, CarVelocityCmS, FVector::ZeroVector);
	}
	return Sparks;
}

void UDNEffectsSubsystem::OnScrape(const FVector& Point, const FVector& CarVelocityCmS, float SpeedMs, float IntervalS)
{
	FDNEffectSpec S;
	if (!Spec(EDNEffect::SparkScrape, S))
	{
		return;
	}
	// as faíscas saem para trás do carro, rente ao chão
	const int32 Count = FMath::RoundToInt(FDNEffects::ScrapeSparkRate(S, SpeedMs) * FMath::Clamp(IntervalS, 0.0f, 0.25f));
	Burst(EDNEffect::SparkScrape, Count, Point, -CarVelocityCmS.GetSafeNormal2D(), CarVelocityCmS, FVector::ZeroVector);
}

bool UDNEffectsSubsystem::OnGlassBurst(const FVector& Center, const FVector& Extent, const FVector& VelocityCmS, float AreaM2)
{
	FDNEffectSpec S;
	if (!Spec(EDNEffect::GlassShards, S) || !System(EDNEffect::GlassShards))
	{
		return false;
	}
	Burst(EDNEffect::GlassShards, FDNEffects::GlassShards(S, AreaM2), Center, FVector::UpVector, VelocityCmS, Extent);
	return true;
}

void UDNEffectsSubsystem::OnPropBroken(ADNBreakableProp& Prop)
{
	const FString Kind = Prop.Kind.ToString();
	FVector Origin, Box;
	Prop.GetActorBounds(false, Origin, Box);
	for (const EDNEffect Effect : { EDNEffect::PropDust, EDNEffect::PropLeaves })
	{
		FDNEffectSpec S;
		if (!Spec(Effect, S) || !S.Props.ContainsByPredicate([&Kind](const FString& K) { return K.Equals(Kind, ESearchCase::IgnoreCase); }))
		{
			continue;
		}
		// pó na base; folhas na copa (o alto da caixa da peça)
		const FVector At = Effect == EDNEffect::PropDust ? Prop.GetActorLocation() : Origin + FVector(0.0f, 0.0f, Box.Z * 0.6f);
		const int32 Count = FMath::RandRange(S.CountMin, FMath::Max(S.CountMin, S.CountMax));
		Burst(Effect, Count, At, FVector::UpVector, FVector::ZeroVector, Box);
	}
}

bool UDNEffectsSubsystem::ClaimContinuous(EDNEffect Effect, const UObject* Owner)
{
	FDNEffectSpec S;
	const int32 Index = static_cast<int32>(Effect);
	if (!Owner || !Spec(Effect, S))
	{
		return false;
	}
	TArray<TWeakObjectPtr<const UObject>>& Owners = ContinuousOwners[Index];
	Owners.RemoveAll([](const TWeakObjectPtr<const UObject>& O) { return !O.IsValid(); });
	if (Owners.Contains(Owner))
	{
		return true;
	}
	if (Owners.Num() >= FMath::Max(1, S.PoolMax))
	{
		return false;
	}
	Owners.Add(Owner);
	return true;
}

void UDNEffectsSubsystem::ReleaseContinuous(EDNEffect Effect, const UObject* Owner)
{
	ContinuousOwners[static_cast<int32>(Effect)].Remove(Owner);
}

float UDNEffectsSubsystem::ReportSmoke(const UObject* Owner, float RequestedSprites)
{
	// E11: a escala sai da soma do quadro anterior (cada dono informa uma vez por quadro)
	if (SmokeFrame != GFrameCounter)
	{
		SmokeFrame = GFrameCounter;
		float Total = 0.0f;
		for (auto It = SmokeRequests.CreateIterator(); It; ++It)
		{
			if (!It.Key().IsValid())
			{
				It.RemoveCurrent();
				continue;
			}
			Total += It.Value();
		}
		SmokeScaleNow = FDNEffects::SmokeScale(Total, EffectsSettings.MaxSmokeSprites);
	}
	SmokeRequests.Add(Owner, FMath::Max(0.0f, RequestedSprites));
	return SmokeScaleNow;
}

void UDNEffectsSubsystem::ResetAll()
{
	for (int32 i = 0; i < EffectCount; ++i)
	{
		for (const TWeakObjectPtr<UNiagaraComponent>& Fx : Slots[i])
		{
			if (UNiagaraComponent* C = Fx.Get())
			{
				C->DestroyComponent();
			}
		}
		Slots[i].Reset();
		Pools[i].Reset();
	}
	GpuBudget.Reset();
	SmokeRequests.Reset();
	SmokeScaleNow = 1.0f;
	UE_LOG(LogDetonauto, Display, TEXT("DNFx: reinício: disparos apagados, pools e teto zerados"));
}
