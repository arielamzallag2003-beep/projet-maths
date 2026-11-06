#include "DelaunayTriangulation.h"
#include "GeometryMathLibrary.h"
#include "Chaos/Collision/ConvexFeature.h"

void UDelaunayTriangulation::IncrementalTriangulation2D(
	const TArray<FVector>& InputPoints, 
    TArray<FGeomVertex>& OutVertices, 
    TArray<FGeomEdge>& OutEdges, 
    TArray<FGeomFace>& OutFaces)
{
	OutVertices.Empty();
    OutEdges.Empty();
    OutFaces.Empty();

    if (InputPoints.Num() < 3) return;

    // --- Étape 1 : Trier les points par abscisse croissante (et ordonnée en cas d'égalité) [Cours 4] ---
    TArray<FVector> SortedPoints = InputPoints;
    SortedPoints.Sort([](const FVector& A, const FVector& B) {
        if (FMath::IsNearlyEqual(A.X, B.X))
        {
            return A.Y < B.Y;
        }
        return A.X < B.X;
    });

    // Création des sommets géométriques
    for (int32 i = 0; i < SortedPoints.Num(); ++i)
    {
        FGeomVertex Vertex;
        Vertex.ID = i;
        Vertex.Position = SortedPoints[i];
        OutVertices.Add(Vertex);
    }

    // --- Étape 2 : Initialisation de la triangulation [Cours 4] ---
    // Recherche des k premiers points colinéaires pour former la base initiale, puis ajout de P_k+1
    int32 k = 2;
    while (k < SortedPoints.Num() && FMath::IsNearlyZero(UGeometryMathLibrary::OrientAngle2D(SortedPoints[0], SortedPoints[1], SortedPoints[k])))
    {
        k++;
    }

    // (Cas général simplifié pour initialiser les premiers triangles de base)
    // On relie les premiers points pour amorcer le polygone initial de la triangulation T_k+1
    for (int32 i = 0; i < k; ++i)
    {
        if (i < k - 1)
        {
            FGeomEdge Edge;
            Edge.ID = OutEdges.Num();
            Edge.V1 = &OutVertices[i];
            Edge.V2 = &OutVertices[i + 1];
            OutEdges.Add(Edge);
        }
    }

    // --- Étape 3 : Boucle incrémentale pour chaque point P_q+1 [Cours 4] ---
    for (int32 q = k; q < SortedPoints.Num(); q++)
    {
        FVector P_next = SortedPoints[q];

        // 3a) Rechercher les arêtes du polygone de bordure "vues" depuis P_next
        // Une arête [A, B] est vue si P_next est du côté "interne/gauche" (demi-espace E-)
        TArray<FGeomEdge*> VisibleEdges;
        for (FGeomEdge& Edge : OutEdges)
        {
            if (Edge.LeftFace == nullptr || Edge.RightFace == nullptr) // Arête de bordure
            {
                if (Edge.V1 && Edge.V2)
                {
                    // Test d'orientation pour vérifier si l'arête est vue par le nouveau point
                    float Orient = UGeometryMathLibrary::OrientAngle2D(Edge.V1->Position, Edge.V2->Position, P_next);
                    if (Orient < 0.0f) // Vue depuis P_next
                    {
                        VisibleEdges.Add(&Edge);
                    }
                }
            }
        }

        // 3b) Construire les nouveaux triangles reliant les arêtes vues au nouveau point P_q+1
        int32 NewVertexID = q;
        FGeomVertex* NewVertex = &OutVertices[NewVertexID];

        for (FGeomEdge* VEdge : VisibleEdges)
        {
            if (!VEdge) continue;

        	FGeomEdge* NewEdgePt1 = GetOrCreateEdge(OutEdges, VEdge->V1, NewVertex);
        	if (NewEdgePt1 == nullptr)
        	{
        		FGeomEdge NewEdge1;
        		NewEdge1.ID = OutEdges.Num();
        		NewEdge1.V1 = VEdge->V1;
        		NewEdge1.V2 = NewVertex;
        		OutEdges.Add(NewEdge1);
        		NewEdgePt1 = &NewEdge1;
        	}
        	
        	FGeomEdge* NewEdgePt2 = GetOrCreateEdge(OutEdges, NewVertex, VEdge->V2);
        	if (NewEdgePt2 == nullptr)
        	{
        		FGeomEdge NewEdge2;
        		NewEdge2.ID = OutEdges.Num();
        		NewEdge2.V1 = NewVertex;
        		NewEdge2.V2 = VEdge->V2;
        		OutEdges.Add(NewEdge2);
        		NewEdgePt2 = &NewEdge2;
        	}

            // Créer un nouveau triangle [VEdge->V1, NewVertex, VEdge->V2]
            FGeomFace NewFace;
            NewFace.ID = OutFaces.Num();
            NewFace.Vertices[0] = VEdge->V1;
            NewFace.Vertices[1] = NewVertex;
            NewFace.Vertices[2] = VEdge->V2;
        	NewFace.Edges[0] = VEdge;
        	NewFace.Edges[1] = NewEdgePt1;
        	NewFace.Edges[2] = NewEdgePt2;
            OutFaces.Add(NewFace);
        	VEdge->LeftFace = &NewFace;
        	NewEdgePt1->LeftFace = &NewFace;
        	NewEdgePt2->LeftFace = &NewFace;
        }
    }
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

FGeomEdge* UDelaunayTriangulation::GetOrCreateEdge(
	TArray<FGeomEdge>& Edges, 
	FGeomVertex* V1, 
	FGeomVertex* V2)
{
	for (FGeomEdge& Edge : Edges)
	{
		if (Edge.V1 == V1 && Edge.V2 == V2)
		{
			return &Edge;
		}
		else if (Edge.V1 == V2 && Edge.V2 == V1)
		{
			FGeomVertex* tempVertex = Edge.V1;
			Edge.V1 = Edge.V2;
			Edge.V2 = tempVertex;
			FGeomFace* tempFace = Edge.LeftFace;
			Edge.LeftFace = Edge.RightFace;
			Edge.RightFace = tempFace;
			return &Edge;
		}
	}
	return nullptr;
}
