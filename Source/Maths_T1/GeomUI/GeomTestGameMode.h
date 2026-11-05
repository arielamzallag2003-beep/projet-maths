#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GeomTestGameMode.generated.h"

class AGeomVisualizer;
class UGeomBridge;
class SWebBrowser;
class SBox;
class SWidget;

/**
 * Mode de jeu de la scène de test de l'UI.
 *
 * À mettre en « GameMode Override » dans les World Settings d'un niveau vide
 * (voir README). Il crée tout seul la caméra, le visualiseur et le panneau web
 * (Content/UI/Web/index.html) affiché à gauche de l'écran.
 */
UCLASS()
class MATHS_T1_API AGeomTestGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AGeomTestGameMode();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;

	void TogglePanel();

	/** Largeur du panneau en unités Slate (avant mise à l'échelle DPI). */
	UPROPERTY(EditAnywhere, Category = "Geometry UI")
	float PanelWidth = 380.f;

	/** Marge autour du panneau, en unités Slate. */
	UPROPERTY(EditAnywhere, Category = "Geometry UI")
	float PanelMargin = 12.f;

	/** Chemin de la page, relatif à Content/. */
	UPROPERTY(EditAnywhere, Category = "Geometry UI")
	FString PagePath = TEXT("UI/Web/index.html");

	/** Images par seconde du navigateur intégré (CEF peint sur le CPU). */
	UPROPERTY(EditAnywhere, Category = "Geometry UI")
	int32 BrowserFrameRate = 30;

private:
	UPROPERTY()
	TObjectPtr<AGeomVisualizer> Visualizer;

	UPROPERTY()
	TObjectPtr<UGeomBridge> Bridge;

	TSharedPtr<SWebBrowser> Browser;
	TSharedPtr<SBox> PanelBox;
	TSharedPtr<SWidget> Root;
	bool bPanelVisible = true;
	bool bFramedOnce = false;

	TWeakObjectPtr<class AGeomTestPlayerController> WiredController;

	float SinceLoad = 0.f;
	int32 LoadAttempts = 0;

	void CreatePanel();
	void LoadPage();
	/** Branche le contrôleur (il peut arriver après BeginPlay). */
	bool TryWireController();
	float GetPanelWidthPx() const;
};
