// Direção de uma cidade no globo. Funções puras, sem UObject, sem Actor, sem mundo.
// Contrato: docs/menu-mapa.md, "Regras de progresso". Testes: Core/Tests/DNGlobe.spec.cpp.
//
// Convenção de eixos do Unreal: X para a frente, Z para cima. A direção de uma cidade (do centro do globo
// para a cidade) é o vetor unitário (cos lat . cos lon, cos lat . sin lon, sin lat).
// Yaw é 0 em +X e cresce em direção a +Y; pitch é positivo para cima. Ângulos em graus.

#pragma once

#include "CoreMinimal.h"

struct FDNGlobe
{
	/** Latitude em [-90, 90] e longitude em [-180, 180] (limites inclusivos). NaN e infinito são inválidos. */
	static bool IsValidLatLon(double LatitudeDegrees, double LongitudeDegrees);

	/**
	 *  Vetor unitário do centro do globo para o ponto. Entrada inválida devolve o vetor zero (nunca um vetor errado).
	 *  Nos múltiplos de 90 graus o resultado é exato (sem resto de ponto flutuante).
	 */
	static FVector LatLonToDirection(double LatitudeDegrees, double LongitudeDegrees);

	/**
	 *  Yaw e pitch da direção (para quem olha DO centro do globo PARA a cidade). Devolve falso, sem mexer nas
	 *  saídas, se o vetor for zero ou tiver NaN. O comprimento do vetor não importa. Yaw em (-180, 180].
	 *  Nos polos o yaw é indefinido e vale 0 (escolha documentada).
	 */
	static bool DirectionToYawPitch(const FVector& Direction, double& OutYawDegrees, double& OutPitchDegrees);

	/**
	 *  Yaw e pitch de uma câmera em órbita: posta FORA do globo, na direção da cidade, olhando para o centro.
	 *  É o sentido oposto ao de DirectionToYawPitch.
	 */
	static bool OrbitCameraYawPitch(const FVector& CityDirection, double& OutYawDegrees, double& OutPitchDegrees);

	/**
	 *  Distância ao longo do círculo máximo entre dois pontos, em graus, de 0 a 180 (pelo lado curto, inclusive
	 *  no antimeridiano). Devolve falso, sem mexer na saída, se algum ponto for inválido.
	 */
	static bool GreatCircleDistanceDegrees(double LatA, double LonA, double LatB, double LonB, double& OutDegrees);
};
