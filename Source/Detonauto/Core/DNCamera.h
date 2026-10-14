// Modos de câmera do carro (lote 14, J4): estrutura pura. Os números moram em Data/Cameras.csv (um modo por linha, na ordem em que a
// tecla C os percorre) e um teste de deriva garante que a linha "alta-longe" é igual ao padrão desta struct.
// Testes: Core/Tests/DNCamera.spec.cpp e Core/Tests/DNDataCameras.spec.cpp.

#pragma once

#include "CoreMinimal.h"

enum class EDNCameraAnchor : uint8
{
	/** Câmera de perseguição: atrás do carro, num braço (spring arm). */
	Chase,
	/** Presa ao carro: capô, para-choque, motorista. Gira, balança e inclina junto com a carroceria. */
	Car
};

struct FDNCameraConfig
{
	EDNCameraAnchor Anchor = EDNCameraAnchor::Chase;
	/** Chase: comprimento do braço atrás do carro [cm]. Car: precisa ser 0. */
	float DistanceCm = 810.0f;
	/** Chase: altura da câmera sobre o ponto de mira [cm]. Car: altura da câmera no espaço do carro [cm]. */
	float HeightCm = 185.0f;
	/** Car: posição à frente (+) ou atrás (-) da origem do carro [cm]. Chase: não usado. */
	float ForwardCm = 0.0f;
	/** Car: posição ao lado [cm]; negativo = esquerda. Chase: não usado. */
	float SideCm = 0.0f;
	/** Inclinação da mira [graus]; negativo olha para baixo. */
	float PitchDeg = 0.0f;
	float Fov = 95.0f;
	/** Chase: velocidade com que a câmera acompanha a rotação do carro (menor = mais solta); 0 = presa, sem atraso. */
	float Smoothing = 2.0f;
};

struct FDNCameraCycle
{
	/** O modo depois de Current, voltando ao primeiro no fim. Sem modos ou índice fora da lista: 0. */
	static int32 Next(int32 Current, int32 Count)
	{
		if (Count <= 0 || Current < 0 || Current >= Count)
		{
			return 0;
		}
		return (Current + 1) % Count;
	}

	/** "Chase" ou "Car", sem diferenciar a caixa. Falso se o texto não é nenhum dos dois. */
	static bool ParseAnchor(const FString& Text, EDNCameraAnchor& Out)
	{
		if (Text.Equals(TEXT("Chase"), ESearchCase::IgnoreCase)) { Out = EDNCameraAnchor::Chase; return true; }
		if (Text.Equals(TEXT("Car"), ESearchCase::IgnoreCase)) { Out = EDNCameraAnchor::Car; return true; }
		return false;
	}
};
