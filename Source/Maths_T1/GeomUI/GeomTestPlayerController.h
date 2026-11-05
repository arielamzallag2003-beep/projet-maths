#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GeomTestPlayerController.generated.h"

class AGeomVisualizer;
class AGeomTestPawn;

UENUM()
enum class EGeomTool : uint8
{
	Add,
	Move,
	Delete
};

/**
 * Souris et clavier dans le niveau de test (quand le panneau web n'a pas le focus).
 * Liaisons de touches directes, sans asset d'Enhanced Input, pour ne rien ajouter
 * dans Content/ à part la page web.
 */
UCLASS()
class MATHS_T1_API AGeomTestPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AGeomTestPlayerController();

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

	void SetVisualizer(AGeomVisualizer* InVisualizer) { Visualizer = InVisualizer; }
	void SetTool(EGeomTool InTool) { Tool = InTool; }

	/** Cadre tous les points dans la zone libre à droite du panneau. */
	void FrameAll();

	/** Largeur en pixels occupée par le panneau à gauche (marges comprises). */
	float PanelWidthPx = 0.f;

	/** Position du curseur dans le plan, pour l'afficher dans la page. */
	FVector2D GetCursorOnPlane() const { return LastCursor; }

	/** Appelé pour les raccourcis qui concernent la page (1-4, H, Ctrl+Z...). */
	TFunction<void(const FString&)> OnShortcut;

private:
	UPROPERTY()
	TObjectPtr<AGeomVisualizer> Visualizer;

	EGeomTool Tool = EGeomTool::Add;
	bool bPanning = false;
	FVector2D PanStartMouse = FVector2D::ZeroVector;
	FVector2D PanStartCenter = FVector2D::ZeroVector;
	FVector2D LastCursor = FVector2D::ZeroVector;

	AGeomTestPawn* GetGeomPawn() const;
	bool GetMouseOnPlane(FVector2D& Out) const;
	double GetPixelSize() const;
	double PickRadius() const { return 14.0 * GetPixelSize(); }
	void Zoom(float Factor);

	void OnLeftPressed();
	void OnLeftReleased();
	void OnRightPressed();
	void OnMiddlePressed();
	void OnMiddleReleased();
	void OnWheelUp() { Zoom(1.f / 1.15f); }
	void OnWheelDown() { Zoom(1.15f); }
	void OnKeyFrame() { FrameAll(); }
	void OnKeyPanel() { if (OnShortcut) OnShortcut(TEXT("panel")); }
	void OnKey1() { if (OnShortcut) OnShortcut(TEXT("algo:jarvis")); }
	void OnKey2() { if (OnShortcut) OnShortcut(TEXT("algo:graham")); }
	void OnKey3() { if (OnShortcut) OnShortcut(TEXT("algo:incr")); }
	void OnKey4() { if (OnShortcut) OnShortcut(TEXT("algo:delaunay")); }
	void OnKeyUndo();
	void OnKeyRedo();
	void OnToolAdd() { Tool = EGeomTool::Add; if (OnShortcut) OnShortcut(TEXT("tool:add")); }
	void OnToolMove() { Tool = EGeomTool::Move; if (OnShortcut) OnShortcut(TEXT("tool:move")); }
	void OnToolDelete() { Tool = EGeomTool::Delete; if (OnShortcut) OnShortcut(TEXT("tool:del")); }
};
