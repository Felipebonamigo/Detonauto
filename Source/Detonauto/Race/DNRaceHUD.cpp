#include "Race/DNRaceHUD.h"
#include "Race/DNRaceGameMode.h"
#include "Core/DNRaceFormat.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"

void ADNRaceHUD::Text(const FString& Value, float X, float Y, float Scale, const FLinearColor& Color, bool bCenter, bool bRight)
{
	UFont* Font = GEngine ? GEngine->GetLargeFont() : nullptr;
	if (!Canvas || !Font)
	{
		return;
	}
	// a fonte grande da engine é pequena em 1080p: os tamanhos do HUD valem em múltiplos desta escala base
	Scale *= 2.4f;
	float W = 0.0f, H = 0.0f;
	Canvas->TextSize(Font, Value, W, H, Scale, Scale);
	const float Left = bCenter ? X - W * 0.5f : (bRight ? X - W : X);
	// sombra: o texto lê sobre o céu claro e sobre o asfalto
	FCanvasTextItem Shadow(FVector2D(Left + 2.0f * Scale, Y + 2.0f * Scale), FText::FromString(Value), Font, FLinearColor(0, 0, 0, 0.7f * Color.A));
	Shadow.Scale = FVector2D(Scale, Scale);
	Canvas->DrawItem(Shadow);
	FCanvasTextItem Item(FVector2D(Left, Y), FText::FromString(Value), Font, Color);
	Item.Scale = FVector2D(Scale, Scale);
	Canvas->DrawItem(Item);
}

void ADNRaceHUD::DrawHUD()
{
	Super::DrawHUD();
	const ADNRaceGameMode* Mode = GetWorld() ? Cast<ADNRaceGameMode>(GetWorld()->GetAuthGameMode()) : nullptr;
	if (!Mode || !Canvas)
	{
		return;
	}
	const FDNRaceHudData D = Mode->BuildHudData();
	const float W = Canvas->SizeX;
	const float H = Canvas->SizeY;
	const float U = FMath::Max(H / 1080.0f, 0.5f); // escala de interface: 1,0 em 1080p
	const float Margin = 40.0f * U;
	const FLinearColor White(1, 1, 1, 1), Yellow(1.0f, 0.85f, 0.2f, 1), Orange(1.0f, 0.55f, 0.05f, 1), Green(0.4f, 1.0f, 0.4f, 1), Red(1.0f, 0.25f, 0.2f, 1);

	// posição e tempo, em cima à esquerda
	Text(FDNRaceFormat::Place(D.Position) + TEXT(" / ") + FString::FromInt(D.Total), Margin, Margin, 2.4f * U, White);
	Text(FDNRaceFormat::Time(D.TimeS), Margin, Margin + 90.0f * U, 1.3f * U, White);

	// velocidade, em cima à direita
	Text(FString::Printf(TEXT("%.0f"), D.SpeedKmh), W - Margin, Margin, 2.4f * U, White, false, true);
	Text(TEXT("km/h"), W - Margin, Margin + 90.0f * U, 1.0f * U, FLinearColor(0.8f, 0.8f, 0.8f, 1), false, true);

	// takedowns, embaixo à esquerda
	Text(FString::Printf(TEXT("Detonas  %d"), D.Takedowns), Margin, H - Margin - 160.0f * U, 1.2f * U, Orange);

	// turbo: barra embaixo no centro
	const float BarW = 520.0f * U, BarH = 22.0f * U;
	const float BarX = (W - BarW) * 0.5f, BarY = H - Margin - 110.0f * U;
	const float Turbo = D.TurboMax > 0.0f ? FMath::Clamp(D.TurboBar / D.TurboMax, 0.0f, 1.0f) : 0.0f;
	DrawRect(FLinearColor(0, 0, 0, 0.6f), BarX - 3.0f, BarY - 3.0f, BarW + 6.0f, BarH + 6.0f);
	DrawRect(Turbo >= 0.999f ? Yellow : Orange, BarX, BarY, BarW * Turbo, BarH);
	Text(TEXT("TURBO"), BarX, BarY - 36.0f * U, 0.8f * U, FLinearColor(0.85f, 0.85f, 0.85f, 1));

	// progresso: barra fina embaixo, com uma marca por rival e a do jogador
	const float PW = W - 2.0f * Margin, PY = H - Margin - 40.0f * U;
	DrawRect(FLinearColor(0, 0, 0, 0.6f), Margin - 3.0f, PY - 3.0f, PW + 6.0f, 14.0f * U + 6.0f);
	DrawRect(FLinearColor(0.25f, 0.25f, 0.25f, 1), Margin, PY, PW, 14.0f * U);
	for (const float P : D.RivalProgress)
	{
		DrawRect(FLinearColor(0.9f, 0.3f, 0.3f, 1), Margin + PW * FMath::Clamp(P, 0.0f, 1.0f) - 4.0f * U, PY - 6.0f * U, 8.0f * U, 26.0f * U);
	}
	DrawRect(Green, Margin + PW * FMath::Clamp(D.PlayerProgress, 0.0f, 1.0f) - 5.0f * U, PY - 10.0f * U, 10.0f * U, 34.0f * U);

	// contagem de largada
	if (D.CountdownLeftS > 0.0f)
	{
		Text(FString::FromInt(FMath::CeilToInt(D.CountdownLeftS)), W * 0.5f, H * 0.28f, 7.0f * U, Red, true);
	}
	else if (D.TimeS < 1.2f && !D.bFinished)
	{
		Text(TEXT("VAI!"), W * 0.5f, H * 0.28f, 6.0f * U, Green, true);
	}

	// aviso de takedown
	if (D.TakedownAgeS < 2.5f)
	{
		const float A = FMath::Clamp(2.5f - D.TakedownAgeS, 0.0f, 1.0f);
		Text(D.TakedownText, W * 0.5f, H * 0.2f, 2.6f * U, FLinearColor(1.0f, 0.3f, 0.1f, A), true);
	}

	// tela de resultado ou de destruição
	if (D.bFinished || D.bPlayerWrecked)
	{
		const float PanelW = 760.0f * U;
		const float RowH = 64.0f * U;
		const float PanelH = 190.0f * U + RowH * D.Rows.Num();
		const float PX = (W - PanelW) * 0.5f;
		const float PY2 = H * 0.5f - PanelH * 0.5f;
		DrawRect(FLinearColor(0, 0, 0, 0.72f), PX, PY2, PanelW, PanelH);
		Text(D.bFinished ? TEXT("CHEGADA") : TEXT("CARRO DESTRUIDO"), W * 0.5f, PY2 + 20.0f * U, 2.2f * U, D.bFinished ? Green : Red, true);
		float Y = PY2 + 110.0f * U;
		for (const FDNRaceHudRow& Row : D.Rows)
		{
			const FLinearColor C = Row.bPlayer ? Yellow : White;
			Text(Row.Place, PX + 40.0f * U, Y, 1.3f * U, C);
			Text(Row.Name, PX + 190.0f * U, Y, 1.3f * U, C);
			Text(Row.Value, PX + PanelW - 40.0f * U, Y, 1.3f * U, C, false, true);
			Y += RowH;
		}
		Text(FString::Printf(TEXT("Detonas: %d     R: recomecar"), D.Takedowns), W * 0.5f, PY2 + PanelH - 60.0f * U, 1.0f * U, FLinearColor(0.85f, 0.85f, 0.85f, 1), true);
	}
}
