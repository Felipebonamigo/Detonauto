// Testes da direção no globo (docs/menu-mapa.md, "Regras de progresso").
// Convenção do Unreal (X para a frente, Z para cima). Direção de uma cidade:
// (cos lat . cos lon, cos lat . sin lon, sin lat). Funções puras, sem mundo.

#include <limits>
#include "Misc/AutomationTest.h"
#include "Core/DNGlobe.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr double Tol = 1e-9;

	bool NearlyEqual(const FVector& A, const FVector& B, double Tolerance = Tol)
	{
		return FMath::Abs(A.X - B.X) <= Tolerance && FMath::Abs(A.Y - B.Y) <= Tolerance && FMath::Abs(A.Z - B.Z) <= Tolerance;
	}

	FString Str(const FVector& V) { return FString::Printf(TEXT("(%.12f, %.12f, %.12f)"), V.X, V.Y, V.Z); }

	/** Distância entre dois pontos pela fórmula de haversine, independente do código testado [graus]. */
	double Haversine(double LatA, double LonA, double LatB, double LonB)
	{
		const double Rad = UE_DOUBLE_PI / 180.0;
		const double DLat = (LatB - LatA) * Rad;
		const double DLon = (LonB - LonA) * Rad;
		const double H = FMath::Square(FMath::Sin(DLat / 2)) + FMath::Cos(LatA * Rad) * FMath::Cos(LatB * Rad) * FMath::Square(FMath::Sin(DLon / 2));
		return 2.0 * FMath::Asin(FMath::Min(1.0, FMath::Sqrt(H))) / Rad;
	}
}

BEGIN_DEFINE_SPEC(FDNGlobeDirectionSpec, "Detonauto.Globo.Direcao",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FDNGlobeDirectionSpec)

void FDNGlobeDirectionSpec::Define()
{
	Describe("Pontos cardeais (exatos)", [this]()
	{
		It("(0, 0) aponta para +X", [this]()
		{
			const FVector D = FDNGlobe::LatLonToDirection(0.0, 0.0);
			TestTrue(Str(D), D == FVector(1, 0, 0));
		});

		It("(0, 90) aponta para +Y e (0, -90) para -Y", [this]()
		{
			TestTrue(Str(FDNGlobe::LatLonToDirection(0.0, 90.0)), FDNGlobe::LatLonToDirection(0.0, 90.0) == FVector(0, 1, 0));
			TestTrue(Str(FDNGlobe::LatLonToDirection(0.0, -90.0)), FDNGlobe::LatLonToDirection(0.0, -90.0) == FVector(0, -1, 0));
		});

		It("(0, 180) e (0, -180) apontam para -X", [this]()
		{
			TestTrue(Str(FDNGlobe::LatLonToDirection(0.0, 180.0)), FDNGlobe::LatLonToDirection(0.0, 180.0) == FVector(-1, 0, 0));
			TestTrue(Str(FDNGlobe::LatLonToDirection(0.0, -180.0)), FDNGlobe::LatLonToDirection(0.0, -180.0) == FVector(-1, 0, 0));
		});

		It("(90, 0) aponta para +Z e (-90, 0) para -Z, qualquer que seja a longitude", [this]()
		{
			TestTrue(Str(FDNGlobe::LatLonToDirection(90.0, 0.0)), FDNGlobe::LatLonToDirection(90.0, 0.0) == FVector(0, 0, 1));
			TestTrue(Str(FDNGlobe::LatLonToDirection(-90.0, 0.0)), FDNGlobe::LatLonToDirection(-90.0, 0.0) == FVector(0, 0, -1));
			TestTrue(Str(FDNGlobe::LatLonToDirection(90.0, 123.0)), FDNGlobe::LatLonToDirection(90.0, 123.0).Equals(FVector(0, 0, 1), 1e-12));
		});
	});

	Describe("Propriedades", [this]()
	{
		It("o vetor sempre tem comprimento 1", [this]()
		{
			for (double Lat = -90.0; Lat <= 90.0; Lat += 7.5)
			{
				for (double Lon = -180.0; Lon <= 180.0; Lon += 15.0)
				{
					const FVector D = FDNGlobe::LatLonToDirection(Lat, Lon);
					if (!TestTrue(FString::Printf(TEXT("(%.1f, %.1f) tem comprimento 1"), Lat, Lon), FMath::Abs(D.Size() - 1.0) <= 1e-12))
					{
						return;
					}
				}
			}
		});

		It("45 graus no equador: (raiz de 1/2, raiz de 1/2, 0)", [this]()
		{
			const double S = FMath::Sqrt(0.5);
			const FVector D = FDNGlobe::LatLonToDirection(0.0, 45.0);
			TestTrue(Str(D), NearlyEqual(D, FVector(S, S, 0.0)));
		});

		It("simetria da latitude: inverter o sinal inverte so o Z", [this]()
		{
			const FVector A = FDNGlobe::LatLonToDirection(-22.9068, -43.1729);
			const FVector B = FDNGlobe::LatLonToDirection(22.9068, -43.1729);
			TestTrue(Str(A) + TEXT(" x ") + Str(B), NearlyEqual(B, FVector(A.X, A.Y, -A.Z)));
		});

		It("simetria da longitude: inverter o sinal inverte so o Y", [this]()
		{
			const FVector A = FDNGlobe::LatLonToDirection(-23.5505, -46.6333);
			const FVector B = FDNGlobe::LatLonToDirection(-23.5505, 46.6333);
			TestTrue(Str(A) + TEXT(" x ") + Str(B), NearlyEqual(B, FVector(A.X, -A.Y, A.Z)));
		});

		It("o antipoda e o vetor oposto", [this]()
		{
			const FVector A = FDNGlobe::LatLonToDirection(30.0, 40.0);
			const FVector B = FDNGlobe::LatLonToDirection(-30.0, -140.0);
			TestTrue(Str(A) + TEXT(" x ") + Str(B), NearlyEqual(B, -A));
		});

		It("longitudes 180 e -180 sao o mesmo ponto", [this]()
		{
			TestTrue("mesmo ponto", NearlyEqual(FDNGlobe::LatLonToDirection(40.0, 180.0), FDNGlobe::LatLonToDirection(40.0, -180.0)));
		});
	});

	Describe("Validacao: latitude em [-90, 90] e longitude em [-180, 180]", [this]()
	{
		It("os limites exatos sao validos", [this]()
		{
			TestTrue("lat 90", FDNGlobe::IsValidLatLon(90.0, 0.0));
			TestTrue("lat -90", FDNGlobe::IsValidLatLon(-90.0, 0.0));
			TestTrue("lon 180", FDNGlobe::IsValidLatLon(0.0, 180.0));
			TestTrue("lon -180", FDNGlobe::IsValidLatLon(0.0, -180.0));
			TestTrue("cantos", FDNGlobe::IsValidLatLon(90.0, 180.0) && FDNGlobe::IsValidLatLon(-90.0, -180.0));
		});

		It("logo alem dos limites e invalido", [this]()
		{
			TestFalse("lat 90,0000001", FDNGlobe::IsValidLatLon(90.0000001, 0.0));
			TestFalse("lat -90,0000001", FDNGlobe::IsValidLatLon(-90.0000001, 0.0));
			TestFalse("lon 180,0000001", FDNGlobe::IsValidLatLon(0.0, 180.0000001));
			TestFalse("lon -180,0000001", FDNGlobe::IsValidLatLon(0.0, -180.0000001));
		});

		It("NaN e infinito sao invalidos", [this]()
		{
			const double NaN = std::numeric_limits<double>::quiet_NaN();
			const double Inf = std::numeric_limits<double>::infinity();
			TestFalse("lat NaN", FDNGlobe::IsValidLatLon(NaN, 0.0));
			TestFalse("lon NaN", FDNGlobe::IsValidLatLon(0.0, NaN));
			TestFalse("lat inf", FDNGlobe::IsValidLatLon(Inf, 0.0));
			TestFalse("lon -inf", FDNGlobe::IsValidLatLon(0.0, -Inf));
		});

		It("entrada invalida devolve o vetor zero, nunca um vetor errado", [this]()
		{
			TestTrue("lat 91", FDNGlobe::LatLonToDirection(91.0, 0.0) == FVector::ZeroVector);
			TestTrue("lon 181", FDNGlobe::LatLonToDirection(0.0, 181.0) == FVector::ZeroVector);
			TestTrue("NaN", FDNGlobe::LatLonToDirection(std::numeric_limits<double>::quiet_NaN(), 0.0) == FVector::ZeroVector);
		});
	});
}

BEGIN_DEFINE_SPEC(FDNGlobeCameraSpec, "Detonauto.Globo.Camera",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FDNGlobeCameraSpec)

void FDNGlobeCameraSpec::Define()
{
	Describe("Direcao para yaw e pitch (yaw 0 em +X, positivo em direcao a +Y; pitch positivo para cima)", [this]()
	{
		It("pontos cardeais", [this]()
		{
			double Yaw = 99, Pitch = 99;
			TestTrue("+X", FDNGlobe::DirectionToYawPitch(FVector(1, 0, 0), Yaw, Pitch));
			TestEqual("yaw +X", Yaw, 0.0); TestEqual("pitch +X", Pitch, 0.0);
			TestTrue("+Y", FDNGlobe::DirectionToYawPitch(FVector(0, 1, 0), Yaw, Pitch));
			TestEqual("yaw +Y", Yaw, 90.0); TestEqual("pitch +Y", Pitch, 0.0);
			TestTrue("-Y", FDNGlobe::DirectionToYawPitch(FVector(0, -1, 0), Yaw, Pitch));
			TestEqual("yaw -Y", Yaw, -90.0);
			TestTrue("-X", FDNGlobe::DirectionToYawPitch(FVector(-1, 0, 0), Yaw, Pitch));
			TestEqual("yaw -X", Yaw, 180.0);
			TestTrue("+Z", FDNGlobe::DirectionToYawPitch(FVector(0, 0, 1), Yaw, Pitch));
			TestEqual("pitch +Z", Pitch, 90.0);
			TestTrue("-Z", FDNGlobe::DirectionToYawPitch(FVector(0, 0, -1), Yaw, Pitch));
			TestEqual("pitch -Z", Pitch, -90.0);
		});

		It("nos polos o yaw e 0 (indefinido, escolha documentada)", [this]()
		{
			double Yaw = 99, Pitch = 99;
			FDNGlobe::DirectionToYawPitch(FDNGlobe::LatLonToDirection(90.0, 77.0), Yaw, Pitch);
			TestEqual("yaw no polo norte", Yaw, 0.0);
			TestEqual("pitch no polo norte", Pitch, 90.0);
		});

		It("ida e volta: latitude vira pitch e longitude vira yaw", [this]()
		{
			const double Cities[][2] = { { -22.9068, -43.1729 }, { -23.5505, -46.6333 }, { 51.5, -0.12 }, { 35.68, 139.69 }, { -33.87, 151.21 } };
			for (const double* C : Cities)
			{
				double Yaw = 0, Pitch = 0;
				TestTrue("valido", FDNGlobe::DirectionToYawPitch(FDNGlobe::LatLonToDirection(C[0], C[1]), Yaw, Pitch));
				TestTrue(FString::Printf(TEXT("pitch %.4f = lat %.4f"), Pitch, C[0]), FMath::Abs(Pitch - C[0]) <= 1e-9);
				TestTrue(FString::Printf(TEXT("yaw %.4f = lon %.4f"), Yaw, C[1]), FMath::Abs(Yaw - C[1]) <= 1e-9);
			}
		});

		It("a escala do vetor nao muda os angulos", [this]()
		{
			double Yaw = 0, Pitch = 0, Yaw2 = 0, Pitch2 = 0;
			FDNGlobe::DirectionToYawPitch(FVector(1, 1, 1), Yaw, Pitch);
			FDNGlobe::DirectionToYawPitch(FVector(50, 50, 50), Yaw2, Pitch2);
			TestTrue("yaw", FMath::Abs(Yaw - Yaw2) <= 1e-9);
			TestTrue("pitch", FMath::Abs(Pitch - Pitch2) <= 1e-9);
		});

		It("vetor zero ou NaN e recusado", [this]()
		{
			double Yaw = 5, Pitch = 5;
			TestFalse("zero", FDNGlobe::DirectionToYawPitch(FVector::ZeroVector, Yaw, Pitch));
			TestFalse("NaN", FDNGlobe::DirectionToYawPitch(FVector(std::numeric_limits<double>::quiet_NaN(), 0, 0), Yaw, Pitch));
		});
	});

	Describe("Camera em orbita (fora do globo, olhando para o centro)", [this]()
	{
		It("a camera olha no sentido oposto ao da cidade", [this]()
		{
			double Yaw = 99, Pitch = 99;
			TestTrue("cidade em +X", FDNGlobe::OrbitCameraYawPitch(FVector(1, 0, 0), Yaw, Pitch));
			TestEqual("yaw", Yaw, 180.0); TestEqual("pitch", Pitch, 0.0);
			TestTrue("cidade em +Y", FDNGlobe::OrbitCameraYawPitch(FVector(0, 1, 0), Yaw, Pitch));
			TestEqual("yaw", Yaw, -90.0);
			TestTrue("cidade em +Z", FDNGlobe::OrbitCameraYawPitch(FVector(0, 0, 1), Yaw, Pitch));
			TestEqual("pitch", Pitch, -90.0);
		});

		It("a direcao do olhar da camera e o vetor oposto", [this]()
		{
			const FVector City = FDNGlobe::LatLonToDirection(-22.9068, -43.1729);
			double Yaw = 0, Pitch = 0;
			TestTrue("valido", FDNGlobe::OrbitCameraYawPitch(City, Yaw, Pitch));
			const double Rad = UE_DOUBLE_PI / 180.0;
			const FVector Look(FMath::Cos(Pitch * Rad) * FMath::Cos(Yaw * Rad), FMath::Cos(Pitch * Rad) * FMath::Sin(Yaw * Rad), FMath::Sin(Pitch * Rad));
			TestTrue(Str(Look) + TEXT(" x ") + Str(-City), NearlyEqual(Look, -City));
		});
	});
}

BEGIN_DEFINE_SPEC(FDNGlobeDistanceSpec, "Detonauto.Globo.Distancia",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FDNGlobeDistanceSpec)

void FDNGlobeDistanceSpec::Define()
{
	Describe("Distancia ao longo do circulo maximo [graus]", [this]()
	{
		It("o mesmo ponto esta a 0", [this]()
		{
			double D = 99;
			TestTrue("valido", FDNGlobe::GreatCircleDistanceDegrees(-22.9068, -43.1729, -22.9068, -43.1729, D));
			TestEqual("distancia", D, 0.0);
		});

		It("casos conhecidos: 90 no equador, do equador ao polo, polo a polo, antipodas", [this]()
		{
			double D = 0;
			FDNGlobe::GreatCircleDistanceDegrees(0, 0, 0, 90, D);   TestTrue("0,0 a 0,90", FMath::Abs(D - 90.0) <= 1e-9);
			FDNGlobe::GreatCircleDistanceDegrees(0, 0, 90, 0, D);   TestTrue("0,0 a 90,0", FMath::Abs(D - 90.0) <= 1e-9);
			FDNGlobe::GreatCircleDistanceDegrees(90, 0, -90, 0, D); TestTrue("polo a polo", FMath::Abs(D - 180.0) <= 1e-9);
			FDNGlobe::GreatCircleDistanceDegrees(0, 0, 0, 180, D);  TestTrue("antipodas no equador", FMath::Abs(D - 180.0) <= 1e-9);
			FDNGlobe::GreatCircleDistanceDegrees(30, 40, -30, -140, D); TestTrue("antipodas", FMath::Abs(D - 180.0) <= 1e-9);
		});

		It("pelo lado curto do antimeridiano", [this]()
		{
			double D = 0;
			FDNGlobe::GreatCircleDistanceDegrees(0, 179, 0, -179, D);
			TestTrue("179 a -179 sao 2 graus", FMath::Abs(D - 2.0) <= 1e-9);
			FDNGlobe::GreatCircleDistanceDegrees(0, 180, 0, -180, D);
			TestTrue("180 a -180 sao o mesmo ponto", FMath::Abs(D) <= 1e-9);
		});

		It("simetrica: A a B = B a A", [this]()
		{
			double AB = 0, BA = 0;
			FDNGlobe::GreatCircleDistanceDegrees(-22.9068, -43.1729, -23.5505, -46.6333, AB);
			FDNGlobe::GreatCircleDistanceDegrees(-23.5505, -46.6333, -22.9068, -43.1729, BA);
			TestTrue("simetria", FMath::Abs(AB - BA) <= 1e-12);
		});

		It("bate com a formula de haversine em varios pares", [this]()
		{
			const double P[][4] = { { -22.9068, -43.1729, -23.5505, -46.6333 }, { 51.5, -0.12, 35.68, 139.69 }, { -33.87, 151.21, 40.71, -74.0 }, { 10, 10, 10.0001, 10 } };
			for (const double* Q : P)
			{
				double D = 0;
				TestTrue("valido", FDNGlobe::GreatCircleDistanceDegrees(Q[0], Q[1], Q[2], Q[3], D));
				const double Expected = Haversine(Q[0], Q[1], Q[2], Q[3]);
				TestTrue(FString::Printf(TEXT("%.9f x haversine %.9f"), D, Expected), FMath::Abs(D - Expected) <= 1e-8);
			}
		});

		// Ângulos minúsculos: 1 / 1000000 de grau. Com acos o cosseno (~1 - 1e-16) perde o ângulo; atan2 não.
		It("mantem a precisao em distancias minusculas", [this]()
		{
			double D = 0;
			TestTrue("valido", FDNGlobe::GreatCircleDistanceDegrees(10.0, 10.0, 10.0, 10.000001, D));
			const double Expected = Haversine(10.0, 10.0, 10.0, 10.000001);
			TestTrue(FString::Printf(TEXT("%.15e x haversine %.15e"), D, Expected), FMath::Abs(D - Expected) <= 1e-12);
			TestTrue("nao zerou", D > 9.0e-7);
		});
		It("o Rio e Sao Paulo ficam a uns 3,2 graus um do outro", [this]()
		{
			double D = 0;
			FDNGlobe::GreatCircleDistanceDegrees(-22.9068, -43.1729, -23.5505, -46.6333, D);
			TestTrue(FString::Printf(TEXT("%.4f graus"), D), D > 3.1 && D < 3.4);
		});

		It("entrada invalida devolve falso e nao mexe na saida", [this]()
		{
			double D = 42.0;
			TestFalse("lat invalida", FDNGlobe::GreatCircleDistanceDegrees(95, 0, 0, 0, D));
			TestFalse("lon invalida", FDNGlobe::GreatCircleDistanceDegrees(0, 0, 0, 190, D));
			TestFalse("NaN", FDNGlobe::GreatCircleDistanceDegrees(0, std::numeric_limits<double>::quiet_NaN(), 0, 0, D));
			TestEqual("saida intacta", D, 42.0);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
