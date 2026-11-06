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
	
	const int32 N = InputPoints.Num();
	OutVertices.Reserve(N);
	OutEdges.Reserve(3 * N);
	OutFaces.Reserve(2 * N);
	
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

        	FGeomEdge* NewEdgePt1 = GetOrNullEdge(OutEdges, VEdge->V1, NewVertex, true);
        	if (NewEdgePt1 == nullptr)
        	{
        		FGeomEdge NewEdge1;
        		NewEdge1.ID = OutEdges.Num();
        		NewEdge1.V1 = VEdge->V1;
        		NewEdge1.V2 = NewVertex;
        		OutEdges.Add(NewEdge1);
        		NewEdgePt1 = &OutEdges.Last();
        	}
        	
        	FGeomEdge* NewEdgePt2 = GetOrNullEdge(OutEdges, NewVertex, VEdge->V2, true);
        	if (NewEdgePt2 == nullptr)
        	{
        		FGeomEdge NewEdge2;
        		NewEdge2.ID = OutEdges.Num();
        		NewEdge2.V1 = NewVertex;
        		NewEdge2.V2 = VEdge->V2;
        		OutEdges.Add(NewEdge2);
        		NewEdgePt2 = &OutEdges.Last();
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
        	FGeomFace* Face = &OutFaces.Last();
        	VEdge->RightFace = Face;
        	NewEdgePt1->LeftFace = Face;
        	NewEdgePt2->LeftFace = Face;
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

			// On se représente le edge traité comme allant de bas en haut
			
			FGeomFace* LeftFace = Edge.LeftFace;
			FGeomFace* RightFace = Edge.RightFace;

			FGeomVertex* V1Bottom = Edge.V1;
			FGeomVertex* V2Top = Edge.V2;
			FGeomVertex* OppLeft = nullptr;
			FGeomVertex* OppRight = nullptr;

			for (int i = 0; i < 3; ++i)
			{
				if (LeftFace->Vertices[i] != V1Bottom && LeftFace->Vertices[i] != V2Top)
				{
					OppLeft = LeftFace->Vertices[i];
				}
				if (RightFace->Vertices[i] != V1Bottom && RightFace->Vertices[i] != V2Top)
				{
					OppRight = RightFace->Vertices[i];
				}
			}

			if (!OppLeft || !OppRight) continue;

			// Test du critère de Delaunay
			if (UGeometryMathLibrary::IsInsideCircumcircle(V1Bottom->Position, V2Top->Position, OppLeft->Position, OppRight->Position))
			{
				// On récupère les 4 arrêtes extérieure du quadrilatère
				FGeomEdge* TopLeftEdge = GetOrNullEdge(Edges, V2Top, OppLeft, false);
				FGeomEdge* TopRightEdge = GetOrNullEdge(Edges, V2Top, OppRight, false);
				FGeomEdge* BottomLeftEdge = GetOrNullEdge(Edges, V1Bottom, OppLeft, false);
				FGeomEdge* BottomRightEdge = GetOrNullEdge(Edges, V1Bottom, OppRight, false);

				// On crée les deux nouvelles faces
				FGeomFace NewFaceTop;
				//NewFaceTop.ID = OutFaces.Num();
				NewFaceTop.Vertices[0] = OppRight;
				NewFaceTop.Vertices[1] = V2Top;
				NewFaceTop.Vertices[2] = OppLeft;
				NewFaceTop.Edges[0] = TopRightEdge;
				NewFaceTop.Edges[1] = TopLeftEdge;
				NewFaceTop.Edges[2] = &Edge;
				//OutFaces.Add(NewFaceTop);

				FGeomFace NewFaceBottom;
				//NewFaceBottom.ID = OutFaces.Num();
				NewFaceBottom.Vertices[0] = OppLeft;
				NewFaceBottom.Vertices[1] = V1Bottom;
				NewFaceBottom.Vertices[2] = OppRight;
				NewFaceBottom.Edges[0] = BottomLeftEdge;
				NewFaceBottom.Edges[1] = BottomRightEdge;
				NewFaceBottom.Edges[2] = &Edge;
				//OutFaces.Add(NewFaceBottom);

				if (TopLeftEdge->LeftFace == LeftFace)
				{
					TopLeftEdge->LeftFace = &NewFaceTop;
				}
				else if (TopLeftEdge->RightFace == RightFace)
				{
					TopLeftEdge->RightFace = &NewFaceTop;
				}
				
				// on flip l'arête [V1, V2] (devient [Opp1, Opp2])
				Edge.V1 = OppLeft;
				Edge.V2 = OppRight;

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

FGeomEdge* UDelaunayTriangulation::GetOrNullEdge(
	TArray<FGeomEdge>& Edges, 
	FGeomVertex* V1,
	FGeomVertex* V2,
	bool canModify)
{
	for (FGeomEdge& Edge : Edges)
	{
		if (Edge.V1 == V1 && Edge.V2 == V2)
		{
			return &Edge;
		}
		else if (Edge.V1 == V2 && Edge.V2 == V1)
		{
			if (canModify)
			{
				FGeomVertex* tempVertex = Edge.V1;
				Edge.V1 = Edge.V2;
				Edge.V2 = tempVertex;
				FGeomFace* tempFace = Edge.LeftFace;
				Edge.LeftFace = Edge.RightFace;
				Edge.RightFace = tempFace;
			}
			return &Edge;
		}
	}
	return nullptr;
}
