// Protótipo de amassado visual (lote 10): a parte pura que liga as regras do dano (Core/DNDamage) ao material de
// deslocamento de vértices (Tools/Unreal/create_dent_material.py). Sem mundo, sem física: testável por Automation
// (DNDentParams.spec.cpp). É protótipo; não é código final do jogo.

#pragma once

#include "CoreMinimal.h"
#include "Core/DNDamage.h"

struct FDNDentParams
{
	/** O material tem 8 trios de parâmetros (D0P..D7P, D0R..D7R e D0D..D7D). */
	static constexpr int32 MaxSlots = 8;

	/**
	 *  Os três parâmetros de um amassado, no espaço local do componente [cm]: a posição (vetor, só X, Y e Z contam: um
	 *  parâmetro vetorial do material entrega apenas RGB ao nó Custom), o raio e a profundidade (escalares).
	 */
	struct FSlot
	{
		FLinearColor Position = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
		float Radius = 0.0f;
		float Depth = 0.0f;
	};

	static FName PositionName(int32 Index) { return FName(*FString::Printf(TEXT("D%dP"), Index)); }
	static FName RadiusName(int32 Index) { return FName(*FString::Printf(TEXT("D%dR"), Index)); }
	static FName DepthName(int32 Index) { return FName(*FString::Printf(TEXT("D%dD"), Index)); }

	/** O slot do amassado Index. Sem amassado nesse índice, índice fora de 0..7 ou valor inválido: slot desligado (raio 0). */
	static FSlot Slot(const TArray<FDNDent>& Dents, int32 Index)
	{
		FSlot Out;
		if (Index < 0 || Index >= MaxSlots || !Dents.IsValidIndex(Index))
		{
			return Out;
		}
		const FDNDent& D = Dents[Index];
		const bool bValid = FMath::IsFinite(D.RadiusCm) && D.RadiusCm > 0.0f && FMath::IsFinite(D.DepthCm)
			&& FMath::IsFinite(D.LocalPositionCm.X) && FMath::IsFinite(D.LocalPositionCm.Y) && FMath::IsFinite(D.LocalPositionCm.Z);
		if (bValid)
		{
			Out.Position = FLinearColor(D.LocalPositionCm.X, D.LocalPositionCm.Y, D.LocalPositionCm.Z, 0.0f);
			Out.Radius = D.RadiusCm;
			Out.Depth = D.DepthCm;
		}
		return Out;
	}

	/** O limite de amassados da configuração, cortado no que o material comporta (e em pelo menos 1). */
	static int32 EffectiveMaxDents(int32 ConfigMaxDents) { return FMath::Clamp(ConfigMaxDents, 1, MaxSlots); }

	/**
	 *  De que lado do carro é um ponto de batida (espaço local, caixa com Center e Extent): vale o eixo em que o ponto está
	 *  mais perto da borda da caixa (maior |ponto - centro| / extensão). Eixo sem extensão não conta; entrada inválida: Front.
	 */
	static EDNDamageZone ZoneOfLocalPoint(const FVector& Point, const FVector& Center, const FVector& Extent)
	{
		if (!FMath::IsFinite(Point.X) || !FMath::IsFinite(Point.Y) || !FMath::IsFinite(Point.Z))
		{
			return EDNDamageZone::Front;
		}
		const FVector Delta = Point - Center;
		const double Rx = Extent.X > 0.0 ? FMath::Abs(Delta.X) / Extent.X : 0.0;
		const double Ry = Extent.Y > 0.0 ? FMath::Abs(Delta.Y) / Extent.Y : 0.0;
		const double Rz = Extent.Z > 0.0 ? FMath::Abs(Delta.Z) / Extent.Z : 0.0;
		if (Rx >= Ry && Rx >= Rz)
		{
			return Delta.X >= 0.0 ? EDNDamageZone::Front : EDNDamageZone::Rear;
		}
		if (Ry >= Rz)
		{
			return Delta.Y >= 0.0 ? EDNDamageZone::Right : EDNDamageZone::Left;
		}
		return Delta.Z >= 0.0 ? EDNDamageZone::Roof : EDNDamageZone::Under;
	}

	/** Variação de velocidade [m/s] de um impulso de colisão [kg cm/s] num corpo de massa MassKg. Entrada inválida: 0. */
	static float DeltaVMsFromImpulse(float ImpulseKgCmPerS, float MassKg)
	{
		if (!FMath::IsFinite(ImpulseKgCmPerS) || ImpulseKgCmPerS <= 0.0f || !FMath::IsFinite(MassKg) || MassKg <= 0.0f)
		{
			return 0.0f;
		}
		return ImpulseKgCmPerS / MassKg / 100.0f;
	}
};
