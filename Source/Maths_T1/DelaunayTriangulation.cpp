#include "DelaunayTriangulation.h"
#include "GeometryMathLibrary.h"

void UDelaunayTriangulation::IncrementalTriangulation2D(
	const TArray<FVector>& InputPoints, 
	TArray<FGeomVertex>& OutVertices, 
	TArray<FGeomEdge>& OutEdges, 
	TArray<FGeomFace>& OutFaces)
{
	// TODO: Étape 1 - Trier les points par abscisse (et ordonnée) croissante [Cours 4]

	// TODO: Étape 2 - Initialiser la triangulation avec les k premiers points colinéaires + 1 [Cours 4]

	// TODO: Étape 3 - Pour chaque nouveau point P_q+1, trouver les arêtes "vues" et ajouter les nouveaux triangles [Cours 4]
}

void UDelaunayTriangulation::FlipEdgesToDelaunay(TArray<FGeomEdge>& Edges, TArray<FGeomFace>& Faces)
{
	// TODO: Parcourir la liste des arêtes non conformes [Cours 4]
	// TODO: Tester le critère de Delaunay (IsInsideCircumcircle) sur chaque arête commune à deux faces
	// TODO: Si non conforme, inverser/basculer l'arête (S3-S4 -> S1-S2) et mettre à jour le graphe d'incidence
}

void UDelaunayTriangulation::DelaunayKernel_AddPoint(
	const FVector& NewPoint, 
	TArray<FGeomVertex>& Vertices, 
	TArray<FGeomEdge>& Edges, 
	TArray<FGeomFace>& Faces)
{
	// TODO: Cas A - Pas de triangle (sommets colinéaires) [Cours 4]
    
	// TODO: Cas B - Contient des triangles [Cours 4]
	// - Cas B1 : Le point est à l'intérieur d'un triangle -> supprimer le triangle et relier aux 3 arêtes
	// - Cas B2 : Le point est hors de la triangulation -> chercher les arêtes externes dont le cercle circonscrit contient P
}

void UDelaunayTriangulation::DelaunayKernel_RemovePoint(
	int32 VertexID, 
	TArray<FGeomVertex>& Vertices, 
	TArray<FGeomEdge>& Edges, 
	TArray<FGeomFace>& Faces)
{
	// TODO: Trouver et supprimer le sommet s, les arêtes incidentes et les triangles incidents [Cours 4]
	// TODO: Re-trianguler le trou polygonal créé (liste La2) [Cours 4]
}