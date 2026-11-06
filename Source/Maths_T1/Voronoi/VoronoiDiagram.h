#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VoronoiDiagram.generated.h"

/**
 * Région de Voronoï d'un site : l'ensemble des points du plan dont ce site est le plus proche.
 * Polygone convexe, sens trigonométrique, Z = 0. Les régions infinies sont coupées par le cadre.
 */
USTRUCT(BlueprintType)
struct FVoronoiCell
{
	GENERATED_BODY()

	/** Indice du site dans le tableau d'entrée. */
	UPROPERTY(BlueprintReadOnly, Category = "Geometry|Voronoi")
	int32 SiteIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Geometry|Voronoi")
	FVector Site = FVector::ZeroVector;

	/** Sommets de la région, sens trigonométrique. */
	UPROPERTY(BlueprintReadOnly, Category = "Geometry|Voronoi")
	TArray<FVector> Polygon;

	/**
	 * EdgeNeighbors[k] : site voisin de l'autre côté de l'arête Polygon[k] -> Polygon[k + 1].
	 * INDEX_NONE si cette arête est un morceau du cadre.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Geometry|Voronoi")
	TArray<int32> EdgeNeighbors;

	/** Vrai si la région touche le cadre (région infinie, ou cadre trop petit). */
	UPROPERTY(BlueprintReadOnly, Category = "Geometry|Voronoi")
	bool bTouchesBounds = false;

	/** Si ce site est un doublon exact d'un site précédent : indice de ce site. Même région. */
	UPROPERTY(BlueprintReadOnly, Category = "Geometry|Voronoi")
	int32 DuplicateOf = INDEX_NONE;
};

/** Arête du diagramme : frontière commune aux régions de SiteA et SiteB (SiteA < SiteB). */
USTRUCT(BlueprintType)
struct FVoronoiEdge
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Geometry|Voronoi")
	FVector A = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Geometry|Voronoi")
	FVector B = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Geometry|Voronoi")
	int32 SiteA = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Geometry|Voronoi")
	int32 SiteB = INDEX_NONE;
};

/**
 * Diagramme de Voronoï 2D par intersection de demi-plans (cours 5).
 *
 * Indépendant des autres algorithmes du projet : n'utilise ni la triangulation
 * ni les primitives de GeometryMathLibrary. Sert de première version et de
 * vérification pour la future construction par dualité avec Delaunay.
 * Détails et formules : Source/Maths_T1/Voronoi/README.md
 */
UCLASS()
class MATHS_T1_API UVoronoiDiagram : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Calcule la région de chaque site dans le plan XY (Z ignoré).
	 *
	 * @param Sites         Points d'entrée. Les doublons exacts reçoivent la région de leur premier exemplaire.
	 * @param BoundsMargin  Marge autour des sites pour le cadre de découpe (cm). <= 0 : automatique,
	 *                      max(la moitié de la plus grande dimension du nuage, 100 cm).
	 * @param OutCells      Une région par site, dans l'ordre de Sites.
	 * @param OutBoundsMin  Coin bas gauche du cadre utilisé.
	 * @param OutBoundsMax  Coin haut droit du cadre utilisé.
	 */
	UFUNCTION(BlueprintCallable, Category = "Geometry|Voronoi")
	static void ComputeVoronoiHalfPlanes(const TArray<FVector>& Sites, double BoundsMargin,
		TArray<FVoronoiCell>& OutCells, FVector& OutBoundsMin, FVector& OutBoundsMax);

	/** Arêtes du diagramme, chacune une seule fois (les morceaux du cadre ne sont pas des arêtes). */
	UFUNCTION(BlueprintCallable, Category = "Geometry|Voronoi")
	static TArray<FVoronoiEdge> ExtractVoronoiEdges(const TArray<FVoronoiCell>& Cells);

	/**
	 * Sommets du diagramme : points où se rejoignent au moins trois régions, sans ceux
	 * situés sur le cadre. Deux sommets plus proches que Tolerance sont fusionnés.
	 */
	UFUNCTION(BlueprintCallable, Category = "Geometry|Voronoi")
	static TArray<FVector> ExtractVoronoiVertices(const TArray<FVoronoiCell>& Cells,
		const FVector& BoundsMin, const FVector& BoundsMax, double Tolerance = 0.01);
};
