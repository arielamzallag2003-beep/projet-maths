#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GeometryTypes.h"
#include "VoronoiDiagram.h"
#include "GeomVisualizer.generated.h"

class UCanvas;

DECLARE_MULTICAST_DELEGATE(FOnGeomChanged);

UENUM()
enum class EGeomAlgorithm : uint8
{
	Jarvis,
	Graham,
	Incremental,
	Delaunay,
	Voronoi
};

/** Segment 2D à dessiner (calques envoyés par la page : référence, Voronoï, cercles...). */
struct FGeomOverlay
{
	TArray<TPair<FVector2D, FVector2D>> Segments;
	FColor Color = FColor::White;
};

/**
 * Scène de test de l'UI : garde les points, appelle les algorithmes existants
 * (sans les modifier) et dessine leurs résultats dans le niveau, sur le Canvas
 * du HUD (AGeomHUD appelle Paint à chaque frame).
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

	/** Dessine la scène en 2D sur le Canvas du HUD (appelé par AGeomHUD::DrawHUD). */
	void Paint(UCanvas* Canvas);

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

	// --- Construction pas à pas d'une région de Voronoï ---
	/** SiteId <= 0 : garde le site courant. Un nouveau site repart de l'étape 0. */
	void SetConstruction(bool bOn, int32 SiteId, int32 Step);
	/** Clic sur un point dans le niveau : en mode construction, il devient le site construit. */
	void OnPointPressed(int32 Id);
	FString ConstructionToJson() const;

	// --- Sérialisation pour la page (texte JSON écrit à la main, sans dépendance) ---
	FString PointsToJson() const;
	FString ResultToJson() const;

	FOnGeomChanged OnPointsChanged;
	FOnGeomChanged OnResultChanged;
	FOnGeomChanged OnConstructionChanged;

	/** Taille d'un pixel écran en cm, mise à jour par le contrôleur (épaisseur des traits). */
	double PixelSize = 4.0;
	int32 HoverId = INDEX_NONE;
	int32 DragId = INDEX_NONE;

	/** Curseur dans le plan (survol d'un sommet de Voronoï). Mis à jour par le contrôleur. */
	FVector2D CursorWorld = FVector2D::ZeroVector;
	bool bCursorValid = false;

	/** Échelle des tailles en pixels (traits, points, texte), réglée par le GameMode selon le DPI. */
	float DrawScale = 1.f;

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

	// Diagramme de Voronoï (Source/Maths_T1/Voronoi, indépendant des autres algorithmes)
	TArray<FVoronoiCell> VoronoiCells;
	TArray<FVoronoiEdge> VoronoiEdges;
	TArray<FVector> VoronoiVertices;
	int32 VoronoiVertexCount = 0;
	FVector VoronoiBoundsMin = FVector::ZeroVector;
	FVector VoronoiBoundsMax = FVector::ZeroVector;

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

	// Construction pas à pas
	bool bConstruction = false;
	int32 ConstructionSiteId = INDEX_NONE;
	int32 ConstructionIndex = INDEX_NONE;
	int32 ConstructionStep = 0;
	int32 ConstructionSkipped = 0;
	TArray<FVoronoiCellStep> ConstructionSteps;
	void RebuildConstruction();
};
