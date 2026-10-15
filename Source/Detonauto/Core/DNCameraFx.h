// Câmera de velocidade e de batida (lote 18, docs/regras-camera.md): campo de visão pela velocidade, tremor, desfoque nas bordas,
// câmera de batida com câmera lenta e o controle do destroço. Regras puras, sem mundo. Testes: Core/Tests/DNCameraFx.spec.cpp.
//
// TODO TEMPO AQUI É TEMPO REAL (delta não dilatado): a câmera lenta muda a escala de tempo do jogo, e se estas regras andassem no
// tempo do jogo a câmera lenta prolongaria a si mesma. Dados: Data/CameraFx.csv (uma linha) e as colunas SpeedFovScale e ShakeScale
// de Data/Cameras.csv.

#pragma once

#include "CoreMinimal.h"
#include "Core/DNBreakable.h"

/** Os valores globais (a linha de Data/CameraFx.csv). */
struct FDNCameraFxSettings
{
	float SpeedFovStartKmh = 60.0f;
	float SpeedFovFullKmh = 200.0f;
	float SpeedFovMaxAddDeg = 8.0f;
	float TurboFovAddDeg = 12.0f;
	float MaxFovDeg = 115.0f;
	float FovBlendDegPerS = 30.0f;
	float ShakeStartKmh = 150.0f;
	float ShakeFullKmh = 250.0f;
	float ShakeMaxDeg = 0.35f;
	float ShakeHz = 9.0f;
	float EdgeBlurStartKmh = 100.0f;
	float EdgeBlurFullKmh = 250.0f;
	float EdgeBlurMax = 0.5f;
	float EdgeBlurInnerRadius = 0.4f;
	float CrashCamMinDeltaVMs = 15.0f;
	float CrashCamTimeScale = 0.25f;
	float CrashCamEnterS = 0.15f;
	float CrashCamHoldS = 1.5f;
	float CrashCamExitS = 0.4f;
	float CrashCamCooldownS = 2.0f;
	float CrashCamOrbitRadiusM = 7.0f;
	float CrashCamOrbitHeightM = 2.5f;
	float CrashCamOrbitDegPerS = 25.0f;
	float CrashCamProbeCm = 20.0f;
	/** A órbita começa tantos graus antes do lado da batida e gira para ele (aprovado por captura no lote 18). */
	float CrashCamLeadDeg = 90.0f;
	float WreckSteerAccelMs2 = 6.0f;
	float WreckSteerMaxDeltaVMs = 6.0f;
};

struct FDNCameraFx
{
	/** smoothstep de 0 a 1 entre A e B. Valor NaN ou negativo conta como 0; com A >= B vira degrau em B. */
	static float Ramp(float Value, float A, float B);
};

struct FDNSpeedFov
{
	/** Alvo: Base + SpeedFovMaxAddDeg x S(v) x SpeedFovScale + TurboFovAddDeg x TurboBlend, no máximo MaxFovDeg. */
	static float Target(const FDNCameraFxSettings& S, float BaseFovDeg, float SpeedKmh, float SpeedFovScale, float TurboBlend);
	/** O valor mostrado anda até o alvo a no máximo FovBlendDegPerS (RealDt em segundos reais): a freada da batida não dá tranco. */
	static float Step(const FDNCameraFxSettings& S, float CurrentDeg, float TargetDeg, float RealDt);
};

struct FDNSpeedShake
{
	/** ShakeMaxDeg x S(v) x ShakeScale x GlobalScale; exatamente 0 abaixo de ShakeStartKmh. GlobalScale é a opção de desligar. */
	static float Amplitude(const FDNCameraFxSettings& S, float SpeedKmh, float ShakeScale, float GlobalScale);
	/** Arfagem (X) e guinada (Y) em graus no instante TimeS (tempo real acumulado): Perlin 1D determinístico, eixos defasados. */
	static FVector2D Offset(const FDNCameraFxSettings& S, float SpeedKmh, float ShakeScale, float GlobalScale, double TimeS);
};

struct FDNEdgeBlur
{
	/** Força do desfoque nas bordas: EdgeBlurMax x S(v) entre EdgeBlurStartKmh e EdgeBlurFullKmh. */
	static float Strength(const FDNCameraFxSettings& S, float SpeedKmh);
};

enum class EDNCrashCamPhase : uint8
{
	Idle,
	Entering,
	Holding,
	Exiting,
	Cooldown
};

struct FDNCrashCamState
{
	EDNCrashCamPhase Phase = EDNCrashCamPhase::Idle;
	/** Tempo real dentro da fase [s]. */
	float PhaseSeconds = 0.0f;
};

struct FDNCrashCam
{
	/** Dispara se estiver em Idle, a fonte contar (FDNImpactPolicy::TriggersCrashCam) e DeltaV >= CrashCamMinDeltaVMs. */
	static bool TryTrigger(const FDNCameraFxSettings& S, FDNCrashCamState& State, float DeltaVMs, EDNImpactSource Source);
	/** Avança RealDt de tempo real; nas bordas troca de fase e a sobra passa para a fase seguinte. */
	static void Step(const FDNCameraFxSettings& S, FDNCrashCamState& State, float RealDt);
	/** Escala de tempo do jogo: 1 fora; no Entering desce de 1 a CrashCamTimeScale com saída suave; no Holding fica nela; no Exiting
	 *  volta a 1 (smoothstep). */
	static float TimeScale(const FDNCameraFxSettings& S, const FDNCrashCamState& State);
	/** Peso da câmera de batida: 0 fora, sobe no Entering (a mesma curva da escala), 1 no Holding, desce no Exiting. */
	static float BlendWeight(const FDNCameraFxSettings& S, const FDNCrashCamState& State);
	/** O controle do destroço vale no Entering e no Holding. */
	static bool SteerActive(const FDNCrashCamState& State);
};

struct FDNWreckSteer
{
	/**
	 *  Velocidade a somar ao carro neste passo [cm/s], sem depender da massa: perpendicular à velocidade horizontal do carro, para o
	 *  lado da tela que a entrada pede (ScreenRight = a direita da câmera), aceleração WreckSteerAccelMs2 x |SteerInput| x RealDt,
	 *  limitada ao que sobra de WreckSteerMaxDeltaVMs na batida (InOutUsedMs soma o que já foi usado). Inativo ou inválido: zero.
	 */
	static FVector DeltaVelocity(const FDNCameraFxSettings& S, bool bActive, float SteerInput, const FVector& CarVelocityCmS,
		const FVector& ScreenRight, float RealDt, float& InOutUsedMs);
};
