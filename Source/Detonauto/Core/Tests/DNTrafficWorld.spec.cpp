// Testes da geometria do tráfego (lote 13a): a estrada de teste (faixas, sentido, onde cada carro nasce) e a montagem da entrada de
// FDNTraffic::NextLevel a partir de posições e velocidades do mundo (projeções, distância, velocidade de aproximação).
// Tudo puro: sem mundo, sem ator. Os atores (ADNTrafficVehicle, ADNTrafficManager) só chamam estas funções.
//
// Decisões (o contrato docs/regras-trafego.md não cobre; ver Core/DNTrafficWorld.h):
//  W1 faixa k fica em (k - (LaneCount-1)/2) x LaneWidthM de lado; as últimas OncomingLanes faixas vão no sentido contrário e
//     sempre sobra pelo menos uma faixa no sentido do jogador;
//  W2 PlanSlots devolve no máximo Count carros, ordenados do mais perto para o mais longe; faixas lotadas (menos que MinGapM
//     entre carros) cortam o excesso em vez de sobrepor;
//  W3 a mesma semente dá a mesma estrada;
//  W5 o pool tem tamanho máximo; Acquire reaproveita o livre de menor índice antes de criar um novo e recusa (INDEX_NONE) quando
//     está cheio; Release de índice inválido ou já livre devolve falso e não muda nada;
//  W4 BuildInput: velocidade de aproximação positiva = a distância entre os centros está diminuindo; vetor de avanço de
//     tamanho zero ou NaN dá projeção 0 (nem na frente nem atrás).

#include <limits>
#include "Misc/AutomationTest.h"
#include "Core/DNTrafficWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FDNTrafficRoadSpec, "Detonauto.Trafego.Estrada",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FDNTrafficRoadSpec)

void FDNTrafficRoadSpec::Define()
{
	Describe("Faixas", [this]()
	{
		It("W1: a faixa do meio fica no centro, as outras a uma largura de cada lado", [this]()
		{
			const FDNTrafficRoadConfig C; // 3 faixas de 3,5 m
			TestEqual("faixa 0", FDNTrafficRoad::LaneOffsetM(C, 0), -3.5f);
			TestEqual("faixa 1", FDNTrafficRoad::LaneOffsetM(C, 1), 0.f);
			TestEqual("faixa 2", FDNTrafficRoad::LaneOffsetM(C, 2), 3.5f);
		});

		It("com 2 faixas o centro fica entre elas; a largura vale", [this]()
		{
			FDNTrafficRoadConfig C;
			C.LaneCount = 2;
			C.LaneWidthM = 4.f;
			TestEqual("faixa 0", FDNTrafficRoad::LaneOffsetM(C, 0), -2.f);
			TestEqual("faixa 1", FDNTrafficRoad::LaneOffsetM(C, 1), 2.f);
		});

		It("faixa fora de 0..LaneCount-1 nao tem posicao nem sentido contrario", [this]()
		{
			const FDNTrafficRoadConfig C;
			TestFalse("faixa -1 nao e contraria", FDNTrafficRoad::IsOncoming(C, -1));
			TestFalse("faixa 3 nao e contraria", FDNTrafficRoad::IsOncoming(C, 3));
		});

		It("as ultimas OncomingLanes faixas vao no sentido contrario", [this]()
		{
			FDNTrafficRoadConfig C; // 3 faixas, 1 contraria
			TestFalse("faixa 0", FDNTrafficRoad::IsOncoming(C, 0));
			TestFalse("faixa 1", FDNTrafficRoad::IsOncoming(C, 1));
			TestTrue("faixa 2", FDNTrafficRoad::IsOncoming(C, 2));
			C.OncomingLanes = 2;
			TestFalse("faixa 0 com 2 contrarias", FDNTrafficRoad::IsOncoming(C, 0));
			TestTrue("faixa 1 com 2 contrarias", FDNTrafficRoad::IsOncoming(C, 1));
			TestTrue("faixa 2 com 2 contrarias", FDNTrafficRoad::IsOncoming(C, 2));
		});

		It("sempre sobra uma faixa no sentido do jogador, mesmo se pedirem todas contrarias", [this]()
		{
			FDNTrafficRoadConfig C;
			C.OncomingLanes = 3;
			TestFalse("faixa 0 segue a favor", FDNTrafficRoad::IsOncoming(C, 0));
			TestTrue("faixa 1", FDNTrafficRoad::IsOncoming(C, 1));
			C.OncomingLanes = -4;
			TestFalse("negativo vale zero", FDNTrafficRoad::IsOncoming(C, 2));
		});
	});

	Describe("PlanSlots", [this]()
	{
		It("devolve Count carros, ordenados do mais perto para o mais longe", [this]()
		{
			const TArray<FDNTrafficSlot> S = FDNTrafficRoad::PlanSlots(FDNTrafficRoadConfig(), 20, 7);
			TestEqual("quantidade", S.Num(), 20);
			for (int32 i = 1; i < S.Num(); ++i)
			{
				TestTrue(FString::Printf(TEXT("ordem em %d"), i), S[i].PositionM >= S[i - 1].PositionM);
			}
		});

		It("Count zero, negativo ou estrada invalida: lista vazia", [this]()
		{
			TestEqual("zero", FDNTrafficRoad::PlanSlots(FDNTrafficRoadConfig(), 0, 1).Num(), 0);
			TestEqual("negativo", FDNTrafficRoad::PlanSlots(FDNTrafficRoadConfig(), -3, 1).Num(), 0);
			FDNTrafficRoadConfig Bad;
			Bad.LengthM = 0.f;
			TestEqual("comprimento 0", FDNTrafficRoad::PlanSlots(Bad, 5, 1).Num(), 0);
			Bad = FDNTrafficRoadConfig();
			Bad.LaneCount = 0;
			TestEqual("sem faixas", FDNTrafficRoad::PlanSlots(Bad, 5, 1).Num(), 0);
			Bad = FDNTrafficRoadConfig();
			Bad.MinGapM = 0.f;
			TestEqual("espaco minimo 0", FDNTrafficRoad::PlanSlots(Bad, 5, 1).Num(), 0);
			Bad = FDNTrafficRoadConfig();
			Bad.EdgeMarginM = 400.f; // sobra comprimento negativo
			TestEqual("margens comem a estrada", FDNTrafficRoad::PlanSlots(Bad, 5, 1).Num(), 0);
			Bad = FDNTrafficRoadConfig();
			Bad.LengthM = std::numeric_limits<float>::quiet_NaN();
			TestEqual("comprimento NaN", FDNTrafficRoad::PlanSlots(Bad, 5, 1).Num(), 0);
		});

		It("todos ficam dentro da estrada, depois da margem da largada e antes da margem do fim", [this]()
		{
			const FDNTrafficRoadConfig C;
			for (const FDNTrafficSlot& S : FDNTrafficRoad::PlanSlots(C, 40, 3))
			{
				TestTrue(FString::Printf(TEXT("%.1f depois da margem"), S.PositionM), S.PositionM >= C.EdgeMarginM);
				TestTrue(FString::Printf(TEXT("%.1f antes do fim"), S.PositionM), S.PositionM <= C.LengthM - C.EdgeMarginM);
			}
		});

		It("na mesma faixa dois carros ficam a pelo menos MinGapM", [this]()
		{
			const FDNTrafficRoadConfig C;
			for (const int32 Seed : { 1, 2, 3, 99 })
			{
				const TArray<FDNTrafficSlot> S = FDNTrafficRoad::PlanSlots(C, 40, Seed);
				for (int32 Lane = 0; Lane < C.LaneCount; ++Lane)
				{
					float Last = -1.0e9f;
					for (const FDNTrafficSlot& Slot : S)
					{
						if (Slot.Lane != Lane) { continue; }
						TestTrue(FString::Printf(TEXT("semente %d faixa %d: %.2f a %.2f"), Seed, Lane, Last, Slot.PositionM), Slot.PositionM - Last >= C.MinGapM - 1.0e-3f);
						Last = Slot.PositionM;
					}
				}
			}
		});

		It("as faixas se enchem por igual (40 carros em 3 faixas: 14, 13, 13 em alguma ordem)", [this]()
		{
			const FDNTrafficRoadConfig C;
			int32 PerLane[3] = {};
			for (const FDNTrafficSlot& S : FDNTrafficRoad::PlanSlots(C, 40, 5))
			{
				if (S.Lane >= 0 && S.Lane < 3) { ++PerLane[S.Lane]; }
			}
			TestEqual("soma", PerLane[0] + PerLane[1] + PerLane[2], 40);
			for (int32 Lane = 0; Lane < 3; ++Lane)
			{
				TestTrue(FString::Printf(TEXT("faixa %d tem %d"), Lane, PerLane[Lane]), PerLane[Lane] >= 13 && PerLane[Lane] <= 14);
			}
		});

		It("o sentido contrario vem da faixa e a velocidade fica entre o minimo e o maximo", [this]()
		{
			const FDNTrafficRoadConfig C;
			for (const FDNTrafficSlot& S : FDNTrafficRoad::PlanSlots(C, 30, 11))
			{
				TestEqual(FString::Printf(TEXT("faixa %d"), S.Lane), S.bOncoming, FDNTrafficRoad::IsOncoming(C, S.Lane));
				TestTrue(FString::Printf(TEXT("velocidade %.1f"), S.SpeedKmh), S.SpeedKmh >= C.MinSpeedKmh && S.SpeedKmh <= C.MaxSpeedKmh);
			}
		});

		It("W3: a mesma semente da a mesma estrada; outra semente da outra", [this]()
		{
			const FDNTrafficRoadConfig C;
			const TArray<FDNTrafficSlot> A = FDNTrafficRoad::PlanSlots(C, 25, 42);
			const TArray<FDNTrafficSlot> B = FDNTrafficRoad::PlanSlots(C, 25, 42);
			const TArray<FDNTrafficSlot> D = FDNTrafficRoad::PlanSlots(C, 25, 43);
			TestEqual("mesmo tamanho", A.Num(), B.Num());
			bool bSame = true;
			bool bDiffers = false;
			for (int32 i = 0; i < A.Num() && i < B.Num() && i < D.Num(); ++i)
			{
				bSame = bSame && A[i].Lane == B[i].Lane && A[i].PositionM == B[i].PositionM && A[i].SpeedKmh == B[i].SpeedKmh;
				bDiffers = bDiffers || A[i].PositionM != D[i].PositionM;
			}
			TestTrue("igual com a mesma semente", bSame);
			TestTrue("diferente com outra semente", bDiffers);
		});

		It("W2: faixas lotadas cortam o excesso em vez de sobrepor", [this]()
		{
			FDNTrafficRoadConfig C;
			C.LengthM = 200.f;
			C.EdgeMarginM = 50.f; // 100 m uteis, espaco de 30 m: no maximo 4 por faixa (0, 30, 60, 90)
			C.LaneCount = 2;
			C.OncomingLanes = 0;
			const TArray<FDNTrafficSlot> S = FDNTrafficRoad::PlanSlots(C, 100, 1);
			TestEqual("capacidade 2 faixas x 4", S.Num(), 8);
			float Last[2] = { -1.0e9f, -1.0e9f };
			for (const FDNTrafficSlot& Slot : S)
			{
				TestTrue("espaco", Slot.PositionM - Last[Slot.Lane] >= C.MinGapM - 1.0e-3f);
				Last[Slot.Lane] = Slot.PositionM;
			}
		});
	});
}

BEGIN_DEFINE_SPEC(FDNTrafficPoolSpec, "Detonauto.Trafego.Pool",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FDNTrafficPoolSpec)

void FDNTrafficPoolSpec::Define()
{
	Describe("FDNTrafficPool", [this]()
	{
		It("W5: cria ate o tamanho maximo; depois recusa", [this]()
		{
			FDNTrafficPool Pool(3);
			for (int32 i = 0; i < 3; ++i)
			{
				const FDNTrafficPool::FAcquired A = Pool.Acquire();
				TestEqual(FString::Printf(TEXT("indice %d"), i), A.Index, i);
				TestTrue("criado", A.bCreated);
			}
			TestEqual("cheio", Pool.Acquire().Index, static_cast<int32>(INDEX_NONE));
			TestEqual("tamanho", Pool.Size(), 3);
			TestEqual("ativos", Pool.Active(), 3);
		});

		It("reaproveita o liberado antes de criar um novo, sempre o de menor indice", [this]()
		{
			FDNTrafficPool Pool(5);
			Pool.Acquire(); Pool.Acquire(); Pool.Acquire(); // 0, 1, 2
			TestTrue("libera o 1", Pool.Release(1));
			TestTrue("libera o 0", Pool.Release(0));
			const FDNTrafficPool::FAcquired A = Pool.Acquire();
			TestEqual("pega o 0", A.Index, 0);
			TestFalse("nao e novo", A.bCreated);
			const FDNTrafficPool::FAcquired B = Pool.Acquire();
			TestEqual("pega o 1", B.Index, 1);
			TestFalse("nao e novo", B.bCreated);
			TestEqual("so agora cria o 3", Pool.Acquire().Index, 3);
			TestEqual("tamanho cresceu so uma vez", Pool.Size(), 4);
		});

		It("liberar duas vezes, indice invalido ou nunca usado: falso e nada muda", [this]()
		{
			FDNTrafficPool Pool(3);
			Pool.Acquire();
			TestTrue("libera", Pool.Release(0));
			TestFalse("de novo", Pool.Release(0));
			TestFalse("negativo", Pool.Release(-1));
			TestFalse("fora do tamanho", Pool.Release(1));
			TestFalse("muito fora", Pool.Release(99));
			TestEqual("ativos", Pool.Active(), 0);
			TestEqual("tamanho", Pool.Size(), 1);
		});

		It("nunca entrega o mesmo indice a dois usuarios", [this]()
		{
			FDNTrafficPool Pool(4);
			TSet<int32> Taken;
			for (int32 i = 0; i < 4; ++i) { TestTrue("indice novo", !Taken.Contains(Pool.Acquire().Index)); }
			Taken.Reset();
			Pool.Release(2);
			const int32 First = Pool.Acquire().Index;
			TestEqual("o 2 volta", First, 2);
			TestEqual("e agora esta cheio", Pool.Acquire().Index, static_cast<int32>(INDEX_NONE));
		});

		It("tamanho maximo zero ou negativo: nunca entrega", [this]()
		{
			TestEqual("zero", FDNTrafficPool(0).Acquire().Index, static_cast<int32>(INDEX_NONE));
			TestEqual("negativo", FDNTrafficPool(-5).Acquire().Index, static_cast<int32>(INDEX_NONE));
		});

		It("os contadores acompanham: ativos sobem ao pegar e descem ao liberar", [this]()
		{
			FDNTrafficPool Pool(10);
			Pool.Acquire(); Pool.Acquire();
			TestEqual("2 ativos", Pool.Active(), 2);
			Pool.Release(0);
			TestEqual("1 ativo", Pool.Active(), 1);
			TestEqual("tamanho segue 2", Pool.Size(), 2);
		});
	});
}

BEGIN_DEFINE_SPEC(FDNTrafficGeometrySpec, "Detonauto.Trafego.Entrada",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	const float NaNf = std::numeric_limits<float>::quiet_NaN();
	// o veículo anda em +X; o jogador anda em +X e fica 10 m à frente, a 100 km/h (2777,8 cm/s) contra 50 km/h (1388,9 cm/s)
	FDNTrafficGeometryInput Base() const
	{
		FDNTrafficGeometryInput I;
		I.VehiclePositionCm = FVector(0, 0, 0);
		I.VehicleForward = FVector(1, 0, 0);
		I.VehicleVelocityCmS = FVector(1388.89, 0, 0);
		I.PlayerPositionCm = FVector(1000, 0, 0);
		I.PlayerForward = FVector(1, 0, 0);
		I.PlayerVelocityCmS = FVector(2777.78, 0, 0);
		return I;
	}
END_DEFINE_SPEC(FDNTrafficGeometrySpec)

void FDNTrafficGeometrySpec::Define()
{
	Describe("BuildInput", [this]()
	{
		It("jogador 10 m a frente: distancia 10, projecoes e velocidade conferem", [this]()
		{
			const FDNTrafficVehicleInput R = FDNTrafficGeometry::BuildInput(Base());
			TestEqual("distancia ao jogador [m]", R.PlayerDistanceM, 10.f, 1e-4f);
			TestEqual("jogador na frente do veiculo [m]", R.PlayerProjectionOnVehicleForwardM, 10.f, 1e-4f);
			TestEqual("veiculo atras do jogador [m]", R.VehicleProjectionOnPlayerForwardM, -10.f, 1e-4f);
			TestEqual("velocidade do veiculo [m/s]", R.SpeedMs, 13.8889f, 1e-3f);
			// o jogador esta na frente e MAIS RAPIDO: a distancia cresce, nao aproxima: -50 km/h
			TestEqual("aproximacao [km/h]", R.ClosingSpeedKmh, -50.f, 0.05f);
		});

		It("veiculo vindo de frente para o jogador: aproximacao e a soma das velocidades; depois de passar, se afastam", [this]()
		{
			FDNTrafficGeometryInput I = Base();
			I.VehicleForward = FVector(-1, 0, 0);
			I.VehicleVelocityCmS = FVector(-1388.89, 0, 0);
			I.PlayerPositionCm = FVector(-1000, 0, 0); // o jogador esta 10 m a frente do veiculo, andando +X, vindo ao encontro
			const FDNTrafficVehicleInput R = FDNTrafficGeometry::BuildInput(I);
			TestEqual("jogador na frente do veiculo", R.PlayerProjectionOnVehicleForwardM, 10.f, 1e-4f);
			TestEqual("veiculo na frente do jogador", R.VehicleProjectionOnPlayerForwardM, 10.f, 1e-4f);
			TestEqual("100 + 50 km/h", R.ClosingSpeedKmh, 150.f, 0.1f);
			I.PlayerPositionCm = FVector(1000, 0, 0); // depois de passar: o jogador esta atras do veiculo e o veiculo atras do jogador
			const FDNTrafficVehicleInput R2 = FDNTrafficGeometry::BuildInput(I);
			TestEqual("jogador atras do veiculo", R2.PlayerProjectionOnVehicleForwardM, -10.f, 1e-4f);
			TestEqual("veiculo atras do jogador", R2.VehicleProjectionOnPlayerForwardM, -10.f, 1e-4f);
			TestEqual("se afastam", R2.ClosingSpeedKmh, -150.f, 0.1f);
		});

		It("jogador atras do veiculo e mais rapido: se aproxima pela traseira", [this]()
		{
			FDNTrafficGeometryInput I = Base();
			I.PlayerPositionCm = FVector(-1000, 0, 0); // 10 m atras do veiculo
			const FDNTrafficVehicleInput R = FDNTrafficGeometry::BuildInput(I);
			TestEqual("projecao do jogador no eixo do veiculo", R.PlayerProjectionOnVehicleForwardM, -10.f, 1e-4f);
			TestEqual("veiculo a frente do jogador", R.VehicleProjectionOnPlayerForwardM, 10.f, 1e-4f);
			TestEqual("aproximacao [km/h]", R.ClosingSpeedKmh, 50.f, 0.05f);
		});

		It("de frente de verdade: o jogador em +X andando -X contra o veiculo andando +X", [this]()
		{
			FDNTrafficGeometryInput I = Base();
			I.PlayerForward = FVector(-1, 0, 0);
			I.PlayerVelocityCmS = FVector(-2777.78, 0, 0);
			const FDNTrafficVehicleInput R = FDNTrafficGeometry::BuildInput(I);
			TestEqual("100 + 50 km/h", R.ClosingSpeedKmh, 150.f, 0.1f);
			TestEqual("jogador na frente do veiculo", R.PlayerProjectionOnVehicleForwardM, 10.f, 1e-4f);
			TestEqual("veiculo na frente do jogador", R.VehicleProjectionOnPlayerForwardM, 10.f, 1e-4f);
		});

		It("lado a lado: projecoes zero, distancia lateral", [this]()
		{
			FDNTrafficGeometryInput I = Base();
			I.PlayerPositionCm = FVector(0, 350, 0);
			const FDNTrafficVehicleInput R = FDNTrafficGeometry::BuildInput(I);
			TestEqual("distancia", R.PlayerDistanceM, 3.5f, 1e-4f);
			TestEqual("projecao no veiculo", R.PlayerProjectionOnVehicleForwardM, 0.f, 1e-4f);
			TestEqual("projecao no jogador", R.VehicleProjectionOnPlayerForwardM, 0.f, 1e-4f);
		});

		It("o eixo de avanco nao precisa ser unitario nem horizontal: so a direcao vale", [this]()
		{
			FDNTrafficGeometryInput I = Base();
			I.VehicleForward = FVector(5, 0, 0);
			I.PlayerForward = FVector(0.2, 0, 0);
			const FDNTrafficVehicleInput R = FDNTrafficGeometry::BuildInput(I);
			TestEqual("projecao no veiculo", R.PlayerProjectionOnVehicleForwardM, 10.f, 1e-4f);
			TestEqual("projecao no jogador", R.VehicleProjectionOnPlayerForwardM, -10.f, 1e-4f);
		});

		It("W4: avanco zero ou NaN da projecao 0", [this]()
		{
			FDNTrafficGeometryInput I = Base();
			I.VehicleForward = FVector::ZeroVector;
			I.PlayerForward = FVector(NaNf, 0, 0);
			const FDNTrafficVehicleInput R = FDNTrafficGeometry::BuildInput(I);
			TestEqual("projecao no veiculo", R.PlayerProjectionOnVehicleForwardM, 0.f);
			TestEqual("projecao no jogador", R.VehicleProjectionOnPlayerForwardM, 0.f);
		});

		It("jogador e veiculo no mesmo ponto: distancia 0 e aproximacao 0 (sem dividir por zero)", [this]()
		{
			FDNTrafficGeometryInput I = Base();
			I.PlayerPositionCm = I.VehiclePositionCm;
			const FDNTrafficVehicleInput R = FDNTrafficGeometry::BuildInput(I);
			TestEqual("distancia", R.PlayerDistanceM, 0.f);
			TestEqual("aproximacao", R.ClosingSpeedKmh, 0.f);
			TestTrue("aproximacao finita", FMath::IsFinite(R.ClosingSpeedKmh));
		});

		It("posicao NaN: distancia NaN, e NextLevel nao acorda nem some (NaN conta como nao)", [this]()
		{
			FDNTrafficGeometryInput I = Base();
			I.PlayerPositionCm = FVector(NaNf, 0, 0);
			const FDNTrafficVehicleInput R = FDNTrafficGeometry::BuildInput(I);
			TestTrue("distancia NaN", FMath::IsNaN(R.PlayerDistanceM));
			TestEqual("fica Rail", FDNTraffic::NextLevel(FDNTrafficSettings(), R), EDNTrafficLevel::Rail);
		});

		It("repassa nivel, impacto, vizinho fisico e rodada ativa sem mexer", [this]()
		{
			FDNTrafficGeometryInput I = Base();
			I.Level = EDNTrafficLevel::Physics;
			I.ImpactForce = 123.f;
			I.NearestPhysicsDistanceM = 4.5f;
			I.bCrashRoundActive = true;
			const FDNTrafficVehicleInput R = FDNTrafficGeometry::BuildInput(I);
			TestEqual("nivel", R.Level, EDNTrafficLevel::Physics);
			TestEqual("impacto", R.ImpactForce, 123.f);
			TestEqual("vizinho", R.NearestPhysicsDistanceM, 4.5f);
			TestTrue("rodada", R.bCrashRoundActive);
		});

		It("a entrada montada acorda um Rail a 20 m a frente do jogador a 100 km/h contra 50 (integracao com NextLevel)", [this]()
		{
			// jogador 20 m atras do veiculo (atras = projecao do jogador no eixo do veiculo < 0): NAO acorda pela regra da frente
			FDNTrafficGeometryInput I = Base();
			I.PlayerPositionCm = FVector(-2000, 0, 0);
			TestEqual("jogador atras: segue Rail", FDNTraffic::NextLevel(FDNTrafficSettings(), FDNTrafficGeometry::BuildInput(I)), EDNTrafficLevel::Rail);
			// veiculo vindo de frente, jogador 20 m a frente do veiculo e se aproximando a 150 km/h: acorda
			FDNTrafficGeometryInput J = Base();
			J.PlayerForward = FVector(-1, 0, 0);
			J.PlayerVelocityCmS = FVector(-2777.78, 0, 0);
			J.PlayerPositionCm = FVector(2000, 0, 0);
			TestEqual("frente e aproximando: Physics", FDNTraffic::NextLevel(FDNTrafficSettings(), FDNTrafficGeometry::BuildInput(J)), EDNTrafficLevel::Physics);
		});
	});

	Describe("NearestDistanceM", [this]()
	{
		It("a menor distancia entre centros, em metros; lista vazia: maximo (nenhum)", [this]()
		{
			const FVector P(0, 0, 0);
			TestEqual("vazia", FDNTrafficGeometry::NearestDistanceM(P, {}), TNumericLimits<float>::Max());
			TestEqual("tres", FDNTrafficGeometry::NearestDistanceM(P, { FVector(3000, 0, 0), FVector(0, 400, 300), FVector(0, 0, 900) }), 5.f, 1e-4f);
		});

		It("NaN na lista e ignorado; so NaN: nenhum", [this]()
		{
			const FVector P(0, 0, 0);
			TestEqual("ignora NaN", FDNTrafficGeometry::NearestDistanceM(P, { FVector(NaNf, 0, 0), FVector(600, 0, 0) }), 6.f, 1e-4f);
			TestEqual("so NaN", FDNTrafficGeometry::NearestDistanceM(P, { FVector(NaNf, 0, 0) }), TNumericLimits<float>::Max());
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
