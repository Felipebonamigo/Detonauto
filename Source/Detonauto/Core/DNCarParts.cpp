#include "Core/DNCarParts.h"

namespace
{
	bool ValidZone(EDNDamageZone Zone)
	{
		const int32 Index = static_cast<int32>(Zone);
		return Index >= 0 && Index < FDNDamageState::ZoneCount;
	}

	/** Valor finito no limiar ou acima; limiar <= 0 ou não finito = nunca. */
	bool Reaches(float Value, float Threshold)
	{
		return FMath::IsFinite(Threshold) && Threshold > 0.0f && Value >= Threshold;
	}
}

float FDNCarParts::ZoneDamage(EDNDamageZone Zone, bool bHasSecondary, EDNDamageZone Secondary, const FDNDamageState& Damage)
{
	float Value = ValidZone(Zone) ? Damage.ZoneDamage[static_cast<int32>(Zone)] : 0.0f;
	if (bHasSecondary && ValidZone(Secondary))
	{
		Value = FMath::Max(Value, Damage.ZoneDamage[static_cast<int32>(Secondary)]);
	}
	return FMath::IsFinite(Value) ? Value : 0.0f;
}

float FDNCarParts::DeltaVOn(EDNDamageZone Zone, bool bHasSecondary, EDNDamageZone Secondary, const FDNImpact* Impact)
{
	// P1: sem impacto, Δv inválido ou zona fora do enum, o Δv não conta
	if (!Impact || !FMath::IsFinite(Impact->DeltaVMs) || Impact->DeltaVMs < 0.0f || !ValidZone(Impact->Zone))
	{
		return 0.0f;
	}
	const bool bHits = Impact->Zone == Zone || (bHasSecondary && Impact->Zone == Secondary);
	return bHits ? Impact->DeltaVMs : 0.0f;
}

TArray<FDNPartChange> FDNCarParts::Evaluate(TConstArrayView<FDNCarPartSpec> Specs, TArray<EDNPartStage>& Stages, const FDNDamageState& Damage,
	const FDNImpact* Impact)
{
	if (Stages.Num() != Specs.Num())
	{
		Stages.Init(EDNPartStage::Attached, Specs.Num()); // P2
	}
	TArray<FDNPartChange> Changes;
	for (int32 i = 0; i < Specs.Num(); ++i)
	{
		const FDNCarPartSpec& Spec = Specs[i];
		const EDNPartStage From = Stages[i];
		if (From == EDNPartStage::Detached)
		{
			continue; // sem volta
		}
		const float PartDamage = ZoneDamage(Spec.Zone, Spec.bHasSecondaryZone, Spec.SecondaryZone, Damage);
		const float DeltaV = DeltaVOn(Spec.Zone, Spec.bHasSecondaryZone, Spec.SecondaryZone, Impact);
		EDNPartStage To = From;
		if (Reaches(PartDamage, Spec.DetachDamage) || Reaches(DeltaV, Spec.DetachDeltaVMs))
		{
			To = EDNPartStage::Detached; // P3: passou dos dois no mesmo impacto vai direto
		}
		else if (From == EDNPartStage::Attached && Spec.HasHinge() && (Reaches(PartDamage, Spec.HangDamage) || Reaches(DeltaV, Spec.HangDeltaVMs)))
		{
			To = EDNPartStage::Hanging;
		}
		if (To != From)
		{
			Stages[i] = To;
			FDNPartChange Change;
			Change.Index = i;
			Change.Part = Spec.Part;
			Change.From = From;
			Change.To = To;
			Changes.Add(Change);
		}
	}
	return Changes;
}

float FDNGlass::Crack(const FDNGlassSpec& Spec, const FDNDamageState& Damage)
{
	const float D = FDNCarParts::ZoneDamage(Spec.Zone, Spec.bHasSecondaryZone, Spec.SecondaryZone, Damage);
	const float From = FMath::IsFinite(Spec.CrackFrom) ? FMath::Max(Spec.CrackFrom, 0.0f) : 1.0f;
	if (From >= 1.0f)
	{
		return D >= 1.0f ? 1.0f : 0.0f; // P4
	}
	return FMath::Clamp((D - From) / (1.0f - From), 0.0f, 1.0f);
}

TArray<int32> FDNGlass::Evaluate(TConstArrayView<FDNGlassSpec> Specs, TArray<bool>& Burst, const FDNDamageState& Damage, const FDNImpact* Impact)
{
	if (Burst.Num() != Specs.Num())
	{
		Burst.Init(false, Specs.Num()); // P2
	}
	TArray<int32> Now;
	for (int32 i = 0; i < Specs.Num(); ++i)
	{
		const FDNGlassSpec& Spec = Specs[i];
		if (Burst[i])
		{
			continue; // uma vez só (o para-brisa, com os dois limiares em 0, nunca chega: Reaches pede limiar acima de 0)
		}
		const float D = FDNCarParts::ZoneDamage(Spec.Zone, Spec.bHasSecondaryZone, Spec.SecondaryZone, Damage);
		const float DeltaV = FDNCarParts::DeltaVOn(Spec.Zone, Spec.bHasSecondaryZone, Spec.SecondaryZone, Impact);
		if (Reaches(D, Spec.BurstDamage) || Reaches(DeltaV, Spec.BurstDeltaVMs))
		{
			Burst[i] = true;
			Now.Add(i);
		}
	}
	return Now;
}


// ---- nomes em texto: os do enum, iguais aos dos CSVs e das malhas ----

namespace
{
	const TCHAR* const PartNames[] = { TEXT("Hood"), TEXT("Trunk"), TEXT("DoorLeft"), TEXT("DoorRight"), TEXT("BumperFront"), TEXT("BumperRear"),
		TEXT("FenderFrontLeft"), TEXT("FenderFrontRight"), TEXT("FenderRearLeft"), TEXT("FenderRearRight"), TEXT("Spoiler") };
	static_assert(UE_ARRAY_COUNT(PartNames) == FDNDamageState::PartCount, "um nome por EDNDamagePart");

	const TCHAR* const ZoneNames[] = { TEXT("Front"), TEXT("Rear"), TEXT("Left"), TEXT("Right"), TEXT("Roof"), TEXT("Under") };
	static_assert(UE_ARRAY_COUNT(ZoneNames) == FDNDamageState::ZoneCount, "um nome por EDNDamageZone");

	const TCHAR* const EdgeNames[] = { TEXT("None"), TEXT("TopRear"), TEXT("TopFront"), TEXT("SideFront"), TEXT("Lateral") };

	const TCHAR* const GlassNames[] = { TEXT("Windshield"), TEXT("SideLeft"), TEXT("SideRight"), TEXT("Rear"), TEXT("HeadlightLeft"),
		TEXT("HeadlightRight"), TEXT("TaillightLeft"), TEXT("TaillightRight") };

	template <typename EnumType, int32 N>
	const TCHAR* NameOf(const TCHAR* const (&Names)[N], EnumType Value)
	{
		const int32 Index = static_cast<int32>(Value);
		return Index >= 0 && Index < N ? Names[Index] : TEXT("");
	}

	template <typename EnumType, int32 N>
	bool ParseName(const TCHAR* const (&Names)[N], const FString& Text, EnumType& Out)
	{
		const FString Trimmed = Text.TrimStartAndEnd();
		for (int32 i = 0; i < N; ++i)
		{
			if (Trimmed.Equals(Names[i], ESearchCase::IgnoreCase))
			{
				Out = static_cast<EnumType>(i);
				return true;
			}
		}
		return false;
	}
}

const TCHAR* FDNCarParts::PartName(EDNDamagePart Part) { return NameOf(PartNames, Part); }
bool FDNCarParts::ParsePart(const FString& Text, EDNDamagePart& Out) { return ParseName(PartNames, Text, Out); }
const TCHAR* FDNCarParts::ZoneName(EDNDamageZone Zone) { return NameOf(ZoneNames, Zone); }
bool FDNCarParts::ParseZone(const FString& Text, EDNDamageZone& Out) { return ParseName(ZoneNames, Text, Out); }
const TCHAR* FDNCarParts::HingeEdgeName(EDNHingeEdge Edge) { return NameOf(EdgeNames, Edge); }
bool FDNCarParts::ParseHingeEdge(const FString& Text, EDNHingeEdge& Out) { return ParseName(EdgeNames, Text, Out); }
const TCHAR* FDNGlass::GlassName(EDNGlass Glass) { return NameOf(GlassNames, Glass); }
bool FDNGlass::ParseGlass(const FString& Text, EDNGlass& Out) { return ParseName(GlassNames, Text, Out); }
