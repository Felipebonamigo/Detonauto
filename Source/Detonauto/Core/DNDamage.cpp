#include "Core/DNDamage.h"

namespace
{
	/** Fusão de amassados: a distância precisa ser no máximo esta fração do raio do amassado existente (regra 3). Fixo no código. */
	constexpr double DentMergeRadiusFraction = 0.5;

	/** Uma zona (Front, Rear, Left, Right ou Roof) com este dano destrói o veículo (regra 5). A Under não conta. Fixo no código. */
	constexpr float ZoneDestroysVehicleAt = 1.0f;

	/** Decimal finito e pelo menos Min; senão, o próprio Min (D7). */
	double AtLeast(float Value, double Min)
	{
		return FMath::IsFinite(Value) ? FMath::Max(static_cast<double>(Value), Min) : Min;
	}

	bool IsFiniteVector(const FVector& V)
	{
		return FMath::IsFinite(V.X) && FMath::IsFinite(V.Y) && FMath::IsFinite(V.Z);
	}

	/** Índice do amassado mais raso; no empate, o mais antigo (primeiro da lista). */
	int32 ShallowestIndex(const TArray<FDNDent>& Dents)
	{
		int32 Index = INDEX_NONE;
		for (int32 i = 0; i < Dents.Num(); ++i)
		{
			if (Index == INDEX_NONE || Dents[i].DepthCm < Dents[Index].DepthCm)
			{
				Index = i;
			}
		}
		return Index;
	}

	/** Guarda o amassado: funde no existente ao alcance, ou entra, ou substitui o mais raso (regra 3). */
	void StoreDent(FDNDamageState& State, const FDNDent& NewDent, double MaxDepth, int32 MaxDents)
	{
		// Fusão: distância <= metade do raio do EXISTENTE (inclusive); vários ao alcance: o mais próximo, empate no mais antigo.
		int32 Target = INDEX_NONE;
		double TargetDistSq = 0.0;
		for (int32 i = 0; i < State.Dents.Num(); ++i)
		{
			const FDNDent& Existing = State.Dents[i];
			const double Dx = static_cast<double>(NewDent.LocalPositionCm.X) - Existing.LocalPositionCm.X;
			const double Dy = static_cast<double>(NewDent.LocalPositionCm.Y) - Existing.LocalPositionCm.Y;
			const double Dz = static_cast<double>(NewDent.LocalPositionCm.Z) - Existing.LocalPositionCm.Z;
			const double DistSq = Dx * Dx + Dy * Dy + Dz * Dz;
			const double Half = DentMergeRadiusFraction * static_cast<double>(Existing.RadiusCm);
			if (DistSq <= Half * Half && (Target == INDEX_NONE || DistSq < TargetDistSq))
			{
				Target = i;
				TargetDistSq = DistSq;
			}
		}

		if (Target != INDEX_NONE)
		{
			FDNDent& Existing = State.Dents[Target];
			if (NewDent.DepthCm > Existing.DepthCm)
			{
				Existing.LocalPositionCm = NewDent.LocalPositionCm; // o centro fica no mais fundo
			}
			Existing.DepthCm = static_cast<float>(FMath::Min(MaxDepth, static_cast<double>(Existing.DepthCm) + NewDent.DepthCm));
			Existing.RadiusCm = FMath::Max(Existing.RadiusCm, NewDent.RadiusCm);
			return;
		}

		// Sem fusão: com o limite cheio, sai o mais raso (empate: o mais antigo); o novo entra como o mais recente.
		while (State.Dents.Num() >= MaxDents)
		{
			State.Dents.RemoveAt(ShallowestIndex(State.Dents));
		}
		State.Dents.Add(NewDent);
	}
}

EDNDamageZone FDNDamage::ZoneOfPart(EDNDamagePart Part)
{
	switch (Part)
	{
	case EDNDamagePart::Hood:
	case EDNDamagePart::BumperFront:
	case EDNDamagePart::FenderFrontLeft:
	case EDNDamagePart::FenderFrontRight:
		return EDNDamageZone::Front;
	case EDNDamagePart::Trunk:
	case EDNDamagePart::BumperRear:
	case EDNDamagePart::FenderRearLeft:
	case EDNDamagePart::FenderRearRight:
	case EDNDamagePart::Spoiler:
		return EDNDamageZone::Rear;
	case EDNDamagePart::DoorLeft:
		return EDNDamageZone::Left;
	case EDNDamagePart::DoorRight:
		return EDNDamageZone::Right;
	}
	return EDNDamageZone::Under;
}

float FDNDamage::TotalDamage(const FDNDamageState& State)
{
	float Sum = 0.0f;
	for (int32 i = 0; i < FDNDamageState::TotalZoneCount; ++i)
	{
		Sum += State.ZoneDamage[i];
	}
	return Sum / static_cast<float>(FDNDamageState::TotalZoneCount);
}

FDNDamageResult FDNDamage::ApplyImpact(const FDNDamageConfig& Config, FDNDamageState& State, const FDNImpact& Impact)
{
	FDNDamageResult Result;

	const int32 ZoneIndex = static_cast<int32>(Impact.Zone);
	if (!FMath::IsFinite(Impact.DeltaVMs) || Impact.DeltaVMs < 0.0f || ZoneIndex < 0 || ZoneIndex >= FDNDamageState::ZoneCount)
	{
		return Result; // D5 e entrada inválida: sem dano
	}

	// Regra 1: limiar.
	const double DentMin = AtLeast(Config.DentMinDeltaVMs, 0.0);
	if (static_cast<double>(Impact.DeltaVMs) < DentMin)
	{
		return Result;
	}
	const double Excess = static_cast<double>(Impact.DeltaVMs) - DentMin;

	// Regra 2: dano da zona, saturado em 1,0 (D2: o resultado é o aumento real).
	const double ZoneDestroy = AtLeast(Config.ZoneDestroyDeltaVMs, 1.0e-9);
	const float Old = State.ZoneDamage[ZoneIndex];
	State.ZoneDamage[ZoneIndex] = static_cast<float>(FMath::Min(1.0, static_cast<double>(Old) + Excess / ZoneDestroy));
	Result.AddedZoneDamage = State.ZoneDamage[ZoneIndex] - Old;

	// Regra 3: amassado (D4: posição não finita não gera amassado).
	if (IsFiniteVector(Impact.LocalPositionCm))
	{
		const double MaxDepth = AtLeast(Config.DentMaxDepthCm, 0.0);
		FDNDent NewDent;
		NewDent.LocalPositionCm = Impact.LocalPositionCm;
		NewDent.DepthCm = static_cast<float>(FMath::Min(MaxDepth, Excess * AtLeast(Config.DentDepthPerMs, 0.0)));
		NewDent.RadiusCm = static_cast<float>(AtLeast(Config.DentBaseRadiusCm, 0.0) + NewDent.DepthCm);
		StoreDent(State, NewDent, MaxDepth, FMath::Max(1, Config.MaxDents));
		Result.bDentAdded = true;
		Result.Dent = NewDent;
	}

	// Regra 4: peças, em ordem do enum, uma única vez cada (D6: todas as zonas).
	for (int32 i = 0; i < FDNDamageState::PartCount; ++i)
	{
		if (State.bPartDetached[i])
		{
			continue;
		}
		const EDNDamagePart Part = static_cast<EDNDamagePart>(i);
		const EDNDamageZone PartZone = ZoneOfPart(Part);
		const bool bByImpact = PartZone == Impact.Zone && Impact.DeltaVMs >= Config.PartDetachDeltaVMs;
		const bool bByZoneDamage = State.ZoneDamage[static_cast<int32>(PartZone)] >= Config.PartDetachZoneDamage;
		if (bByImpact || bByZoneDamage)
		{
			State.bPartDetached[i] = true;
			Result.DetachedParts.Add(Part);
		}
	}

	// Regras 5 e 6: destruído (permanente; o primeiro impacto que causa devolve bJustDestroyed, uma vez).
	if (!State.bDestroyed)
	{
		const bool bTotal = TotalDamage(State) >= Config.DestroyedTotalDamage;
		// Qualquer das 5 zonas do total em 1,0 destrói: bater de lado ou no teto até a carcaça também acaba o carro.
		bool bAnyZoneFull = false;
		for (int32 i = 0; i < FDNDamageState::TotalZoneCount; ++i)
		{
			bAnyZoneFull = bAnyZoneFull || State.ZoneDamage[i] >= ZoneDestroysVehicleAt;
		}
		if (bTotal || bAnyZoneFull)
		{
			State.bDestroyed = true;
			Result.bJustDestroyed = true;
		}
	}
	return Result;
}

FDNPartPoolPlan FDNPartPool::Plan(int32 MaxActive, int32 Active, int32 Requested)
{
	const int64 Max = FMath::Max(MaxActive, 0);
	const int64 Act = FMath::Max(Active, 0);
	const int64 Req = FMath::Max(Requested, 0);

	FDNPartPoolPlan Plan;
	Plan.Accepted = static_cast<int32>(FMath::Min(Req, Max));
	Plan.Recycle = static_cast<int32>(FMath::Clamp<int64>(Act + Req - Max, 0, Act));
	return Plan;
}
