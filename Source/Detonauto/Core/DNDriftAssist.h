// Assistência de derrapagem arcade (lote 14, J2): regra pura, sem mundo e sem física. Testes: Core/Tests/DNDriftAssist.spec.cpp
// (decisões A1 a A6 lá). Parâmetros: Data/Cars.csv (DriftAssistStartDeg, DriftAssistGain, DriftAssistMax).
//
// Ideia: o carro deve derrapar bonito e voltar a ser controlável, nunca rodar. Quando o ângulo de deslize passa de um limite, uma
// aceleração de guinada gira o nariz de volta para a direção da velocidade (contra-esterço automático), proporcional ao excesso e com
// teto. Quem chama aplica o resultado como aceleração angular em Z (ADNVehicle::Tick).

#pragma once

#include "CoreMinimal.h"

struct FDNDriftAssistConfig
{
	/** Ângulo de deslize [graus] a partir do qual a assistência age. */
	float StartDeg = 15.0f;
	/** Aceleração de guinada [graus/s²] por grau de excesso. 0 desliga a assistência. */
	float Gain = 0.0f;
	/** Teto da aceleração de guinada [graus/s²]. 0 desliga a assistência. */
	float Max = 0.0f;
};

struct FDNDriftAssist
{
	/** Abaixo desta velocidade [km/h] não age; sobe linear até FullSpeedKmh. */
	static constexpr float MinSpeedKmh = 10.0f;
	static constexpr float FullSpeedKmh = 30.0f;
	/** Girando para a direção certa a esta guinada [graus/s] ou mais, não empurra mais (evita passar do ponto). */
	static constexpr float RecoveringYawDegS = 120.0f;
	/** Rodas em contato com o chão para a assistência agir (A7): menos que isso é carro no ar, de lado ou capotado. */
	static constexpr int32 MinWheelsInContact = 3;

	/** Carro no chão para efeito da assistência (A7). */
	static bool IsGrounded(int32 WheelsInContact) { return WheelsInContact >= MinWheelsInContact; }

	/** Ângulo de deslize [graus]: atan2(Vy, Vx) da velocidade no espaço do carro (positivo = velocidade à direita do nariz). */
	static float SlipAngleDeg(const FVector& LocalVelocityCmS);

	/**
	 *  Aceleração de guinada [graus/s², positiva = para a direita] que gira o nariz para a direção da velocidade.
	 *  Zero se desligada, abaixo do início, devagar demais, ou com entrada inválida (A6).
	 */
	static float YawAcceleration(const FDNDriftAssistConfig& Config, float SlipAngleDeg, float SpeedKmh, float YawRateDegS);
};
