// Projeto descartável 2 (Sprint 4) e lote 8 (afinação arcade): parâmetros de ajuste do carro lidos de CSV.
// Esta parte não depende de mundo nem de física, então é testável por Automation
// (DNCarTuning.spec.cpp).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "DNCarTuning.generated.h"

/**
 *  Uma linha de Data/TestCars.csv. A primeira coluna do CSV é o nome da linha.
 *  Os nomes das colunas são os nomes dos campos abaixo.
 *
 *  Os valores PADRÃO dos campos novos (lote 8) são os do carro do template: uma linha só com as cinco colunas
 *  antigas continua dando o mesmo carro de antes.
 */
USTRUCT(BlueprintType)
struct FDNTestCarRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Massa do chassi [kg]. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MassKg = 1500.0f;

	/** Torque máximo do motor [N·m, na unidade do Chaos]. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxTorque = 750.0f;

	/** Multiplicador da curva de aderência lateral (LateralSlipGraphMultiplier do Chaos), aplicado nas 4 rodas (1 = padrão). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float LateralGrip = 1.0f;

	/** Rigidez da mola da suspensão (SpringRate do Chaos; o padrão é 250). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SuspensionStiffness = 250.0f;

	/** Ganho do volante: multiplica a entrada de direção antes de ir ao Chaos (1 = padrão). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SteeringGain = 1.0f;

	// ---------------- lote 8: afinação arcade ----------------

	/** Quanto o centro de massa desce em relação ao calculado pela malha [cm]. 0 = não mexe. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CenterOfMassDropCm = 0.0f;

	/** Amortecimento da suspensão (SuspensionDampingRatio do Chaos, 0 a 1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SuspensionDampingRatio = 0.5f;

	/** Curso da suspensão para cima e para baixo [cm] (SuspensionMaxRaise e SuspensionMaxDrop do Chaos). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SuspensionTravelCm = 10.0f;

	/** Barra estabilizadora (RollbarScaling do Chaos, 0 a 1). Menor = a carroceria rola mais nas curvas. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float RollbarScaling = 0.15f;

	/**
	 *  Aderência do eixo TRASEIRO em relação ao padrão (1 = igual ao template). Multiplica a FrictionForceMultiplier
	 *  e o multiplicador da curva lateral das rodas traseiras. Abaixo de ~0,75 a traseira passa a ter menos
	 *  aderência que a dianteira (no template a traseira tem 4,0 contra 3,0 da frente) e o carro tende a sair de traseira.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float RearGripFactor = 1.0f;

	/** Multiplicador extra da aderência traseira com o freio de mão acionado (1 = o freio de mão não solta a traseira). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float HandbrakeRearGrip = 1.0f;

	/** Tempo para a perda de aderência do freio de mão chegar ao máximo, depois de apertar [s]. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float HandbrakeRampInSeconds = 0.1f;

	/** Tempo para a aderência voltar ao normal depois de soltar o freio de mão [s]. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float HandbrakeRampOutSeconds = 0.5f;

	/** Torque do freio de mão nas rodas traseiras (MaxHandBrakeTorque do Chaos). O template usa 6000. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float HandbrakeTorque = 6000.0f;

	/** Câmera: distância atrás do carro [cm] (TargetArmLength). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CameraDistanceCm = 650.0f;

	/** Câmera: altura acima do carro [cm] (SocketOffset.Z). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CameraHeightCm = 150.0f;

	/** Câmera: campo de visão [graus]. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CameraFov = 90.0f;

	/** Nitro de teste: aceleração extra para frente enquanto Shift está apertado [m/s²]. 0 = sem nitro. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float NitroAccelMps2 = 10.0f;

	/** Nitro de teste: quanto da barra (0 a 100) gasta por segundo. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float NitroDrainPerSecond = 25.0f;

	/** Nitro de teste: quanto da barra recarrega por segundo com o Shift solto. 0 = não recarrega. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float NitroRegenPerSecond = 10.0f;
};

/**
 *  Parâmetros já validados e limitados (clamp), prontos para aplicar no veículo.
 *  Regra: valores fora do intervalo são limitados; NaN vira o valor padrão.
 */
struct FDNCarTuning
{
	// Limites (suposição de trabalho: faixas largas o bastante para experimentar sem quebrar a simulação)
	static constexpr float MinMassKg = 100.0f;
	static constexpr float MaxMassKg = 10000.0f;
	static constexpr float MinMaxTorque = 50.0f;
	static constexpr float MaxMaxTorque = 5000.0f;
	static constexpr float MinLateralGrip = 0.1f;
	static constexpr float MaxLateralGrip = 5.0f;
	static constexpr float MinSpringRate = 10.0f;
	static constexpr float MaxSpringRate = 2000.0f;
	static constexpr float MinSteeringGain = 0.1f;
	static constexpr float MaxSteeringGain = 3.0f;

	static constexpr float MaxCenterOfMassDropCm = 60.0f;
	static constexpr float MinDampingRatio = 0.05f;
	static constexpr float MaxDampingRatio = 1.0f;
	static constexpr float MinTravelCm = 2.0f;
	static constexpr float MaxTravelCm = 40.0f;
	static constexpr float MaxRollbarScaling = 1.0f;
	static constexpr float MinRearGripFactor = 0.2f;
	static constexpr float MaxRearGripFactor = 2.0f;
	static constexpr float MinHandbrakeRearGrip = 0.05f;
	static constexpr float MaxHandbrakeRearGrip = 1.5f;
	static constexpr float MinRampSeconds = 0.01f;
	static constexpr float MaxRampSeconds = 3.0f;
	static constexpr float MaxHandbrakeTorque = 20000.0f;
	static constexpr float MinCameraDistanceCm = 150.0f;
	static constexpr float MaxCameraDistanceCm = 2000.0f;
	static constexpr float MaxCameraHeightCm = 600.0f;
	static constexpr float MinCameraFov = 60.0f;
	static constexpr float MaxCameraFov = 120.0f;
	static constexpr float MaxNitroAccel = 40.0f;
	static constexpr float MinNitroDrain = 0.1f;
	static constexpr float MaxNitroDrain = 200.0f;
	static constexpr float MaxNitroRegen = 100.0f;

	/** Caminho do CSV relativo à pasta do projeto. */
	static constexpr const TCHAR* CsvRelativePath = TEXT("Data/TestCars.csv");

	/** Linha usada quando nenhuma outra é pedida. */
	inline static const FName DefaultRowName = FName(TEXT("Padrao"));

	float MassKg = 1500.0f;
	float MaxTorque = 750.0f;
	float LateralGrip = 1.0f;
	float SpringRate = 250.0f;
	float SteeringGain = 1.0f;

	float CenterOfMassDropCm = 0.0f;
	float SuspensionDampingRatio = 0.5f;
	float SuspensionTravelCm = 10.0f;
	float RollbarScaling = 0.15f;
	float RearGripFactor = 1.0f;
	float HandbrakeRearGrip = 1.0f;
	float HandbrakeRampInSeconds = 0.1f;
	float HandbrakeRampOutSeconds = 0.5f;
	float HandbrakeTorque = 6000.0f;
	float CameraDistanceCm = 650.0f;
	float CameraHeightCm = 150.0f;
	float CameraFov = 90.0f;
	float NitroAccelMps2 = 10.0f;
	float NitroDrainPerSecond = 25.0f;
	float NitroRegenPerSecond = 10.0f;

	/** Converte uma linha da tabela em parâmetros, aplicando os limites. */
	static FDNCarTuning FromRow(const FDNTestCarRow& Row)
	{
		FDNCarTuning Out;
		Out.MassKg = Sanitize(Row.MassKg, MinMassKg, MaxMassKg, Out.MassKg);
		Out.MaxTorque = Sanitize(Row.MaxTorque, MinMaxTorque, MaxMaxTorque, Out.MaxTorque);
		Out.LateralGrip = Sanitize(Row.LateralGrip, MinLateralGrip, MaxLateralGrip, Out.LateralGrip);
		Out.SpringRate = Sanitize(Row.SuspensionStiffness, MinSpringRate, MaxSpringRate, Out.SpringRate);
		Out.SteeringGain = Sanitize(Row.SteeringGain, MinSteeringGain, MaxSteeringGain, Out.SteeringGain);

		Out.CenterOfMassDropCm = Sanitize(Row.CenterOfMassDropCm, 0.0f, MaxCenterOfMassDropCm, Out.CenterOfMassDropCm);
		Out.SuspensionDampingRatio = Sanitize(Row.SuspensionDampingRatio, MinDampingRatio, MaxDampingRatio, Out.SuspensionDampingRatio);
		Out.SuspensionTravelCm = Sanitize(Row.SuspensionTravelCm, MinTravelCm, MaxTravelCm, Out.SuspensionTravelCm);
		Out.RollbarScaling = Sanitize(Row.RollbarScaling, 0.0f, MaxRollbarScaling, Out.RollbarScaling);
		Out.RearGripFactor = Sanitize(Row.RearGripFactor, MinRearGripFactor, MaxRearGripFactor, Out.RearGripFactor);
		Out.HandbrakeRearGrip = Sanitize(Row.HandbrakeRearGrip, MinHandbrakeRearGrip, MaxHandbrakeRearGrip, Out.HandbrakeRearGrip);
		Out.HandbrakeRampInSeconds = Sanitize(Row.HandbrakeRampInSeconds, MinRampSeconds, MaxRampSeconds, Out.HandbrakeRampInSeconds);
		Out.HandbrakeRampOutSeconds = Sanitize(Row.HandbrakeRampOutSeconds, MinRampSeconds, MaxRampSeconds, Out.HandbrakeRampOutSeconds);
		Out.HandbrakeTorque = Sanitize(Row.HandbrakeTorque, 0.0f, MaxHandbrakeTorque, Out.HandbrakeTorque);
		Out.CameraDistanceCm = Sanitize(Row.CameraDistanceCm, MinCameraDistanceCm, MaxCameraDistanceCm, Out.CameraDistanceCm);
		Out.CameraHeightCm = Sanitize(Row.CameraHeightCm, 0.0f, MaxCameraHeightCm, Out.CameraHeightCm);
		Out.CameraFov = Sanitize(Row.CameraFov, MinCameraFov, MaxCameraFov, Out.CameraFov);
		Out.NitroAccelMps2 = Sanitize(Row.NitroAccelMps2, 0.0f, MaxNitroAccel, Out.NitroAccelMps2);
		Out.NitroDrainPerSecond = Sanitize(Row.NitroDrainPerSecond, MinNitroDrain, MaxNitroDrain, Out.NitroDrainPerSecond);
		Out.NitroRegenPerSecond = Sanitize(Row.NitroRegenPerSecond, 0.0f, MaxNitroRegen, Out.NitroRegenPerSecond);
		return Out;
	}

	/**
	 *  Uma linha é válida se todos os valores são finitos e maiores que zero (os campos novos que podem valer zero estão
	 *  marcados abaixo). Fora dos limites ainda é válido (será limitado).
	 */
	static bool ValidateRow(const FDNTestCarRow& Row, FString& OutError)
	{
		struct FField { const TCHAR* Name; float Value; bool bZeroOk; };
		const FField Fields[] = {
			{ TEXT("MassKg"), Row.MassKg, false }, { TEXT("MaxTorque"), Row.MaxTorque, false },
			{ TEXT("LateralGrip"), Row.LateralGrip, false }, { TEXT("SuspensionStiffness"), Row.SuspensionStiffness, false },
			{ TEXT("SteeringGain"), Row.SteeringGain, false },
			{ TEXT("CenterOfMassDropCm"), Row.CenterOfMassDropCm, true },
			{ TEXT("SuspensionDampingRatio"), Row.SuspensionDampingRatio, false },
			{ TEXT("SuspensionTravelCm"), Row.SuspensionTravelCm, false },
			{ TEXT("RollbarScaling"), Row.RollbarScaling, true },
			{ TEXT("RearGripFactor"), Row.RearGripFactor, false },
			{ TEXT("HandbrakeRearGrip"), Row.HandbrakeRearGrip, false },
			{ TEXT("HandbrakeRampInSeconds"), Row.HandbrakeRampInSeconds, false },
			{ TEXT("HandbrakeRampOutSeconds"), Row.HandbrakeRampOutSeconds, false },
			{ TEXT("HandbrakeTorque"), Row.HandbrakeTorque, true },
			{ TEXT("CameraDistanceCm"), Row.CameraDistanceCm, false },
			{ TEXT("CameraHeightCm"), Row.CameraHeightCm, true },
			{ TEXT("CameraFov"), Row.CameraFov, false },
			{ TEXT("NitroAccelMps2"), Row.NitroAccelMps2, true },
			{ TEXT("NitroDrainPerSecond"), Row.NitroDrainPerSecond, false },
			{ TEXT("NitroRegenPerSecond"), Row.NitroRegenPerSecond, true } };

		for (const FField& Field : Fields)
		{
			const bool bTooLow = Field.bZeroOk ? Field.Value < 0.0f : Field.Value <= 0.0f;
			if (!FMath::IsFinite(Field.Value) || bTooLow)
			{
				OutError = FString::Printf(TEXT("%s precisa ser finito e %s (veio %f)"), Field.Name,
					Field.bZeroOk ? TEXT("maior ou igual a zero") : TEXT("maior que zero"), Field.Value);
				return false;
			}
		}
		return true;
	}

private:

	static float Sanitize(float Value, float Min, float Max, float Fallback)
	{
		return FMath::IsFinite(Value) ? FMath::Clamp(Value, Min, Max) : FMath::Clamp(Fallback, Min, Max);
	}
};

/**
 *  Rampa do freio de mão: quanto do efeito está aplicado agora (0 a 1). Sobe em InSeconds ao apertar e desce em
 *  OutSeconds ao soltar. O Chaos não tem isso; é a "rampa de volta" pedida no lote 8.
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

/**
 *  Nitro de teste (só para o Felipe sentir a diferença no projeto descartável). NÃO é o FDNTurbo do projeto principal:
 *  barra fictícia de 0 a 100, que gasta segurando e recarrega solto.
 */
struct FDNTestNitro
{
	static constexpr float MaxBar = 100.0f;

	float Bar = MaxBar;
	bool bActive = false;

	void Step(bool bHeld, float DeltaTime, float DrainPerSecond, float RegenPerSecond)
	{
		if (!FMath::IsFinite(Bar))
		{
			Bar = 0.0f;
		}
		if (!FMath::IsFinite(DeltaTime) || DeltaTime <= 0.0f)
		{
			return;
		}
		const float Drain = FMath::IsFinite(DrainPerSecond) ? FMath::Max(0.0f, DrainPerSecond) : 0.0f;
		const float Regen = FMath::IsFinite(RegenPerSecond) ? FMath::Max(0.0f, RegenPerSecond) : 0.0f;

		if (bHeld && Bar > 0.0f)
		{
			Bar = FMath::Max(0.0f, Bar - Drain * DeltaTime);
			bActive = Bar > 0.0f;
		}
		else
		{
			bActive = false;
			if (!bHeld)
			{
				Bar = FMath::Min(MaxBar, Bar + Regen * DeltaTime);
			}
		}
	}
};
