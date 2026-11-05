#include "ConvexHullAlgorithms.h"
#include "GeometryMathLibrary.h"

#define GetAngle UGeometryMathLibrary::GetAngle
#define GetNorm UGeometryMathLibrary::GetNorm
#define OrientAngle2D UGeometryMathLibrary::OrientAngle2D

TArray<FVector> UConvexHullAlgorithms::ComputeJarvisMarch(const TArray<FVector>& Points)
{
	TArray<FVector> ConvexHull;

	if (Points.Num() <= 3)
	{
		return Points;
	}

	//trouver le point le plus à gauche
	int i0 = 0;
	float xmin = Points[i0].X;
	float ymin = Points[i0].Y;
    
	for (int i = 1; i < Points.Num(); ++i)
	{
		if (Points[i].X < xmin || (Points[i].X == xmin && Points[i].Y < ymin))
		{
			i0 = i;
			xmin = Points[i].X;
			ymin = Points[i].Y;
		}
	}
	
	
	FVector v(0, -1, 0);
	int i = i0;

	//trouver chaque point de l'enveloppe convexe
	do
	{
		ConvexHull.Add(Points[i]);
		
		int j;
		if (i == 0)
		{
			j = 1;
		}
		else
		{
			j = 0;
		}
		FVector PiPj = Points[j] - Points[i];
		float amin = GetAngle(v, PiPj);
		float lmax = GetNorm(PiPj);
		int inew = j;

		for (j = inew + 1; j < Points.Num(); j++)
		{
			if (j != i)
			{
				float a = GetAngle(v, PiPj);
				if (a < amin || (a == amin && GetNorm(PiPj) < lmax))
				{
					amin = a;
					lmax = GetNorm(PiPj);
					inew = j;
				}
			}
		}
		v = FVector(Points[inew].X - Points[i].X, Points[inew].Y - Points[i].Y, 0);
		i = inew;
	}
	while (i != i0);
	
	
	return ConvexHull;
}

TArray<FVector> UConvexHullAlgorithms::ComputeGrahamScan(const TArray<FVector>& Points)
{
	TArray<FVector> ConvexHull;

	if (Points.Num() <= 3)
	{
		return Points;
	}
	// Calcul du Barycentre
    FVector B = FVector::ZeroVector;
    for (const FVector& Pt : Points)
    {
        B += Pt;
    }
    B /= Points.Num();

    // Trier les points en fonction de l'angle
    TArray<FVector> SortedPoints = Points;
    SortedPoints.Sort([B](const FVector& P1, const FVector& P2) {
        float Angle1 = FMath::Atan2(P1.Y - B.Y, P1.X - B.X);
        float Angle2 = FMath::Atan2(P2.Y - B.Y, P2.X - B.X);
        
        if (FMath::IsNearlyEqual(Angle1, Angle2))
        {
            return FVector::DistSquared(B, P1) < FVector::DistSquared(B, P2);
        }
        return Angle1 < Angle2;
    });

    // construction de la liste chainée
    int N = SortedPoints.Num();
    TArray<FCircularNode> Nodes;
    Nodes.SetNum(N);

    for (int i = 0; i < N; i++)
    {
        Nodes[i].Position = SortedPoints[i];
        Nodes[i].Prec = &Nodes[(i - 1 + N) % N];
        Nodes[i].Suiv = &Nodes[(i + 1) % N];
    }

    // suppression des sommets concaves
    FCircularNode* Pivot = &Nodes[0];
    FCircularNode* Sinit = Pivot;
    bool bAvance = true;

    do
    {
        // Test d'orientation du triplet (Précédent, Pivot, Suivant)
        float Orient = OrientAngle2D(Pivot->Prec->Position, Pivot->Position, Pivot->Suiv->Position);
        
        if (Orient <= 0.0f) // Cas convexe
        {
            Pivot = Pivot->Suiv;
            bAvance = true;
        }
        else // Cas concave : suppression du sommet
        {
            FCircularNode* PrevNode = Pivot->Prec;
            FCircularNode* NextNode = Pivot->Suiv;
            
            PrevNode->Suiv = NextNode;
            NextNode->Prec = PrevNode;

            Sinit = PrevNode;
            Pivot = Sinit;
            bAvance = false;
        }
    } while (Pivot != Sinit || !bAvance);

    // Récupération des sommets restants dans la liste circulaire pour former l'enveloppe finale
    FCircularNode* Curr = Sinit;
    do
    {
        ConvexHull.Add(Curr->Position);
        Curr = Curr->Suiv;
    } while (Curr != Sinit);
	
	return ConvexHull;
}