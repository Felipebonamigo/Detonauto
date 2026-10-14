// Testes da conta das rodas visuais (lote 14, B1): o giro (pitch) e o esterçamento (yaw) que o Chaos calcula valem no espaço do
// COMPONENTE (como no FAnimNode_WheelController do template), e a roda visual está presa a um osso que pode ter qualquer orientação.
// A rotação relativa ao osso precisa ser Osso^-1 x Giro x Osso. Com a conta errada (aplicar o giro no espaço do osso) a roda gira
// em torno do eixo errado quando o osso não tem o eixo da roda em Y: foi o "rodas de lado" do playtest 1.

#include "Misc/AutomationTest.h"
#include "Core/DNWheelVisual.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FDNWheelVisualSpec, "Detonauto.Carro.RodaVisual",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	static bool SameRotation(const FQuat& A, const FQuat& B) { return A.Equals(B, 1.0e-4f) || A.Equals(-B, 1.0e-4f); }
END_DEFINE_SPEC(FDNWheelVisualSpec)

void FDNWheelVisualSpec::Define()
{
	Describe("RelativeRotation", [this]()
	{
		It("osso alinhado com o componente: a relativa e o proprio giro e esterco", [this]()
		{
			const FQuat Off(FRotator(30.f, 20.f, 0.f));
			TestTrue("igual ao giro", SameRotation(FDNWheelVisual::RelativeRotation(FQuat::Identity, 30.f, 20.f), Off));
		});

		It("osso girado (qualquer orientacao): osso x relativa = giro x osso, ou seja, o giro vale no espaco do componente", [this]()
		{
			for (const FRotator& BoneRotator : { FRotator(0, 90, 0), FRotator(0, -90, 0), FRotator(0, 180, 0), FRotator(15, 40, 10), FRotator(90, 0, 0), FRotator(0, 0, 90) })
			{
				const FQuat Bone(BoneRotator);
				const FQuat Off(FRotator(37.f, -12.f, 0.f));
				const FQuat World = Bone * FDNWheelVisual::RelativeRotation(Bone, 37.f, -12.f);
				TestTrue(FString::Printf(TEXT("osso %s"), *BoneRotator.ToString()), SameRotation(World, Off * Bone));
			}
		});

		It("o giro puro (pitch) e uma rotacao em torno do eixo Y do COMPONENTE, qualquer que seja o osso", [this]()
		{
			const FQuat Bone(FRotator(0, 90, 0));
			const FQuat World = Bone * FDNWheelVisual::RelativeRotation(Bone, 45.f, 0.f);
			// World x Osso^-1 = rotacao nova aplicada por cima do osso, no espaco do componente: eixo Y, 45 graus
			const FQuat Delta = World * Bone.Inverse();
			FVector Axis; float Angle;
			Delta.ToAxisAndAngle(Axis, Angle);
			TestEqual("angulo", FMath::RadiansToDegrees(Angle), 45.f, 0.01f);
			TestTrue(FString::Printf(TEXT("eixo %s e o Y do componente"), *Axis.ToString()), FMath::Abs(FMath::Abs(Axis.Y) - 1.0) < 1.0e-4);
		});

		It("o esterco puro (yaw) e uma rotacao em torno do eixo Z do COMPONENTE", [this]()
		{
			const FQuat Bone(FRotator(0, 90, 0));
			const FQuat Delta = (Bone * FDNWheelVisual::RelativeRotation(Bone, 0.f, 25.f)) * Bone.Inverse();
			FVector Axis; float Angle;
			Delta.ToAxisAndAngle(Axis, Angle);
			TestEqual("angulo", FMath::RadiansToDegrees(Angle), 25.f, 0.01f);
			TestTrue("eixo Z", FMath::Abs(FMath::Abs(Axis.Z) - 1.0) < 1.0e-4);
		});

		It("giro e esterco zero: a relativa e a identidade, com qualquer osso (a roda fica como no pose de referencia)", [this]()
		{
			TestTrue("identidade", SameRotation(FDNWheelVisual::RelativeRotation(FQuat(FRotator(10, 60, 20)), 0.f, 0.f), FQuat::Identity));
		});

		It("o que NAO e a conta certa (Giro no espaco do osso) da outra rotacao com osso girado", [this]()
		{
			const FQuat Bone(FRotator(0, 90, 0));
			const FQuat Off(FRotator(37.f, 0.f, 0.f));
			TestFalse("conta errada difere", SameRotation(Bone * Off, Off * Bone));
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
