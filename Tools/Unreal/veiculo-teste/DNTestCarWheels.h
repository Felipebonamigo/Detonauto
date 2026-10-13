// Rodas do carro de teste (projeto descartável 2), definidas inteiramente em C++.

#pragma once

#include "CoreMinimal.h"
#include "ChaosVehicleWheel.h"
#include "DNTestCarWheels.generated.h"

/** Roda dianteira: esterça e freia. Valores iniciais iguais aos do carro esportivo do template. */
UCLASS()
class UDNTestWheelFront : public UChaosVehicleWheel
{
	GENERATED_BODY()

public:

	/** FrictionForceMultiplier do template (a frente tem 3,0). */
	static constexpr float TemplateFriction = 3.0f;

	UDNTestWheelFront()
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
class UDNTestWheelRear : public UChaosVehicleWheel
{
	GENERATED_BODY()

public:

	/** FrictionForceMultiplier do template (a traseira tem 4,0, mais que a frente). */
	static constexpr float TemplateFriction = 4.0f;
	/** MaxHandBrakeTorque do template. */
	static constexpr float TemplateHandbrakeTorque = 6000.0f;

	UDNTestWheelRear()
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
		MaxBrakeTorque = 4500.0f;
		MaxHandBrakeTorque = 6000.0f;
	}
};
