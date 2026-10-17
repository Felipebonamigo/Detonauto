// Piloto de corrida (marco 1b de docs/fatia-jogavel.md): dado o erro de rumo até um ponto perto e outro longe da linha do percurso e a
// velocidade, devolve esterço, acelerador e freio. Regras puras, sem UObject. Testes: Core/Tests/DNRivalDriver.spec.cpp.
//
// Decisões:
//  D1 o esterço é o erro de rumo perto dividido por SteerFullDeg, vezes um ganho que cai com a velocidade (15 m/s dividido pela velocidade
//     em m/s, entre 0,2 e 1: a 100 km/h o volante cheio faz o carro oscilar), preso em [-1, 1]; mesmo sinal do erro (FDNCourse, C6);
//  D2 a velocidade desejada vai de StraightKmh (reta) a CornerKmh (curva fechada) conforme o erro de rumo longe em módulo, de 0 a
//     CornerFullDeg;
//  D3 acelerador = 0,4 + 0,08 por km/h que falta para a desejada, preso em [0, 1];
//  D4 freio só se a velocidade passa da desejada em mais de 10 km/h: 0,04 por km/h acima dela, preso em [0, 1];
//  D5 entrada não finita (erro ou velocidade): tudo zero;
//  D6 o ponto de mira fica à frente LookAheadMinCm mais LookAheadPerKmhCm por km/h de velocidade.

#pragma once

#include "CoreMinimal.h"

/** Como um piloto dirige; o nível de cada rival (Data/Rivals.csv) é um conjunto destes números. */
struct FDNRivalConfig
{
	float StraightKmh = 100.0f;
	float CornerKmh = 45.0f;
	float CornerFullDeg = 40.0f;
	float SteerFullDeg = 30.0f;
	float LookAheadMinCm = 1500.0f;
	float LookAheadPerKmhCm = 40.0f;
};

struct FDNDriveCommand
{
	float Steer = 0.0f;
	float Throttle = 0.0f;
	float Brake = 0.0f;
};

struct FDNRivalDriver
{
	/** D1 a D5. NearErrorDeg e FarErrorDeg vêm de FDNCourse::HeadingErrorDeg. */
	static FDNDriveCommand Command(float NearErrorDeg, float FarErrorDeg, float SpeedKmh, const FDNRivalConfig& Config);

	/** D6: a distância à frente, ao longo da linha, do ponto de mira [cm]. */
	static float LookAheadCm(float SpeedKmh, const FDNRivalConfig& Config);
};
