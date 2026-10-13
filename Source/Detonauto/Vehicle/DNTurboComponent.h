// Cola fina entre o carro e o turbo puro: repassa as entradas do quadro para FDNTurbo::Tick e expõe estado e eventos.
// Nenhuma regra aqui. Testes: Core/Tests/DNComponents.spec.cpp.
//
// K1: os eventos são delegates nativos; a versão dinâmica (Blueprint/UI) entra quando a UI pedir.
// K2: o componente não tica sozinho: quem tem as entradas (o veículo) chama Step() a cada quadro.
// K3: um evento só dispara quando o valor muda de verdade (comparação antes/depois do Core).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/DNTurbo.h"
#include "DNTurboComponent.generated.h"

class UDNGameDataSubsystem;

DECLARE_MULTICAST_DELEGATE_OneParam(FDNOnTurboActiveChanged, bool /*bActive*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FDNOnTurboBarChanged, float /*Bar*/);

UCLASS()
class DETONAUTO_API UDNTurboComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDNTurboComponent();

	virtual void BeginPlay() override;

	/** Troca os números do turbo. */
	void Configure(const FDNTurboConfig& NewConfig) { Config = NewConfig; }

	/** Pega os números do CSV (ou o padrão, se o subsistema é nulo ou a tabela é inválida). */
	void ConfigureFromData(const UDNGameDataSubsystem* DataSubsystem);

	/** Um quadro: repassa as entradas ao Core e avisa o que mudou. */
	void Step(const FDNTurboFrameInput& Input);

	/** Volta ao estado inicial (barra vazia, desligado) e avisa o que mudou. */
	void ResetState();

	const FDNTurboState& GetState() const { return State; }
	float GetBar() const { return State.Bar; }
	bool IsActive() const { return State.bActive; }

	FDNOnTurboActiveChanged OnActiveChanged;
	FDNOnTurboBarChanged OnBarChanged;

private:
	void Broadcast(float BarBefore, bool bActiveBefore);

	FDNTurboConfig Config;
	FDNTurboState State;
};
