#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GeometryTypes.h"
#include "DelaunayTriangulation.generated.h"

UCLASS()
class MATHS_T1_API UDelaunayTriangulation : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Triangulation 2D incrémentale basique - À compléter par vos soins
	UFUNCTION(BlueprintCallable, Category = "Geometry | Delaunay")
	static void IncrementalTriangulation2D(const TArray<FVector>& InputPoints, TArray<FGeomVertex>& OutVertices, TArray<FGeomEdge>& OutEdges, TArray<FGeomFace>& OutFaces);

	// Algorithme de Flipping d'arêtes pour transformer une triangulation en Delaunay - À compléter par vos soins
	UFUNCTION(BlueprintCallable, Category = "Geometry | Delaunay")
	static void FlipEdgesToDelaunay(TArray<FGeomEdge>& Edges, TArray<FGeomFace>& Faces);

	// Noyau de Delaunay : Ajout direct d'un point sans re-trianguler l'enveloppe totale - À compléter par vos soins
	UFUNCTION(BlueprintCallable, Category = "Geometry | Delaunay")
	static void DelaunayKernel_AddPoint(const FVector& NewPoint, TArray<FGeomVertex>& Vertices, TArray<FGeomEdge>& Edges, TArray<FGeomFace>& Faces);

	// Noyau de Delaunay : Suppression d'un point - À compléter par vos soins
	UFUNCTION(BlueprintCallable, Category = "Geometry | Delaunay")
	static void DelaunayKernel_RemovePoint(int32 VertexID, TArray<FGeomVertex>& Vertices, TArray<FGeomEdge>& Edges, TArray<FGeomFace>& Faces);
};