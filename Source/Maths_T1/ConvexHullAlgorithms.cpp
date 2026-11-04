#include "ConvexHullAlgorithms.h"
#include "GeometryMathLibrary.h"

TArray<FVector> UConvexHullAlgorithms::ComputeJarvisMarch(const TArray<FVector>& Points)
{
	TArray<FVector> ConvexHull;

	if (Points.Num() <= 3)
	{
		return Points;
	}

	// TODO: Étape 0 - Trouver le point le plus à gauche (S1) [Cours 1]
	
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
	
	ConvexHull.Add(Points[i0]);
	
	FVector v(0, -1, 0);
	
	// TODO: Étape 1 - Boucle de rotation dans le sens trigonométrique pour trouver le pivot suivant [Cours 1]

	return ConvexHull;
}

TArray<FVector> UConvexHullAlgorithms::ComputeGrahamScan(const TArray<FVector>& Points)
{
	TArray<FVector> ConvexHull;

	if (Points.Num() < 3)
	{
		return Points;
	}

	// TODO: Étape 1 - Calculer le barycentre B des points [Cours 3]

	// TODO: Étape 2 - Trier les points selon l'angle polaire Ox par rapport au barycentre [Cours 3]

	// TODO: Étape 3 - Construire la liste chaînée circulaire (FCircularNode) [Cours 3]

	// TODO: Étape 4 - Boucle de suppression des sommets concaves (orient2D <= 0) [Cours 3]

	return ConvexHull;
}