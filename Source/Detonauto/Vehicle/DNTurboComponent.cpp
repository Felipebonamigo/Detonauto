#include "Vehicle/DNTurboComponent.h"
#include "Core/DNGameDataSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

UDNTurboComponent::UDNTurboComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDNTurboComponent::BeginPlay()
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

void UDNTurboComponent::ConfigureFromData(const UDNGameDataSubsystem* DataSubsystem)
{
	Config = DataSubsystem ? DataSubsystem->GetTurboConfig() : FDNTurboConfig();
}

void UDNTurboComponent::Step(const FDNTurboFrameInput& Input)
{
	const float BarBefore = State.Bar;
	const bool bActiveBefore = State.bActive;
	FDNTurbo::Tick(Config, State, Input);
	Broadcast(BarBefore, bActiveBefore);
}

void UDNTurboComponent::ResetState()
{
	const float BarBefore = State.Bar;
	const bool bActiveBefore = State.bActive;
	State.Reset();
	Broadcast(BarBefore, bActiveBefore);
}

void UDNTurboComponent::Broadcast(float BarBefore, bool bActiveBefore)
{
	if (State.bActive != bActiveBefore)
	{
		OnActiveChanged.Broadcast(State.bActive);
	}
	if (State.Bar != BarBefore)
	{
		OnBarChanged.Broadcast(State.Bar);
	}
}
