#include "Crash/DNCrashRoundComponent.h"
#include "Core/DNGameDataSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

UDNCrashRoundComponent::UDNCrashRoundComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDNCrashRoundComponent::BeginPlay()
{
	Super::BeginPlay();
	// Só com mundo de jogo; sem ele (testes) quem chama usa Configure/ConfigureFromData.
	if (const UWorld* World = GetWorld())
	{
		if (const UGameInstance* GameInstance = World->GetGameInstance())
		{
			ConfigureFromData(GameInstance->GetSubsystem<UDNGameDataSubsystem>());
		}
	}
}

void UDNCrashRoundComponent::ConfigureFromData(const UDNGameDataSubsystem* DataSubsystem)
{
	Config = DataSubsystem ? DataSubsystem->GetCrashConfig() : FDNCrashConfig();
}

void UDNCrashRoundComponent::RegisterImpact()
{
	FDNCrash::RegisterImpact(Config, State);
}

int32 UDNCrashRoundComponent::RegisterDestruction(int32 Value)
{
	const int32 ScoreBefore = State.Score.Total;
	const int32 MultiplierBefore = State.Multiplier;
	const bool bEndedBefore = State.bEnded;
	const int32 Points = FDNCrash::RegisterDestruction(Config, State, Value);
	Broadcast(ScoreBefore, MultiplierBefore, bEndedBefore);
	return Points;
}

int32 UDNCrashRoundComponent::RegisterDamage(FDNCrashVehicleLedger& Ledger, int32 Value, float AddedDamageFraction, bool bDestroyed)
{
	const int32 ScoreBefore = State.Score.Total;
	const int32 MultiplierBefore = State.Multiplier;
	const bool bEndedBefore = State.bEnded;
	const int32 Points = FDNCrash::RegisterDamage(Config, State, Ledger, Value, AddedDamageFraction, bDestroyed);
	Broadcast(ScoreBefore, MultiplierBefore, bEndedBefore);
	return Points;
}

void UDNCrashRoundComponent::CollectMultiplier()
{
	const int32 ScoreBefore = State.Score.Total;
	const int32 MultiplierBefore = State.Multiplier;
	const bool bEndedBefore = State.bEnded;
	FDNCrash::CollectMultiplier(Config, State);
	Broadcast(ScoreBefore, MultiplierBefore, bEndedBefore);
}

void UDNCrashRoundComponent::Step(float DeltaTime, float MaxVehicleSpeedMps)
{
	const int32 ScoreBefore = State.Score.Total;
	const int32 MultiplierBefore = State.Multiplier;
	const bool bEndedBefore = State.bEnded;
	FDNCrash::Tick(Config, State, DeltaTime, MaxVehicleSpeedMps);
	Broadcast(ScoreBefore, MultiplierBefore, bEndedBefore);
}

void UDNCrashRoundComponent::ResetRound()
{
	const int32 ScoreBefore = State.Score.Total;
	const int32 MultiplierBefore = State.Multiplier;
	const bool bEndedBefore = State.bEnded;
	State.Reset();
	Broadcast(ScoreBefore, MultiplierBefore, bEndedBefore);
}

void UDNCrashRoundComponent::Broadcast(int32 ScoreBefore, int32 MultiplierBefore, bool bEndedBefore)
{
	if (State.Score.Total != ScoreBefore)
	{
		OnScoreChanged.Broadcast(State.Score.Total);
	}
	if (State.Multiplier != MultiplierBefore)
	{
		OnMultiplierChanged.Broadcast(State.Multiplier);
	}
	if (State.bEnded && !bEndedBefore)
	{
		OnRoundEnded.Broadcast(State.Score.Total);
	}
}
