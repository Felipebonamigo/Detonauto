// Conta das rodas visuais (lote 14, B1): puro, sem mundo. Testes: Core/Tests/DNWheelVisual.spec.cpp.
//
// O Chaos entrega, por roda, o giro (pitch), o esterçamento (yaw) e o curso da suspensão no ESPAÇO DO COMPONENTE (é o que o
// FAnimNode_WheelController do template faz: rotação nova = RotOffset x rotação do osso, tudo no espaço do componente). Nossa roda
// visual é uma malha estática presa ao osso, que pode ter qualquer orientação; por isso a rotação e o deslocamento relativos ao osso
// desfazem a orientação do osso e aplicam o que o Chaos calculou no espaço do componente (lote 16: a malha não herda o osso).

#pragma once

#include "CoreMinimal.h"

struct FDNWheelVisual
{
	/**
	 *  Rotação da malha relativa ao osso: Osso^-1 x Giro(pitch, yaw) x Base(BaseYaw em Z). No espaço do componente a malha fica com
	 *  Giro x Base, qualquer que seja a orientação do osso: o eixo do cubo (Y local da malha) fica no Y do componente, e Base = 180
	 *  vira a face do aro para o outro lado. (Lote 14 usava "x Osso" no fim: a malha herdava a orientação do osso, girado ±90 graus
	 *  no SportsCar, e a roda ficava de lado.)
	 */
	static FQuat RelativeRotation(const FQuat& BoneRotationInComponent, float PitchDegrees, float YawDegrees, float BaseYawDegrees)
	{
		const FQuat Offset(FRotator(PitchDegrees, YawDegrees, 0.0f));
		return BoneRotationInComponent.Inverse() * Offset * FQuat(FRotator(0.0f, BaseYawDegrees, 0.0f));
	}

	/** Deslocamento (espaço do componente) levado para o espaço do osso. */
	static FVector RelativeLocation(const FQuat& BoneRotationInComponent, const FVector& OffsetInComponent)
	{
		return BoneRotationInComponent.UnrotateVector(OffsetInComponent);
	}
};
