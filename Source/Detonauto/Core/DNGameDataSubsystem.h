// Cola fina: carrega as tabelas de Data/ na inicialização do jogo, valida e entrega os dados só para leitura.
// Nenhuma regra aqui: o carregador (DNDataLoader) e os validadores (DNDataValidation) fazem o trabalho.
// Erro de carga ou de validação NÃO derruba o jogo: é logado em LogDetonauto e marca os dados como inválidos.
// Testes: Core/Tests/DNSubsystems.spec.cpp.
//
// S1: as configurações de turbo, Crash e takedown voltam ao padrão da struct pura SE a tabela delas for inválida
//     (cada uma isolada das outras tabelas): um erro em Cities.csv não desfaz o balanceamento do turbo.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/Data/DNDataRows.h"
#include "Core/DNMedals.h"
#include "DNGameDataSubsystem.generated.h"

/** Um modo de câmera: o id de Cameras.csv e os números. */
struct FDNCameraMode
{
	FString Id;
	FDNCameraConfig Config;
};

UCLASS()
class DETONAUTO_API UDNGameDataSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Lê as tabelas de Data/ de novo e valida. Substitui o estado anterior. */
	void Reload();

	/** Para quem já tem os dados lidos (testes): valida e guarda. LoadErrors são os erros do carregador. */
	void ApplyData(FDNData NewData, const TArray<FString>& LoadErrors);

	const FDNData& GetData() const { return Data; }

	/** (Não se chama IsDataValid: o UObject já tem um com outra assinatura.) Verdadeiro se carregou e passou em todos os validadores. */
	bool AreTablesValid() const { return bDataValid; }

	/** Erros de carga e de validação, em português, na ordem em que apareceram. */
	const TArray<FString>& GetErrors() const { return Errors; }

	/** A configuração do CSV, ou o padrão da struct pura se a tabela dela é inválida ou não foi carregada (S1). */
	FDNTurboConfig GetTurboConfig() const;
	FDNCrashConfig GetCrashConfig() const;
	FDNTakedownConfig GetTakedownConfig() const;
	FDNDamageConfig GetDamageConfig() const;
	/** Os números do tráfego do CSV, ou o padrão se TrafficSettings.csv é inválido (S1). */
	FDNTrafficSettings GetTrafficSettings() const;
	/** Os modos de câmera do CSV, na ordem da tecla C. Tabela inválida ou não carregada: só o modo padrão (alta-longe). */
	TArray<FDNCameraMode> GetCameraModes() const;
	/** Os carros de tráfego do CSV; vazio se TrafficVehicles.csv é inválido ou não foi carregado. */
	const TArray<FDNTrafficVehicleEntry>& GetTrafficVehicles() const;
	/** Limites das medalhas do cruzamento (sem diferenciar a caixa). Falso, sem mexer em Out, se não existe ou os limites são inválidos. */
	bool TryGetMedalThresholds(const FString& CrossingId, FDNMedalThresholds& Out) const;

	/**
	 *  O preset de carro de Data/Cars.csv (sem diferenciar a caixa). Falso, sem mexer em Out, se o preset não existe ou se a
	 *  tabela Cars.csv é inválida: com a tabela inválida nenhum preset vale, nem o Padrao; quem chama decide o que fazer.
	 */
	bool TryGetCarConfig(const FString& Id, FDNCarConfig& Out) const;

	/** O tipo de peça de rua de Data/Breakables.csv (sem diferenciar a caixa). Falso, sem mexer em Out, se o tipo não existe ou a
	 *  tabela é inválida: quem chama trata a peça como sólida e registra no log (nenhum tipo vale o padrão às escondidas). */
	bool TryGetBreakableKind(const FString& Kind, FDNBreakableKind& Out) const;
	/** Os valores globais de Data/BreakableSettings.csv, ou o padrão se a tabela é inválida (S1). */
	FDNBreakableSettings GetBreakableSettings() const;
	/** Os valores globais de Data/CameraFx.csv (lote 18), ou o padrão se a tabela é inválida (S1). */
	FDNCameraFxSettings GetCameraFxSettings() const;
	/** As peças que soltam do carro CarId (Data/CarParts.csv, lote 18b), na ordem do arquivo; sem diferenciar a caixa. Vazio se o carro
	 *  não tem linhas ou se a tabela é inválida (nenhuma peça vale o padrão às escondidas: o carro fica com a lataria inteira). */
	TArray<FDNCarPartSpec> GetCarParts(const FString& CarId) const;
	/** Os vidros e lentes do carro CarId (Data/CarGlass.csv), na ordem do arquivo; vazio se não tem ou se a tabela é inválida. */
	TArray<FDNGlassSpec> GetCarGlass(const FString& CarId) const;
	/** O efeito (Data/Effects.csv, lote 19). Falso, sem mexer em Out, se a tabela é inválida: nenhum efeito sai com números inventados. */
	bool GetEffect(EDNEffect Effect, FDNEffectSpec& Out) const;
	/** Os tetos de Data/EffectsSettings.csv, ou o padrão da struct se a tabela é inválida. */
	FDNEffectsSettings GetEffectsSettings() const;
	/** A linha central do percurso CourseId (Data/CoursePoints.csv), na ordem de Order, e a largura da pista [cm]; sem diferenciar a caixa.
	 *  Falso, sem mexer nas saídas, se o percurso não existe ou se a tabela é inválida. */
	bool GetCourse(const FString& CourseId, TArray<FVector>& OutPoints, float& OutWidthCm) const;
	/** Os rivais de corrida (Data/Rivals.csv) na ordem da grade: nome e nível do piloto. Vazio se a tabela é inválida. */
	TArray<TPair<FString, FDNRivalConfig>> GetRivals() const;

private:
	FDNData Data;
	TArray<FString> Errors;
	bool bDataValid = false;
	bool bTurboValid = false;
	bool bCrashValid = false;
	bool bTakedownValid = false;
	bool bDamageValid = false;
	bool bCarsValid = false;
	bool bCamerasValid = false;
	bool bTrafficSettingsValid = false;
	bool bTrafficVehiclesValid = false;
	bool bBreakablesValid = false;
	bool bBreakableSettingsValid = false;
	bool bCameraFxValid = false;
	bool bCarPartsValid = false;
	bool bCarGlassValid = false;
	bool bEffectsValid = false;
	bool bEffectsSettingsValid = false;
	bool bCoursesValid = false;
	bool bRivalsValid = false;
};
