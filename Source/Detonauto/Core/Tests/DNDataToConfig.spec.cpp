// Testes de ToConfig() das linhas de ajuste (Turbo, Crash, Takedown, Damage): cada campo da linha chega ao campo de
// mesmo nome da struct pura.
//
// Por que existe: o teste de deriva compara o CSV (via ToConfig) com o PADRÃO da struct. Se ToConfig() esquecesse de
// copiar um campo, a struct ficaria com o padrão dela, que é igual ao CSV, e a deriva passaria sem ver o erro. Aqui
// cada campo recebe um valor DIFERENTE do padrão e diferente dos demais.

#include "Misc/AutomationTest.h"
#include "Core/Data/DNDataRows.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FDNDataToConfigSpec, "Detonauto.Dados.ToConfig",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FDNDataToConfigSpec)

void FDNDataToConfigSpec::Define()
{
	Describe("ToConfig copia todos os campos", [this]()
	{
		It("Turbo", [this]()
		{
			FDNTurboConfigRow R;
			R.MaxBar = 101.f; R.WrongWayMinSpeedKmh = 81.f; R.WrongWayGainPerSecond = 13.f; R.NearMissMaxLateralCm = 151.f;
			R.NearMissMinSpeedKmh = 61.f; R.NearMissGain = 7.f; R.NearMissChainWindowSeconds = 2.5f;
			R.NearMissChainMultipliers = { 1.1f, 1.3f, 9.f }; R.DriftMinAngleDeg = 16.f; R.DriftMaxAngleDeg = 71.f;
			R.DriftMinSpeedKmh = 62.f; R.DriftGainPerSecond = 9.f; R.AirTimeThresholdSeconds = 0.4f; R.AirGainPerSecond = 11.f;
			R.AirMaxGainPerJump = 21.f; R.TakedownSetsBarTo = 99.f; R.ConsumptionPerSecond = 26.f; R.ExtendedBonus = 51.f;
			const FDNTurboConfig C = R.ToConfig();
			TestEqual("MaxBar", C.MaxBar, 101.f);
			TestEqual("WrongWayMinSpeedKmh", C.WrongWayMinSpeedKmh, 81.f);
			TestEqual("WrongWayGainPerSecond", C.WrongWayGainPerSecond, 13.f);
			TestEqual("NearMissMaxLateralCm", C.NearMissMaxLateralCm, 151.f);
			TestEqual("NearMissMinSpeedKmh", C.NearMissMinSpeedKmh, 61.f);
			TestEqual("NearMissGain", C.NearMissGain, 7.f);
			TestEqual("NearMissChainWindowSeconds", C.NearMissChainWindowSeconds, 2.5f);
			if (TestEqual("NearMissChainMultipliers (tamanho)", C.NearMissChainMultipliers.Num(), 3))
			{
				TestEqual("NearMissChainMultipliers[0]", C.NearMissChainMultipliers[0], 1.1f);
				TestEqual("NearMissChainMultipliers[2]", C.NearMissChainMultipliers[2], 9.f);
			}
			TestEqual("DriftMinAngleDeg", C.DriftMinAngleDeg, 16.f);
			TestEqual("DriftMaxAngleDeg", C.DriftMaxAngleDeg, 71.f);
			TestEqual("DriftMinSpeedKmh", C.DriftMinSpeedKmh, 62.f);
			TestEqual("DriftGainPerSecond", C.DriftGainPerSecond, 9.f);
			TestEqual("AirTimeThresholdSeconds", C.AirTimeThresholdSeconds, 0.4f);
			TestEqual("AirGainPerSecond", C.AirGainPerSecond, 11.f);
			TestEqual("AirMaxGainPerJump", C.AirMaxGainPerJump, 21.f);
			TestEqual("TakedownSetsBarTo", C.TakedownSetsBarTo, 99.f);
			TestEqual("ConsumptionPerSecond", C.ConsumptionPerSecond, 26.f);
			TestEqual("ExtendedBonus", C.ExtendedBonus, 51.f);
		});

		It("Crash", [this]()
		{
			FDNCrashConfigRow R;
			R.MaxMultiplier = 9; R.ChainWindowSeconds = 1.6f; R.ChainBonusPercentPerLink = 11; R.ChainBonusMaxPercent = 101;
			R.QuietSpeedMetersPerSecond = 2.1f; R.QuietSecondsToEnd = 3.1f; R.MaxSecondsAfterFirstImpact = 31.f;
			const FDNCrashConfig C = R.ToConfig();
			TestEqual("MaxMultiplier", C.MaxMultiplier, 9);
			TestEqual("ChainWindowSeconds", C.ChainWindowSeconds, 1.6f);
			TestEqual("ChainBonusPercentPerLink", C.ChainBonusPercentPerLink, 11);
			TestEqual("ChainBonusMaxPercent", C.ChainBonusMaxPercent, 101);
			TestEqual("QuietSpeedMetersPerSecond", C.QuietSpeedMetersPerSecond, 2.1f);
			TestEqual("QuietSecondsToEnd", C.QuietSecondsToEnd, 3.1f);
			TestEqual("MaxSecondsAfterFirstImpact", C.MaxSecondsAfterFirstImpact, 31.f);
		});

		It("Takedown", [this]()
		{
			FDNTakedownConfigRow R;
			R.MinRelativeSpeedKmh = 21.f; R.RivalCrashWindowSeconds = 1.6f; R.VerticalMinHeightCm = 101.f; R.BehindMaxAngleDegrees = 31.f;
			R.TBoneMinAngleDegrees = 61.f; R.TBoneMaxAngleDegrees = 121.f; R.SideMaxAngleDegrees = 59.f; R.ZoneHalfAngleDegrees = 44.f;
			const FDNTakedownConfig C = R.ToConfig();
			TestEqual("MinRelativeSpeedKmh", C.MinRelativeSpeedKmh, 21.f);
			TestEqual("RivalCrashWindowSeconds", C.RivalCrashWindowSeconds, 1.6f);
			TestEqual("VerticalMinHeightCm", C.VerticalMinHeightCm, 101.f);
			TestEqual("BehindMaxAngleDegrees", C.BehindMaxAngleDegrees, 31.f);
			TestEqual("TBoneMinAngleDegrees", C.TBoneMinAngleDegrees, 61.f);
			TestEqual("TBoneMaxAngleDegrees", C.TBoneMaxAngleDegrees, 121.f);
			TestEqual("SideMaxAngleDegrees", C.SideMaxAngleDegrees, 59.f);
			TestEqual("ZoneHalfAngleDegrees", C.ZoneHalfAngleDegrees, 44.f);
		});

		It("Car", [this]()
		{
			FDNCarConfigRow R;
			R.MassKg = 1201.f; R.MaxTorque = 851.f; R.LateralGrip = 1.1f; R.SuspensionStiffness = 171.f; R.SteeringGain = 1.2f;
			R.CenterOfMassDropCm = 26.f; R.SuspensionDampingRatio = 0.56f; R.SuspensionTravelCm = 19.f; R.RollbarScaling = 0.06f;
			R.RearGripFactor = 0.86f; R.HandbrakeRearGrip = 0.36f; R.HandbrakeRampInSeconds = 0.13f; R.HandbrakeRampOutSeconds = 0.71f;
			R.HandbrakeTorque = 3001.f; R.NitroAccelMps2 = 13.f;
			R.DriftAssistStartDeg = 22.f; R.DriftAssistGain = 11.f; R.DriftAssistMax = 333.f;
			const FDNCarConfig C = R.ToConfig();
			TestEqual("MassKg", C.MassKg, 1201.f);
			TestEqual("MaxTorque", C.MaxTorque, 851.f);
			TestEqual("LateralGrip", C.LateralGrip, 1.1f);
			TestEqual("SuspensionStiffness", C.SuspensionStiffness, 171.f);
			TestEqual("SteeringGain", C.SteeringGain, 1.2f);
			TestEqual("CenterOfMassDropCm", C.CenterOfMassDropCm, 26.f);
			TestEqual("SuspensionDampingRatio", C.SuspensionDampingRatio, 0.56f);
			TestEqual("SuspensionTravelCm", C.SuspensionTravelCm, 19.f);
			TestEqual("RollbarScaling", C.RollbarScaling, 0.06f);
			TestEqual("RearGripFactor", C.RearGripFactor, 0.86f);
			TestEqual("HandbrakeRearGrip", C.HandbrakeRearGrip, 0.36f);
			TestEqual("HandbrakeRampInSeconds", C.HandbrakeRampInSeconds, 0.13f);
			TestEqual("HandbrakeRampOutSeconds", C.HandbrakeRampOutSeconds, 0.71f);
			TestEqual("HandbrakeTorque", C.HandbrakeTorque, 3001.f);
			TestEqual("DriftAssistStartDeg", C.DriftAssistStartDeg, 22.f);
			TestEqual("DriftAssistGain", C.DriftAssistGain, 11.f);
			TestEqual("DriftAssistMax", C.DriftAssistMax, 333.f);
			TestEqual("NitroAccelMps2", C.NitroAccelMps2, 13.f);
		});

		It("Camera", [this]()
		{
			FDNCameraConfigRow R;
			R.Anchor = TEXT("Car"); R.DistanceCm = 0.f; R.HeightCm = 101.f; R.ForwardCm = 143.f; R.SideCm = -31.f; R.PitchDeg = -4.f; R.Fov = 103.f; R.Smoothing = 7.f;
			const FDNCameraConfig C = R.ToConfig();
			TestEqual("Anchor", C.Anchor, EDNCameraAnchor::Car);
			TestEqual("DistanceCm", C.DistanceCm, 0.f);
			TestEqual("HeightCm", C.HeightCm, 101.f);
			TestEqual("ForwardCm", C.ForwardCm, 143.f);
			TestEqual("SideCm", C.SideCm, -31.f);
			TestEqual("PitchDeg", C.PitchDeg, -4.f);
			TestEqual("Fov", C.Fov, 103.f);
			TestEqual("Smoothing", C.Smoothing, 7.f);
			FDNCameraConfigRow Chase;
			Chase.Anchor = TEXT("chase");
			TestEqual("Chase (caixa diferente)", Chase.ToConfig().Anchor, EDNCameraAnchor::Chase);
			FDNCameraConfigRow Invalido;
			Invalido.Anchor = TEXT("Orbit");
			TestEqual("texto invalido cai em Chase (a validacao ja recusou antes)", Invalido.ToConfig().Anchor, EDNCameraAnchor::Chase);
		});

		It("Damage", [this]()
		{
			FDNDamageConfigRow R;
			R.DentMinDeltaVMs = 2.5f; R.ZoneDestroyDeltaVMs = 31.f; R.DentMaxDepthCm = 26.f; R.DentDepthPerMs = 1.6f;
			R.DentBaseRadiusCm = 31.f; R.MaxDents = 9; R.PartDetachDeltaVMs = 13.f; R.PartDetachZoneDamage = 0.7f;
			R.DestroyedTotalDamage = 0.6f; R.MaxActive = 25; R.MorphCurveExponent = 0.8f; R.MorphRisePerSecond = 12.f;
			const FDNDamageConfig C = R.ToConfig();
			TestEqual("MorphCurveExponent", C.MorphCurveExponent, 0.8f);
			TestEqual("MorphRisePerSecond", C.MorphRisePerSecond, 12.f);
			TestEqual("DentMinDeltaVMs", C.DentMinDeltaVMs, 2.5f);
			TestEqual("ZoneDestroyDeltaVMs", C.ZoneDestroyDeltaVMs, 31.f);
			TestEqual("DentMaxDepthCm", C.DentMaxDepthCm, 26.f);
			TestEqual("DentDepthPerMs", C.DentDepthPerMs, 1.6f);
			TestEqual("DentBaseRadiusCm", C.DentBaseRadiusCm, 31.f);
			TestEqual("MaxDents", C.MaxDents, 9);
			TestEqual("PartDetachDeltaVMs", C.PartDetachDeltaVMs, 13.f);
			TestEqual("PartDetachZoneDamage", C.PartDetachZoneDamage, 0.7f);
			TestEqual("DestroyedTotalDamage", C.DestroyedTotalDamage, 0.6f);
			TestEqual("MaxActive", C.MaxActive, 25);
		});

		It("TrafficSettings (ToSettings)", [this]()
		{
			FDNTrafficSettingsRow R;
			R.WakeDistanceM = 5.f; R.WakeAheadM = 24.f; R.WakeAheadMinClosingKmh = 19.f; R.DespawnDistanceM = 199.f; R.StillSpeedMs = 1.9f;
			R.MaxSpawnPerFrame = 3; R.FollowMinGapM = 7.f; R.FollowCatchUpGapM = 21.f; R.RollingFriction = 0.03f;
			const FDNTrafficSettings S = R.ToSettings();
			TestEqual("WakeDistanceM", S.WakeDistanceM, 5.f);
			TestEqual("WakeAheadM", S.WakeAheadM, 24.f);
			TestEqual("WakeAheadMinClosingKmh", S.WakeAheadMinClosingKmh, 19.f);
			TestEqual("DespawnDistanceM", S.DespawnDistanceM, 199.f);
			TestEqual("StillSpeedMs", S.StillSpeedMs, 1.9f);
			TestEqual("MaxSpawnPerFrame", S.MaxSpawnPerFrame, 3);
			TestEqual("FollowMinGapM", S.FollowMinGapM, 7.f);
			TestEqual("FollowCatchUpGapM", S.FollowCatchUpGapM, 21.f);
			TestEqual("RollingFriction", S.RollingFriction, 0.03f);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
