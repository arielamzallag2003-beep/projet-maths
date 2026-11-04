#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GeometryTypes.h"
#include "ConvexHullAlgorithms.generated.h"

UCLASS()
class MATHS_T1_API UConvexHullAlgorithms : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// 1) Marche de Jarvis (Gift Wrapping) - À compléter par vos soins
	UFUNCTION(BlueprintCallable, Category = "Geometry | Convex Hull")
	static TArray<FVector> ComputeJarvisMarch(const TArray<FVector>& Points);

	// 2) Graham Scan (avec tri polaire et liste circulaire) - À compléter par vos soins
	UFUNCTION(BlueprintCallable, Category = "Geometry | Convex Hull")
	static TArray<FVector> ComputeGrahamScan(const TArray<FVector>& Points);
};