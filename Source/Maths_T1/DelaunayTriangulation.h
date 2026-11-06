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
	
	UFUNCTION(BlueprintCallable, Category = "Geometry | Delaunay")
	static void IncrementalTriangulation2D(const TArray<FVector>& InputPoints, TArray<FGeomVertex>& OutVertices, TArray<FGeomEdge>& OutEdges, TArray<FGeomFace>& OutFaces);
	
	UFUNCTION(BlueprintCallable, Category = "Geometry | Delaunay")
	static void FlipEdgesToDelaunay(TArray<FGeomEdge>& Edges, TArray<FGeomFace>& Faces);

	//Bonus
	UFUNCTION(BlueprintCallable, Category = "Geometry | Delaunay")
	static void DelaunayKernel_AddPoint(const FVector& NewPoint, TArray<FGeomVertex>& Vertices, TArray<FGeomEdge>& Edges, TArray<FGeomFace>& Faces);
	
	UFUNCTION(BlueprintCallable, Category = "Geometry | Delaunay")
	static void DelaunayKernel_RemovePoint(int32 VertexID, TArray<FGeomVertex>& Vertices, TArray<FGeomEdge>& Edges, TArray<FGeomFace>& Faces);

	// Existence d'un Edge
private:
	static FGeomEdge* GetOrCreateEdge(TArray<FGeomEdge>& Edges, FGeomVertex* V1, FGeomVertex* V2);
};