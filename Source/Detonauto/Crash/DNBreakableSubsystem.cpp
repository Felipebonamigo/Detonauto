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
	for (int32 i = 0; i < Plan.Recycle; ++i)
	{
		if (ADNBreakableProp* Oldest = Active.IsEmpty() ? nullptr : Active[0].Get())
		{
			Oldest->Freeze(); // congela onde está e sai da lista (OnPropFrozen)
		}
		if (!Active.IsEmpty() && Active[0].Get() == nullptr)
		{
			Active.RemoveAt(0);
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
	if (Plan.Recycle > 0)
	{
		UE_LOG(LogDetonauto, Display, TEXT("DNProp: orçamento de %d peças soltas cheio: %d congelada(s) onde estavam"), MaxActive, Plan.Recycle);
	}
}

void UDNBreakableSubsystem::OnPropFrozen(ADNBreakableProp& Prop)
{
	Active.RemoveAll([&Prop](const TWeakObjectPtr<ADNBreakableProp>& P) { return !P.IsValid() || P.Get() == &Prop; });
}
