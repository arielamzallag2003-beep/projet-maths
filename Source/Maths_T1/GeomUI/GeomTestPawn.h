#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "GeomTestPawn.generated.h"

class UCameraComponent;

/**
 * Caméra orthographique de la scène de test.
 *
 * Unreal est en repère main gauche : vu d'au-dessus avec X à droite, Y pointe
 * vers le bas et le sens trigonométrique apparaît à l'envers. La caméra est donc
 * placée SOUS le plan (Z négatif) et regarde vers +Z : X va à droite, Y vers le
 * haut, et un polygone en sens trigonométrique tourne à l'écran comme au tableau.
 */
UCLASS()
class MATHS_T1_API AGeomTestPawn : public APawn
{
	GENERATED_BODY()

public:
	AGeomTestPawn();
	virtual void BeginPlay() override;

	UCameraComponent* GetCamera() const { return Camera; }

	/** Largeur visible en cm (zoom). */
	float GetOrthoWidth() const;
	void SetOrthoWidth(float Width);

	/** Centre de la vue dans le plan XY. */
	FVector2D GetViewCenter() const;
	void SetViewCenter(const FVector2D& Center);

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UCameraComponent> Camera;
};
