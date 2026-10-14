// Amassado visual: a parte pura que liga as regras do dano (Core/DNDamage) ao material de deslocamento de vértices
// (Tools/Unreal/create_dent_material.py; docs/prototipo-amassado.md). Sem mundo, sem física: testável por Automation
// (Core/Tests/DNDentParams.spec.cpp).

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

	/**
	 *  Variação de velocidade [m/s] de uma batida medida no próprio corpo: módulo da diferença entre a velocidade linear antes
	 *  e depois do contato (entradas em cm/s, como a física entrega). Inclui o quique. Entrada inválida: 0.
	 */
	static float DeltaVMsBetween(const FVector& VelocityBeforeCmPerS, const FVector& VelocityAfterCmPerS)
	{
		const bool bFinite = FMath::IsFinite(VelocityBeforeCmPerS.X) && FMath::IsFinite(VelocityBeforeCmPerS.Y) && FMath::IsFinite(VelocityBeforeCmPerS.Z)
			&& FMath::IsFinite(VelocityAfterCmPerS.X) && FMath::IsFinite(VelocityAfterCmPerS.Y) && FMath::IsFinite(VelocityAfterCmPerS.Z);
		return bFinite ? static_cast<float>((VelocityBeforeCmPerS - VelocityAfterCmPerS).Size() / 100.0) : 0.0f;
	}

	/**
	 *  Ponto do amassado a partir do ponto de contato (espaço local): sobe até MinHeightAboveBaseCm acima da base da caixa do
	 *  corpo (o contato com a parede costuma cair na altura do chão) e depois entra na caixa (Center +- Extent). A caixa é só
	 *  uma aproximação da superfície do corpo. Entrada inválida: o centro.
	 */
	static FVector ProjectToBody(const FVector& Point, const FVector& Center, const FVector& Extent, float MinHeightAboveBaseCm)
	{
		if (!FMath::IsFinite(Point.X) || !FMath::IsFinite(Point.Y) || !FMath::IsFinite(Point.Z))
		{
			return Center;
		}
		const FVector Min = Center - Extent;
		const FVector Max = Center + Extent;
		FVector P = Point;
		const double LowestZ = static_cast<double>(Min.Z) + FMath::Max(0.0f, MinHeightAboveBaseCm);
		if (P.Z < LowestZ)
		{
			P.Z = static_cast<float>(LowestZ);
		}
		return FVector(FMath::Clamp(P.X, Min.X, Max.X), FMath::Clamp(P.Y, Min.Y, Max.Y), FMath::Clamp(P.Z, Min.Z, Max.Z));
	}
};

/**
 *  Agrupa os contatos de uma mesma batida. Cada contato chega com sua variação de velocidade; durante uma janela a partir do
 *  primeiro contato só vale o MAIOR valor (sem somar, para o quique não contar de novo). Fechada a janela, TryFlush entrega o
 *  maior valor e o ponto dele, e o próximo contato abre outra janela.
 */
struct FDNImpactGrouper
{
	static constexpr double WindowSeconds = 0.15;

	struct FGroup
	{
		float DeltaVMs = 0.0f;
		FVector LocalPoint = FVector::ZeroVector;
		/** Instante do primeiro contato da janela. */
		double StartTime = 0.0;
	};

	/** Valor inválido (0, negativo, NaN, infinito) é ignorado: não abre janela nem muda o maior. */
	void Add(double Now, float DeltaVMs, const FVector& LocalPoint)
	{
		if (!FMath::IsFinite(DeltaVMs) || DeltaVMs <= 0.0f)
		{
			return;
		}
		if (!bOpen)
		{
			bOpen = true;
			Current.DeltaVMs = DeltaVMs;
			Current.LocalPoint = LocalPoint;
			Current.StartTime = Now;
		}
		else if (DeltaVMs > Current.DeltaVMs)
		{
			Current.DeltaVMs = DeltaVMs;
			Current.LocalPoint = LocalPoint;
		}
	}

	/** Se a janela aberta já passou de 0,15 s (inclusive, com folga de 1 microssegundo), entrega o grupo e fecha. */
	bool TryFlush(double Now, FGroup& Out)
	{
		if (bOpen && (Now - Current.StartTime) + 1.0e-6 >= WindowSeconds)
		{
			Out = Current;
			bOpen = false;
			Current = FGroup();
			return true;
		}
		return false;
	}

	bool IsOpen() const { return bOpen; }

private:
	bool bOpen = false;
	FGroup Current;
};
