#include "DelaunayTriangulation.h"
#include "GeometryMathLibrary.h"

void UDelaunayTriangulation::IncrementalTriangulation2D(
	const TArray<FVector>& InputPoints, 
    TArray<FGeomVertex>& OutVertices, 
    TArray<FGeomEdge>& OutEdges, 
    TArray<FGeomFace>& OutFaces)
{
    OutVertices.Empty();
    OutEdges.Empty();
    OutFaces.Empty();

    // TODO :
}

void UDelaunayTriangulation::FlipEdgesToDelaunay(TArray<FGeomEdge>& Edges, TArray<FGeomFace>& Faces)
{
	bool bHasFlipped;
	do
	{
		bHasFlipped = false;

		for (FGeomEdge& Edge : Edges)
		{
			if (Edge.LeftFace == nullptr || Edge.RightFace == nullptr)
			{
				continue;
			}

			FGeomFace* Face1 = Edge.LeftFace;
			FGeomFace* Face2 = Edge.RightFace;

			FGeomVertex* V1 = Edge.V1;
			FGeomVertex* V2 = Edge.V2;
			FGeomVertex* Opp1 = nullptr;
			FGeomVertex* Opp2 = nullptr;

			for (int i = 0; i < 3; ++i)
			{
				if (Face1->Vertices[i] != V1 && Face1->Vertices[i] != V2)
				{
					Opp1 = Face1->Vertices[i];
				}
				if (Face2->Vertices[i] != V1 && Face2->Vertices[i] != V2)
				{
					Opp2 = Face2->Vertices[i];
				}
			}

			if (!Opp1 || !Opp2) continue;

			// Test du critère de Delaunay
			if (UGeometryMathLibrary::IsInsideCircumcircle(V1->Position, V2->Position, Opp1->Position, Opp2->Position))
			{
				// on flip l'arête [V1, V2] (devient [Opp1, Opp2])
				Edge.V1 = Opp1;
				Edge.V2 = Opp2;

				// TODO : Mise à jour de la topologie des faces adjacentes
				// (Le quadrilatère [V1, Opp1, V2, Opp2] est réorganisé)
				bHasFlipped = true;
				break;
			}
		}
	} while (bHasFlipped);
}

void UDelaunayTriangulation::DelaunayKernel_AddPoint(
	const FVector& NewPoint, 
	TArray<FGeomVertex>& Vertices, 
	TArray<FGeomEdge>& Edges, 
	TArray<FGeomFace>& Faces)
{
	// TODO :
}

void UDelaunayTriangulation::DelaunayKernel_RemovePoint(
	int32 VertexID, 
	TArray<FGeomVertex>& Vertices, 
	TArray<FGeomEdge>& Edges, 
	TArray<FGeomFace>& Faces)
{
	// TODO :
}