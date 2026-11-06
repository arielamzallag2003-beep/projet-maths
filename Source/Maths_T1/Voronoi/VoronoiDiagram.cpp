#include "VoronoiDiagram.h"

// Toutes les fonctions internes sont dans un espace de noms à part, pour ne rien
// partager avec les autres fichiers du module (aucun conflit de nom possible).
namespace VoronoiHalfPlanes
{
	/** Sommet du polygone en cours de découpe, avec le site qui a créé l'arête qui en part. */
	struct FClipVertex
	{
		FVector2D P;
		int32 Neighbor; // arête P -> sommet suivant ; INDEX_NONE = morceau du cadre
	};

	/**
	 * Garde la partie du polygone qui est dans le demi-plan E_ij (points au moins aussi
	 * près de Pi que de Pj). Découpe de Sutherland-Hodgman par une seule droite.
	 *
	 * f(X) = n · (X - M), avec n = (Pj - Pi) / |Pj - Pi| et M le milieu de [Pi Pj] :
	 * distance signée de X à la médiatrice, négative du côté de Pi. X est gardé si f(X) <= Eps.
	 */
	void ClipByBisector(TArray<FClipVertex>& Poly, const FVector2D& Pi, const FVector2D& Pj, int32 J, double Eps,
		TArray<FClipVertex>& Scratch)
	{
		const FVector2D D = Pj - Pi;
		const double Length = D.Size();
		if (Length <= 0.0)
		{
			return;
		}
		const FVector2D N = D / Length;
		const FVector2D M = (Pi + Pj) * 0.5;

		Scratch.Reset();
		const int32 Count = Poly.Num();
		for (int32 k = 0; k < Count; ++k)
		{
			const FClipVertex& A = Poly[k];
			const FClipVertex& B = Poly[(k + 1) % Count];
			const double Fa = FVector2D::DotProduct(N, A.P - M);
			const double Fb = FVector2D::DotProduct(N, B.P - M);
			const bool bInA = Fa <= Eps;
			const bool bInB = Fb <= Eps;

			if (bInA)
			{
				Scratch.Add(A);
			}
			if (bInA != bInB)
			{
				// Fa et Fb sont de part et d'autre de Eps : Fa - Fb ne peut pas être nul
				const double T = FMath::Clamp(Fa / (Fa - Fb), 0.0, 1.0);
				const FVector2D I = A.P + (B.P - A.P) * T;
				// On sort par I : la suite longe la médiatrice (voisin J).
				// On rentre par I : la suite longe l'arête d'origine (même voisin que A).
				Scratch.Add({ I, bInA ? J : A.Neighbor });
			}
		}
		Swap(Poly, Scratch);
	}

	/**
	 * Supprime les sommets confondus (arêtes de longueur quasi nulle, par exemple quand
	 * quatre sites sont sur un même cercle). En retirant le premier des deux sommets,
	 * l'arête qui arrive garde son voisin et celle qui repart garde le sien.
	 */
	void RemoveTinyEdges(TArray<FClipVertex>& Poly, double MergeDist)
	{
		const double Merge2 = MergeDist * MergeDist;
		int32 k = 0;
		while (Poly.Num() > 3 && k < Poly.Num())
		{
			const int32 Next = (k + 1) % Poly.Num();
			if (FVector2D::DistSquared(Poly[k].P, Poly[Next].P) <= Merge2)
			{
				Poly.RemoveAt(k);
			}
			else
			{
				++k;
			}
		}
	}

	/** Données communes à toutes les régions : sites en 2D, doublons, cadre et tolérances. */
	struct FSetup
	{
		TArray<FVector2D> P;
		TArray<int32> DuplicateOf;
		FVector2D BoxMin = FVector2D::ZeroVector;
		FVector2D BoxMax = FVector2D::ZeroVector;
		double Eps = 0.0;
		double Merge = 0.0;
	};

	void Prepare(const TArray<FVector>& Sites, double BoundsMargin, FSetup& S)
	{
		const int32 N = Sites.Num();
		S.P.Reset();
		S.P.Reserve(N);

		// Cadre de découpe : boîte englobante des sites, agrandie de la marge
		FVector2D BoxMin(TNumericLimits<double>::Max(), TNumericLimits<double>::Max());
		FVector2D BoxMax(-TNumericLimits<double>::Max(), -TNumericLimits<double>::Max());
		for (const FVector& Site : Sites)
		{
			const FVector2D Q(Site.X, Site.Y);
			S.P.Add(Q);
			BoxMin = FVector2D(FMath::Min(BoxMin.X, Q.X), FMath::Min(BoxMin.Y, Q.Y));
			BoxMax = FVector2D(FMath::Max(BoxMax.X, Q.X), FMath::Max(BoxMax.Y, Q.Y));
		}
		const double Extent = FMath::Max(BoxMax.X - BoxMin.X, BoxMax.Y - BoxMin.Y);
		const double Margin = BoundsMargin > 0.0 ? BoundsMargin : FMath::Max(0.5 * Extent, 100.0);
		S.BoxMin = BoxMin - FVector2D(Margin, Margin);
		S.BoxMax = BoxMax + FVector2D(Margin, Margin);

		// Tolérances relatives à la taille du cadre (le même code marche à 6 cm comme à 60 m)
		const double Scale = FMath::Max(S.BoxMax.X - S.BoxMin.X, S.BoxMax.Y - S.BoxMin.Y);
		S.Eps = 1e-9 * Scale;   // « sur la médiatrice »
		S.Merge = 1e-6 * Scale; // deux sommets confondus, deux sites confondus

		// Doublons exacts : une seule région, partagée
		S.DuplicateOf.Init(INDEX_NONE, N);
		for (int32 i = 0; i < N; ++i)
		{
			for (int32 j = 0; j < i; ++j)
			{
				if (S.DuplicateOf[j] == INDEX_NONE && FVector2D::DistSquared(S.P[i], S.P[j]) <= S.Merge * S.Merge)
				{
					S.DuplicateOf[i] = j;
					break;
				}
			}
		}
	}

	TArray<FVector> ToVectors(const TArray<FClipVertex>& Poly)
	{
		TArray<FVector> Out;
		Out.Reserve(Poly.Num());
		for (const FClipVertex& V : Poly)
		{
			Out.Add(FVector(V.P.X, V.P.Y, 0.0));
		}
		return Out;
	}

	double Area(const TArray<FClipVertex>& Poly)
	{
		double Sum = 0.0;
		for (int32 k = 0; k < Poly.Num(); ++k)
		{
			const FVector2D& A = Poly[k].P;
			const FVector2D& B = Poly[(k + 1) % Poly.Num()].P;
			Sum += A.X * B.Y - B.X * A.Y;
		}
		return 0.5 * Sum;
	}

	/**
	 * Région du site i : le cadre (sens trigonométrique) coupé par les demi-plans E_ij,
	 * du site le plus proche au plus loin. Si Steps n'est pas nul, chaque découpe y est
	 * enregistrée (construction pas à pas) ; OutSkipped reçoit le nombre de sites ignorés
	 * grâce à l'arrêt anticipé.
	 */
	void BuildCell(const FSetup& S, int32 i, TArray<FClipVertex>& Poly, TArray<FClipVertex>& Scratch, TArray<int32>& Order,
		TArray<FVoronoiCellStep>* Steps, int32* OutSkipped)
	{
		const int32 N = S.P.Num();
		Poly.Reset();
		Poly.Add({ FVector2D(S.BoxMin.X, S.BoxMin.Y), INDEX_NONE });
		Poly.Add({ FVector2D(S.BoxMax.X, S.BoxMin.Y), INDEX_NONE });
		Poly.Add({ FVector2D(S.BoxMax.X, S.BoxMax.Y), INDEX_NONE });
		Poly.Add({ FVector2D(S.BoxMin.X, S.BoxMax.Y), INDEX_NONE });

		if (Steps)
		{
			FVoronoiCellStep& First = Steps->AddDefaulted_GetRef();
			First.Polygon = ToVectors(Poly);
		}

		// Les autres sites, du plus proche au plus loin
		Order.Reset();
		for (int32 j = 0; j < N; ++j)
		{
			if (j != i && S.DuplicateOf[j] == INDEX_NONE)
			{
				Order.Add(j);
			}
		}
		const FVector2D Pi = S.P[i];
		const TArray<FVector2D>& P = S.P;
		Order.Sort([&P, &Pi](int32 A, int32 B) { return FVector2D::DistSquared(P[A], Pi) < FVector2D::DistSquared(P[B], Pi); });

		TArray<FClipVertex> Removed;
		for (int32 o = 0; o < Order.Num(); ++o)
		{
			const int32 j = Order[o];

			// Arrêt anticipé : si tout le polygone tient dans le disque de centre Pi et de rayon
			// |Pj - Pi| / 2, il est entièrement dans E_ij, et aussi dans E_ik pour tous les sites
			// k plus loin que j. Les découpes restantes ne changeraient rien.
			double Radius2 = 0.0;
			for (const FClipVertex& V : Poly)
			{
				Radius2 = FMath::Max(Radius2, FVector2D::DistSquared(V.P, Pi));
			}
			if (0.25 * FVector2D::DistSquared(P[j], Pi) > Radius2 + S.Eps)
			{
				if (OutSkipped)
				{
					*OutSkipped = Order.Num() - o;
				}
				return;
			}

			if (Steps)
			{
				// Partie retirée : ce qui est du côté de Pj
				Removed = Poly;
				ClipByBisector(Removed, P[j], Pi, i, S.Eps, Scratch);
			}

			ClipByBisector(Poly, Pi, P[j], j, S.Eps, Scratch);
			RemoveTinyEdges(Poly, S.Merge);

			if (Steps)
			{
				FVoronoiCellStep& Step = Steps->AddDefaulted_GetRef();
				Step.Neighbor = j;
				Step.Polygon = ToVectors(Poly);
				Step.bCut = Removed.Num() >= 3 && FMath::Abs(Area(Removed)) > S.Merge * S.Merge;
				if (Step.bCut)
				{
					Step.Removed = ToVectors(Removed);
				}
			}
		}
		if (OutSkipped)
		{
			*OutSkipped = 0;
		}
	}

	uint64 PairKey(int32 A, int32 B)
	{
		const uint32 Lo = static_cast<uint32>(FMath::Min(A, B));
		const uint32 Hi = static_cast<uint32>(FMath::Max(A, B));
		return (static_cast<uint64>(Lo) << 32) | Hi;
	}
}

void UVoronoiDiagram::ComputeVoronoiHalfPlanes(const TArray<FVector>& Sites, double BoundsMargin,
	TArray<FVoronoiCell>& OutCells, FVector& OutBoundsMin, FVector& OutBoundsMax)
{
	using namespace VoronoiHalfPlanes;

	OutCells.Reset();
	OutBoundsMin = FVector::ZeroVector;
	OutBoundsMax = FVector::ZeroVector;
	const int32 N = Sites.Num();
	if (N == 0)
	{
		return;
	}

	FSetup S;
	Prepare(Sites, BoundsMargin, S);
	OutBoundsMin = FVector(S.BoxMin.X, S.BoxMin.Y, 0.0);
	OutBoundsMax = FVector(S.BoxMax.X, S.BoxMax.Y, 0.0);

	OutCells.SetNum(N);
	TArray<FClipVertex> Poly, Scratch;
	TArray<int32> Order;
	Order.Reserve(N);

	for (int32 i = 0; i < N; ++i)
	{
		FVoronoiCell& Cell = OutCells[i];
		Cell.SiteIndex = i;
		Cell.Site = FVector(S.P[i].X, S.P[i].Y, 0.0);
		Cell.DuplicateOf = S.DuplicateOf[i];
		if (S.DuplicateOf[i] != INDEX_NONE)
		{
			continue; // rempli plus bas avec la région du premier exemplaire
		}

		BuildCell(S, i, Poly, Scratch, Order, nullptr, nullptr);

		Cell.Polygon.Reserve(Poly.Num());
		Cell.EdgeNeighbors.Reserve(Poly.Num());
		for (const FClipVertex& V : Poly)
		{
			Cell.Polygon.Add(FVector(V.P.X, V.P.Y, 0.0));
			Cell.EdgeNeighbors.Add(V.Neighbor);
			Cell.bTouchesBounds |= V.Neighbor == INDEX_NONE;
		}
	}

	for (int32 i = 0; i < N; ++i)
	{
		if (S.DuplicateOf[i] != INDEX_NONE)
		{
			const FVoronoiCell& Source = OutCells[S.DuplicateOf[i]];
			OutCells[i].Polygon = Source.Polygon;
			OutCells[i].EdgeNeighbors = Source.EdgeNeighbors;
			OutCells[i].bTouchesBounds = Source.bTouchesBounds;
		}
	}
}

void UVoronoiDiagram::ComputeVoronoiCellSteps(const TArray<FVector>& Sites, int32 SiteIndex, double BoundsMargin,
	TArray<FVoronoiCellStep>& OutSteps, int32& OutSkippedSites, FVector& OutBoundsMin, FVector& OutBoundsMax)
{
	using namespace VoronoiHalfPlanes;

	OutSteps.Reset();
	OutSkippedSites = 0;
	OutBoundsMin = FVector::ZeroVector;
	OutBoundsMax = FVector::ZeroVector;
	if (!Sites.IsValidIndex(SiteIndex))
	{
		return;
	}

	FSetup S;
	Prepare(Sites, BoundsMargin, S);
	OutBoundsMin = FVector(S.BoxMin.X, S.BoxMin.Y, 0.0);
	OutBoundsMax = FVector(S.BoxMax.X, S.BoxMax.Y, 0.0);

	// Un doublon a la région de son premier exemplaire
	const int32 Canonical = S.DuplicateOf[SiteIndex] != INDEX_NONE ? S.DuplicateOf[SiteIndex] : SiteIndex;
	TArray<FClipVertex> Poly, Scratch;
	TArray<int32> Order;
	BuildCell(S, Canonical, Poly, Scratch, Order, &OutSteps, &OutSkippedSites);
}

TArray<FVoronoiEdge> UVoronoiDiagram::ExtractVoronoiEdges(const TArray<FVoronoiCell>& Cells)
{
	using namespace VoronoiHalfPlanes;

	// Chaque arête apparaît dans deux régions. On la garde une fois par paire de sites ;
	// si les deux copies diffèrent un peu (arrondis), on garde la plus longue.
	TMap<uint64, int32> ByPair;
	TArray<FVoronoiEdge> Out;
	for (const FVoronoiCell& Cell : Cells)
	{
		if (Cell.DuplicateOf != INDEX_NONE)
		{
			continue;
		}
		const int32 Count = Cell.Polygon.Num();
		if (Cell.EdgeNeighbors.Num() != Count)
		{
			continue;
		}
		for (int32 k = 0; k < Count; ++k)
		{
			const int32 Other = Cell.EdgeNeighbors[k];
			if (Other == INDEX_NONE || Other == Cell.SiteIndex)
			{
				continue;
			}
			FVoronoiEdge Edge;
			Edge.A = Cell.Polygon[k];
			Edge.B = Cell.Polygon[(k + 1) % Count];
			Edge.SiteA = FMath::Min(Cell.SiteIndex, Other);
			Edge.SiteB = FMath::Max(Cell.SiteIndex, Other);

			const uint64 Key = PairKey(Edge.SiteA, Edge.SiteB);
			if (const int32* Found = ByPair.Find(Key))
			{
				FVoronoiEdge& Kept = Out[*Found];
				if (FVector::DistSquared(Edge.A, Edge.B) > FVector::DistSquared(Kept.A, Kept.B))
				{
					Kept = Edge;
				}
			}
			else
			{
				ByPair.Add(Key, Out.Add(Edge));
			}
		}
	}
	return Out;
}

TArray<FVector> UVoronoiDiagram::ExtractVoronoiVertices(const TArray<FVoronoiCell>& Cells,
	const FVector& BoundsMin, const FVector& BoundsMax, double Tolerance)
{
	const double Tol = FMath::Max(Tolerance, 0.0);
	auto OnBounds = [&](const FVector& V)
	{
		return FMath::Abs(V.X - BoundsMin.X) <= Tol || FMath::Abs(V.X - BoundsMax.X) <= Tol
			|| FMath::Abs(V.Y - BoundsMin.Y) <= Tol || FMath::Abs(V.Y - BoundsMax.Y) <= Tol;
	};

	TArray<FVector> Out;
	for (const FVoronoiCell& Cell : Cells)
	{
		if (Cell.DuplicateOf != INDEX_NONE)
		{
			continue;
		}
		for (const FVector& V : Cell.Polygon)
		{
			if (OnBounds(V))
			{
				continue;
			}
			const bool bKnown = Out.ContainsByPredicate([&](const FVector& W) { return FVector::DistSquared2D(V, W) <= Tol * Tol; });
			if (!bKnown)
			{
				Out.Add(V);
			}
		}
	}
	return Out;
}
