#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "GeomHUD.generated.h"

class AGeomVisualizer;

/**
 * HUD de la scène de test : dessine le visualiseur sur le Canvas à chaque frame
 * (voir AGeomVisualizer::Paint et FGeomCanvas).
 */
UCLASS()
class MATHS_T1_API AGeomHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	TWeakObjectPtr<AGeomVisualizer> Visualizer;
};
