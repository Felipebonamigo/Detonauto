// Orçamento das peças de rua soltas (lote 17c, docs/regras-mobiliario.md, "Física e custo"): no máximo MaxActiveProps simulando ao
// mesmo tempo, pela mesma regra do FDNPartPool::Plan (orçamento próprio, separado das peças do carro). Passando do limite, a mais
// antiga congela onde está: nada some à vista. Também avisa quem quiser saber de cada quebra (as capturas de teste).

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DNBreakableSubsystem.generated.h"

class ADNBreakableProp;

DECLARE_MULTICAST_DELEGATE_OneParam(FDNOnPropBroken, ADNBreakableProp&);

UCLASS()
class DETONAUTO_API UDNBreakableSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Uma peça acabou de quebrar e simular: entra no orçamento (congelando as mais antigas, se preciso). */
	void OnPropBroken(ADNBreakableProp& Prop);

	/** Um destroço que já dormiu voltou a simular (o carro o varreu, B9): só entra no orçamento; não é quebra nova. */
	void OnPropWoke(ADNBreakableProp& Prop);

	/** Uma peça parou de simular (dormiu ou foi congelada). */
	void OnPropFrozen(ADNBreakableProp& Prop);

	int32 ActiveCount() const { return Active.Num(); }
	int32 BrokenTotal() const { return Broken; }

	FDNOnPropBroken OnBroken;

private:
	/** As peças simulando, da mais antiga para a mais nova. */
	TArray<TWeakObjectPtr<ADNBreakableProp>> Active;
	int32 Broken = 0;

	/** Põe a peça no orçamento, congelando as mais antigas se preciso. */
	void Admit(ADNBreakableProp& Prop);
};
