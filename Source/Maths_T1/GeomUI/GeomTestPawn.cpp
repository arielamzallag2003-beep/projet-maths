#include "GeomTestPawn.h"
#include "Camera/CameraComponent.h"

namespace
{
	constexpr double CameraHeight = -5000.0;
}

AGeomTestPawn::AGeomTestPawn()
{
	PrimaryActorTick.bCanEverTick = false;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	RootComponent = Camera;

	Camera->ProjectionMode = ECameraProjectionMode::Orthographic;
	Camera->OrthoWidth = 4000.f;
	Camera->bConstrainAspectRatio = false;

	// La rotation est fixe : le contrôleur ne la pilote pas
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Niveau vide et sombre : exposition fixe pour que les traits ne « brûlent » pas
	FPostProcessSettings& PP = Camera->PostProcessSettings;
	PP.bOverride_AutoExposureMinBrightness = true;
	PP.AutoExposureMinBrightness = 1.f;
	PP.bOverride_AutoExposureMaxBrightness = true;
	PP.AutoExposureMaxBrightness = 1.f;
	PP.bOverride_BloomIntensity = true;
	PP.BloomIntensity = 0.35f;
	Camera->PostProcessBlendWeight = 1.f;
}

void AGeomTestPawn::BeginPlay()
{
	Super::BeginPlay();
	// Sous le plan, regard vers +Z, X à droite et Y en haut (voir le .h).
	// Fait ici et non dans le constructeur : le GameMode écrase la transformation au spawn.
	SetActorLocationAndRotation(FVector(0.0, 0.0, CameraHeight), FRotator(90.0, -90.0, 0.0));
}

float AGeomTestPawn::GetOrthoWidth() const
{
	return Camera->OrthoWidth;
}

void AGeomTestPawn::SetOrthoWidth(float Width)
{
	Camera->SetOrthoWidth(FMath::Clamp(Width, 200.f, 200000.f));
}

FVector2D AGeomTestPawn::GetViewCenter() const
{
	const FVector L = GetActorLocation();
	return FVector2D(L.X, L.Y);
}

void AGeomTestPawn::SetViewCenter(const FVector2D& Center)
{
	SetActorLocation(FVector(Center.X, Center.Y, CameraHeight));
}
