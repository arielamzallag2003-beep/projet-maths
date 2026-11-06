#include "GeomCanvas.h"

#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "RenderUtils.h" // GWhiteTexture

namespace
{
	constexpr double Fringe = 1.0; // largeur du bord adouci, en pixels
}

FGeomCanvas::FGeomCanvas(UCanvas* InCanvas)
	: Canvas(InCanvas)
{
	if (!Canvas)
	{
		return;
	}
	Size = FVector2D(Canvas->ClipX, Canvas->ClipY);
	Font = GEngine ? GEngine->GetSmallFont() : nullptr;

	// Projection orthographique : l'image de (0,0), (1000,0) et (0,1000) suffit à tout décrire
	const FVector O = Canvas->Project(FVector(0.0, 0.0, 0.0), false);
	const FVector X = Canvas->Project(FVector(1000.0, 0.0, 0.0), false);
	const FVector Y = Canvas->Project(FVector(0.0, 1000.0, 0.0), false);
	Origin = FVector2D(O.X, O.Y);
	AxisX = (FVector2D(X.X, X.Y) - Origin) / 1000.0;
	AxisY = (FVector2D(Y.X, Y.Y) - Origin) / 1000.0;

	// Pas encore de vue valide (première frame) : on ne dessine rien plutôt que n'importe quoi
	const double Det = AxisX.X * AxisY.Y - AxisX.Y * AxisY.X;
	if (!FMath::IsFinite(Det) || FMath::Abs(Det) < 1e-12 || Size.X <= 0.0 || Size.Y <= 0.0)
	{
		Canvas = nullptr;
	}
}

FGeomCanvas::~FGeomCanvas()
{
	Flush();
}

FVector2D FGeomCanvas::ToWorld(const FVector2D& Screen) const
{
	const FVector2D R = Screen - Origin;
	const double Det = AxisX.X * AxisY.Y - AxisX.Y * AxisY.X;
	if (FMath::Abs(Det) < 1e-12)
	{
		return FVector2D::ZeroVector;
	}
	return FVector2D((R.X * AxisY.Y - R.Y * AxisY.X) / Det, (AxisX.X * R.Y - AxisX.Y * R.X) / Det);
}

FLinearColor FGeomCanvas::Hex(const TCHAR* InHex, float Alpha)
{
	FLinearColor C = FLinearColor::FromSRGBColor(FColor::FromHex(FString(InHex)));
	C.A = Alpha;
	return C;
}

// ---------------------------------------------------------------- Triangles

void FGeomCanvas::Tri(const FVector2D& P0, const FLinearColor& C0, const FVector2D& P1, const FLinearColor& C1, const FVector2D& P2, const FLinearColor& C2)
{
	FCanvasUVTri& T = Tris.AddDefaulted_GetRef();
	T.V0_Pos = P0; T.V0_UV = FVector2D::ZeroVector; T.V0_Color = C0;
	T.V1_Pos = P1; T.V1_UV = FVector2D::ZeroVector; T.V1_Color = C1;
	T.V2_Pos = P2; T.V2_UV = FVector2D::ZeroVector; T.V2_Color = C2;
}

void FGeomCanvas::Quad(const FVector2D& P0, const FLinearColor& C0, const FVector2D& P1, const FLinearColor& C1,
	const FVector2D& P2, const FLinearColor& C2, const FVector2D& P3, const FLinearColor& C3)
{
	Tri(P0, C0, P1, C1, P2, C2);
	Tri(P0, C0, P2, C2, P3, C3);
}

void FGeomCanvas::Flush()
{
	if (!Canvas || Tris.Num() == 0)
	{
		Tris.Reset();
		return;
	}
	FCanvasTriangleItem Item(FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D::ZeroVector, GWhiteTexture);
	Item.TriangleList = MoveTemp(Tris);
	Item.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Item);
	Tris.Reset();
}

// ---------------------------------------------------------------- Formes

void FGeomCanvas::Background(const FLinearColor& Top, const FLinearColor& Bottom)
{
	if (!Canvas) return;
	Quad(FVector2D(0.0, 0.0), Top, FVector2D(Size.X, 0.0), Top, FVector2D(Size.X, Size.Y), Bottom, FVector2D(0.0, Size.Y), Bottom);
}

void FGeomCanvas::Line(const FVector2D& A, const FVector2D& B, float Width, const FLinearColor& ColorA, const FLinearColor& ColorB)
{
	if (!Canvas) return;
	FVector2D D = B - A;
	const double Length = D.Size();
	if (Length < 1e-3 || !FMath::IsFinite(Length)) return;
	D /= Length;
	const FVector2D N(-D.Y, D.X);

	// Un trait plus fin qu'un pixel est dessiné à un pixel, plus transparent
	FLinearColor Ca = ColorA, Cb = ColorB;
	if (Width < 1.f)
	{
		Ca.A *= FMath::Max(Width, 0.f);
		Cb.A *= FMath::Max(Width, 0.f);
		Width = 1.f;
	}
	const FLinearColor Ca0 = WithAlpha(Ca, 0.f), Cb0 = WithAlpha(Cb, 0.f);
	const double Core = FMath::Max(0.0, 0.5 * Width - 0.5 * Fringe);
	const double Outer = 0.5 * Width + 0.5 * Fringe;
	const FVector2D A2 = A - D * 0.5, B2 = B + D * 0.5; // un demi-pixel de plus aux bouts

	if (Core > 0.0)
	{
		Quad(A2 + N * Core, Ca, B2 + N * Core, Cb, B2 - N * Core, Cb, A2 - N * Core, Ca);
	}
	Quad(A2 + N * Outer, Ca0, B2 + N * Outer, Cb0, B2 + N * Core, Cb, A2 + N * Core, Ca);
	Quad(A2 - N * Core, Ca, B2 - N * Core, Cb, B2 - N * Outer, Cb0, A2 - N * Outer, Ca0);
}

void FGeomCanvas::DashedLine(const FVector2D& A, const FVector2D& B, float Width, const FLinearColor& Color, float Dash, float Gap)
{
	if (!Canvas) return;
	const double Length = FVector2D::Distance(A, B);
	if (Length < 1e-3 || Dash <= 0.f) return;
	const FVector2D D = (B - A) / Length;
	const double Period = Dash + FMath::Max(Gap, 0.f);
	// Au-delà de 2 000 tirets on se contente d'un trait plein, plus léger
	if (Length / Period > 2000.0)
	{
		Line(A, B, Width, WithAlpha(Color, Color.A * Dash / Period));
		return;
	}
	for (double T = 0.0; T < Length; T += Period)
	{
		Line(A + D * T, A + D * FMath::Min(T + Dash, Length), Width, Color);
	}
}

void FGeomCanvas::InfiniteLine(const FVector2D& P, const FVector2D& Dir, float Width, const FLinearColor& Color, float Dash, float Gap)
{
	const double L = Dir.Size();
	if (L < 1e-9) return;
	const FVector2D D = Dir / L;
	// Assez long pour traverser l'écran depuis n'importe quel point visible
	const double Reach = 2.0 * Size.Size() + FVector2D::Distance(P, Size * 0.5);
	if (Dash > 0.f)
	{
		DashedLine(P - D * Reach, P + D * Reach, Width, Color, Dash, Gap);
	}
	else
	{
		Line(P - D * Reach, P + D * Reach, Width, Color);
	}
}

void FGeomCanvas::Polyline(const TArray<FVector2D>& Points, bool bClosed, float Width, const FLinearColor& Color)
{
	const int32 Count = Points.Num();
	for (int32 i = 0; i + 1 < Count; ++i)
	{
		Line(Points[i], Points[i + 1], Width, Color);
	}
	if (bClosed && Count > 2)
	{
		Line(Points[Count - 1], Points[0], Width, Color);
	}
}

void FGeomCanvas::ConvexFill(const TArray<FVector2D>& Points, const FVector2D& Center, const FLinearColor& CenterColor, const FLinearColor& RimColor)
{
	if (!Canvas) return;
	const int32 Count = Points.Num();
	if (Count < 3) return;
	for (int32 i = 0; i < Count; ++i)
	{
		Tri(Center, CenterColor, Points[i], RimColor, Points[(i + 1) % Count], RimColor);
	}
}

void FGeomCanvas::ConvexFill(const TArray<FVector2D>& Points, const FLinearColor& Color)
{
	if (!Canvas) return;
	for (int32 i = 1; i + 1 < Points.Num(); ++i)
	{
		Tri(Points[0], Color, Points[i], Color, Points[i + 1], Color);
	}
}

void FGeomCanvas::Annulus(const FVector2D& Center, float Inner, float Outer, const FLinearColor& Color)
{
	// Couronne entre Inner et Outer, bords adoucis d'un pixel des deux côtés
	if (!Canvas || Outer <= 0.f) return;
	const int32 Segments = FMath::Clamp(FMath::CeilToInt(Outer * 1.2f), 16, 96);
	const FLinearColor C0 = WithAlpha(Color, 0.f);
	const double In0 = FMath::Max(0.0, Inner - 0.5 * Fringe), In1 = FMath::Max(0.0, Inner + 0.5 * Fringe);
	const double Out0 = FMath::Max(In1, Outer - 0.5 * Fringe), Out1 = Outer + 0.5 * Fringe;
	for (int32 s = 0; s < Segments; ++s)
	{
		const double A0 = 2.0 * PI * s / Segments, A1 = 2.0 * PI * (s + 1) / Segments;
		const FVector2D U0(FMath::Cos(A0), FMath::Sin(A0)), U1(FMath::Cos(A1), FMath::Sin(A1));
		if (In1 > 0.0)
		{
			Quad(Center + U0 * In0, C0, Center + U1 * In0, C0, Center + U1 * In1, Color, Center + U0 * In1, Color);
		}
		if (Out0 > In1)
		{
			Quad(Center + U0 * In1, Color, Center + U1 * In1, Color, Center + U1 * Out0, Color, Center + U0 * Out0, Color);
		}
		Quad(Center + U0 * Out0, Color, Center + U1 * Out0, Color, Center + U1 * Out1, C0, Center + U0 * Out1, C0);
	}
}

void FGeomCanvas::Disc(const FVector2D& Center, float Radius, const FLinearColor& Color)
{
	if (!Canvas || Radius <= 0.f) return;
	const int32 Segments = FMath::Clamp(FMath::CeilToInt(Radius * 1.5f), 12, 96);
	const double Core = FMath::Max(0.0, Radius - 0.5 * Fringe), Edge = Core + Fringe;
	const FLinearColor C0 = WithAlpha(Color, 0.f);
	for (int32 s = 0; s < Segments; ++s)
	{
		const double A0 = 2.0 * PI * s / Segments, A1 = 2.0 * PI * (s + 1) / Segments;
		const FVector2D U0(FMath::Cos(A0), FMath::Sin(A0)), U1(FMath::Cos(A1), FMath::Sin(A1));
		if (Core > 0.0)
		{
			Tri(Center, Color, Center + U0 * Core, Color, Center + U1 * Core, Color);
		}
		// Bord adouci : de l'opacité pleine à transparent sur un pixel
		Quad(Center + U0 * Core, Color, Center + U1 * Core, Color, Center + U1 * Edge, C0, Center + U0 * Edge, C0);
	}
}

void FGeomCanvas::Ring(const FVector2D& Center, float Radius, float Width, const FLinearColor& Color)
{
	Annulus(Center, Radius - 0.5f * Width, Radius + 0.5f * Width, Color);
}

// ---------------------------------------------------------------- Texte

FVector2D FGeomCanvas::TextSize(const FString& InText, float Scale) const
{
	if (!Canvas || !Font) return FVector2D::ZeroVector;
	float XL = 0.f, YL = 0.f;
	Canvas->TextSize(Font, InText, XL, YL, Scale, Scale);
	return FVector2D(XL, YL);
}

void FGeomCanvas::Text(const FVector2D& Position, const FString& InText, const FLinearColor& Color, float Scale, bool bCenterX)
{
	if (!Canvas || !Font) return;
	Flush(); // le texte passe au-dessus de ce qui est déjà dessiné
	FCanvasTextItem Item(Position, FText::FromString(InText), Font, Color);
	Item.Scale = FVector2D(Scale, Scale);
	Item.bCentreX = bCenterX;
	Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.85f));
	Canvas->DrawItem(Item);
}
