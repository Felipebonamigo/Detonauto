// Testes da conta das rodas visuais. O giro (pitch) e o esterçamento (yaw) que o Chaos calcula valem no espaço do COMPONENTE (como no
// FAnimNode_WheelController do template), e a roda visual é uma malha estática presa a um osso que pode ter qualquer orientação.
//
// Lote 14 (B1) fez Osso^-1 x Giro x Osso: o giro ficou no eixo certo, mas a malha herdava a orientação do osso. Os ossos das rodas do
// SportsCar estão girados ±90 graus em Z no pose de referência e a malha da roda tem o eixo do cubo no Y local: com aquela conta o
// eixo da roda apontava para a frente do carro ("rodas de lado", visto de lado; de trás o erro se disfarça). Os testes do lote 14 só
// olhavam o eixo do giro, não o da malha.
//
// Decisões (lote 16):
//  R1 a orientação da malha no espaço do componente é Giro x Base, qualquer que seja o osso (a relativa é Osso^-1 x Giro x Base);
//  R2 sem giro nem esterço, o eixo do cubo da malha (Y local) fica no Y do componente; Base = 180 graus em Z vira a malha para o
//     outro lado (face do aro para fora nas rodas do lado oposto);
//  R3 o giro puro é uma rotação em torno do Y do componente e o esterço puro em torno do Z, com qualquer osso.

#include "Misc/AutomationTest.h"
#include "Core/DNWheelVisual.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FDNWheelVisualSpec, "Detonauto.Carro.RodaVisual",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	static bool SameRotation(const FQuat& A, const FQuat& B) { return A.Equals(B, 1.0e-4f) || A.Equals(-B, 1.0e-4f); }
	TArray<FRotator> Bones() const
	{
		return { FRotator(0, 90, 0), FRotator(0, -90, 0), FRotator(0, 0, 0), FRotator(0, 180, 0), FRotator(15, 40, 10), FRotator(90, 0, 0), FRotator(0, 0, 90) };
	}
END_DEFINE_SPEC(FDNWheelVisualSpec)

void FDNWheelVisualSpec::Define()
{
	Describe("RelativeRotation", [this]()
	{
		It("R1: osso x relativa = giro x base, qualquer que seja o osso", [this]()
		{
			for (const FRotator& BoneRotator : Bones())
			{
				const FQuat Bone(BoneRotator);
				for (const float BaseYaw : { 0.f, 180.f })
				{
					const FQuat World = Bone * FDNWheelVisual::RelativeRotation(Bone, 37.f, -12.f, BaseYaw);
					const FQuat Expected = FQuat(FRotator(37.f, -12.f, 0.f)) * FQuat(FRotator(0.f, BaseYaw, 0.f));
					TestTrue(FString::Printf(TEXT("osso %s, base %.0f"), *BoneRotator.ToString(), BaseYaw), SameRotation(World, Expected));
				}
			}
		});

		It("R2: sem giro nem esterco, o eixo do cubo (Y local da malha) fica no Y do componente; base 180 vira para -Y", [this]()
		{
			for (const FRotator& BoneRotator : Bones())
			{
				const FQuat Bone(BoneRotator);
				const FVector Axle0 = (Bone * FDNWheelVisual::RelativeRotation(Bone, 0.f, 0.f, 0.f)).RotateVector(FVector::YAxisVector);
				const FVector Axle180 = (Bone * FDNWheelVisual::RelativeRotation(Bone, 0.f, 0.f, 180.f)).RotateVector(FVector::YAxisVector);
				TestTrue(FString::Printf(TEXT("osso %s, base 0: eixo %s"), *BoneRotator.ToString(), *Axle0.ToString()), Axle0.Equals(FVector::YAxisVector, 1.0e-4));
				TestTrue(FString::Printf(TEXT("osso %s, base 180: eixo %s"), *BoneRotator.ToString(), *Axle180.ToString()), Axle180.Equals(-FVector::YAxisVector, 1.0e-4));
			}
		});

		It("R2: o giro (pitch) nao tira o eixo do cubo do Y do componente", [this]()
		{
			const FQuat Bone(FRotator(0, 90, 0));
			const FVector Axle = (Bone * FDNWheelVisual::RelativeRotation(Bone, 63.f, 0.f, 0.f)).RotateVector(FVector::YAxisVector);
			TestTrue(FString::Printf(TEXT("eixo %s"), *Axle.ToString()), Axle.Equals(FVector::YAxisVector, 1.0e-4));
		});

		It("R3: o giro puro e uma rotacao em torno do Y do COMPONENTE, qualquer que seja o osso", [this]()
		{
			for (const FRotator& BoneRotator : Bones())
			{
				const FQuat Bone(BoneRotator);
				const FQuat Delta = (Bone * FDNWheelVisual::RelativeRotation(Bone, 45.f, 0.f, 0.f)); // base 0: a orientação é só o giro
				FVector Axis; float Angle;
				Delta.ToAxisAndAngle(Axis, Angle);
				TestEqual(FString::Printf(TEXT("angulo, osso %s"), *BoneRotator.ToString()), FMath::RadiansToDegrees(Angle), 45.f, 0.01f);
				TestTrue(FString::Printf(TEXT("eixo %s e o Y do componente"), *Axis.ToString()), FMath::Abs(FMath::Abs(Axis.Y) - 1.0) < 1.0e-4);
			}
		});

		It("R3: o esterco puro e uma rotacao em torno do Z do COMPONENTE", [this]()
		{
			const FQuat Bone(FRotator(0, -90, 0));
			const FQuat Delta = Bone * FDNWheelVisual::RelativeRotation(Bone, 0.f, 25.f, 0.f);
			FVector Axis; float Angle;
			Delta.ToAxisAndAngle(Axis, Angle);
			TestEqual("angulo", FMath::RadiansToDegrees(Angle), 25.f, 0.01f);
			TestTrue("eixo Z", FMath::Abs(FMath::Abs(Axis.Z) - 1.0) < 1.0e-4);
		});
	});

	Describe("RelativeLocation", [this]()
	{
		It("o curso da suspensao vem no espaco do componente e e levado para o espaco do osso", [this]()
		{
			const FQuat Bone(FRotator(0, 90, 0)); // gira 90 graus em Z: X do osso aponta para Y do componente
			const FVector Local = FDNWheelVisual::RelativeLocation(Bone, FVector(0, 0, 10)); // 10 cm para cima (Z): Z nao muda
			TestTrue("Z fica", Local.Equals(FVector(0, 0, 10), 1.0e-3));
			const FVector Side = FDNWheelVisual::RelativeLocation(Bone, FVector(0, 10, 0)); // 10 cm em Y do componente = 10 cm em X do osso
			TestTrue(FString::Printf(TEXT("Y do componente vira X do osso: %s"), *Side.ToString()), Side.Equals(FVector(10, 0, 0), 1.0e-3));
		});

		It("osso alinhado: o deslocamento passa igual", [this]()
		{
			TestTrue("igual", FDNWheelVisual::RelativeLocation(FQuat::Identity, FVector(1, 2, 3)).Equals(FVector(1, 2, 3), 1.0e-6));
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
