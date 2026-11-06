#pragma once

#include "CoreMinimal.h"
#include "Engine/Canvas.h" // UCanvas, FCanvasUVTri

class UFont;

/**
 * Dessin 2D de la scène de test, en pixels écran, sur le Canvas du HUD.
 *
 * Tout est fait de triangles colorés (couleur par sommet, mélange translucide) :
 * traits d'épaisseur constante à l'écran avec un bord adouci d'un pixel (antialiasing),
 * disques, anneaux, polygones remplis en dégradé. Les triangles sont accumulés puis
 * envoyés en un seul appel (Flush). Contrairement à DrawDebug, ça s'affiche aussi en Shipping.
 *
 * Le passage monde -> écran est affine (caméra orthographique) : il est mesuré une fois
 * par frame avec UCanvas::Project.
 */
class FGeomCanvas
{
public:
	explicit FGeomCanvas(UCanvas* InCanvas);
	~FGeomCanvas();

	FGeomCanvas(const FGeomCanvas&) = delete;
	FGeomCanvas& operator=(const FGeomCanvas&) = delete;

	bool IsValid() const { return Canvas != nullptr; }
	FVector2D GetSize() const { return Size; }

	/** Plan XY (cm) -> écran (pixels). */
	FVector2D ToScreen(const FVector2D& World) const { return Origin + AxisX * World.X + AxisY * World.Y; }
	FVector2D ToScreen(const FVector& World) const { return ToScreen(FVector2D(World.X, World.Y)); }
	/** Écran (pixels) -> plan XY (cm). */
	FVector2D ToWorld(const FVector2D& Screen) const;
	/** Pixels par centimètre. */
	double PixelsPerCm() const { return AxisX.Size(); }

	// ---- Formes (coordonnées écran) ----

	/** Dégradé vertical sur tout l'écran. */
	void Background(const FLinearColor& Top, const FLinearColor& Bottom);
	/** Trait antialiasé, couleur qui peut varier d'un bout à l'autre. */
	void Line(const FVector2D& A, const FVector2D& B, float Width, const FLinearColor& ColorA, const FLinearColor& ColorB);
	void Line(const FVector2D& A, const FVector2D& B, float Width, const FLinearColor& Color) { Line(A, B, Width, Color, Color); }
	void DashedLine(const FVector2D& A, const FVector2D& B, float Width, const FLinearColor& Color, float Dash, float Gap);
	/** Ligne qui traverse tout l'écran, passant par P dans la direction Dir. */
	void InfiniteLine(const FVector2D& P, const FVector2D& Dir, float Width, const FLinearColor& Color, float Dash = 0.f, float Gap = 0.f);
	void Polyline(const TArray<FVector2D>& Points, bool bClosed, float Width, const FLinearColor& Color);
	/** Polygone convexe rempli en éventail depuis Center (couleur Center au centre, Rim sur le bord). */
	void ConvexFill(const TArray<FVector2D>& Points, const FVector2D& Center, const FLinearColor& CenterColor, const FLinearColor& RimColor);
	void ConvexFill(const TArray<FVector2D>& Points, const FLinearColor& Color);
	void Disc(const FVector2D& Center, float Radius, const FLinearColor& Color);
	void Ring(const FVector2D& Center, float Radius, float Width, const FLinearColor& Color);

	// ---- Texte (envoyé après les triangles déjà accumulés) ----

	FVector2D TextSize(const FString& Text, float Scale = 1.f) const;
	void Text(const FVector2D& Position, const FString& Text, const FLinearColor& Color, float Scale = 1.f, bool bCenterX = false);

	/** Envoie les triangles accumulés au Canvas. Appelé aussi par le destructeur. */
	void Flush();

	/** Couleur depuis "#RRGGBB" (sRGB), avec une opacité. */
	static FLinearColor Hex(const TCHAR* InHex, float Alpha = 1.f);
	static FLinearColor WithAlpha(const FLinearColor& Color, float Alpha) { return FLinearColor(Color.R, Color.G, Color.B, Alpha); }

private:
	UCanvas* Canvas = nullptr;
	UFont* Font = nullptr;
	FVector2D Size = FVector2D::ZeroVector;
	FVector2D Origin = FVector2D::ZeroVector;
	FVector2D AxisX = FVector2D(1.0, 0.0);
	FVector2D AxisY = FVector2D(0.0, -1.0);
	TArray<FCanvasUVTri> Tris;

	void Tri(const FVector2D& P0, const FLinearColor& C0, const FVector2D& P1, const FLinearColor& C1, const FVector2D& P2, const FLinearColor& C2);
	void Quad(const FVector2D& P0, const FLinearColor& C0, const FVector2D& P1, const FLinearColor& C1,
		const FVector2D& P2, const FLinearColor& C2, const FVector2D& P3, const FLinearColor& C3);
	void Annulus(const FVector2D& Center, float Inner, float Outer, const FLinearColor& Color);
};
