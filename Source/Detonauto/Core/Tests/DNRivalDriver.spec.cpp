// Testes do piloto de corrida (D1 a D6): esterço com ganho que cai com a velocidade, velocidade desejada por curva, acelerador, freio.

#include <limits>
#include "Misc/AutomationTest.h"
#include "Core/DNRivalDriver.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FDNRivalDriverSpec, "Detonauto.Rival",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	const float NaNf = std::numeric_limits<float>::quiet_NaN();
	FDNRivalConfig Cfg; // 100 reta, 45 curva, 40 graus de curva cheia, 30 de volante cheio, mira 15 m + 0,4 m por km/h
END_DEFINE_SPEC(FDNRivalDriverSpec)

void FDNRivalDriverSpec::Define()
{
	Describe("Esterco (D1)", [this]()
	{
		It("tem o sinal do erro e cresce com ele", [this]()
		{
			// a 54 km/h (15 m/s) o ganho e 1
			TestEqual("sem erro", FDNRivalDriver::Command(0.f, 0.f, 54.f, Cfg).Steer, 0.f, 0.001f);
			TestEqual("15 graus: meio volante", FDNRivalDriver::Command(15.f, 0.f, 54.f, Cfg).Steer, 0.5f, 0.001f);
			TestEqual("-15 graus: meio volante para o outro lado", FDNRivalDriver::Command(-15.f, 0.f, 54.f, Cfg).Steer, -0.5f, 0.001f);
		});

		It("fica preso em 1", [this]()
		{
			TestEqual("90 graus", FDNRivalDriver::Command(90.f, 0.f, 54.f, Cfg).Steer, 1.f, 0.001f);
			TestEqual("-90 graus", FDNRivalDriver::Command(-90.f, 0.f, 54.f, Cfg).Steer, -1.f, 0.001f);
		});

		It("o ganho cai com a velocidade, com piso de 0,2 e teto de 1", [this]()
		{
			// 108 km/h = 30 m/s: ganho 0,5; 30 graus daria 1 com ganho 1, aqui 0,5
			TestEqual("108 km/h", FDNRivalDriver::Command(30.f, 0.f, 108.f, Cfg).Steer, 0.5f, 0.001f);
			// 360 km/h = 100 m/s: ganho 0,15 sobe ao piso 0,2
			TestEqual("360 km/h", FDNRivalDriver::Command(30.f, 0.f, 360.f, Cfg).Steer, 0.2f, 0.001f);
			// parado: velocidade tratada como 1 m/s, ganho 15 preso em 1
			TestEqual("parado", FDNRivalDriver::Command(15.f, 0.f, 0.f, Cfg).Steer, 0.5f, 0.001f);
		});
	});

	Describe("Velocidade desejada, acelerador e freio (D2 a D4)", [this]()
	{
		It("na reta o alvo e StraightKmh: parado, acelerador cheio e sem freio", [this]()
		{
			const FDNDriveCommand C = FDNRivalDriver::Command(0.f, 0.f, 0.f, Cfg);
			TestEqual("acelerador", C.Throttle, 1.f, 0.001f);
			TestEqual("freio", C.Brake, 0.f, 0.001f);
		});

		It("o acelerador sobe 0,08 por km/h que falta", [this]()
		{
			TestEqual("a 95 km/h na reta: 0,4 + 5 x 0,08", FDNRivalDriver::Command(0.f, 0.f, 95.f, Cfg).Throttle, 0.8f, 0.001f);
		});

		It("no regime da reta o acelerador e 0,4 (so manter)", [this]()
		{
			TestEqual("a 100 km/h", FDNRivalDriver::Command(0.f, 0.f, 100.f, Cfg).Throttle, 0.4f, 0.001f);
		});

		It("o alvo desce de reta para curva com o erro longe, em modulo", [this]()
		{
			// erro longe 20 graus = metade da escala: alvo 72,5 km/h; a 72,5 o acelerador e 0,4
			TestEqual("20 graus a 72,5 km/h", FDNRivalDriver::Command(0.f, 20.f, 72.5f, Cfg).Throttle, 0.4f, 0.001f);
			TestEqual("-20 graus (modulo)", FDNRivalDriver::Command(0.f, -20.f, 72.5f, Cfg).Throttle, 0.4f, 0.001f);
			// curva cheia (40 graus ou mais): alvo 45; a 45 o acelerador e 0,4
			TestEqual("40 graus a 45 km/h", FDNRivalDriver::Command(0.f, 40.f, 45.f, Cfg).Throttle, 0.4f, 0.001f);
			TestEqual("90 graus a 45 km/h: o alvo nao passa de CornerKmh", FDNRivalDriver::Command(0.f, 90.f, 45.f, Cfg).Throttle, 0.4f, 0.001f);
		});

		It("freia so quando passa do alvo em mais de 10 km/h", [this]()
		{
			// alvo 45 (curva cheia)
			TestEqual("a 55 km/h: nao freia", FDNRivalDriver::Command(0.f, 40.f, 55.f, Cfg).Brake, 0.f, 0.001f);
			TestEqual("a 85 km/h: freia (85-45)*0,04 = 1,6, preso em 1", FDNRivalDriver::Command(0.f, 40.f, 85.f, Cfg).Brake, 1.f, 0.001f);
			TestEqual("a 65 km/h: (65-45)*0,04 = 0,8", FDNRivalDriver::Command(0.f, 40.f, 65.f, Cfg).Brake, 0.8f, 0.001f);
			TestEqual("acima do alvo o acelerador cai a zero", FDNRivalDriver::Command(0.f, 40.f, 100.f, Cfg).Throttle, 0.f, 0.001f);
		});

		It("o nivel muda o alvo (outro StraightKmh)", [this]()
		{
			FDNRivalConfig Slow = Cfg;
			Slow.StraightKmh = 60.f;
			TestEqual("a 60 km/h na reta, alvo 60: acelerador 0,4", FDNRivalDriver::Command(0.f, 0.f, 60.f, Slow).Throttle, 0.4f, 0.001f);
		});
	});

	Describe("Entrada invalida (D5) e mira (D6)", [this]()
	{
		It("NaN: tudo zero", [this]()
		{
			for (const FDNDriveCommand& C : { FDNRivalDriver::Command(NaNf, 0.f, 20.f, Cfg), FDNRivalDriver::Command(0.f, NaNf, 20.f, Cfg), FDNRivalDriver::Command(0.f, 0.f, NaNf, Cfg) })
			{
				TestEqual("esterco", C.Steer, 0.f);
				TestEqual("acelerador", C.Throttle, 0.f);
				TestEqual("freio", C.Brake, 0.f);
			}
		});

		It("a mira cresce com a velocidade", [this]()
		{
			TestEqual("parado", FDNRivalDriver::LookAheadCm(0.f, Cfg), 1500.f, 0.01f);
			TestEqual("100 km/h", FDNRivalDriver::LookAheadCm(100.f, Cfg), 5500.f, 0.01f);
			TestEqual("velocidade negativa vale zero", FDNRivalDriver::LookAheadCm(-50.f, Cfg), 1500.f, 0.01f);
			TestEqual("NaN vale a minima", FDNRivalDriver::LookAheadCm(NaNf, Cfg), 1500.f, 0.01f);
		});
	});
}

#endif
