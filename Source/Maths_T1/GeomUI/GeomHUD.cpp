#include "GeomHUD.h"
#include "GeomVisualizer.h"
#include "Engine/Canvas.h"
#include "EngineUtils.h"

void AGeomHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas)
	{
		return;
	}
	if (!Visualizer.IsValid())
	{
		for (TActorIterator<AGeomVisualizer> It(GetWorld()); It; ++It)
		{
			Visualizer = *It;
			break;
		}
	}
	if (AGeomVisualizer* V = Visualizer.Get())
	{
		V->Paint(Canvas);
	}
}
