// Testes das peças que soltam e dos vidros que quebram (lote 18b, docs/regras-pecas.md): Presa → Pendurada → Solta pelo dano da peça
// (o maior entre a zona e a secundária) ou pelo Δv na zona, limiares inclusivos, sem volta; trinca e estouro dos vidros. Os números
// são os da tabela do contrato. Decisões P1 a P4: Core/DNCarParts.h.

#include <limits>
#include "Misc/AutomationTest.h"
#include "Core/DNCarParts.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FDNCarPartSpec Part(EDNDamagePart P, EDNDamageZone Zone, float Hang, float HangDv, float Detach, float DetachDv, float HingeDeg)
	{
		FDNCarPartSpec S;
		S.Part = P;
		S.Zone = Zone;
		S.HangDamage = Hang;
		S.HangDeltaVMs = HangDv;
		S.DetachDamage = Detach;
		S.DetachDeltaVMs = DetachDv;
		S.HingeMaxDeg = HingeDeg;
		return S;
	}

	FDNCarPartSpec WithSecondary(FDNCarPartSpec S, EDNDamageZone Secondary)
	{
		S.bHasSecondaryZone = true;
		S.SecondaryZone = Secondary;
		return S;
	}

	FDNCarPartSpec BumperFront() { return Part(EDNDamagePart::BumperFront, EDNDamageZone::Front, 0.30f, 8.f, 0.60f, 12.f, 60.f); }
	FDNCarPartSpec Hood() { return WithSecondary(Part(EDNDamagePart::Hood, EDNDamageZone::Front, 0.35f, 9.f, 0.75f, 16.f, 55.f), EDNDamageZone::Roof); }
	FDNCarPartSpec FenderFrontLeft() { return WithSecondary(Part(EDNDamagePart::FenderFrontLeft, EDNDamageZone::Front, 0.f, 0.f, 0.70f, 14.f, 0.f), EDNDamageZone::Left); }

	/** A tabela inteira do contrato, na ordem dele. */
	TArray<FDNCarPartSpec> ContractParts()
	{
		return {
			BumperFront(),
			Part(EDNDamagePart::BumperRear, EDNDamageZone::Rear, 0.30f, 8.f, 0.60f, 12.f, 60.f),
			Hood(),
			Part(EDNDamagePart::Trunk, EDNDamageZone::Rear, 0.35f, 9.f, 0.75f, 16.f, 55.f),
			Part(EDNDamagePart::DoorLeft, EDNDamageZone::Left, 0.35f, 9.f, 0.80f, 16.f, 70.f),
			Part(EDNDamagePart::DoorRight, EDNDamageZone::Right, 0.35f, 9.f, 0.80f, 16.f, 70.f),
			FenderFrontLeft(),
			WithSecondary(Part(EDNDamagePart::FenderFrontRight, EDNDamageZone::Front, 0.f, 0.f, 0.70f, 14.f, 0.f), EDNDamageZone::Right),
			WithSecondary(Part(EDNDamagePart::FenderRearLeft, EDNDamageZone::Rear, 0.f, 0.f, 0.70f, 14.f, 0.f), EDNDamageZone::Left),
			WithSecondary(Part(EDNDamagePart::FenderRearRight, EDNDamageZone::Rear, 0.f, 0.f, 0.70f, 14.f, 0.f), EDNDamageZone::Right),
			WithSecondary(Part(EDNDamagePart::Spoiler, EDNDamageZone::Rear, 0.f, 0.f, 0.50f, 10.f, 0.f), EDNDamageZone::Roof) };
	}

	FDNImpact Hit(float DeltaV, EDNDamageZone Zone)
	{
		FDNImpact I;
		I.DeltaVMs = DeltaV;
		I.Zone = Zone;
		return I;
	}

	FDNDamageState Damaged(EDNDamageZone Zone, float Value)
	{
		FDNDamageState S;
		S.ZoneDamage[static_cast<int32>(Zone)] = Value;
		return S;
	}

	FDNGlassSpec Glass(EDNGlass G, EDNDamageZone Zone, float CrackFrom, float BurstDamage, float BurstDv)
	{
		FDNGlassSpec S;
		S.Glass = G;
		S.Zone = Zone;
		S.CrackFrom = CrackFrom;
		S.BurstDamage = BurstDamage;
		S.BurstDeltaVMs = BurstDv;
		return S;
	}

	FDNGlassSpec WithSecondary(FDNGlassSpec S, EDNDamageZone Secondary)
	{
		S.bHasSecondaryZone = true;
		S.SecondaryZone = Secondary;
		return S;
	}

	FDNGlassSpec Windshield() { return WithSecondary(Glass(EDNGlass::Windshield, EDNDamageZone::Front, 0.40f, 0.f, 0.f), EDNDamageZone::Roof); }
	FDNGlassSpec SideLeft() { return Glass(EDNGlass::SideLeft, EDNDamageZone::Left, 0.30f, 0.60f, 12.f); }
	FDNGlassSpec RearGlass() { return WithSecondary(Glass(EDNGlass::Rear, EDNDamageZone::Rear, 0.30f, 0.65f, 12.f), EDNDamageZone::Roof); }
	FDNGlassSpec HeadlightLeft() { return Glass(EDNGlass::HeadlightLeft, EDNDamageZone::Front, 0.30f, 0.60f, 0.f); }

	/** Um vidro só, avaliado do zero (nenhum estourado). */
	bool BurstsAlone(const FDNGlassSpec& Spec, const FDNDamageState& Damage, const FDNImpact* Impact)
	{
		TArray<bool> Burst;
		return FDNGlass::Evaluate(MakeArrayView(&Spec, 1), Burst, Damage, Impact).Num() == 1;
	}
}

BEGIN_DEFINE_SPEC(FDNCarPartsSpec, "Detonauto.Dano.PecasDoCarro",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	const float NaNf = std::numeric_limits<float>::quiet_NaN();

	/** Estágio de uma peça só depois de avaliar, partindo de From. */
	EDNPartStage After(const FDNCarPartSpec& Spec, EDNPartStage From, const FDNDamageState& Damage, const FDNImpact* Impact, int32* OutChanges = nullptr)
	{
		TArray<EDNPartStage> Stages = { From };
		const TArray<FDNPartChange> Changes = FDNCarParts::Evaluate(MakeArrayView(&Spec, 1), Stages, Damage, Impact);
		if (OutChanges)
		{
			*OutChanges = Changes.Num();
		}
		return Stages.IsEmpty() ? EDNPartStage::Attached : Stages[0];
	}
END_DEFINE_SPEC(FDNCarPartsSpec)

void FDNCarPartsSpec::Define()
{
	Describe("Pecas: Presa, Pendurada, Solta", [this]()
	{
		It("presa pendura no dano da peca de 0,30 (inclusivo); 0,2999 nao", [this]()
		{
			TestTrue("0,2999 continua presa", After(BumperFront(), EDNPartStage::Attached, Damaged(EDNDamageZone::Front, 0.2999f), nullptr) == EDNPartStage::Attached);
			TestTrue("0,30 pendura", After(BumperFront(), EDNPartStage::Attached, Damaged(EDNDamageZone::Front, 0.30f), nullptr) == EDNPartStage::Hanging);
		});

		It("pendura com Dv >= 8 na zona; 7,99 nao; Dv numa zona que nao e da peca nao conta", [this]()
		{
			const FDNDamageState None;
			const FDNImpact Low = Hit(7.99f, EDNDamageZone::Front);
			const FDNImpact Edge = Hit(8.f, EDNDamageZone::Front);
			const FDNImpact Other = Hit(30.f, EDNDamageZone::Left);
			TestTrue("7,99", After(BumperFront(), EDNPartStage::Attached, None, &Low) == EDNPartStage::Attached);
			TestTrue("8,0", After(BumperFront(), EDNPartStage::Attached, None, &Edge) == EDNPartStage::Hanging);
			TestTrue("30 na esquerda", After(BumperFront(), EDNPartStage::Attached, None, &Other) == EDNPartStage::Attached);
		});

		It("o Dv na zona secundaria tambem conta", [this]()
		{
			const FDNImpact Roof = Hit(9.f, EDNDamageZone::Roof);
			TestTrue("capo com 9 m/s no teto", After(Hood(), EDNPartStage::Attached, FDNDamageState(), &Roof) == EDNPartStage::Hanging);
		});

		It("o dano da peca e o maior entre a zona e a secundaria", [this]()
		{
			FDNDamageState S;
			S.ZoneDamage[static_cast<int32>(EDNDamageZone::Front)] = 0.1f;
			S.ZoneDamage[static_cast<int32>(EDNDamageZone::Roof)] = 0.36f;
			TestEqual("com secundaria", FDNCarParts::ZoneDamage(EDNDamageZone::Front, true, EDNDamageZone::Roof, S), 0.36f);
			TestEqual("sem secundaria", FDNCarParts::ZoneDamage(EDNDamageZone::Front, false, EDNDamageZone::Roof, S), 0.1f);
			TestTrue("capo pendura pelo teto", After(Hood(), EDNPartStage::Attached, S, nullptr) == EDNPartStage::Hanging);
		});

		It("pendurada solta com o dano de 0,60 ou Dv de 12", [this]()
		{
			const FDNImpact Edge = Hit(12.f, EDNDamageZone::Front);
			const FDNImpact Low = Hit(11.99f, EDNDamageZone::Front);
			TestTrue("0,5999", After(BumperFront(), EDNPartStage::Hanging, Damaged(EDNDamageZone::Front, 0.5999f), nullptr) == EDNPartStage::Hanging);
			TestTrue("0,60", After(BumperFront(), EDNPartStage::Hanging, Damaged(EDNDamageZone::Front, 0.60f), nullptr) == EDNPartStage::Detached);
			TestTrue("11,99", After(BumperFront(), EDNPartStage::Hanging, FDNDamageState(), &Low) == EDNPartStage::Hanging);
			TestTrue("12,0", After(BumperFront(), EDNPartStage::Hanging, FDNDamageState(), &Edge) == EDNPartStage::Detached);
		});

		It("o mesmo impacto que pendura e solta leva direto para Solta, numa mudanca so (P3)", [this]()
		{
			const FDNCarPartSpec Spec = BumperFront();
			TArray<EDNPartStage> Stages = { EDNPartStage::Attached };
			const FDNImpact I = Hit(12.f, EDNDamageZone::Front);
			const TArray<FDNPartChange> Changes = FDNCarParts::Evaluate(MakeArrayView(&Spec, 1), Stages, FDNDamageState(), &I);
			if (!TestEqual("uma mudanca", Changes.Num(), 1))
			{
				return;
			}
			TestTrue("de Presa", Changes[0].From == EDNPartStage::Attached);
			TestTrue("para Solta", Changes[0].To == EDNPartStage::Detached);
			TestTrue("estagio", Stages[0] == EDNPartStage::Detached);
		});

		It("sem dobradica: nunca pendura e pula de Presa para Solta (0,70 ou 14 m/s)", [this]()
		{
			const FDNImpact Low = Hit(13.99f, EDNDamageZone::Left);
			const FDNImpact Edge = Hit(14.f, EDNDamageZone::Left);
			TestTrue("0,69 na frente", After(FenderFrontLeft(), EDNPartStage::Attached, Damaged(EDNDamageZone::Front, 0.69f), nullptr) == EDNPartStage::Attached);
			TestTrue("0,70 na esquerda (secundaria)", After(FenderFrontLeft(), EDNPartStage::Attached, Damaged(EDNDamageZone::Left, 0.70f), nullptr) == EDNPartStage::Detached);
			TestTrue("13,99 na esquerda", After(FenderFrontLeft(), EDNPartStage::Attached, FDNDamageState(), &Low) == EDNPartStage::Attached);
			TestTrue("14 na esquerda", After(FenderFrontLeft(), EDNPartStage::Attached, FDNDamageState(), &Edge) == EDNPartStage::Detached);
			const FDNImpact Big = Hit(13.f, EDNDamageZone::Front);
			TestTrue("nunca pendura, mesmo acima do limiar de pendurar das outras", After(FenderFrontLeft(), EDNPartStage::Attached, Damaged(EDNDamageZone::Front, 0.5f), &Big) == EDNPartStage::Attached);
			// quem decide é a dobradiça, não os limiares: sem dobradiça, nem com os limiares de pendurar preenchidos ela pendura
			FDNCarPartSpec NoHinge = BumperFront();
			NoHinge.HingeMaxDeg = 0.f;
			TestTrue("limiares de pendurar sem dobradica", After(NoHinge, EDNPartStage::Attached, Damaged(EDNDamageZone::Front, 0.5f), nullptr) == EDNPartStage::Attached);
		});

		It("sem volta: solta continua solta, pendurada nao volta a presa, e nada e relatado de novo", [this]()
		{
			int32 Changes = -1;
			const FDNImpact Big = Hit(40.f, EDNDamageZone::Front);
			TestTrue("solta fica solta", After(BumperFront(), EDNPartStage::Detached, Damaged(EDNDamageZone::Front, 1.f), &Big, &Changes) == EDNPartStage::Detached);
			TestEqual("solta: nenhuma mudanca", Changes, 0);
			TestTrue("pendurada sem dano fica pendurada", After(BumperFront(), EDNPartStage::Hanging, FDNDamageState(), nullptr, &Changes) == EDNPartStage::Hanging);
			TestEqual("pendurada: nenhuma mudanca", Changes, 0);
		});

		It("impacto invalido: o Dv nao conta, o dano acumulado conta (P1)", [this]()
		{
			const FDNImpact NaNHit = Hit(NaNf, EDNDamageZone::Front);
			const FDNImpact Negative = Hit(-20.f, EDNDamageZone::Front);
			const FDNImpact BadZone = Hit(30.f, static_cast<EDNDamageZone>(9));
			TestTrue("NaN", After(BumperFront(), EDNPartStage::Attached, FDNDamageState(), &NaNHit) == EDNPartStage::Attached);
			TestTrue("negativo", After(BumperFront(), EDNPartStage::Attached, FDNDamageState(), &Negative) == EDNPartStage::Attached);
			TestTrue("zona fora do enum", After(BumperFront(), EDNPartStage::Attached, FDNDamageState(), &BadZone) == EDNPartStage::Attached);
			TestTrue("sem impacto, com o dano da zona", After(BumperFront(), EDNPartStage::Attached, Damaged(EDNDamageZone::Front, 0.6f), nullptr) == EDNPartStage::Detached);
			TestEqual("Dv de zona fora do enum", FDNCarParts::DeltaVOn(EDNDamageZone::Front, false, EDNDamageZone::Front, &BadZone), 0.f);
			TestEqual("Dv negativo vale 0", FDNCarParts::DeltaVOn(EDNDamageZone::Front, false, EDNDamageZone::Front, &Negative), 0.f);
			TestEqual("Dv NaN vale 0", FDNCarParts::DeltaVOn(EDNDamageZone::Front, false, EDNDamageZone::Front, &NaNHit), 0.f);
			TestEqual("sem impacto vale 0", FDNCarParts::DeltaVOn(EDNDamageZone::Front, false, EDNDamageZone::Front, nullptr), 0.f);
		});

		It("estagios de tamanho errado sao refeitos com todas presas (P2)", [this]()
		{
			const TArray<FDNCarPartSpec> Specs = ContractParts();
			TArray<EDNPartStage> Stages = { EDNPartStage::Detached };
			FDNCarParts::Evaluate(Specs, Stages, FDNDamageState(), nullptr);
			if (!TestEqual("tamanho", Stages.Num(), Specs.Num()))
			{
				return;
			}
			TestTrue("a primeira volta a presa", Stages[0] == EDNPartStage::Attached);
			// maior que a lista também é refeito
			const FDNCarPartSpec One = BumperFront();
			TArray<EDNPartStage> Longer = { EDNPartStage::Detached, EDNPartStage::Hanging };
			FDNCarParts::Evaluate(MakeArrayView(&One, 1), Longer, FDNDamageState(), nullptr);
			TestTrue("maior: refeito com uma presa", Longer.Num() == 1 && Longer[0] == EDNPartStage::Attached);
		});

		It("a tabela do contrato numa batida de frente de 12 m/s: o para-choque solta e o capo pendura, nessa ordem", [this]()
		{
			const TArray<FDNCarPartSpec> Specs = ContractParts();
			TArray<EDNPartStage> Stages;
			Stages.Init(EDNPartStage::Attached, Specs.Num());
			const FDNImpact I = Hit(12.f, EDNDamageZone::Front);
			const TArray<FDNPartChange> Changes = FDNCarParts::Evaluate(Specs, Stages, FDNDamageState(), &I);
			if (!TestEqual("duas mudancas", Changes.Num(), 2))
			{
				return;
			}
			TestTrue("1a: para-choque, indice 0, Presa -> Solta", Changes[0].Part == EDNDamagePart::BumperFront && Changes[0].Index == 0
				&& Changes[0].From == EDNPartStage::Attached && Changes[0].To == EDNPartStage::Detached);
			TestTrue("2a: capo, indice 2, Presa -> Pendurada", Changes[1].Part == EDNDamagePart::Hood && Changes[1].Index == 2
				&& Changes[1].From == EDNPartStage::Attached && Changes[1].To == EDNPartStage::Hanging);
			TestTrue("para-lama a 12 m/s fica", Stages[6] == EDNPartStage::Attached);
		});
	});

	Describe("Vidros", [this]()
	{
		It("a trinca vai de 0 no inicio a 1 em 1,0, pelo maior entre a zona e a secundaria", [this]()
		{
			TestEqual("0,39", FDNGlass::Crack(Windshield(), Damaged(EDNDamageZone::Front, 0.39f)), 0.f);
			TestEqual("0,40", FDNGlass::Crack(Windshield(), Damaged(EDNDamageZone::Front, 0.40f)), 0.f);
			TestEqual("0,70", FDNGlass::Crack(Windshield(), Damaged(EDNDamageZone::Front, 0.70f)), 0.5f, 1.e-5f);
			TestEqual("1,0", FDNGlass::Crack(Windshield(), Damaged(EDNDamageZone::Front, 1.f)), 1.f);
			TestEqual("0,70 no teto", FDNGlass::Crack(Windshield(), Damaged(EDNDamageZone::Roof, 0.70f)), 0.5f, 1.e-5f);
		});

		It("trinca a partir de 1,0: 0 abaixo, 1 em 1,0 (P4)", [this]()
		{
			const FDNGlassSpec Late = Glass(EDNGlass::SideLeft, EDNDamageZone::Left, 1.f, 0.f, 0.f);
			TestEqual("0,99", FDNGlass::Crack(Late, Damaged(EDNDamageZone::Left, 0.99f)), 0.f);
			TestEqual("1,0", FDNGlass::Crack(Late, Damaged(EDNDamageZone::Left, 1.f)), 1.f);
		});

		It("o para-brisa nunca estoura", [this]()
		{
			const FDNImpact Big = Hit(50.f, EDNDamageZone::Front);
			TestFalse("dano 1,0 e 50 m/s", BurstsAlone(Windshield(), Damaged(EDNDamageZone::Front, 1.f), &Big));
		});

		It("o lateral estoura com 0,60 ou Dv >= 12 na zona; nao na outra zona", [this]()
		{
			const FDNImpact Low = Hit(11.99f, EDNDamageZone::Left);
			const FDNImpact Edge = Hit(12.f, EDNDamageZone::Left);
			const FDNImpact Other = Hit(30.f, EDNDamageZone::Right);
			TestFalse("0,5999", BurstsAlone(SideLeft(), Damaged(EDNDamageZone::Left, 0.5999f), nullptr));
			TestTrue("0,60", BurstsAlone(SideLeft(), Damaged(EDNDamageZone::Left, 0.60f), nullptr));
			TestFalse("11,99", BurstsAlone(SideLeft(), FDNDamageState(), &Low));
			TestTrue("12", BurstsAlone(SideLeft(), FDNDamageState(), &Edge));
			TestFalse("30 na direita", BurstsAlone(SideLeft(), FDNDamageState(), &Other));
		});

		It("o traseiro conta o teto (secundaria) no dano e no Dv", [this]()
		{
			const FDNImpact Roof = Hit(12.f, EDNDamageZone::Roof);
			TestTrue("0,65 no teto", BurstsAlone(RearGlass(), Damaged(EDNDamageZone::Roof, 0.65f), nullptr));
			TestFalse("0,6499 no teto", BurstsAlone(RearGlass(), Damaged(EDNDamageZone::Roof, 0.6499f), nullptr));
			TestTrue("12 m/s no teto", BurstsAlone(RearGlass(), FDNDamageState(), &Roof));
		});

		It("o farol apaga em 0,60 e nunca pelo Dv (0 = nunca)", [this]()
		{
			const FDNImpact Big = Hit(50.f, EDNDamageZone::Front);
			TestFalse("50 m/s sem dano", BurstsAlone(HeadlightLeft(), FDNDamageState(), &Big));
			TestTrue("0,60", BurstsAlone(HeadlightLeft(), Damaged(EDNDamageZone::Front, 0.60f), nullptr));
		});

		It("estoura uma vez so, e Burst de tamanho errado e refeito (P2)", [this]()
		{
			const TArray<FDNGlassSpec> Specs = { Windshield(), SideLeft() };
			TArray<bool> Burst;
			const FDNDamageState S = Damaged(EDNDamageZone::Left, 0.8f);
			const TArray<int32> First = FDNGlass::Evaluate(Specs, Burst, S, nullptr);
			TestTrue("1a vez: o lateral (posicao 1)", First.Num() == 1 && First[0] == 1);
			if (!TestEqual("tamanho", Burst.Num(), 2))
			{
				return;
			}
			TestTrue("marcado", Burst[1]);
			TestEqual("2a vez: nada", FDNGlass::Evaluate(Specs, Burst, S, nullptr).Num(), 0);
			TestTrue("continua estourado", Burst[1]);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
