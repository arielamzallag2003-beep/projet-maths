#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GeometryTypes.h"
#include "GeomVisualizer.generated.h"

DECLARE_MULTICAST_DELEGATE(FOnGeomChanged);

UENUM()
enum class EGeomAlgorithm : uint8
{
	Jarvis,
	Graham,
	Incremental,
	Delaunay
};

/** Segment 2D à dessiner (calques envoyés par la page : référence, Voronoï, cercles...). */
struct FGeomOverlay
{
	TArray<TPair<FVector2D, FVector2D>> Segments;
	FColor Color = FColor::White;
};

/**
 * Scène de test de l'UI : garde les points, appelle les algorithmes existants
 * (sans les modifier) et dessine leurs résultats dans le niveau.
 *
 * Plan XY, Z = 0. La caméra regarde depuis -Z (voir AGeomTestPawn) pour que
 * X aille à droite et Y vers le haut, comme dans le cours.
 */
UCLASS()
class MATHS_T1_API AGeomVisualizer : public AActor
{
	GENERATED_BODY()

public:
	AGeomVisualizer();
	virtual void Tick(float DeltaSeconds) override;

	// --- Points ---
	int32 AddPoint(const FVector2D& P);
	void RemovePoint(int32 Id);
	void MovePoint(int32 Id, const FVector2D& P, bool bRecompute);
	void SetAllPoints(const TArray<int32>& Ids, const TArray<FVector2D>& Positions);
	int32 FindNearest(const FVector2D& P, double MaxDistance) const;
	FBox2D GetBounds() const;
	int32 NumPoints() const { return Points.Num(); }
	/** Durée du dernier appel d'algorithme, en ms. */
	double GetLastMs() const { return LastMs; }

	// --- Réglages venant du panneau ---
	void SetAlgorithm(EGeomAlgorithm InAlgorithm);
	void SetLayer(const FString& Name, bool bVisible);
	void SetKernelMode(bool bEnabled);
	void SetOverlay(const FString& Name, const FString& PackedSegments, const FColor& Color);

	/** Relance l'algorithme courant sur tous les points. */
	void Recompute();

	// --- Sérialisation pour la page (texte JSON écrit à la main, sans dépendance) ---
	FString PointsToJson() const;
	FString ResultToJson() const;

	FOnGeomChanged OnPointsChanged;
	FOnGeomChanged OnResultChanged;

	/** Taille d'un pixel écran en cm, mise à jour par le contrôleur (épaisseur des traits). */
	double PixelSize = 4.0;
	int32 HoverId = INDEX_NONE;
	int32 DragId = INDEX_NONE;

private:
	TArray<FVector> Points;
	TArray<int32> PointIds;
	int32 NextId = 1;

	EGeomAlgorithm Algorithm = EGeomAlgorithm::Jarvis;
	bool bKernelMode = false;
	TMap<FString, bool> Layers;
	TMap<FString, FGeomOverlay> Overlays;

	// Résultats bruts des fonctions des algorithmes
	TArray<FVector> Hull;
	TArray<FGeomVertex> Vertices;
	TArray<FGeomEdge> Edges;
	TArray<FGeomFace> Faces;
	bool bHasTriangulation = false;

	// Résultats extraits pour le dessin et la page
	TArray<TArray<FVector>> ResultTriangles;
	TArray<TPair<FVector, FVector>> ResultEdges;
	int32 InvalidFaces = 0;
	int32 InvalidEdges = 0;
	double LastMs = 0.0;
	FString LastCall;
	int32 ResultSeq = 0;
	bool bPendingMove = false;

	void ClearResults();
	void ExtractTriangulation();
	bool IsValidVertex(const FGeomVertex* V) const;
	int32 FindVertexId(const FVector& P) const;
	bool IsLayerOn(const FString& Name) const;
	void NotifyPoints();

	void DrawLine(const FVector2D& A, const FVector2D& B, const FColor& Color, double WidthPx, double Z, uint8 Priority) const;
	void DrawGrid() const;
};
