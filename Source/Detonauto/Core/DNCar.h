// Afinação do carro (lote 12): tudo o que muda de um preset para outro, sem depender de mundo nem de física.
// Os valores iniciais são os do carro do template Vehicle do Chaos (a linha "Padrao"); o CSV é Data/Cars.csv e um teste de
// deriva garante que a linha Padrao é igual ao padrão desta struct. Faixas válidas: Core/Data/DNDataValidation.cpp (ValidateCars).
// Medições dos presets: docs/presets-carro.md. Testes: Core/Tests/DNDataCars.spec.cpp e DNCar.spec.cpp.

#pragma once

#include "CoreMinimal.h"

/** Um preset de carro. As unidades são as do Chaos Vehicles (cm, kg, N·m) quando não disser outra coisa. */
struct FDNCarConfig
{
	/** Massa do chassi [kg]. */
	float MassKg = 1500.0f;
	/** Torque máximo do motor [N·m]. */
	float MaxTorque = 750.0f;
	/** Multiplicador da curva de aderência lateral das 4 rodas (1 = padrão do template). */
	float LateralGrip = 1.0f;
	/** Rigidez da mola da suspensão (SpringRate do Chaos). */
	float SuspensionStiffness = 250.0f;
	/** Ganho do volante: multiplica a entrada de direção antes de ir ao Chaos (1 = padrão). */
	float SteeringGain = 1.0f;

	/** Quanto o centro de massa desce em relação ao calculado pela malha [cm]. 0 = não mexe. */
	float CenterOfMassDropCm = 0.0f;
	/** Amortecimento da suspensão (SuspensionDampingRatio do Chaos, 0 a 1). */
	float SuspensionDampingRatio = 0.5f;
	/** Curso da suspensão para cima e para baixo [cm]. */
	float SuspensionTravelCm = 10.0f;
	/** Barra estabilizadora (RollbarScaling do Chaos, 0 a 1). Só vale na criação do veículo. */
	float RollbarScaling = 0.15f;
	/** Aderência do eixo TRASEIRO em relação ao dianteiro (1 = igual ao template). */
	float RearGripFactor = 1.0f;
	/** Multiplicador extra da aderência traseira com o freio de mão cheio (1 = o freio de mão não solta a traseira). */
	float HandbrakeRearGrip = 1.0f;
	/** Tempo para a perda de aderência chegar ao máximo, depois de apertar o freio de mão [s]. */
	float HandbrakeRampInSeconds = 0.1f;
	/** Tempo para a aderência voltar ao normal depois de soltar o freio de mão [s]. */
	float HandbrakeRampOutSeconds = 0.5f;
	/** Torque do freio de mão nas rodas traseiras (MaxHandBrakeTorque do Chaos). */
	float HandbrakeTorque = 6000.0f;

	/** Câmera: distância atrás do carro [cm]. */
	float CameraDistanceCm = 650.0f;
	/** Câmera: altura acima do carro [cm]. */
	float CameraHeightCm = 150.0f;
	/** Câmera: campo de visão [graus]. */
	float CameraFov = 90.0f;

	/** Aceleração extra para frente enquanto o turbo está ativo [m/s²]. 0 = o turbo não empurra. */
	float NitroAccelMps2 = 10.0f;
};

/**
 *  Rampa do freio de mão: quanto do efeito está aplicado agora (0 a 1). Sobe em InSeconds ao apertar e desce em
 *  OutSeconds ao soltar. O Chaos não tem isso.
 */
struct FDNHandbrakeRamp
{
	static float Step(float Current, bool bHeld, float DeltaTime, float InSeconds, float OutSeconds)
	{
		const float Safe = FMath::IsFinite(Current) ? FMath::Clamp(Current, 0.0f, 1.0f) : 0.0f;
		if (!FMath::IsFinite(DeltaTime) || DeltaTime <= 0.0f)
		{
			return Safe;
		}
		const float Seconds = bHeld ? InSeconds : OutSeconds;
		if (!FMath::IsFinite(Seconds) || Seconds <= 0.0f)
		{
			return bHeld ? 1.0f : 0.0f;
		}
		const float Delta = DeltaTime / Seconds;
		return FMath::Clamp(bHeld ? Safe + Delta : Safe - Delta, 0.0f, 1.0f);
	}

	/** Multiplicador da aderência traseira: 1 sem freio de mão, HandbrakeRearGrip com a rampa cheia, interpolado no meio. */
	static float RearGripMultiplier(float HandbrakeRearGrip, float Ramp)
	{
		return FMath::Lerp(1.0f, HandbrakeRearGrip, FMath::Clamp(Ramp, 0.0f, 1.0f));
	}
};
