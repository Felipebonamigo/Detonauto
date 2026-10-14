// Rodas do carro (lote 12), definidas inteiramente em C++: no template elas são Blueprints (BP_SportsCarWheelFront/Rear); aqui
// não há Blueprint nenhum. Valores iniciais iguais aos do carro esportivo do template Vehicle.
//
// O Chaos lê estas classes (o CDO) ao criar o veículo; por isso ADNVehicle grava os parâmetros de afinação aqui ANTES do
// registro e, ao trocar de preset jogando, recria o estado físico (ver ADNVehicle::WriteCreationParameters).

#pragma once

#include "CoreMinimal.h"
#include "ChaosVehicleWheel.h"
#include "DNVehicleWheels.generated.h"

/** Roda dianteira: esterça e freia. */
UCLASS()
class UDNVehicleWheelFront : public UChaosVehicleWheel
{
	GENERATED_BODY()

public:

	/** FrictionForceMultiplier do template (a frente tem 3,0). */
	static constexpr float TemplateFriction = 3.0f;

	UDNVehicleWheelFront()
	{
		AxleType = EAxleType::Front;
		bAffectedBySteering = true;
		MaxSteerAngle = 40.0f;

		WheelRadius = 39.0f;
		WheelWidth = 35.0f;
		FrictionForceMultiplier = TemplateFriction;
		MaxBrakeTorque = 4500.0f;
		MaxHandBrakeTorque = 6000.0f;
	}
};

/** Roda traseira: recebe o motor e o freio de mão. */
UCLASS()
class UDNVehicleWheelRear : public UChaosVehicleWheel
{
	GENERATED_BODY()

public:

	/** FrictionForceMultiplier do template (a traseira tem 4,0, mais que a frente). */
	static constexpr float TemplateFriction = 4.0f;

	UDNVehicleWheelRear()
	{
		AxleType = EAxleType::Rear;
		bAffectedByHandbrake = true;
		bAffectedByEngine = true;

		WheelRadius = 40.0f;
		WheelWidth = 40.0f;
		FrictionForceMultiplier = TemplateFriction;
		SlipThreshold = 100.0f;
		SkidThreshold = 100.0f;
		MaxSteerAngle = 0.0f;
		MaxBrakeTorque = 1500.0f; // o template tem 1500 atrás (o protótipo usava 4500, errado)
		MaxHandBrakeTorque = 6000.0f;
	}
};
