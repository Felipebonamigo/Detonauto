// Testes do registro do jogador (docs/regras-save.md): melhor pontuação por cruzamento, medalha derivada dos limites
// e texto JSON versão 1 com tratamento rígido de entrada ruim. Funções puras: o texto entra e sai por FString.
//
// Decisões de implementação (o contrato não cobre; ver Core/DNRecords.h):
//  R1 número JSON "inteiro" = valor sem parte fracionária (4200 e 4200.0 valem; 4200.5 não);
//  R2 "best" ausente ou que não é objeto, numa versão 1: erro (um save assim gravado por cima apagaria tudo);
//  R3 sobra de texto depois do objeto raiz é JSON inválido: erro;
//  R4 a razão do erro vai como primeiro aviso.

#include "Misc/AutomationTest.h"
#include "Core/DNRecords.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FString Show(const TOptional<int32>& V) { return V.IsSet() ? FString::FromInt(V.GetValue()) : FString(TEXT("sem registro")); }

	FDNRecordsLoadResult Load(const FString& Text) { return FDNRecords::Deserialize(Text); }

	/** Texto de versão 1 com o corpo de "best" dado. */
	FString V1(const FString& Best) { return FString::Printf(TEXT("{\"version\":1,\"best\":{%s}}"), *Best); }

	FDNCrossingDef Def(const TCHAR* Id, const TCHAR* City, int32 Bronze = 1000, int32 Silver = 2000, int32 Gold = 3000)
	{
		FDNCrossingDef D;
		D.CrossingId = FName(Id);
		D.CityId = FName(City);
		D.Thresholds.Bronze = Bronze;
		D.Thresholds.Silver = Silver;
		D.Thresholds.Gold = Gold;
		return D;
	}
}

BEGIN_DEFINE_SPEC(FDNRecordsSpec, "Detonauto.Registro",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	void ExpectError(const TCHAR* What, const FString& Text)
	{
		const FDNRecordsLoadResult R = Load(Text);
		TestTrue(FString::Printf(TEXT("%s: esperava erro"), What), R.bError);
		TestEqual(FString::Printf(TEXT("%s: registro vazio no erro"), What), R.Records.Num(), 0);
		TestTrue(FString::Printf(TEXT("%s: a razão vem nos avisos"), What), R.Warnings.Num() >= 1);
	}
END_DEFINE_SPEC(FDNRecordsSpec)

void FDNRecordsSpec::Define()
{
	Describe("RegisterResult e BestScore", [this]()
	{
		It("o primeiro resultado vira o registro e conta como novo recorde, mesmo 0", [this]()
		{
			FDNRecords R;
			TestTrue("0 e novo recorde", R.RegisterResult(TEXT("rio-01"), 0));
			TestEqual("guardou 0", Show(R.BestScore(TEXT("rio-01"))), FString(TEXT("0")));
			TestTrue("4200 e novo recorde", R.RegisterResult(TEXT("rio-02"), 4200));
			TestEqual("guardou 4200", Show(R.BestScore(TEXT("rio-02"))), FString(TEXT("4200")));
		});

		It("sem registro e diferente de zero", [this]()
		{
			FDNRecords R;
			TestFalse("nada guardado", R.BestScore(TEXT("rio-01")).IsSet());
			R.RegisterResult(TEXT("rio-01"), 0);
			TestTrue("0 guardado", R.BestScore(TEXT("rio-01")).IsSet());
			TestEqual("valor", R.BestScore(TEXT("rio-01")).GetValue(), 0);
			TestFalse("outro id continua sem registro", R.BestScore(TEXT("rio-02")).IsSet());
		});

		It("guarda o maior; so e recorde se for ESTRITAMENTE maior", [this]()
		{
			FDNRecords R;
			R.RegisterResult(TEXT("a"), 1000);
			TestFalse("igual nao e recorde", R.RegisterResult(TEXT("a"), 1000));
			TestFalse("pior nao e recorde", R.RegisterResult(TEXT("a"), 999));
			TestEqual("pior nao diminui", Show(R.BestScore(TEXT("a"))), FString(TEXT("1000")));
			TestTrue("1 a mais e recorde", R.RegisterResult(TEXT("a"), 1001));
			TestEqual("subiu", Show(R.BestScore(TEXT("a"))), FString(TEXT("1001")));
		});

		It("pontuacao negativa e ignorada: falso, sem registro novo e sem mexer no antigo", [this]()
		{
			FDNRecords R;
			TestFalse("-1 num id novo", R.RegisterResult(TEXT("a"), -1));
			TestFalse("id novo nao foi criado", R.BestScore(TEXT("a")).IsSet());
			TestEqual("nenhuma entrada", R.Num(), 0);
			R.RegisterResult(TEXT("a"), 500);
			TestFalse("MIN_int32 num id existente", R.RegisterResult(TEXT("a"), MIN_int32));
			TestEqual("antigo intacto", Show(R.BestScore(TEXT("a"))), FString(TEXT("500")));
		});

		It("id vazio e ignorado", [this]()
		{
			FDNRecords R;
			TestFalse("vazio", R.RegisterResult(TEXT(""), 100));
			TestEqual("nenhuma entrada", R.Num(), 0);
			TestFalse("consulta de id vazio", R.BestScore(TEXT("")).IsSet());
		});

		It("MAX_int32 cabe e MAX_int32 de novo nao e recorde", [this]()
		{
			FDNRecords R;
			TestTrue("MAX", R.RegisterResult(TEXT("a"), MAX_int32));
			TestFalse("MAX de novo", R.RegisterResult(TEXT("a"), MAX_int32));
			TestEqual("valor", Show(R.BestScore(TEXT("a"))), FString::FromInt(MAX_int32));
		});

		It("ids comparam sem diferenciar a caixa e o id guardado e o da primeira vez", [this]()
		{
			FDNRecords R;
			TestTrue("primeiro", R.RegisterResult(TEXT("Rio-01"), 100));
			TestTrue("mesmo id com outra caixa, maior", R.RegisterResult(TEXT("RIO-01"), 300));
			TestFalse("mesmo id com outra caixa, menor", R.RegisterResult(TEXT("rio-01"), 200));
			TestEqual("uma entrada so", R.Num(), 1);
			TestEqual("consulta em qualquer caixa", Show(R.BestScore(TEXT("rIo-01"))), FString(TEXT("300")));
			const TArray<FDNRecordEntry> E = R.SortedEntries();
			if (TestEqual("uma entrada ordenada", E.Num(), 1))
			{
				TestTrue("grafia da primeira vez", E[0].Id.Equals(TEXT("Rio-01"), ESearchCase::CaseSensitive));
				TestEqual("pontuacao", E[0].Score, 300);
			}
		});
	});

	Describe("ToProgressInput", [this]()
	{
		It("sem registro = nenhuma medalha, e vem um item por cruzamento do jogo, na ordem", [this]()
		{
			FDNRecords R;
			const TArray<FDNCrossingProgress> P = R.ToProgressInput({ Def(TEXT("b"), TEXT("rio")), Def(TEXT("a"), TEXT("sp")) });
			if (!TestEqual("dois itens", P.Num(), 2)) { return; }
			TestEqual("id 0", P[0].CrossingId, FName(TEXT("b")));
			TestEqual("cidade 0", P[0].CityId, FName(TEXT("rio")));
			TestTrue("medalha 0", P[0].Medal == EDNMedal::None);
			TestEqual("id 1", P[1].CrossingId, FName(TEXT("a")));
			TestTrue("medalha 1", P[1].Medal == EDNMedal::None);
		});

		It("a medalha vem dos limites ATUAIS: abaixo do bronze, bronze, prata, ouro (limite inclusive)", [this]()
		{
			FDNRecords R;
			R.RegisterResult(TEXT("x0"), 999);
			R.RegisterResult(TEXT("x1"), 1000);
			R.RegisterResult(TEXT("x2"), 2000);
			R.RegisterResult(TEXT("x3"), 3000);
			R.RegisterResult(TEXT("x4"), MAX_int32);
			R.RegisterResult(TEXT("x5"), 0);
			const TArray<FDNCrossingProgress> P = R.ToProgressInput({
				Def(TEXT("x0"), TEXT("c")), Def(TEXT("x1"), TEXT("c")), Def(TEXT("x2"), TEXT("c")),
				Def(TEXT("x3"), TEXT("c")), Def(TEXT("x4"), TEXT("c")), Def(TEXT("x5"), TEXT("c")) });
			if (!TestEqual("itens", P.Num(), 6)) { return; }
			TestTrue("999 abaixo do bronze", P[0].Medal == EDNMedal::None);
			TestTrue("1000 bronze", P[1].Medal == EDNMedal::Bronze);
			TestTrue("2000 prata", P[2].Medal == EDNMedal::Silver);
			TestTrue("3000 ouro", P[3].Medal == EDNMedal::Gold);
			TestTrue("MAX ouro", P[4].Medal == EDNMedal::Gold);
			TestTrue("0 registrado, sem medalha", P[5].Medal == EDNMedal::None);
		});

		It("mudar o limite muda a medalha sem mexer no registro", [this]()
		{
			FDNRecords R;
			R.RegisterResult(TEXT("x"), 1500);
			TestTrue("com bronze 1000", R.ToProgressInput({ Def(TEXT("x"), TEXT("c"), 1000, 2000, 3000) })[0].Medal == EDNMedal::Bronze);
			TestTrue("com bronze 1600", R.ToProgressInput({ Def(TEXT("x"), TEXT("c"), 1600, 2000, 3000) })[0].Medal == EDNMedal::None);
			TestTrue("com ouro 1500", R.ToProgressInput({ Def(TEXT("x"), TEXT("c"), 100, 200, 1500) })[0].Medal == EDNMedal::Gold);
		});

		It("a busca do registro ignora a caixa; limites invalidos nao dao medalha", [this]()
		{
			FDNRecords R;
			R.RegisterResult(TEXT("Rio-01"), 5000);
			TestTrue("outra caixa", R.ToProgressInput({ Def(TEXT("rio-01"), TEXT("c")) })[0].Medal == EDNMedal::Gold);
			TestTrue("limites iguais (invalidos)", R.ToProgressInput({ Def(TEXT("rio-01"), TEXT("c"), 100, 100, 100) })[0].Medal == EDNMedal::None);
		});

		It("alimenta FDNProgress::Compute (cruzamento sem registro nao conta como feito)", [this]()
		{
			FDNRecords R;
			R.RegisterResult(TEXT("a"), 3500);
			R.RegisterResult(TEXT("b"), 1200);
			FDNCityRegion City;
			City.CityId = FName(TEXT("rio"));
			City.RegionId = FName(TEXT("brasil"));
			const FDNProgressResult Result = FDNProgress::Compute(
				R.ToProgressInput({ Def(TEXT("a"), TEXT("rio")), Def(TEXT("b"), TEXT("rio")), Def(TEXT("c"), TEXT("rio")) }), { City });
			TestEqual("total", Result.Overall.Total, 3);
			TestEqual("feitos", Result.Overall.Done, 2);
			TestEqual("ouros", Result.Overall.Gold, 1);
		});
	});

	Describe("Serialize", [this]()
	{
		It("registro vazio", [this]()
		{
			TestEqual("texto", FDNRecords().Serialize(), FString(TEXT("{\"version\":1,\"best\":{}}")));
		});

		It("formato do contrato, com ordem alfabetica sem diferenciar a caixa", [this]()
		{
			FDNRecords R;
			R.RegisterResult(TEXT("exemplo-rio-02"), 0);
			R.RegisterResult(TEXT("exemplo-rio-01"), 4200);
			TestEqual("texto", R.Serialize(), FString(TEXT("{\"version\":1,\"best\":{\"exemplo-rio-01\":4200,\"exemplo-rio-02\":0}}")));
		});

		It("a ordem nao depende da ordem de insercao nem da caixa", [this]()
		{
			FDNRecords A, B;
			for (const TCHAR* Id : { TEXT("b"), TEXT("A"), TEXT("c"), TEXT("B2"), TEXT("a1") }) { A.RegisterResult(Id, 1); }
			for (const TCHAR* Id : { TEXT("a1"), TEXT("B2"), TEXT("c"), TEXT("A"), TEXT("b") }) { B.RegisterResult(Id, 1); }
			TestEqual("mesmo texto", A.Serialize(), B.Serialize());
			TestEqual("ordem", A.Serialize(), FString(TEXT("{\"version\":1,\"best\":{\"A\":1,\"a1\":1,\"b\":1,\"B2\":1,\"c\":1}}")));
		});

		It("MAX_int32 sai por extenso, sem notacao cientifica", [this]()
		{
			FDNRecords R;
			R.RegisterResult(TEXT("a"), MAX_int32);
			TestEqual("texto", R.Serialize(), FString::Printf(TEXT("{\"version\":1,\"best\":{\"a\":%d}}"), MAX_int32));
		});

		It("id com aspas, barra e acento sai escapado e volta igual", [this]()
		{
			FDNRecords R;
			R.RegisterResult(TEXT("a\"b\\c"), 7);
			R.RegisterResult(TEXT("São-Paulo"), 8);
			const FDNRecordsLoadResult Back = Load(R.Serialize());
			TestFalse("sem erro", Back.bError);
			TestTrue("igual", Back.Records == R);
		});
	});

	Describe("Deserialize, texto ruim", [this]()
	{
		It("texto vazio, so espacos, lixo", [this]()
		{
			ExpectError(TEXT("vazio"), TEXT(""));
			ExpectError(TEXT("espacos"), TEXT("   \n\t "));
			ExpectError(TEXT("lixo"), TEXT("lixo"));
			ExpectError(TEXT("chave sem fechar"), TEXT("{\"version\":1,\"best\":{"));
			ExpectError(TEXT("virgula sobrando"), TEXT("{\"version\":1,\"best\":{},}"));
		});

		It("raiz que nao e objeto", [this]()
		{
			ExpectError(TEXT("array"), TEXT("[]"));
			ExpectError(TEXT("array com versao"), TEXT("[{\"version\":1,\"best\":{}}]"));
			ExpectError(TEXT("null"), TEXT("null"));
			ExpectError(TEXT("numero"), TEXT("42"));
			ExpectError(TEXT("texto"), TEXT("\"version\""));
		});

		It("R3: sobra de texto depois do objeto e erro", [this]()
		{
			ExpectError(TEXT("lixo depois"), TEXT("{\"version\":1,\"best\":{}} xyz"));
			ExpectError(TEXT("dois objetos"), TEXT("{\"version\":1,\"best\":{}}{\"version\":1,\"best\":{}}"));
		});

		It("versao ausente, 0, 2, nao numerica, nao inteira: erro; versao maior nunca e lida", [this]()
		{
			ExpectError(TEXT("ausente"), TEXT("{\"best\":{\"a\":1}}"));
			ExpectError(TEXT("0"), TEXT("{\"version\":0,\"best\":{\"a\":1}}"));
			ExpectError(TEXT("2"), TEXT("{\"version\":2,\"best\":{\"a\":1}}"));
			ExpectError(TEXT("-1"), TEXT("{\"version\":-1,\"best\":{\"a\":1}}"));
			ExpectError(TEXT("texto \"1\""), TEXT("{\"version\":\"1\",\"best\":{\"a\":1}}"));
			ExpectError(TEXT("1.5"), TEXT("{\"version\":1.5,\"best\":{\"a\":1}}"));
			ExpectError(TEXT("true"), TEXT("{\"version\":true,\"best\":{\"a\":1}}"));
			ExpectError(TEXT("null"), TEXT("{\"version\":null,\"best\":{\"a\":1}}"));
			ExpectError(TEXT("array"), TEXT("{\"version\":[1],\"best\":{\"a\":1}}"));
			ExpectError(TEXT("1e9"), TEXT("{\"version\":1000000000,\"best\":{\"a\":1}}"));
		});

		It("R7: version ou best repetidos na raiz sao erro; nomes de campo valem so em minusculas", [this]()
		{
			ExpectError(TEXT("version repetido"), TEXT("{\"version\":1,\"version\":1,\"best\":{}}"));
			ExpectError(TEXT("best repetido"), TEXT("{\"version\":1,\"best\":{},\"best\":{}}"));
			ExpectError(TEXT("Version com V maiusculo"), TEXT("{\"Version\":1,\"best\":{}}"));
			ExpectError(TEXT("BEST maiusculo"), TEXT("{\"version\":1,\"BEST\":{}}"));
		});

		It("campo desconhecido mal formado derruba o texto todo; bem formado e aninhado e ignorado", [this]()
		{
			ExpectError(TEXT("aninhado sem fechar"), TEXT("{\"version\":1,\"best\":{},\"x\":{\"y\":[1,2}"));
			const FDNRecordsLoadResult R = Load(TEXT("{\"x\":{\"y\":[1,{\"z\":[]}],\"version\":99},\"version\":1,\"best\":{\"a\":1}}"));
			TestFalse("sem erro", R.bError);
			TestEqual("a", Show(R.Records.BestScore(TEXT("a"))), FString(TEXT("1")));
		});

		It("entrada com valor aninhado dentro de best nao quebra as seguintes", [this]()
		{
			const FDNRecordsLoadResult R = Load(V1(TEXT("\"a\":{\"b\":[1,{\"c\":2}]},\"d\":[1,[2]],\"ok\":9")));
			TestFalse("sem erro", R.bError);
			TestEqual("so a boa", R.Records.Num(), 1);
			TestEqual("ok", Show(R.Records.BestScore(TEXT("ok"))), FString(TEXT("9")));
			TestEqual("dois avisos", R.Warnings.Num(), 2);
		});

		It("versao 1 escrita 1.0 vale (R1)", [this]()
		{
			const FDNRecordsLoadResult R = Load(TEXT("{\"version\":1.0,\"best\":{\"a\":1}}"));
			TestFalse("sem erro", R.bError);
			TestEqual("a", Show(R.Records.BestScore(TEXT("a"))), FString(TEXT("1")));
		});

		It("R2: best ausente ou de tipo errado e erro", [this]()
		{
			ExpectError(TEXT("best ausente"), TEXT("{\"version\":1}"));
			ExpectError(TEXT("best array"), TEXT("{\"version\":1,\"best\":[1,2]}"));
			ExpectError(TEXT("best numero"), TEXT("{\"version\":1,\"best\":5}"));
			ExpectError(TEXT("best null"), TEXT("{\"version\":1,\"best\":null}"));
			ExpectError(TEXT("best texto"), TEXT("{\"version\":1,\"best\":\"x\"}"));
		});
	});

	Describe("Deserialize, entradas ruins valem so a boa", [this]()
	{
		auto Ignored = [this](const TCHAR* What, const FString& Entry)
		{
			// A entrada ruim vem entre duas boas: elas valem, a ruim some e gera aviso.
			const FDNRecordsLoadResult R = Load(V1(FString::Printf(TEXT("\"ok1\":10,%s,\"ok2\":20"), *Entry)));
			TestFalse(FString::Printf(TEXT("%s: nao e erro"), What), R.bError);
			TestEqual(FString::Printf(TEXT("%s: so as duas boas"), What), R.Records.Num(), 2);
			TestEqual(FString::Printf(TEXT("%s: ok1"), What), Show(R.Records.BestScore(TEXT("ok1"))), FString(TEXT("10")));
			TestEqual(FString::Printf(TEXT("%s: ok2"), What), Show(R.Records.BestScore(TEXT("ok2"))), FString(TEXT("20")));
			TestFalse(FString::Printf(TEXT("%s: nao criou o id ruim"), What), R.Records.BestScore(TEXT("ruim")).IsSet());
			TestEqual(FString::Printf(TEXT("%s: um aviso"), What), R.Warnings.Num(), 1);
			if (R.Warnings.Num() == 1)
			{
				TestTrue(FString::Printf(TEXT("%s: o aviso cita o id: %s"), What, *R.Warnings[0]), R.Warnings[0].Contains(TEXT("ruim")));
			}
		};

		It("negativa (-1), nao inteira (4200.5), tipo errado e grande demais", [this, Ignored]()
		{
			Ignored(TEXT("-1"), TEXT("\"ruim\":-1"));
			Ignored(TEXT("MIN_int32"), TEXT("\"ruim\":-2147483648"));
			Ignored(TEXT("4200.5"), TEXT("\"ruim\":4200.5"));
			Ignored(TEXT("0.5"), TEXT("\"ruim\":0.5"));
			Ignored(TEXT("MAX_int32 + 1"), TEXT("\"ruim\":2147483648"));
			Ignored(TEXT("1e10"), TEXT("\"ruim\":10000000000"));
			Ignored(TEXT("1e30"), TEXT("\"ruim\":1e30"));
			Ignored(TEXT("texto"), TEXT("\"ruim\":\"4200\""));
			Ignored(TEXT("booleano"), TEXT("\"ruim\":true"));
			Ignored(TEXT("null"), TEXT("\"ruim\":null"));
			Ignored(TEXT("array"), TEXT("\"ruim\":[1]"));
			Ignored(TEXT("objeto"), TEXT("\"ruim\":{\"a\":1}"));
		});

		It("os limites validos entram: 0, MAX_int32, 4200.0 (R1) e -0", [this]()
		{
			const FDNRecordsLoadResult R = Load(V1(FString::Printf(TEXT("\"zero\":0,\"max\":%d,\"inteiro\":4200.0,\"menoszero\":-0"), MAX_int32)));
			TestFalse("sem erro", R.bError);
			TestEqual("sem avisos", R.Warnings.Num(), 0);
			TestEqual("zero", Show(R.Records.BestScore(TEXT("zero"))), FString(TEXT("0")));
			TestEqual("max", Show(R.Records.BestScore(TEXT("max"))), FString::FromInt(MAX_int32));
			TestEqual("4200.0", Show(R.Records.BestScore(TEXT("inteiro"))), FString(TEXT("4200")));
			TestEqual("-0", Show(R.Records.BestScore(TEXT("menoszero"))), FString(TEXT("0")));
		});

		It("id vazio na entrada e ignorado com aviso", [this]()
		{
			const FDNRecordsLoadResult R = Load(V1(TEXT("\"\":5,\"ok\":6")));
			TestFalse("sem erro", R.bError);
			TestEqual("so uma entrada", R.Records.Num(), 1);
			TestEqual("um aviso", R.Warnings.Num(), 1);
		});

		It("id repetido ignorando a caixa: vale o MAIOR, com aviso, e a grafia e a da primeira vez", [this]()
		{
			const FDNRecordsLoadResult A = Load(V1(TEXT("\"Rio-01\":100,\"rio-01\":300")));
			TestFalse("A sem erro", A.bError);
			TestEqual("A uma entrada", A.Records.Num(), 1);
			TestEqual("A vale o maior", Show(A.Records.BestScore(TEXT("RIO-01"))), FString(TEXT("300")));
			TestEqual("A um aviso", A.Warnings.Num(), 1);
			const TArray<FDNRecordEntry> EA = A.Records.SortedEntries();
			if (TestEqual("A entradas", EA.Num(), 1)) { TestTrue("A grafia da primeira vez", EA[0].Id.Equals(TEXT("Rio-01"), ESearchCase::CaseSensitive)); }

			const FDNRecordsLoadResult B = Load(V1(TEXT("\"Rio-01\":900,\"rio-01\":300,\"RIO-01\":50")));
			TestEqual("B vale o maior", Show(B.Records.BestScore(TEXT("rio-01"))), FString(TEXT("900")));
			TestEqual("B dois avisos", B.Warnings.Num(), 2);
			TestEqual("B uma entrada", B.Records.Num(), 1);
		});

		It("a mesma chave escrita duas vezes tambem vale o maior, com aviso", [this]()
		{
			const FDNRecordsLoadResult R = Load(V1(TEXT("\"a\":900,\"a\":300")));
			TestFalse("sem erro", R.bError);
			TestEqual("vale o maior", Show(R.Records.BestScore(TEXT("a"))), FString(TEXT("900")));
			TestEqual("um aviso", R.Warnings.Num(), 1);
		});

		It("a repetida ruim nao derruba a boa", [this]()
		{
			const FDNRecordsLoadResult R = Load(V1(TEXT("\"Rio-01\":100,\"rio-01\":-5,\"RIO-01\":\"x\"")));
			TestFalse("sem erro", R.bError);
			TestEqual("fica a boa", Show(R.Records.BestScore(TEXT("rio-01"))), FString(TEXT("100")));
			TestEqual("dois avisos", R.Warnings.Num(), 2);
		});

		It("campos desconhecidos na raiz sao ignorados, sem aviso", [this]()
		{
			const FDNRecordsLoadResult R = Load(TEXT("{\"money\":99,\"version\":1,\"cars\":[1,2],\"best\":{\"a\":5},\"extra\":{\"x\":null}}"));
			TestFalse("sem erro", R.bError);
			TestEqual("sem avisos", R.Warnings.Num(), 0);
			TestEqual("a", Show(R.Records.BestScore(TEXT("a"))), FString(TEXT("5")));
		});

		It("espacos e quebras de linha entre os elementos valem; a ordem das chaves da raiz nao importa", [this]()
		{
			const FDNRecordsLoadResult R = Load(TEXT("\n  {\n \"best\" : { \"a\" : 5 } ,\n \"version\" : 1\n}\n  "));
			TestFalse("sem erro", R.bError);
			TestEqual("a", Show(R.Records.BestScore(TEXT("a"))), FString(TEXT("5")));
		});

		It("best vazio e valido", [this]()
		{
			const FDNRecordsLoadResult R = Load(V1(TEXT("")));
			TestFalse("sem erro", R.bError);
			TestEqual("vazio", R.Records.Num(), 0);
		});
	});

	Describe("Ida e volta", [this]()
	{
		It("Deserialize(Serialize(R)) e igual a R, sem avisos", [this]()
		{
			FDNRecords R;
			R.RegisterResult(TEXT("exemplo-rio-01"), 4200);
			R.RegisterResult(TEXT("Exemplo-Rio-02"), 0);
			R.RegisterResult(TEXT("z"), MAX_int32);
			const FDNRecordsLoadResult Back = Load(R.Serialize());
			TestFalse("sem erro", Back.bError);
			TestEqual("sem avisos", Back.Warnings.Num(), 0);
			TestTrue("igual", Back.Records == R);
		});

		It("registro vazio tambem volta igual", [this]()
		{
			const FDNRecordsLoadResult Back = Load(FDNRecords().Serialize());
			TestFalse("sem erro", Back.bError);
			TestTrue("igual", Back.Records == FDNRecords());
		});

		It("o texto volta identico (serializar de novo da o mesmo texto)", [this]()
		{
			FDNRecords R;
			R.RegisterResult(TEXT("b"), 2);
			R.RegisterResult(TEXT("A"), 1);
			const FString Text = R.Serialize();
			TestEqual("texto", Load(Text).Records.Serialize(), Text);
		});

		It("a igualdade distingue pontuacao e grafia do id", [this]()
		{
			FDNRecords A, B, C;
			A.RegisterResult(TEXT("a"), 1);
			B.RegisterResult(TEXT("a"), 2);
			C.RegisterResult(TEXT("A"), 1);
			TestFalse("pontuacao diferente", A == B);
			TestFalse("grafia diferente", A == C);
			TestTrue("igual a si", A == A);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
