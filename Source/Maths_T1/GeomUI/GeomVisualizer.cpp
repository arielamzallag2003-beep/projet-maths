#include "GeomVisualizer.h"
#include "ConvexHullAlgorithms.h"
#include "DelaunayTriangulation.h"
#include "GeomCanvas.h"
#include "Maths_T1.h"

AGeomVisualizer::AGeomVisualizer()
{
	PrimaryActorTick.bCanEverTick = false; // le dessin se fait dans le HUD (Paint)
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
		++ResultSeq;
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
			++ResultSeq;
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
	const bool bMoved = !FVector2D(Points[Index]).Equals(P, 0.01);
	Points[Index] = FVector(P.X, P.Y, 0.0);
	if (bRecompute && (bMoved || bPendingMove))
	{
		bPendingMove = false;
		NotifyPoints();
		Recompute();
	}
	else if (bMoved)
	{
		bPendingMove = true; // déplacé sans recalcul : il faudra recalculer au prochain appel
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
	VoronoiCells.Reset();
	VoronoiEdges.Reset();
	VoronoiVertices.Reset();
	VoronoiVertexCount = 0;
	VoronoiBoundsMin = VoronoiBoundsMax = FVector::ZeroVector;
}

void AGeomVisualizer::Recompute()
{
	ClearResults();
	LastMs = 0.0;

	if (Points.Num() < 3)
	{
		LastCall = TEXT("");
		++ResultSeq;
		OnResultChanged.Broadcast();
		RebuildConstruction();
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

	case EGeomAlgorithm::Voronoi:
		LastCall = TEXT("ComputeVoronoiHalfPlanes");
		UVoronoiDiagram::ComputeVoronoiHalfPlanes(Points, 0.0, VoronoiCells, VoronoiBoundsMin, VoronoiBoundsMax);
		break;
	}
	LastMs = (FPlatformTime::Seconds() - T0) * 1000.0;
	++ResultSeq;

	if (bHasTriangulation)
	{
		ExtractTriangulation();
	}
	if (Algorithm == EGeomAlgorithm::Voronoi)
	{
		// Hors chronométrage : on ne mesure que le calcul des régions
		VoronoiEdges = UVoronoiDiagram::ExtractVoronoiEdges(VoronoiCells);
		VoronoiVertices = UVoronoiDiagram::ExtractVoronoiVertices(VoronoiCells, VoronoiBoundsMin, VoronoiBoundsMax, 0.01);
		VoronoiVertexCount = VoronoiVertices.Num();
	}
	OnResultChanged.Broadcast();
	RebuildConstruction();
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

namespace
{
	// Nombre JSON sûr : un NaN ou un infini renvoyé par un algorithme casserait le JSON côté page.
	FString JsonNum(double V, int32& NonFinite)
	{
		if (!FMath::IsFinite(V))
		{
			++NonFinite;
			return TEXT("0");
		}
		return FString::Printf(TEXT("%.2f"), V);
	}

	// Les noms d'appel sont fixes, mais on échappe quand même pour ne jamais produire un JSON invalide
	FString JsonString(const FString& In)
	{
		return In.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\""));
	}
}

FString AGeomVisualizer::PointsToJson() const
{
	int32 Ignored = 0;
	FString Out = TEXT("{\"points\":[");
	for (int32 i = 0; i < Points.Num(); ++i)
	{
		Out += FString::Printf(TEXT("%s{\"id\":%d,\"x\":%s,\"y\":%s}"), i ? TEXT(",") : TEXT(""), PointIds[i],
			*JsonNum(Points[i].X, Ignored), *JsonNum(Points[i].Y, Ignored));
	}
	Out += TEXT("]}");
	return Out;
}

FString AGeomVisualizer::ResultToJson() const
{
	const bool bTri = Algorithm == EGeomAlgorithm::Incremental || Algorithm == EGeomAlgorithm::Delaunay;
	const bool bVoronoi = Algorithm == EGeomAlgorithm::Voronoi;
	static const TCHAR* Names[] = { TEXT("jarvis"), TEXT("graham"), TEXT("incr"), TEXT("delaunay"), TEXT("voronoi") };
	int32 NonFinite = 0;

	FString Body = TEXT("\"hull\":[");
	for (int32 i = 0; i < Hull.Num(); ++i)
	{
		Body += FString::Printf(TEXT("%s[%s,%s]"), i ? TEXT(",") : TEXT(""), *JsonNum(Hull[i].X, NonFinite), *JsonNum(Hull[i].Y, NonFinite));
	}
	Body += TEXT("],\"tris\":[");
	for (int32 i = 0; i < ResultTriangles.Num(); ++i)
	{
		const TArray<FVector>& T = ResultTriangles[i];
		Body += FString::Printf(TEXT("%s[%s,%s,%s,%s,%s,%s]"), i ? TEXT(",") : TEXT(""),
			*JsonNum(T[0].X, NonFinite), *JsonNum(T[0].Y, NonFinite), *JsonNum(T[1].X, NonFinite),
			*JsonNum(T[1].Y, NonFinite), *JsonNum(T[2].X, NonFinite), *JsonNum(T[2].Y, NonFinite));
	}
	Body += TEXT("],\"edges\":[");
	for (int32 i = 0; i < ResultEdges.Num(); ++i)
	{
		const TPair<FVector, FVector>& E = ResultEdges[i];
		Body += FString::Printf(TEXT("%s[%s,%s,%s,%s]"), i ? TEXT(",") : TEXT(""),
			*JsonNum(E.Key.X, NonFinite), *JsonNum(E.Key.Y, NonFinite), *JsonNum(E.Value.X, NonFinite), *JsonNum(E.Value.Y, NonFinite));
	}
	Body += TEXT("]");

	// Voronoï. cells : [id, touche le cadre, x du site, y du site, x1, y1, x2, y2...] (une par site distinct)
	// vedges : [x1, y1, x2, y2, xA, yA, xB, yB] (extrémités, puis les deux sites séparés)
	int32 DistinctCells = 0;
	if (bVoronoi)
	{
		Body += TEXT(",\"cells\":[");
		for (const FVoronoiCell& Cell : VoronoiCells)
		{
			if (Cell.DuplicateOf != INDEX_NONE || !PointIds.IsValidIndex(Cell.SiteIndex))
			{
				continue;
			}
			Body += FString::Printf(TEXT("%s[%d,%d,%s,%s"), DistinctCells ? TEXT(",") : TEXT(""), PointIds[Cell.SiteIndex],
				Cell.bTouchesBounds ? 1 : 0, *JsonNum(Cell.Site.X, NonFinite), *JsonNum(Cell.Site.Y, NonFinite));
			for (const FVector& V : Cell.Polygon)
			{
				Body += FString::Printf(TEXT(",%s,%s"), *JsonNum(V.X, NonFinite), *JsonNum(V.Y, NonFinite));
			}
			Body += TEXT("]");
			++DistinctCells;
		}
		Body += TEXT("],\"vedges\":[");
		for (int32 i = 0; i < VoronoiEdges.Num(); ++i)
		{
			const FVoronoiEdge& E = VoronoiEdges[i];
			const FVector SA = VoronoiCells.IsValidIndex(E.SiteA) ? VoronoiCells[E.SiteA].Site : FVector::ZeroVector;
			const FVector SB = VoronoiCells.IsValidIndex(E.SiteB) ? VoronoiCells[E.SiteB].Site : FVector::ZeroVector;
			Body += FString::Printf(TEXT("%s[%s,%s,%s,%s,%s,%s,%s,%s]"), i ? TEXT(",") : TEXT(""),
				*JsonNum(E.A.X, NonFinite), *JsonNum(E.A.Y, NonFinite), *JsonNum(E.B.X, NonFinite), *JsonNum(E.B.Y, NonFinite),
				*JsonNum(SA.X, NonFinite), *JsonNum(SA.Y, NonFinite), *JsonNum(SB.X, NonFinite), *JsonNum(SB.Y, NonFinite));
		}
		Body += FString::Printf(TEXT("],\"bounds\":[%s,%s,%s,%s]"),
			*JsonNum(VoronoiBoundsMin.X, NonFinite), *JsonNum(VoronoiBoundsMin.Y, NonFinite),
			*JsonNum(VoronoiBoundsMax.X, NonFinite), *JsonNum(VoronoiBoundsMax.Y, NonFinite));
	}

	// Pour Voronoï, les compteurs décrivent le diagramme : régions, arêtes, sommets
	const int32 CountVertices = bVoronoi ? VoronoiVertexCount : Vertices.Num();
	const int32 CountEdges = bVoronoi ? VoronoiEdges.Num() : Edges.Num();
	const int32 CountFaces = bVoronoi ? DistinctCells : Faces.Num();

	// seq : numéro du calcul ; firstId : identifiant du premier point (la page s'en sert pour
	// reconnaître la réponse à une requête précise, par exemple pendant la batterie de tests)
	return FString::Printf(
		TEXT("{\"algo\":\"%s\",\"call\":\"%s\",\"ms\":%.4f,\"n\":%d,\"seq\":%d,\"firstId\":%d,\"kind\":\"%s\",\"kernel\":%s,%s,")
		TEXT("\"counts\":{\"vertices\":%d,\"edges\":%d,\"faces\":%d,\"invalidFaces\":%d,\"invalidEdges\":%d,\"nonFinite\":%d}}"),
		Names[(int32)Algorithm], *JsonString(LastCall), FMath::IsFinite(LastMs) ? LastMs : 0.0, Points.Num(), ResultSeq,
		PointIds.Num() ? PointIds[0] : 0, bVoronoi ? TEXT("voronoi") : bTri ? TEXT("tri") : TEXT("hull"), bKernelMode ? TEXT("true") : TEXT("false"), *Body,
		CountVertices, CountEdges, CountFaces, InvalidFaces, InvalidEdges, NonFinite);
}

// ---------------------------------------------------------------- Construction pas à pas

void AGeomVisualizer::SetConstruction(bool bOn, int32 SiteId, int32 Step)
{
	const bool bNewSite = SiteId > 0 && SiteId != ConstructionSiteId;
	bConstruction = bOn;
	if (SiteId > 0)
	{
		ConstructionSiteId = SiteId;
	}
	ConstructionStep = bNewSite ? 0 : FMath::Max(0, Step);
	RebuildConstruction();
}

void AGeomVisualizer::OnPointPressed(int32 Id)
{
	if (bConstruction && Id != INDEX_NONE && Id != ConstructionSiteId)
	{
		ConstructionSiteId = Id;
		ConstructionStep = 0;
		RebuildConstruction();
	}
}

void AGeomVisualizer::RebuildConstruction()
{
	ConstructionSteps.Reset();
	ConstructionIndex = INDEX_NONE;
	ConstructionSkipped = 0;

	if (bConstruction && Algorithm == EGeomAlgorithm::Voronoi && Points.Num() >= 3)
	{
		ConstructionIndex = PointIds.IndexOfByKey(ConstructionSiteId);
		if (ConstructionIndex == INDEX_NONE)
		{
			ConstructionIndex = 0; // site supprimé ou jamais choisi : le premier point
			ConstructionSiteId = PointIds[0];
		}
		FVector BoundsMin, BoundsMax;
		UVoronoiDiagram::ComputeVoronoiCellSteps(Points, ConstructionIndex, 0.0, ConstructionSteps, ConstructionSkipped, BoundsMin, BoundsMax);
		ConstructionStep = FMath::Clamp(ConstructionStep, 0, FMath::Max(0, ConstructionSteps.Num() - 1));
	}
	OnConstructionChanged.Broadcast();
}

FString AGeomVisualizer::ConstructionToJson() const
{
	const bool bOn = ConstructionSteps.Num() > 0;
	const FVoronoiCellStep* Step = ConstructionSteps.IsValidIndex(ConstructionStep) ? &ConstructionSteps[ConstructionStep] : nullptr;
	const int32 Neighbor = Step && PointIds.IsValidIndex(Step->Neighbor) ? PointIds[Step->Neighbor] : -1;
	FString Cuts;
	for (int32 k = 0; k < ConstructionSteps.Num(); ++k)
	{
		if (ConstructionSteps[k].bCut)
		{
			Cuts += FString::Printf(TEXT("%s%d"), Cuts.IsEmpty() ? TEXT("") : TEXT(","), k);
		}
	}
	return FString::Printf(TEXT("{\"on\":%s,\"site\":%d,\"step\":%d,\"total\":%d,\"neighbor\":%d,\"cut\":%s,\"skipped\":%d,\"cutSteps\":[%s]}"),
		bOn ? TEXT("true") : TEXT("false"), ConstructionSiteId, ConstructionStep, ConstructionSteps.Num(), Neighbor,
		Step && Step->bCut ? TEXT("true") : TEXT("false"), ConstructionSkipped, *Cuts);
}

// ---------------------------------------------------------------- Dessin (Canvas du HUD)
// Mêmes formes, couleurs et tailles que la simulation navigateur (Content/UI/Web/sim.js) :
// on règle l'apparence dans Chrome, puis on la reporte ici.

namespace GeomStyle
{
	using Gfx = FGeomCanvas;

	/** Teinte sourde par région, du cyan au rose (jamais de jaune ni d'olive, ternes sur fond sombre). */
	FLinearColor CellColor(int32 Index, float Alpha)
	{
		const double H = 150.0 + FMath::Fmod(Index * 137.508, 180.0), S = 0.35, V = 0.9;
		auto F = [&](double N)
		{
			const double K = FMath::Fmod(N + H / 60.0, 6.0);
			return static_cast<uint8>(FMath::RoundToInt((V - V * S * FMath::Max(0.0, FMath::Min3(K, 4.0 - K, 1.0))) * 255.0));
		};
		FLinearColor Out = FLinearColor::FromSRGBColor(FColor(F(5.0), F(3.0), F(1.0)));
		Out.A = Alpha;
		return Out;
	}
}

void AGeomVisualizer::Paint(UCanvas* InCanvas)
{
	using namespace GeomStyle;
	FGeomCanvas Cv(InCanvas);
	if (!Cv.IsValid())
	{
		return;
	}
	const float K = FMath::Clamp(DrawScale, 0.75f, 2.f); // tailles en pixels, suivant le DPI
	const FVector2D Screen = Cv.GetSize();
	auto S = [&Cv](const FVector& P) { return Cv.ToScreen(P); };

	// Couleurs (sRGB)
	const FLinearColor Teal = Gfx::Hex(TEXT("#7FE3DA")), TealDeep = Gfx::Hex(TEXT("#4FD1C5")), TealPale = Gfx::Hex(TEXT("#D9FBF7"));
	const FLinearColor White = Gfx::Hex(TEXT("#EEF3F9")), Ink = Gfx::Hex(TEXT("#8592A5")), Shadow = Gfx::Hex(TEXT("#0A0E14"));
	const FLinearColor HullC = Gfx::Hex(TEXT("#FFB547")), TriC = Gfx::Hex(TEXT("#5FA8FF")), Violet = Gfx::Hex(TEXT("#C58BFF"));
	const FLinearColor Red = Gfx::Hex(TEXT("#FF7A59")), Pink = Gfx::Hex(TEXT("#FF5D7A"));

	const FLinearColor Paper = Gfx::Hex(TEXT("#0D1117"));
	Cv.Background(Paper, Paper); // fond uni, comme une feuille

	// ---- Grille et axes
	if (IsLayerOn(TEXT("grid")))
	{
		const FVector2D W0 = Cv.ToWorld(FVector2D::ZeroVector), W1 = Cv.ToWorld(Screen);
		const FVector2D Lo(FMath::Min(W0.X, W1.X), FMath::Min(W0.Y, W1.Y)), Hi(FMath::Max(W0.X, W1.X), FMath::Max(W0.Y, W1.Y));
		struct FLevel { double Step; float Alpha; };
		const FLevel Levels[] = { { 100.0, 0.04f }, { 500.0, 0.07f }, { 1000.0, 0.1f } };
		for (const FLevel& L : Levels)
		{
			if (L.Step * Cv.PixelsPerCm() < 8.0 || (Hi.X - Lo.X) / L.Step > 600.0) continue; // trop serré à ce zoom
			const FLinearColor G(1.f, 1.f, 1.f, L.Alpha);
			for (double X = FMath::FloorToDouble(Lo.X / L.Step) * L.Step; X <= Hi.X; X += L.Step)
			{
				const double Sx = Cv.ToScreen(FVector2D(X, 0.0)).X;
				Cv.Line(FVector2D(Sx, 0.0), FVector2D(Sx, Screen.Y), 1.f, G);
			}
			for (double Y = FMath::FloorToDouble(Lo.Y / L.Step) * L.Step; Y <= Hi.Y; Y += L.Step)
			{
				const double Sy = Cv.ToScreen(FVector2D(0.0, Y)).Y;
				Cv.Line(FVector2D(0.0, Sy), FVector2D(Screen.X, Sy), 1.f, G);
			}
		}
		const FVector2D O = Cv.ToScreen(FVector2D::ZeroVector);
		Cv.Line(O, FVector2D(Screen.X, O.Y), 1.f * K, Gfx::Hex(TEXT("#FF5D5D"), 0.4f));
		Cv.Line(O, FVector2D(O.X, 0.0), 1.f * K, Gfx::Hex(TEXT("#6FDC95"), 0.4f));
	}

	const int32 Focus = DragId != INDEX_NONE ? DragId : HoverId;
	const int32 FocusIndex = Focus != INDEX_NONE ? PointIds.IndexOfByKey(Focus) : INDEX_NONE;
	const bool bVoronoi = Algorithm == EGeomAlgorithm::Voronoi && VoronoiCells.Num() == Points.Num() && Points.Num() > 0;
	const FVoronoiCellStep* Step = ConstructionSteps.IsValidIndex(ConstructionStep) ? &ConstructionSteps[ConstructionStep] : nullptr;
	const bool bBuild = bVoronoi && Step && Points.IsValidIndex(ConstructionIndex);
	const float Dim = bBuild ? 0.35f : 1.f;
	auto Canonical = [this](int32 i) { return VoronoiCells.IsValidIndex(i) && VoronoiCells[i].DuplicateOf != INDEX_NONE ? VoronoiCells[i].DuplicateOf : i; };

	struct FTip { FVector2D At; FString Text; FLinearColor Color; };
	TArray<FTip> Tips;

	// ---- Voronoï
	int32 HotCell = INDEX_NONE;
	TSet<int32> Near;
	if (bVoronoi)
	{
		if (FocusIndex != INDEX_NONE && !bBuild)
		{
			HotCell = Canonical(FocusIndex);
			for (const int32 j : VoronoiCells[HotCell].EdgeNeighbors)
			{
				if (j != INDEX_NONE) Near.Add(j);
			}
		}

		// Régions : aplat léger, une teinte par région
		TArray<FVector2D> Poly;
		for (int32 i = 0; i < VoronoiCells.Num(); ++i)
		{
			const FVoronoiCell& Cell = VoronoiCells[i];
			if (Cell.DuplicateOf != INDEX_NONE) continue;
			Poly.Reset();
			for (const FVector& V : Cell.Polygon) Poly.Add(S(V));
			const bool bHot = i == HotCell, bNear = Near.Contains(i);
			Cv.ConvexFill(Poly, CellColor(i, (bHot ? 0.24f : bNear ? 0.16f : 0.1f) * Dim));
		}

		if (IsLayerOn(TEXT("edges")))
		{
			// Arêtes : un trait net, jusqu'au cadre
			for (const FVoronoiEdge& E : VoronoiEdges)
			{
				Cv.Line(S(E.A), S(E.B), 1.4f * K, Gfx::WithAlpha(Teal, 0.9f * Dim));
			}
			// Cadre qui coupe les régions infinies, en pointillés
			const FVector2D F0 = S(VoronoiBoundsMin), F2 = S(VoronoiBoundsMax), F1(F2.X, F0.Y), F3(F0.X, F2.Y);
			const FLinearColor Frame(1.f, 1.f, 1.f, 0.14f);
			Cv.DashedLine(F0, F1, 1.f * K, Frame, 6.f * K, 6.f * K);
			Cv.DashedLine(F1, F2, 1.f * K, Frame, 6.f * K, 6.f * K);
			Cv.DashedLine(F2, F3, 1.f * K, Frame, 6.f * K, 6.f * K);
			Cv.DashedLine(F3, F0, 1.f * K, Frame, 6.f * K, 6.f * K);
			for (const FVector& V : VoronoiVertices)
			{
				Cv.Disc(S(V), 1.8f * K, Gfx::WithAlpha(Teal, 0.9f * Dim));
			}
		}

		// Survol d'un site : sa région en surbrillance, liens vers ses voisins (arêtes de Delaunay)
		if (HotCell != INDEX_NONE)
		{
			Poly.Reset();
			for (const FVector& V : VoronoiCells[HotCell].Polygon) Poly.Add(S(V));
			Cv.Polyline(Poly, true, 2.f * K, Gfx::WithAlpha(TealPale, 0.95f));
			for (const int32 j : Near)
			{
				if (VoronoiCells.IsValidIndex(j))
				{
					Cv.DashedLine(S(VoronoiCells[HotCell].Site), S(VoronoiCells[j].Site), 1.3f * K, Gfx::WithAlpha(TriC, 0.85f), 5.f * K, 4.f * K);
				}
			}
		}

		// Survol d'un sommet : son cercle vide passe par au moins 3 sites
		if (FocusIndex == INDEX_NONE && !bBuild && bCursorValid)
		{
			const FVector2D Cursor = Cv.ToScreen(CursorWorld);
			const FVector* Best = nullptr;
			double BestDist = 12.0 * K;
			for (const FVector& V : VoronoiVertices)
			{
				const double D = FVector2D::Distance(S(V), Cursor);
				if (D < BestDist) { BestDist = D; Best = &V; }
			}
			if (Best)
			{
				double Radius = TNumericLimits<double>::Max();
				for (const FVector& P : Points) Radius = FMath::Min(Radius, FVector::Dist2D(P, *Best));
				const FVector2D Cs = S(*Best);
				Cv.Ring(Cs, static_cast<float>(Radius * Cv.PixelsPerCm()), 1.6f * K, Gfx::WithAlpha(Violet, 0.95f));
				int32 OnCircle = 0;
				for (const FVector& P : Points)
				{
					if (FVector::Dist2D(P, *Best) <= Radius * (1.0 + 1e-6) + 0.5)
					{
						++OnCircle;
						Cv.DashedLine(Cs, S(P), 1.1f * K, Gfx::WithAlpha(Violet, 0.7f), 4.f * K, 4.f * K);
					}
				}
				Cv.Disc(Cs, 4.5f * K, Violet);
				Tips.Add({ Cs + FVector2D(12.0, 8.0) * K, FString::Printf(TEXT("sommet · cercle vide · r = %.0f cm · %d sites"), Radius, OnCircle), Gfx::Hex(TEXT("#E9D8FF")) });
			}
		}

		// Construction pas à pas : région courante, partie retirée, médiatrice
		if (bBuild)
		{
			const FVector Pi = Points[ConstructionIndex];
			if (Step->Removed.Num() >= 3)
			{
				Poly.Reset();
				for (const FVector& V : Step->Removed) Poly.Add(S(V));
				Cv.ConvexFill(Poly, Gfx::WithAlpha(Pink, 0.18f));
				for (int32 k = 0; k < Poly.Num(); ++k) Cv.DashedLine(Poly[k], Poly[(k + 1) % Poly.Num()], 1.2f * K, Gfx::WithAlpha(Pink, 0.6f), 5.f * K, 4.f * K);
			}
			Poly.Reset();
			for (const FVector& V : Step->Polygon) Poly.Add(S(V));
			Cv.ConvexFill(Poly, Gfx::WithAlpha(TealDeep, 0.2f));
			Cv.Polyline(Poly, true, 2.2f * K, Teal);
			if (Points.IsValidIndex(Step->Neighbor))
			{
				const FVector Pj = Points[Step->Neighbor];
				const FVector2D Si = S(Pi), Sj = S(Pj), Ms = (Si + Sj) * 0.5, D = Sj - Si;
				Cv.InfiniteLine(Ms, FVector2D(-D.Y, D.X), 1.6f * K, Gfx::WithAlpha(Red, Step->bCut ? 0.95f : 0.45f), 9.f * K, 6.f * K);
				Cv.DashedLine(Si, Sj, 1.f * K, Gfx::WithAlpha(Red, 0.5f), 3.f * K, 4.f * K);
				Cv.Ring(Sj, 10.f * K, 2.f * K, Red);
				Cv.Disc(Ms, 3.f * K, Red);
				const int32 IdJ = PointIds.IsValidIndex(Step->Neighbor) ? PointIds[Step->Neighbor] : -1;
				Tips.Add({ Ms + FVector2D(8.0, 6.0) * K, FString::Printf(TEXT("médiatrice de [P%d P%d]%s"), ConstructionSiteId, IdJ, Step->bCut ? TEXT("") : TEXT(" · ne coupe rien")), Gfx::Hex(TEXT("#FFD2C4")) });
			}
			Cv.Ring(S(Pi), 12.f * K, 2.f * K, Teal);
		}
	}

	// ---- Triangulation
	if (IsLayerOn(TEXT("edges")) && !bVoronoi)
	{
		TArray<FVector2D> T;
		for (const TArray<FVector>& Tri : ResultTriangles)
		{
			T.Reset();
			for (const FVector& V : Tri) T.Add(S(V));
			Cv.ConvexFill(T, Gfx::WithAlpha(TriC, 0.04f));
			Cv.Polyline(T, true, 1.2f * K, Gfx::WithAlpha(TriC, 0.9f));
		}
		if (ResultTriangles.Num() == 0)
		{
			for (const TPair<FVector, FVector>& E : ResultEdges)
			{
				Cv.Line(S(E.Key), S(E.Value), 1.2f * K, Gfx::WithAlpha(TriC, 0.9f));
			}
		}
	}

	// ---- Calques calculés par la page (référence, cercles, différences...)
	for (const TPair<FString, FGeomOverlay>& Pair : Overlays)
	{
		const FLinearColor Color = Gfx::WithAlpha(FLinearColor::FromSRGBColor(Pair.Value.Color), 0.9f);
		for (const TPair<FVector2D, FVector2D>& Seg : Pair.Value.Segments)
		{
			Cv.Line(Cv.ToScreen(Seg.Key), Cv.ToScreen(Seg.Value), 1.2f * K, Color);
		}
	}

	// ---- Enveloppe convexe : voile léger, trait
	if (IsLayerOn(TEXT("hull")) && Hull.Num() > 1)
	{
		TArray<FVector2D> H;
		for (const FVector& V : Hull) H.Add(S(V));
		if (H.Num() > 2) Cv.ConvexFill(H, Gfx::WithAlpha(HullC, 0.05f));
		const int32 Count = H.Num() > 2 ? H.Num() : 1;
		for (int32 i = 0; i < Count; ++i)
		{
			Cv.Line(H[i], H[(i + 1) % H.Num()], 2.f * K, HullC);
		}
	}

	// ---- Points : disque plein, fin contour sombre
	const int32 BuildNeighbor = bBuild && Points.IsValidIndex(Step->Neighbor) ? Step->Neighbor : INDEX_NONE;
	for (int32 i = 0; i < Points.Num(); ++i)
	{
		const FVector2D P = S(Points[i]);
		if (P.X < -50.0 || P.Y < -50.0 || P.X > Screen.X + 50.0 || P.Y > Screen.Y + 50.0) continue;
		const bool bOnHull = Hull.ContainsByPredicate([&](const FVector& Hv) { return FVector::DistSquared2D(Hv, Points[i]) < 1.0; });
		const float A = !bBuild || i == ConstructionIndex || i == BuildNeighbor ? 1.f : 0.45f;
		Cv.Disc(P, 4.8f * K, Gfx::WithAlpha(Paper, A));
		Cv.Disc(P, 3.6f * K, Gfx::WithAlpha(bOnHull ? HullC : White, A));
		if (PointIds[i] == DragId) Cv.Ring(P, 9.f * K, 1.5f * K, Violet);
		else if (PointIds[i] == HoverId) Cv.Ring(P, 9.f * K, 1.5f * K, FLinearColor(1.f, 1.f, 1.f, 0.9f));
	}

	// ---- Étiquettes : jamais l'une sur l'autre, le point survolé en premier
	if (IsLayerOn(TEXT("labels")) && Points.Num() <= 80)
	{
		TArray<int32> Order;
		for (int32 i = 0; i < Points.Num(); ++i) Order.Add(i);
		if (FocusIndex != INDEX_NONE) { Order.Remove(FocusIndex); Order.Insert(FocusIndex, 0); }
		TArray<FBox2D> Placed;
		for (const int32 i : Order)
		{
			const FVector2D P = S(Points[i]);
			const FString Label = FString::Printf(TEXT("P%d"), PointIds[i]);
			const FVector2D Size = Cv.TextSize(Label, K);
			const FVector2D Min = P + FVector2D(8.0, -20.0) * K;
			const FBox2D Box(Min, Min + Size);
			if (Box.Min.X > Screen.X || Box.Min.Y > Screen.Y || Box.Max.X < 0.0 || Box.Max.Y < 0.0) continue;
			const FBox2D Padded(Box.Min - FVector2D(3.0, 0.0), Box.Max + FVector2D(3.0, 0.0));
			if (Placed.ContainsByPredicate([&](const FBox2D& Q) { return Padded.Intersect(Q); })) continue;
			Placed.Add(Box);
			Cv.Text(Min, Label, i == FocusIndex ? FLinearColor::White : Gfx::WithAlpha(Ink, 0.95f), K);
		}
	}

	// ---- Bulles d'information, sur un fond sombre pour rester lisibles
	for (const FTip& Tip : Tips)
	{
		const FVector2D Size = Cv.TextSize(Tip.Text, K), Pad(6.0 * K, 3.0 * K);
		const FVector2D Lo = Tip.At - Pad, Hi = Tip.At + Size + Pad;
		Cv.ConvexFill({ Lo, FVector2D(Hi.X, Lo.Y), Hi, FVector2D(Lo.X, Hi.Y) }, Gfx::WithAlpha(Shadow, 0.85f));
		Cv.Text(Tip.At, Tip.Text, Tip.Color, K);
	}
}
