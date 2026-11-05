#include "GeomVisualizer.h"
#include "ConvexHullAlgorithms.h"
#include "DelaunayTriangulation.h"
#include "DrawDebugHelpers.h"
#include "Maths_T1.h"

namespace GeomColors
{
	const FColor Grid100(26, 33, 44);
	const FColor Grid500(36, 46, 60);
	const FColor Grid1000(48, 60, 78);
	const FColor AxisX(170, 60, 60);
	const FColor AxisY(70, 150, 80);
	const FColor Hull(255, 181, 71);
	const FColor Tri(95, 168, 255);
	const FColor Point(238, 243, 249);
	const FColor Label(133, 146, 165);
	const FColor Hover(255, 255, 255);
	const FColor Drag(197, 139, 255);
}

// Caméra en -Z : plus Z est petit, plus c'est proche de la caméra (donc devant).
namespace GeomDepth
{
	constexpr double Grid = 30.0;
	constexpr double Triangles = 10.0;
	constexpr double Overlay = 5.0;
	constexpr double Hull = 0.0;
	constexpr double Points = -10.0;
}

AGeomVisualizer::AGeomVisualizer()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Layers.Add(TEXT("hull"), true);
	Layers.Add(TEXT("edges"), true);
	Layers.Add(TEXT("labels"), true);
	Layers.Add(TEXT("grid"), true);
}

// ---------------------------------------------------------------- Points

int32 AGeomVisualizer::AddPoint(const FVector2D& P)
{
	// Ignore un clic exactement sur un point existant (doublon)
	if (FindNearest(P, 1.0) != INDEX_NONE)
	{
		return INDEX_NONE;
	}

	const int32 Id = NextId++;
	Points.Add(FVector(P.X, P.Y, 0.0));
	PointIds.Add(Id);

	// Mode noyau : on teste DelaunayKernel_AddPoint sur la triangulation existante
	if (bKernelMode && Algorithm == EGeomAlgorithm::Delaunay && bHasTriangulation)
	{
		const double T0 = FPlatformTime::Seconds();
		UDelaunayTriangulation::DelaunayKernel_AddPoint(FVector(P.X, P.Y, 0.0), Vertices, Edges, Faces);
		LastMs = (FPlatformTime::Seconds() - T0) * 1000.0;
		LastCall = TEXT("DelaunayKernel_AddPoint");
		ExtractTriangulation();
		NotifyPoints();
		OnResultChanged.Broadcast();
		return Id;
	}

	NotifyPoints();
	Recompute();
	return Id;
}

void AGeomVisualizer::RemovePoint(int32 Id)
{
	const int32 Index = PointIds.IndexOfByKey(Id);
	if (Index == INDEX_NONE)
	{
		return;
	}
	const FVector Removed = Points[Index];
	Points.RemoveAt(Index);
	PointIds.RemoveAt(Index);
	if (HoverId == Id) HoverId = INDEX_NONE;

	if (bKernelMode && Algorithm == EGeomAlgorithm::Delaunay && bHasTriangulation)
	{
		const int32 VertexId = FindVertexId(Removed);
		if (VertexId != INDEX_NONE)
		{
			const double T0 = FPlatformTime::Seconds();
			UDelaunayTriangulation::DelaunayKernel_RemovePoint(VertexId, Vertices, Edges, Faces);
			LastMs = (FPlatformTime::Seconds() - T0) * 1000.0;
			LastCall = TEXT("DelaunayKernel_RemovePoint");
			ExtractTriangulation();
			NotifyPoints();
			OnResultChanged.Broadcast();
			return;
		}
		UE_LOG(LogMaths_T1, Warning, TEXT("Mode noyau : sommet introuvable dans la triangulation, recalcul complet."));
	}

	NotifyPoints();
	Recompute();
}

void AGeomVisualizer::MovePoint(int32 Id, const FVector2D& P, bool bRecompute)
{
	const int32 Index = PointIds.IndexOfByKey(Id);
	if (Index == INDEX_NONE)
	{
		return;
	}
	Points[Index] = FVector(P.X, P.Y, 0.0);
	if (bRecompute)
	{
		NotifyPoints();
		Recompute();
	}
}

void AGeomVisualizer::SetAllPoints(const TArray<int32>& Ids, const TArray<FVector2D>& Positions)
{
	Points.Reset();
	PointIds.Reset();
	NextId = 1;
	for (int32 i = 0; i < Positions.Num(); ++i)
	{
		const int32 Id = Ids.IsValidIndex(i) && Ids[i] > 0 ? Ids[i] : NextId;
		Points.Add(FVector(Positions[i].X, Positions[i].Y, 0.0));
		PointIds.Add(Id);
		NextId = FMath::Max(NextId, Id + 1);
	}
	HoverId = INDEX_NONE;
	NotifyPoints();
	Recompute();
}

int32 AGeomVisualizer::FindNearest(const FVector2D& P, double MaxDistance) const
{
	int32 Best = INDEX_NONE;
	double BestDist = MaxDistance * MaxDistance;
	for (int32 i = 0; i < Points.Num(); ++i)
	{
		const double D = FVector2D::DistSquared(P, FVector2D(Points[i].X, Points[i].Y));
		if (D <= BestDist)
		{
			BestDist = D;
			Best = PointIds[i];
		}
	}
	return Best;
}

FBox2D AGeomVisualizer::GetBounds() const
{
	FBox2D Box(ForceInit);
	for (const FVector& P : Points)
	{
		Box += FVector2D(P.X, P.Y);
	}
	return Box;
}

// ---------------------------------------------------------------- Réglages

void AGeomVisualizer::SetAlgorithm(EGeomAlgorithm InAlgorithm)
{
	Algorithm = InAlgorithm;
	Recompute();
}

void AGeomVisualizer::SetLayer(const FString& Name, bool bVisible)
{
	Layers.FindOrAdd(Name) = bVisible;
}

void AGeomVisualizer::SetKernelMode(bool bEnabled)
{
	bKernelMode = bEnabled;
}

void AGeomVisualizer::SetOverlay(const FString& Name, const FString& PackedSegments, const FColor& Color)
{
	// Format : "x1,y1,x2,y2;x1,y1,x2,y2;..." ; chaîne vide = calque retiré
	if (PackedSegments.IsEmpty())
	{
		Overlays.Remove(Name);
		return;
	}
	FGeomOverlay& Overlay = Overlays.FindOrAdd(Name);
	Overlay.Segments.Reset();
	Overlay.Color = Color;

	TArray<FString> Items;
	PackedSegments.ParseIntoArray(Items, TEXT(";"));
	for (const FString& Item : Items)
	{
		TArray<FString> V;
		Item.ParseIntoArray(V, TEXT(","));
		if (V.Num() == 4)
		{
			Overlay.Segments.Emplace(FVector2D(FCString::Atod(*V[0]), FCString::Atod(*V[1])),
			                         FVector2D(FCString::Atod(*V[2]), FCString::Atod(*V[3])));
		}
	}
}

// ---------------------------------------------------------------- Appel des algorithmes

void AGeomVisualizer::ClearResults()
{
	Hull.Reset();
	Vertices.Reset();
	Edges.Reset();
	Faces.Reset();
	ResultTriangles.Reset();
	ResultEdges.Reset();
	InvalidFaces = 0;
	InvalidEdges = 0;
	bHasTriangulation = false;
}

void AGeomVisualizer::Recompute()
{
	ClearResults();
	LastMs = 0.0;

	if (Points.Num() < 3)
	{
		LastCall = TEXT("");
		OnResultChanged.Broadcast();
		return;
	}

	const double T0 = FPlatformTime::Seconds();
	switch (Algorithm)
	{
	case EGeomAlgorithm::Jarvis:
		LastCall = TEXT("ComputeJarvisMarch");
		Hull = UConvexHullAlgorithms::ComputeJarvisMarch(Points);
		break;

	case EGeomAlgorithm::Graham:
		LastCall = TEXT("ComputeGrahamScan");
		Hull = UConvexHullAlgorithms::ComputeGrahamScan(Points);
		break;

	case EGeomAlgorithm::Incremental:
		LastCall = TEXT("IncrementalTriangulation2D");
		UDelaunayTriangulation::IncrementalTriangulation2D(Points, Vertices, Edges, Faces);
		bHasTriangulation = true;
		break;

	case EGeomAlgorithm::Delaunay:
		LastCall = TEXT("IncrementalTriangulation2D + FlipEdgesToDelaunay");
		UDelaunayTriangulation::IncrementalTriangulation2D(Points, Vertices, Edges, Faces);
		UDelaunayTriangulation::FlipEdgesToDelaunay(Edges, Faces);
		bHasTriangulation = true;
		break;
	}
	LastMs = (FPlatformTime::Seconds() - T0) * 1000.0;

	if (bHasTriangulation)
	{
		ExtractTriangulation();
	}
	OnResultChanged.Broadcast();
}

bool AGeomVisualizer::IsValidVertex(const FGeomVertex* V) const
{
	// Les faces et arêtes pointent dans le tableau Vertices : on vérifie que le
	// pointeur y est bien, pour signaler un pointeur invalide au lieu de planter.
	if (!V || Vertices.Num() == 0)
	{
		return false;
	}
	const FGeomVertex* Begin = Vertices.GetData();
	return V >= Begin && V < Begin + Vertices.Num();
}

void AGeomVisualizer::ExtractTriangulation()
{
	ResultTriangles.Reset();
	ResultEdges.Reset();
	InvalidFaces = 0;
	InvalidEdges = 0;

	for (const FGeomFace& Face : Faces)
	{
		if (IsValidVertex(Face.Vertices[0]) && IsValidVertex(Face.Vertices[1]) && IsValidVertex(Face.Vertices[2]))
		{
			ResultTriangles.Add({ Face.Vertices[0]->Position, Face.Vertices[1]->Position, Face.Vertices[2]->Position });
		}
		else
		{
			++InvalidFaces;
		}
	}
	for (const FGeomEdge& Edge : Edges)
	{
		if (IsValidVertex(Edge.V1) && IsValidVertex(Edge.V2))
		{
			ResultEdges.Emplace(Edge.V1->Position, Edge.V2->Position);
		}
		else
		{
			++InvalidEdges;
		}
	}
}

int32 AGeomVisualizer::FindVertexId(const FVector& P) const
{
	for (const FGeomVertex& V : Vertices)
	{
		if (FVector::DistSquared2D(V.Position, P) < 1.0)
		{
			return V.ID;
		}
	}
	return INDEX_NONE;
}

bool AGeomVisualizer::IsLayerOn(const FString& Name) const
{
	const bool* Value = Layers.Find(Name);
	return Value && *Value;
}

void AGeomVisualizer::NotifyPoints()
{
	OnPointsChanged.Broadcast();
}

// ---------------------------------------------------------------- JSON pour la page

FString AGeomVisualizer::PointsToJson() const
{
	FString Out = TEXT("{\"points\":[");
	for (int32 i = 0; i < Points.Num(); ++i)
	{
		Out += FString::Printf(TEXT("%s{\"id\":%d,\"x\":%.2f,\"y\":%.2f}"), i ? TEXT(",") : TEXT(""), PointIds[i], Points[i].X, Points[i].Y);
	}
	Out += TEXT("]}");
	return Out;
}

FString AGeomVisualizer::ResultToJson() const
{
	const bool bTri = Algorithm == EGeomAlgorithm::Incremental || Algorithm == EGeomAlgorithm::Delaunay;
	static const TCHAR* Names[] = { TEXT("jarvis"), TEXT("graham"), TEXT("incr"), TEXT("delaunay") };

	FString Out = FString::Printf(TEXT("{\"algo\":\"%s\",\"call\":\"%s\",\"ms\":%.4f,\"n\":%d,\"kind\":\"%s\",\"kernel\":%s"),
		Names[(int32)Algorithm], *LastCall, LastMs, Points.Num(), bTri ? TEXT("tri") : TEXT("hull"),
		bKernelMode ? TEXT("true") : TEXT("false"));

	Out += TEXT(",\"hull\":[");
	for (int32 i = 0; i < Hull.Num(); ++i)
	{
		Out += FString::Printf(TEXT("%s[%.2f,%.2f]"), i ? TEXT(",") : TEXT(""), Hull[i].X, Hull[i].Y);
	}
	Out += TEXT("],\"tris\":[");
	for (int32 i = 0; i < ResultTriangles.Num(); ++i)
	{
		const TArray<FVector>& T = ResultTriangles[i];
		Out += FString::Printf(TEXT("%s[%.2f,%.2f,%.2f,%.2f,%.2f,%.2f]"), i ? TEXT(",") : TEXT(""), T[0].X, T[0].Y, T[1].X, T[1].Y, T[2].X, T[2].Y);
	}
	Out += TEXT("],\"edges\":[");
	for (int32 i = 0; i < ResultEdges.Num(); ++i)
	{
		const TPair<FVector, FVector>& E = ResultEdges[i];
		Out += FString::Printf(TEXT("%s[%.2f,%.2f,%.2f,%.2f]"), i ? TEXT(",") : TEXT(""), E.Key.X, E.Key.Y, E.Value.X, E.Value.Y);
	}
	Out += FString::Printf(TEXT("],\"counts\":{\"vertices\":%d,\"edges\":%d,\"faces\":%d,\"invalidFaces\":%d,\"invalidEdges\":%d}}"),
		Vertices.Num(), Edges.Num(), Faces.Num(), InvalidFaces, InvalidEdges);
	return Out;
}

// ---------------------------------------------------------------- Dessin (DrawDebug, hors Shipping)

void AGeomVisualizer::DrawLine(const FVector2D& A, const FVector2D& B, const FColor& Color, double WidthPx, double Z, uint8 Priority) const
{
	DrawDebugLine(GetWorld(), FVector(A.X, A.Y, Z), FVector(B.X, B.Y, Z), Color, false, -1.f, Priority, static_cast<float>(WidthPx * PixelSize));
}

void AGeomVisualizer::DrawGrid() const
{
	FBox2D Box = GetBounds();
	if (!Box.bIsValid)
	{
		Box = FBox2D(FVector2D(-2000.0, -1500.0), FVector2D(2000.0, 1500.0));
	}
	const double Margin = 3000.0;
	struct FLevel { double Step; FColor Color; };
	const FLevel Levels[] = { { 100.0, GeomColors::Grid100 }, { 500.0, GeomColors::Grid500 }, { 1000.0, GeomColors::Grid1000 } };

	for (const FLevel& L : Levels)
	{
		if (L.Step / PixelSize < 8.0) continue; // trop serré à ce zoom
		const double X0 = FMath::FloorToDouble((Box.Min.X - Margin) / L.Step) * L.Step;
		const double Y0 = FMath::FloorToDouble((Box.Min.Y - Margin) / L.Step) * L.Step;
		const double X1 = Box.Max.X + Margin, Y1 = Box.Max.Y + Margin;
		for (double X = X0; X <= X1; X += L.Step) DrawLine(FVector2D(X, Y0), FVector2D(X, Y1), L.Color, 1.0, GeomDepth::Grid, SDPG_World);
		for (double Y = Y0; Y <= Y1; Y += L.Step) DrawLine(FVector2D(X0, Y), FVector2D(X1, Y), L.Color, 1.0, GeomDepth::Grid, SDPG_World);
	}
	DrawLine(FVector2D(0, 0), FVector2D(Box.Max.X + Margin, 0), GeomColors::AxisX, 1.5, GeomDepth::Grid - 1, SDPG_World);
	DrawLine(FVector2D(0, 0), FVector2D(0, Box.Max.Y + Margin), GeomColors::AxisY, 1.5, GeomDepth::Grid - 1, SDPG_World);
}

void AGeomVisualizer::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UWorld* World = GetWorld();
	if (!World) return;

	if (IsLayerOn(TEXT("grid"))) DrawGrid();

	// Triangulation : faces si présentes, sinon arêtes
	if (IsLayerOn(TEXT("edges")))
	{
		for (const TArray<FVector>& T : ResultTriangles)
		{
			for (int32 i = 0; i < 3; ++i)
			{
				DrawLine(FVector2D(T[i]), FVector2D(T[(i + 1) % 3]), GeomColors::Tri, 1.3, GeomDepth::Triangles, SDPG_World);
			}
		}
		if (ResultTriangles.Num() == 0)
		{
			for (const TPair<FVector, FVector>& E : ResultEdges)
			{
				DrawLine(FVector2D(E.Key), FVector2D(E.Value), GeomColors::Tri, 1.3, GeomDepth::Triangles, SDPG_World);
			}
		}
	}

	// Calques calculés par la page (référence, Voronoï, cercles circonscrits)
	for (const TPair<FString, FGeomOverlay>& Pair : Overlays)
	{
		for (const TPair<FVector2D, FVector2D>& S : Pair.Value.Segments)
		{
			DrawLine(S.Key, S.Value, Pair.Value.Color, 1.0, GeomDepth::Overlay, SDPG_World);
		}
	}

	// Enveloppe convexe
	if (IsLayerOn(TEXT("hull")) && Hull.Num() > 1)
	{
		for (int32 i = 0; i < Hull.Num(); ++i)
		{
			const bool bClose = i + 1 < Hull.Num() || Hull.Num() > 2;
			if (!bClose) break;
			DrawLine(FVector2D(Hull[i]), FVector2D(Hull[(i + 1) % Hull.Num()]), GeomColors::Hull, 2.5, GeomDepth::Hull, SDPG_Foreground);
		}
	}

	// Points, étiquettes, survol
	const bool bLabels = IsLayerOn(TEXT("labels")) && Points.Num() <= 80;
	for (int32 i = 0; i < Points.Num(); ++i)
	{
		const FVector P(Points[i].X, Points[i].Y, GeomDepth::Points);
		const bool bOnHull = Hull.ContainsByPredicate([&](const FVector& H) { return FVector::DistSquared2D(H, Points[i]) < 1.0; });
		DrawDebugPoint(World, P, bOnHull ? 9.f : 7.f, bOnHull ? GeomColors::Hull : GeomColors::Point, false, -1.f, SDPG_Foreground);
		if (bLabels)
		{
			DrawDebugString(World, P + FVector(8.0 * PixelSize, 8.0 * PixelSize, 0.0), FString::Printf(TEXT("P%d"), PointIds[i]), nullptr, GeomColors::Label, 0.f, false, 0.9f);
		}
		if (PointIds[i] == HoverId || PointIds[i] == DragId)
		{
			DrawDebugCircle(World, P, static_cast<float>(12.0 * PixelSize), 32, PointIds[i] == DragId ? GeomColors::Drag : GeomColors::Hover,
				false, -1.f, SDPG_Foreground, static_cast<float>(1.5 * PixelSize), FVector(1, 0, 0), FVector(0, 1, 0), false);
		}
	}
}
