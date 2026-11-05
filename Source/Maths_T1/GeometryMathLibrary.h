#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GeometryMathLibrary.generated.h"

UCLASS()
class MATHS_T1_API UGeometryMathLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // Produit vectoriel, Produit scalaire et Orientation
    UFUNCTION(BlueprintCallable, Category = "Geometry Math")
    static float CrossProduct2D(const FVector& u, const FVector& v);

    UFUNCTION(BlueprintCallable, Category = "Geometry Math")
    static float DotProduct2D(const FVector& u, const FVector& v);

    UFUNCTION(BlueprintCallable, Category = "Geometry Math")
    static float OrientAngle2D(const FVector& A, const FVector& B, const FVector& C);

    // Angle et Norme
    UFUNCTION(BlueprintCallable, Category = "Geometry Math")
    static float GetAngle(const FVector& v, const FVector& u);
    
    UFUNCTION(BlueprintCallable, Category = "Geometry Math")
    static float GetNorm(const FVector& v);

    
    // Matrices (Déterminant et Inversion)
    UFUNCTION(BlueprintCallable, Category = "Geometry Math")
    static float MatrixDeterminant3x3(const FMatrix& Matrix);

    UFUNCTION(BlueprintCallable, Category = "Geometry Math")
    static bool InverseMatrix3x3(const FMatrix& InMatrix, FMatrix& OutInverseMatrix);

    // Tests d'intériorité et Visibilité
    UFUNCTION(BlueprintCallable, Category = "Geometry Math")
    static bool IsPointInsideTriangle2D(const FVector& P, const FVector& A, const FVector& B, const FVector& C);

    UFUNCTION(BlueprintCallable, Category = "Geometry Math")
    static bool IsVisibleFromPoint(const FVector& Point, const FVector& FaceA, const FVector& FaceB, const FVector& Normal);

    // Normales
    UFUNCTION(BlueprintCallable, Category = "Geometry Math")
    static FVector ComputeOuterNormal2D(const FVector& A, const FVector& B);

    // Cercles et Sphères
    UFUNCTION(BlueprintCallable, Category = "Geometry Math")
    static FVector GetCircumcenter2D(const FVector& A, const FVector& B, const FVector& C, float& OutRadiusSquared);

    UFUNCTION(BlueprintCallable, Category = "Geometry Math")
    static FVector GetSphereCircumcenter3D(const FVector& A, const FVector& B, const FVector& C, const FVector& D, float& OutRadiusSquared);

    // Critère de Delaunay
    UFUNCTION(BlueprintCallable, Category = "Geometry Math")
    static bool IsInsideCircumcircle(const FVector& A, const FVector& B, const FVector& C, const FVector& D);
};