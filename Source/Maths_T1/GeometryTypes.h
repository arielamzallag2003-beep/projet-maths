#pragma once

#include "CoreMinimal.h"
#include "GeometryTypes.generated.h"


struct FGeomVertex;
struct FGeomEdge;
struct FGeomFace;

USTRUCT(BlueprintType)
struct FCircularNode
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry | Graham")
    FVector Position = FVector::ZeroVector;
    
    FCircularNode* Prec = nullptr;
    FCircularNode* Suiv = nullptr;

    bool bConvex = true;
};

USTRUCT(BlueprintType)
struct FGeomVertex
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry")
    int32 ID = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry")
    FVector Position = FVector::ZeroVector;

    TArray<FGeomEdge*> IncidentEdges;
};

USTRUCT(BlueprintType)
struct FGeomEdge
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry")
    int32 ID = 0;

    FGeomVertex* V1 = nullptr;
    FGeomVertex* V2 = nullptr;
    
    FGeomFace* LeftFace = nullptr;
    FGeomFace* RightFace = nullptr;
};

USTRUCT(BlueprintType)
struct FGeomFace
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry")
    int32 ID = 0;

    FGeomVertex* Vertices[3] = { nullptr, nullptr, nullptr };
    FGeomEdge* Edges[3] = { nullptr, nullptr, nullptr };
};