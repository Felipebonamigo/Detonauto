// Testes dos componentes de cola fina: UDNTurboComponent e UDNCrashRoundComponent so repassam as entradas ao Core e
// expoem estado e eventos. Sem mundo, sem ator, sem fisica: NewObject e quadros simulados.
// Como nao tem logica propria, o teste principal e de EQUIVALENCIA: o componente e o Core chamado direto, com a mesma
// configuracao e as mesmas entradas, ficam no mesmo estado a cada quadro.
//
// Decisoes de implementacao (ver os headers):
//  K1 os eventos sao delegates nativos (TMulticastDelegate); versao dinamica para Blueprint/UI fica para quando a UI pedir;
//  K2 os componentes nao tickam sozinhos (bCanEverTick = false): quem tem as entradas chama Step();
//  K3 um evento so dispara quando o valor muda de verdade (comparacao antes/depois do Core, sem regra nova).

#include "Misc/AutomationTest.h"
#include "Core/DNGameDataSubsystem.h"
#include "Vehicle/DNTurboComponent.h"
#include "Crash/DNCrashRoundComponent.h"
#include "Core/Tests/DNTestData.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FDNTurboFrameInput Frame(float Dt, bool bButton, bool bTakedown = false, float SpeedKmh = 0.f, bool bWrongWay = false)
	{
		FDNTurboFrameInput F;
		F.DeltaTime = Dt;
		F.bButtonHeld = bButton;
		F.bTakedown = bTakedown;
		F.SpeedKmh = SpeedKmh;
		F.bWrongWay = bWrongWay;
		return F;
	}
}

BEGIN_DEFINE_SPEC(FDNTurboComponentSpec, "Detonauto.Componentes.Turbo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FDNTurboComponentSpec)

void FDNTurboComponentSpec::Define()
{
	Describe("UDNTurboComponent", [this]()
	{
		It("nao tica sozinho", [this]()
		{
			UDNTurboComponent* C = NewObject<UDNTurboComponent>(GetTransientPackage());
			TestFalse("sem tick proprio", C->PrimaryComponentTick.bCanEverTick);
		});

		It("comeca no padrao do Core: barra vazia, desligado", [this]()
		{
			UDNTurboComponent* C = NewObject<UDNTurboComponent>(GetTransientPackage());
			TestEqual("barra", C->GetBar(), 0.f);
			TestFalse("desligado", C->IsActive());
		});

		It("EQUIVALENCIA: a mesma sequencia de quadros deixa o componente e o Core no mesmo estado", [this]()
		{
			UDNTurboComponent* C = NewObject<UDNTurboComponent>(GetTransientPackage());
			const FDNTurboConfig Config;
			FDNTurboState Direct;

			TArray<FDNTurboFrameInput> Frames;
			Frames.Add(Frame(0.1f, false, true));            // takedown: enche a barra
			Frames.Add(Frame(0.1f, true));                   // aperta: liga
			Frames.Add(Frame(0.5f, true));                   // consome
			Frames.Add(Frame(0.5f, false));                  // solta: desliga
			Frames.Add(Frame(1.0f, false, false, 120.f, true)); // contramao rapida: ganha
			FDNTurboFrameInput Near = Frame(0.1f, false, false, 100.f);
			FDNNearMissEvent Event; Event.VehicleId = 7; Event.LateralDistanceCm = 50.f;
			Near.NearMisses.Add(Event);
			Frames.Add(Near);                                // passada raspando
			Frames.Add(Frame(0.1f, true));
			Frames.Add(Frame(10.0f, true));                  // esgota

			int32 Index = 0;
			for (const FDNTurboFrameInput& F : Frames)
			{
				C->Step(F);
				FDNTurbo::Tick(Config, Direct, F);
				TestEqual(FString::Printf(TEXT("quadro %d: barra"), Index), C->GetBar(), Direct.Bar);
				TestEqual(FString::Printf(TEXT("quadro %d: ligado"), Index), C->IsActive(), Direct.bActive);
				TestEqual(FString::Printf(TEXT("quadro %d: cadeia"), Index), C->GetState().NearMissChainLength, Direct.NearMissChainLength);
				++Index;
			}
		});

		It("Configure troca os numeros: a mesma entrada da outro resultado", [this]()
		{
			UDNTurboComponent* C = NewObject<UDNTurboComponent>(GetTransientPackage());
			FDNTurboConfig Config;
			Config.MaxBar = 40.f;
			Config.TakedownSetsBarTo = 40.f;
			C->Configure(Config);
			C->Step(Frame(0.1f, false, true));
			TestEqual("barra da config nova", C->GetBar(), 40.f);
		});

		It("ConfigureFromData usa o CSV via subsistema; invalido volta ao padrao", [this]()
		{
			UDNGameDataSubsystem* Sub = DNTest::NewSubsystem<UDNGameDataSubsystem>();
			FDNData Data;
			Data.TurboConfig = { DNTest::TurboEntry() };
			Data.TurboConfig[0].Row.MaxBar = 80.f;
			Data.TurboConfig[0].Row.TakedownSetsBarTo = 80.f;
			AddExpectedError(TEXT("[Dados]"), EAutomationExpectedErrorFlags::Contains, 0);
			Sub->ApplyData(MoveTemp(Data), {});

			UDNTurboComponent* C = NewObject<UDNTurboComponent>(GetTransientPackage());
			C->ConfigureFromData(Sub);
			C->Step(Frame(0.1f, false, true));
			TestEqual("barra do CSV", C->GetBar(), 80.f);

			UDNTurboComponent* D = NewObject<UDNTurboComponent>(GetTransientPackage());
			D->ConfigureFromData(nullptr); // sem subsistema: segue com o padrao, nao quebra
			D->Step(Frame(0.1f, false, true));
			TestEqual("barra padrao", D->GetBar(), FDNTurboConfig().TakedownSetsBarTo);
		});

		It("eventos: liga e desliga disparam uma vez cada; a barra avisa so quando muda", [this]()
		{
			UDNTurboComponent* C = NewObject<UDNTurboComponent>(GetTransientPackage());
			TArray<bool> ActiveEvents;
			int32 BarEvents = 0;
			float LastBar = -1.f;
			C->OnActiveChanged.AddLambda([&ActiveEvents](bool b) { ActiveEvents.Add(b); });
			C->OnBarChanged.AddLambda([&BarEvents, &LastBar](float Bar) { ++BarEvents; LastBar = Bar; });

			C->Step(Frame(0.1f, false));                 // nada muda
			TestEqual("sem evento de barra", BarEvents, 0);
			C->Step(Frame(0.1f, false, true));           // barra 0 -> 100
			TestEqual("1 evento de barra", BarEvents, 1);
			TestEqual("barra do evento", LastBar, 100.f);
			C->Step(Frame(0.1f, true));                  // liga e consome
			TestEqual("ligou uma vez", ActiveEvents.Num(), 1);
			if (ActiveEvents.Num() == 1) { TestTrue("ligou", ActiveEvents[0]); }
			C->Step(Frame(0.1f, true));                  // continua ligado: so a barra muda
			TestEqual("continua 1 evento de ativo", ActiveEvents.Num(), 1);
			C->Step(Frame(0.1f, false));                 // desliga
			TestEqual("desligou", ActiveEvents.Num(), 2);
			if (ActiveEvents.Num() == 2) { TestFalse("desligou", ActiveEvents[1]); }
		});

		It("ResetState volta ao zero e avisa a barra", [this]()
		{
			UDNTurboComponent* C = NewObject<UDNTurboComponent>(GetTransientPackage());
			C->Step(Frame(0.1f, false, true));
			float LastBar = -1.f;
			C->OnBarChanged.AddLambda([&LastBar](float Bar) { LastBar = Bar; });
			C->ResetState();
			TestEqual("barra zerada", C->GetBar(), 0.f);
			TestEqual("evento com zero", LastBar, 0.f);
			TestFalse("desligado", C->IsActive());
		});
	});
}

BEGIN_DEFINE_SPEC(FDNCrashRoundComponentSpec, "Detonauto.Componentes.Crash",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FDNCrashRoundComponentSpec)

void FDNCrashRoundComponentSpec::Define()
{
	Describe("UDNCrashRoundComponent", [this]()
	{
		It("nao tica sozinho e comeca zerado", [this]()
		{
			UDNCrashRoundComponent* C = NewObject<UDNCrashRoundComponent>(GetTransientPackage());
			TestFalse("sem tick proprio", C->PrimaryComponentTick.bCanEverTick);
			TestEqual("pontos", C->GetScore(), 0);
			TestEqual("multiplicador", C->GetMultiplier(), 1);
			TestFalse("nao encerrou", C->IsEnded());
		});

		It("EQUIVALENCIA: componente e Core chamado direto dao os mesmos pontos e estado", [this]()
		{
			UDNCrashRoundComponent* C = NewObject<UDNCrashRoundComponent>(GetTransientPackage());
			const FDNCrashConfig Config;
			FDNCrashState Direct;

			auto Destroy = [&](int32 Value, float WaitSeconds)
			{
				C->Step(WaitSeconds, 50.f);
				FDNCrash::Tick(Config, Direct, WaitSeconds, 50.f);
				const int32 Got = C->RegisterDestruction(Value);
				const int32 Expected = FDNCrash::RegisterDestruction(Config, Direct, Value);
				TestEqual(FString::Printf(TEXT("pontos de %d"), Value), Got, Expected);
				TestEqual("total", C->GetScore(), Direct.Score.Total);
				TestEqual("cadeia", C->GetState().ChainLinks, Direct.ChainLinks);
			};
			Destroy(1000, 0.1f);
			Destroy(2000, 0.5f);   // dentro da janela: cadeia
			C->CollectMultiplier(); FDNCrash::CollectMultiplier(Config, Direct);
			TestEqual("multiplicador", C->GetMultiplier(), Direct.Multiplier);
			Destroy(500, 5.0f);    // fora da janela: reinicia
			Destroy(0, 0.1f);      // valor 0 ignorado
			Destroy(-50, 0.1f);    // negativo ignorado
		});

		It("Configure e ConfigureFromData trocam os numeros", [this]()
		{
			UDNCrashRoundComponent* C = NewObject<UDNCrashRoundComponent>(GetTransientPackage());
			FDNCrashConfig Config;
			Config.MaxMultiplier = 2;
			C->Configure(Config);
			C->CollectMultiplier(); C->CollectMultiplier(); C->CollectMultiplier();
			TestEqual("teto 2", C->GetMultiplier(), 2);

			UDNGameDataSubsystem* Sub = DNTest::NewSubsystem<UDNGameDataSubsystem>();
			FDNData Data;
			Data.CrashConfig = { DNTest::CrashEntry() };
			Data.CrashConfig[0].Row.MaxMultiplier = 3;
			AddExpectedError(TEXT("[Dados]"), EAutomationExpectedErrorFlags::Contains, 0);
			Sub->ApplyData(MoveTemp(Data), {});
			UDNCrashRoundComponent* D = NewObject<UDNCrashRoundComponent>(GetTransientPackage());
			D->ConfigureFromData(Sub);
			for (int32 i = 0; i < 10; ++i) { D->CollectMultiplier(); }
			TestEqual("teto 3 do CSV", D->GetMultiplier(), 3);

			UDNCrashRoundComponent* E = NewObject<UDNCrashRoundComponent>(GetTransientPackage());
			E->ConfigureFromData(nullptr);
			for (int32 i = 0; i < 20; ++i) { E->CollectMultiplier(); }
			TestEqual("teto padrao", E->GetMultiplier(), FDNCrashConfig().MaxMultiplier);
		});

		It("eventos: pontos, multiplicador e fim da rodada, cada um so quando muda", [this]()
		{
			UDNCrashRoundComponent* C = NewObject<UDNCrashRoundComponent>(GetTransientPackage());
			TArray<int32> Scores, Multipliers, Ends;
			C->OnScoreChanged.AddLambda([&Scores](int32 Total) { Scores.Add(Total); });
			C->OnMultiplierChanged.AddLambda([&Multipliers](int32 M) { Multipliers.Add(M); });
			C->OnRoundEnded.AddLambda([&Ends](int32 Final) { Ends.Add(Final); });

			C->RegisterDestruction(0);
			TestEqual("valor 0 nao avisa", Scores.Num(), 0);
			C->RegisterDestruction(1000);
			TestEqual("um aviso de pontos", Scores.Num(), 1);
			if (Scores.Num() == 1) { TestEqual("total 1000", Scores[0], 1000); }
			C->CollectMultiplier();
			TestEqual("um aviso de multiplicador", Multipliers.Num(), 1);
			if (Multipliers.Num() == 1) { TestEqual("multiplicador 2", Multipliers[0], 2); }

			C->Step(2.9f, 0.f);
			TestEqual("ainda nao acabou", Ends.Num(), 0);
			C->Step(0.2f, 0.f);                               // 3,1 s parado: encerra
			TestEqual("acabou uma vez", Ends.Num(), 1);
			if (Ends.Num() == 1) { TestEqual("pontuacao final", Ends[0], 1000); }
			TestTrue("IsEnded", C->IsEnded());
			C->Step(5.f, 0.f);
			TestEqual("nao avisa de novo", Ends.Num(), 1);
			TestEqual("depois do fim nao pontua", C->RegisterDestruction(5000), 0);
			TestEqual("nem avisa pontos", Scores.Num(), 1);
			C->CollectMultiplier();
			TestEqual("nem multiplicador", Multipliers.Num(), 1);
		});

		It("o relogio maximo tambem encerra, e RegisterImpact liga os relogios", [this]()
		{
			UDNCrashRoundComponent* C = NewObject<UDNCrashRoundComponent>(GetTransientPackage());
			int32 Ends = 0;
			C->OnRoundEnded.AddLambda([&Ends](int32) { ++Ends; });
			C->Step(100.f, 0.f);
			TestEqual("sem impacto o relogio nao anda", Ends, 0);
			C->RegisterImpact();
			C->Step(31.f, 50.f); // em movimento, mas passou de 30 s
			TestEqual("encerrou pelo tempo maximo", Ends, 1);
		});

		It("ResetRound zera tudo e avisa pontos e multiplicador", [this]()
		{
			UDNCrashRoundComponent* C = NewObject<UDNCrashRoundComponent>(GetTransientPackage());
			C->RegisterDestruction(1000);
			C->CollectMultiplier();
			int32 LastScore = -1, LastMultiplier = -1;
			C->OnScoreChanged.AddLambda([&LastScore](int32 T) { LastScore = T; });
			C->OnMultiplierChanged.AddLambda([&LastMultiplier](int32 M) { LastMultiplier = M; });
			C->ResetRound();
			TestEqual("pontos", C->GetScore(), 0);
			TestEqual("multiplicador", C->GetMultiplier(), 1);
			TestFalse("nao encerrado", C->IsEnded());
			TestEqual("evento de pontos", LastScore, 0);
			TestEqual("evento de multiplicador", LastMultiplier, 1);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
