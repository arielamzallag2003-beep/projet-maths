#include "GeometryMathLibrary.h"

float UGeometryMathLibrary::CrossProduct2D(const FVector& u, const FVector& v)
{
    return u.X * v.Y - u.Y * v.X;
}

float UGeometryMathLibrary::DotProduct2D(const FVector& u, const FVector& v)
{
    return u.X * v.X + u.Y * v.Y;
}

float UGeometryMathLibrary::OrientAngle2D(const FVector& A, const FVector& B, const FVector& C)
{
    FVector AB = B - A;
    FVector AC = C - A;
    return CrossProduct2D(AB, AC);
}

float UGeometryMathLibrary::GetAngle(const FVector& v, const FVector& u)
{
    return acos(DotProduct2D(v, u)/(GetNorm(v) * GetNorm(u)));
}

float UGeometryMathLibrary::GetNorm(const FVector& v)
{
    return sqrt(v.X * v.X + v.Y * v.Y);
}

float UGeometryMathLibrary::MatrixDeterminant3x3(const FMatrix& Matrix)
{
    return Matrix.Determinant();
}

bool UGeometryMathLibrary::InverseMatrix3x3(const FMatrix& InMatrix, FMatrix& OutInverseMatrix)
{
    OutInverseMatrix = InMatrix.Inverse();
    return !FMath::IsNearlyZero(InMatrix.Determinant());
}

bool UGeometryMathLibrary::IsPointInsideTriangle2D(const FVector& P, const FVector& A, const FVector& B, const FVector& C)
{
    float d1 = OrientAngle2D(P, A, B);
    float d2 = OrientAngle2D(P, B, C);
    float d3 = OrientAngle2D(P, C, A);

    bool hasNeg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    bool hasPos = (d1 > 0) || (d2 > 0) || (d3 > 0);

    return !(hasNeg && hasPos);
}

bool UGeometryMathLibrary::IsVisibleFromPoint(const FVector& Point, const FVector& FaceA, const FVector& FaceB, const FVector& Normal)
{
    FVector MidPoint = (FaceA + FaceB) * 0.5f;
    FVector Dir = Point - MidPoint;
    return FVector::DotProduct(Dir, Normal) > 0.0f;
}

FVector UGeometryMathLibrary::ComputeOuterNormal2D(const FVector& A, const FVector& B)
{
    FVector EdgeDir = B - A;
    FVector Normal(-EdgeDir.Y, EdgeDir.X, 0.0f);
    Normal.Normalize();
    return Normal;
}

FVector UGeometryMathLibrary::GetCircumcenter2D(const FVector& A, const FVector& B, const FVector& C, float& OutRadiusSquared)
{
    float D = 2.0f * (A.X * (B.Y - C.Y) + B.X * (C.Y - A.Y) + C.X * (A.Y - B.Y));
    if (FMath::IsNearlyZero(D))
    {
        OutRadiusSquared = 0.0f;
        return A;
    }

    float LenA = A.X * A.X + A.Y * A.Y;
    float LenB = B.X * B.X + B.Y * B.Y;
    float LenC = C.X * C.X + C.Y * C.Y;

    float X = (LenA * (B.Y - C.Y) + LenB * (C.Y - A.Y) + LenC * (A.Y - B.Y)) / D;
    float Y = (LenA * (C.X - B.X) + LenB * (A.X - C.X) + LenC * (B.X - A.X)) / D;

    FVector Center(X, Y, 0.0f);
    OutRadiusSquared = FVector::DistSquared(Center, A);
    return Center;
}


bool UGeometryMathLibrary::IsInsideCircumcircle(const FVector& A, const FVector& B, const FVector& C, const FVector& D)
{
    FMatrix Matrix(
        FPlane(A.X - D.X, A.Y - D.Y, FMath::Square(A.X - D.X) + FMath::Square(A.Y - D.Y), 0.0f),
        FPlane(B.X - D.X, B.Y - D.Y, FMath::Square(B.X - D.X) + FMath::Square(B.Y - D.Y), 0.0f),
        FPlane(C.X - D.X, C.Y - D.Y, FMath::Square(C.X - D.X) + FMath::Square(C.Y - D.Y), 0.0f),
        FPlane(0.0f, 0.0f, 0.0f, 1.0f)
    );

    return Matrix.Determinant() > 0.0f;
}