#pragma once

#include "CoreMinimal.h"
#include "GeometryTypes.generated.h"

// Déclarations anticipées
struct FGeomVertex;
struct FGeomEdge;
struct FGeomFace;

// ==========================================
// 1. STRUCTURE POUR LE GRAHAM SCAN (Cours 3)
// ==========================================
USTRUCT(BlueprintType)
struct FCircularNode
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry | Graham")
    FVector Position = FVector::ZeroVector;

    // Pointeurs pour la liste chaînée circulaire
    FCircularNode* Prec = nullptr;
    FCircularNode* Suiv = nullptr;

    bool bConvex = true;
};

// ==========================================
// 2. STRUCTURES POUR LE GRAPHE D'INCIDENCE (2D & 3D)
// ==========================================

// États de marquage pour l'enveloppe convexe incrémentale 3D (Cours 3)
UENUM(BlueprintType)
enum class EElementColor : uint8
{
    Red      UMETA(DisplayName = "Rouge (Interne / Redondant)"),
    Blue     UMETA(DisplayName = "Bleu (Visible / Supprimé)"),
    Violet   UMETA(DisplayName = "Violet (Bordure / Frontière)"),
    None     UMETA(DisplayName = "Neutre")
};

USTRUCT(BlueprintType)
struct FGeomVertex
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry")
    int32 ID = 0;

    // Utilisation de FVector (utilisable en 2D avec Z=0 et en 3D)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry")
    FVector Position = FVector::ZeroVector;

    TArray<FGeomEdge*> IncidentEdges;

    // Marquage pour l'incrémental 3D (Cours 3)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry | 3D")
    EElementColor Color = EElementColor::None;
};

USTRUCT(BlueprintType)
struct FGeomEdge
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry")
    int32 ID = 0;

    FGeomVertex* V1 = nullptr;
    FGeomVertex* V2 = nullptr;
    
    FGeomFace* LeftFace = nullptr;  // Sens trigonométrique
    FGeomFace* RightFace = nullptr;

    // Marquage pour l'incrémental 3D (Cours 3 : arête rouge, bleue ou violette)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry | 3D")
    EElementColor Color = EElementColor::None;
};

USTRUCT(BlueprintType)
struct FGeomFace
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry")
    int32 ID = 0;

    FGeomVertex* Vertices[3] = { nullptr, nullptr, nullptr };
    FGeomEdge* Edges[3] = { nullptr, nullptr, nullptr };

    // Marquage pour l'incrémental 3D (Cours 3 : faces rouges / bleues vues par P_q+1)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry | 3D")
    EElementColor Color = EElementColor::None;
};