// Testes da colisão do mobiliário destrutível (lote 17c): quem enxerga o destroço e quem passa por ele. Sem mundo e sem física: os
// objetos padrão (CDO) do carro e da peça, e um componente solto para MakeDebris.
//
// Por que existe: o traço da suspensão das rodas da Chaos corre no canal WorldDynamic. Com o destroço nesse tipo, o carro subia no
// banco que voava à frente dele, sem contato nenhum no chassi, e saía lançado a 100 km/h girando (visto na fileira de 16 peças).
//  C1 a base (ou o toco) é destroço desde o início: o carro e as rodas passam por ela;
//  C2 a peça inteira é WorldDynamic e segura o carro (é o contato que a quebra);
//  C3 MakeDebris: tipo ECC_DNDebris, ignora carro e pedestre, bloqueia o resto (chão, prédios, peças inteiras, outro destroço, tráfego);
//  C4 as rodas do carro ignoram destroço e continuam vendo chão, prédios e o tráfego (PhysicsBody);
//  C5 peça do carro pendurada ou solta (lote 18b): destroço como o C3, mas a câmera a atravessa. A esfera da câmera de batida sai do
//     meio do carro e parava na peça ao lado a 10% do caminho, com a câmera dentro do carro (visto na batida na parede a 100 km/h).

#include "Misc/AutomationTest.h"
#include "Detonauto.h"
#include "Crash/DNBreakableProp.h"
#include "Vehicle/DNCarPart.h"
#include "Vehicle/DNVehicle.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/StaticMeshComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FDNBreakableCollisionSpec, "Detonauto.Mobiliario.Colisao",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FDNBreakableCollisionSpec)

void FDNBreakableCollisionSpec::Define()
{
	It("C1 a base e destroco desde o inicio: o carro e as rodas passam por ela", [this]()
	{
		const UStaticMeshComponent* Base = GetDefault<ADNBreakableProp>()->GetBaseComponent();
		TestEqual("tipo", static_cast<int32>(Base->GetCollisionObjectType()), static_cast<int32>(ECC_DNDebris));
		TestEqual("carro", static_cast<int32>(Base->GetCollisionResponseToChannel(ECC_Vehicle)), static_cast<int32>(ECR_Ignore));
		TestEqual("pedestre", static_cast<int32>(Base->GetCollisionResponseToChannel(ECC_Pawn)), static_cast<int32>(ECR_Ignore));
	});

	It("C2 a peca inteira e WorldDynamic e segura o carro", [this]()
	{
		const UStaticMeshComponent* Body = GetDefault<ADNBreakableProp>()->GetBodyComponent();
		TestEqual("tipo", static_cast<int32>(Body->GetCollisionObjectType()), static_cast<int32>(ECC_WorldDynamic));
		TestEqual("carro", static_cast<int32>(Body->GetCollisionResponseToChannel(ECC_Vehicle)), static_cast<int32>(ECR_Block));
	});

	It("C3 MakeDebris: tipo destroco, ignora carro e pedestre, bloqueia o resto", [this]()
	{
		UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(GetTransientPackage());
		Comp->SetCollisionObjectType(ECC_WorldDynamic);
		Comp->SetCollisionResponseToAllChannels(ECR_Block);
		ADNBreakableProp::MakeDebris(*Comp);
		TestEqual("tipo", static_cast<int32>(Comp->GetCollisionObjectType()), static_cast<int32>(ECC_DNDebris));
		TestEqual("carro", static_cast<int32>(Comp->GetCollisionResponseToChannel(ECC_Vehicle)), static_cast<int32>(ECR_Ignore));
		TestEqual("pedestre", static_cast<int32>(Comp->GetCollisionResponseToChannel(ECC_Pawn)), static_cast<int32>(ECR_Ignore));
		for (const ECollisionChannel Channel : { ECC_WorldStatic, ECC_WorldDynamic, ECC_PhysicsBody, ECC_DNDebris })
		{
			TestEqual(FString::Printf(TEXT("bloqueia o canal %d"), static_cast<int32>(Channel)),
				static_cast<int32>(Comp->GetCollisionResponseToChannel(Channel)), static_cast<int32>(ECR_Block));
		}
	});

	It("C4 as rodas do carro ignoram destroco e veem chao, predios e trafego", [this]()
	{
		const UChaosWheeledVehicleMovementComponent* Move =
			CastChecked<UChaosWheeledVehicleMovementComponent>(GetDefault<ADNVehicle>()->GetVehicleMovementComponent());
		const FCollisionResponseContainer& R = Move->WheelTraceCollisionResponses;
		TestEqual("destroco", static_cast<int32>(R.GetResponse(ECC_DNDebris)), static_cast<int32>(ECR_Ignore));
		for (const ECollisionChannel Channel : { ECC_WorldStatic, ECC_WorldDynamic, ECC_PhysicsBody })
		{
			TestEqual(FString::Printf(TEXT("ve o canal %d"), static_cast<int32>(Channel)),
				static_cast<int32>(R.GetResponse(Channel)), static_cast<int32>(ECR_Block));
		}
	});

	It("C5 peca do carro: destroco que a camera atravessa", [this]()
	{
		UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(GetTransientPackage());
		Comp->SetCollisionObjectType(ECC_WorldDynamic);
		Comp->SetCollisionResponseToAllChannels(ECR_Block);
		ADNCarPart::MakePartDebris(*Comp);
		TestEqual("tipo", static_cast<int32>(Comp->GetCollisionObjectType()), static_cast<int32>(ECC_DNDebris));
		TestEqual("carro", static_cast<int32>(Comp->GetCollisionResponseToChannel(ECC_Vehicle)), static_cast<int32>(ECR_Ignore));
		TestEqual("camera", static_cast<int32>(Comp->GetCollisionResponseToChannel(ECC_Camera)), static_cast<int32>(ECR_Ignore));
		TestEqual("chao", static_cast<int32>(Comp->GetCollisionResponseToChannel(ECC_WorldStatic)), static_cast<int32>(ECR_Block));
	});
}

#endif
