// Conta das rodas visuais (lote 14, B1): puro, sem mundo. Testes: Core/Tests/DNWheelVisual.spec.cpp.
//
// O Chaos entrega, por roda, o giro (pitch), o esterçamento (yaw) e o curso da suspensão no ESPAÇO DO COMPONENTE (é o que o
// FAnimNode_WheelController do template faz: rotação nova = RotOffset x rotação do osso, tudo no espaço do componente). Nossa roda
// visual é uma malha estática presa ao osso, que pode ter qualquer orientação; por isso a rotação e o deslocamento relativos ao osso
// são a conjugação do que o Chaos calculou.

#pragma once

#include "CoreMinimal.h"

struct FDNWheelVisual
{
	/** Rotação da malha relativa ao osso: Osso^-1 x Giro(pitch, yaw) x Osso. Giro e esterço zero: identidade. */
	static FQuat RelativeRotation(const FQuat& BoneRotationInComponent, float PitchDegrees, float YawDegrees)
	{
		const FQuat Offset(FRotator(PitchDegrees, YawDegrees, 0.0f));
		return BoneRotationInComponent.Inverse() * Offset * BoneRotationInComponent;
	}

	/** Deslocamento (espaço do componente) levado para o espaço do osso. */
	static FVector RelativeLocation(const FQuat& BoneRotationInComponent, const FVector& OffsetInComponent)
	{
		return BoneRotationInComponent.UnrotateVector(OffsetInComponent);
	}
};
