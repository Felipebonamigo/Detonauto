#pragma once

#include "CoreMinimal.h"

/** Categoria de log principal do jogo. */
DECLARE_LOG_CATEGORY_EXTERN(LogDetonauto, Log, All);

/**
 *  Tipo de colisão do destroço de peça de rua (lote 17c; canal "DNDebris" em Config/DefaultEngine.ini). As rodas do carro o ignoram:
 *  o traço da suspensão da Chaos corre no canal WorldDynamic, e com o destroço nesse tipo o carro subia na peça que voava à frente
 *  (sem contato nenhum no chassi) e era lançado a 100 km/h girando.
 */
#define ECC_DNDebris ECC_GameTraceChannel1