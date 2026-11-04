#include "GeometryMathLibrary.h"

float UGeometryMathLibrary::CrossProduct2D(const FVector& A, const FVector& B)
{
    return A.X * B.Y - A.Y * B.X;
}

float UGeometryMathLibrary::Orient2D(const FVector& A, const FVector& B, const FVector& C)
{
    return (B.X - A.X) * (C.Y - A.Y) - (B.Y - A.Y) * (C.X - A.X);
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
    float d1 = Orient2D(P, A, B);
    float d2 = Orient2D(P, B, C);
    float d3 = Orient2D(P, C, A);

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

FVector UGeometryMathLibrary::GetSphereCircumcenter3D(const FVector& A, const FVector& B, const FVector& C, const FVector& D, float& OutRadiusSquared)
{
    FMatrix M(
        FPlane(B.X - A.X, B.Y - A.Y, B.Z - A.Z, 0.0f),
        FPlane(C.X - A.X, C.Y - A.Y, C.Z - A.Z, 0.0f),
        FPlane(D.X - A.X, D.Y - A.Y, D.Z - A.Z, 0.0f),
        FPlane(0.0f, 0.0f, 0.0f, 1.0f)
    );

    float Det = M.Determinant();
    if (FMath::IsNearlyZero(Det))
    {
        OutRadiusSquared = 0.0f;
        return A;
    }

    float LenA = A.SizeSquared();
    float LenB = B.SizeSquared();
    float LenC = C.SizeSquared();
    float LenD = D.SizeSquared();

    FMatrix M1(
        FPlane(LenB - LenA, B.Y - A.Y, B.Z - A.Z, 0.0f),
        FPlane(LenC - LenA, C.Y - A.Y, C.Z - A.Z, 0.0f),
        FPlane(LenD - LenA, D.Y - A.Y, D.Z - A.Z, 0.0f),
        FPlane(0.0f, 0.0f, 0.0f, 1.0f)
    );

    FMatrix M2(
        FPlane(B.X - A.X, LenB - LenA, B.Z - A.Z, 0.0f),
        FPlane(C.X - A.X, LenC - LenA, C.Z - A.Z, 0.0f),
        FPlane(D.X - A.X, LenD - LenA, D.Z - A.Z, 0.0f),
        FPlane(0.0f, 0.0f, 0.0f, 1.0f)
    );

    FMatrix M3(
        FPlane(B.X - A.X, B.Y - A.Y, LenB - LenA, 0.0f),
        FPlane(C.X - A.X, C.Y - A.Y, LenC - LenA, 0.0f),
        FPlane(D.X - A.X, D.Y - A.Y, LenD - LenA, 0.0f),
        FPlane(0.0f, 0.0f, 0.0f, 1.0f)
    );

    float X = M1.Determinant() / (2.0f * Det);
    float Y = M2.Determinant() / (2.0f * Det);
    float Z = M3.Determinant() / (2.0f * Det);

    FVector Center(X, Y, Z);
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