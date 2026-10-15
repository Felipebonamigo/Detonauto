#include "Crash/DNBreakableSubsystem.h"
#include "Crash/DNBreakableProp.h"
#include "Core/DNDamage.h"
#include "Core/DNGameDataSubsystem.h"
#include "Detonauto.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

void UDNBreakableSubsystem::OnPropBroken(ADNBreakableProp& Prop)
{
	Admit(Prop);
	++Broken;
	OnBroken.Broadcast(Prop);
}

void UDNBreakableSubsystem::OnPropWoke(ADNBreakableProp& Prop)
{
	Admit(Prop);
}

void UDNBreakableSubsystem::Admit(ADNBreakableProp& Prop)
{
	Active.RemoveAll([](const TWeakObjectPtr<ADNBreakableProp>& P) { return !P.IsValid(); });
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UDNGameDataSubsystem* Data = GameInstance ? GameInstance->GetSubsystem<UDNGameDataSubsystem>() : nullptr;
	const int32 MaxActive = (Data ? Data->GetBreakableSettings() : FDNBreakableSettings()).MaxActiveProps;
	const FDNPartPoolPlan Plan = FDNPartPool::Plan(MaxActive, Active.Num(), 1);
	for (int32 i = 0; i < Plan.Recycle && !Active.IsEmpty(); ++i)
	{
		// B10: a mais lenta, nunca uma no ar se houver outra no chão (congelada no ar ela ficaria flutuando)
		TArray<FDNFreezeCandidate> Candidates;
		for (const TWeakObjectPtr<ADNBreakableProp>& P : Active)
		{
			Candidates.Add(P.IsValid() ? P->FreezeCandidate() : FDNFreezeCandidate());
		}
		const int32 Pick = FDNBreakable::PickToFreeze(Candidates);
		if (ADNBreakableProp* Chosen = Active.IsValidIndex(Pick) ? Active[Pick].Get() : nullptr)
		{
			UE_LOG(LogDetonauto, Display, TEXT("DNProp: orçamento de %d peças soltas cheio: congela %s (%s) a %.1f m/s, %s, entre %d"), MaxActive,
				*Chosen->Kind.ToString(), *Chosen->GetName(), Candidates[Pick].SpeedMs, Candidates[Pick].bGrounded ? TEXT("no chão") : TEXT("no ar"),
				Candidates.Num());
			Chosen->Freeze(); // congela onde está e sai da lista (OnPropFrozen)
			Active.RemoveAll([Chosen](const TWeakObjectPtr<ADNBreakableProp>& P) { return P.Get() == Chosen; }); // mesmo se já não simulava
		}
		else if (Active.IsValidIndex(Pick))
		{
			Active.RemoveAt(Pick);
		}
	}
	if (Plan.Accepted > 0)
	{
		Active.Add(&Prop);
	}
	else
	{
		Prop.Freeze();
	}
}

void UDNBreakableSubsystem::OnPropFrozen(ADNBreakableProp& Prop)
{
	Active.RemoveAll([&Prop](const TWeakObjectPtr<ADNBreakableProp>& P) { return !P.IsValid() || P.Get() == &Prop; });
}
