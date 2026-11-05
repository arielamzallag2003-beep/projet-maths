#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GeomBridge.generated.h"

class AGeomVisualizer;
class AGeomTestPlayerController;
class SWebBrowser;

/**
 * Pont entre la page web et la scène de test.
 *
 * Page -> C++ : l'objet est lié sous le nom « geom » (SWebBrowser::BindUObject).
 *   Dans la page : window.ue.geom.setalgorithm("graham"). Unreal met les noms
 *   en minuscules et chaque appel renvoie une Promise.
 * C++ -> page : ExecuteJavascript appelle window.onGeomPoints / onGeomResult /
 *   onGeomState / onGeomCursor / onGeomShortcut.
 *
 * Les envois vers la page sont regroupés (Flush, 20 fois par seconde au plus) :
 * le navigateur intégré repeint sur le CPU, inutile de lui envoyer chaque frame.
 */
UCLASS()
class MATHS_T1_API UGeomBridge : public UObject
{
	GENERATED_BODY()

public:
	void Init(AGeomVisualizer* InVisualizer, AGeomTestPlayerController* InController, TSharedPtr<SWebBrowser> InBrowser);

	void SetController(AGeomTestPlayerController* InController) { Controller = InController; }

	/** À appeler à chaque tick : envoie ce qui a changé. */
	void Flush(float DeltaSeconds);

	void PushShortcut(const FString& Name);

	bool IsReady() const { return bReady; }

	// ---- Appelées par la page (JavaScript) ----

	/** La page est chargée : on lui envoie l'état complet. */
	UFUNCTION()
	void Ready();

	/** "jarvis" | "graham" | "incr" | "delaunay" */
	UFUNCTION()
	void SetAlgorithm(const FString& Id);

	/** Remplace tous les points. Format : "id,x,y;id,x,y;..." */
	UFUNCTION()
	void SetPoints(const FString& Packed);

	/** Calques dessinés par le C++ : "hull", "edges", "labels", "grid". */
	UFUNCTION()
	void SetLayer(const FString& Name, bool bVisible);

	/** "add" | "move" | "del" */
	UFUNCTION()
	void SetTool(const FString& Tool);

	/** Delaunay uniquement : ajout / suppression via DelaunayKernel_AddPoint / RemovePoint. */
	UFUNCTION()
	void SetKernelMode(bool bEnabled);

	/** Calque calculé par la page. Format : "x1,y1,x2,y2;..." ; chaîne vide = retirer. Couleur "#RRGGBB". */
	UFUNCTION()
	void SetOverlay(const FString& Name, const FString& Packed, const FString& ColorHex);

	UFUNCTION()
	void FrameAll();

	UFUNCTION()
	void Recompute();

	/** Écrit un message de la page dans le journal Unreal (LogMaths_T1). */
	UFUNCTION()
	void Log(const FString& Message);

private:
	TWeakObjectPtr<AGeomVisualizer> Visualizer;
	TWeakObjectPtr<AGeomTestPlayerController> Controller;
	TWeakPtr<SWebBrowser> Browser;

	bool bReady = false;
	bool bPointsDirty = false;
	bool bResultDirty = false;
	float SinceLastPush = 0.f;
	float SinceLastCursor = 0.f;
	FVector2D LastSentCursor = FVector2D(1e30, 1e30);
	FString Algorithm = TEXT("jarvis");
	FString Tool = TEXT("add");
	bool bKernel = false;

	void MarkPointsDirty_Internal() { bPointsDirty = true; }
	void MarkResultDirty_Internal() { bResultDirty = true; }
	void Exec(const FString& Script) const;
	void PushState();
};
