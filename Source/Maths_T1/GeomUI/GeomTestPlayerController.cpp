#include "GeomTestPlayerController.h"
#include "GeomTestPawn.h"
#include "GeomVisualizer.h"
#include "Components/InputComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"

AGeomTestPlayerController::AGeomTestPlayerController()
{
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Crosshairs;
}

void AGeomTestPlayerController::BeginPlay()
{
	Super::BeginPlay();

	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
}

void AGeomTestPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	check(InputComponent);

	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AGeomTestPlayerController::OnLeftPressed);
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &AGeomTestPlayerController::OnLeftReleased);
	InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &AGeomTestPlayerController::OnRightPressed);
	InputComponent->BindKey(EKeys::MiddleMouseButton, IE_Pressed, this, &AGeomTestPlayerController::OnMiddlePressed);
	InputComponent->BindKey(EKeys::MiddleMouseButton, IE_Released, this, &AGeomTestPlayerController::OnMiddleReleased);
	InputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &AGeomTestPlayerController::OnWheelUp);
	InputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &AGeomTestPlayerController::OnWheelDown);

	InputComponent->BindKey(EKeys::F, IE_Pressed, this, &AGeomTestPlayerController::OnKeyFrame);
	InputComponent->BindKey(EKeys::H, IE_Pressed, this, &AGeomTestPlayerController::OnKeyPanel);
	InputComponent->BindKey(EKeys::One, IE_Pressed, this, &AGeomTestPlayerController::OnKey1);
	InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &AGeomTestPlayerController::OnKey2);
	InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &AGeomTestPlayerController::OnKey3);
	InputComponent->BindKey(EKeys::Four, IE_Pressed, this, &AGeomTestPlayerController::OnKey4);
	InputComponent->BindKey(EKeys::A, IE_Pressed, this, &AGeomTestPlayerController::OnToolAdd);
	InputComponent->BindKey(EKeys::M, IE_Pressed, this, &AGeomTestPlayerController::OnToolMove);
	InputComponent->BindKey(EKeys::X, IE_Pressed, this, &AGeomTestPlayerController::OnToolDelete);
	InputComponent->BindKey(EKeys::Z, IE_Pressed, this, &AGeomTestPlayerController::OnKeyUndo);
	InputComponent->BindKey(EKeys::Y, IE_Pressed, this, &AGeomTestPlayerController::OnKeyRedo);
}

AGeomTestPawn* AGeomTestPlayerController::GetGeomPawn() const
{
	return Cast<AGeomTestPawn>(GetPawn());
}

double AGeomTestPlayerController::GetPixelSize() const
{
	int32 SizeX = 0, SizeY = 0;
	GetViewportSize(SizeX, SizeY);
	const AGeomTestPawn* P = GetGeomPawn();
	return (P && SizeX > 0) ? P->GetOrthoWidth() / SizeX : 4.0;
}

bool AGeomTestPlayerController::GetMouseOnPlane(FVector2D& Out) const
{
	FVector Origin, Direction;
	if (!DeprojectMousePositionToWorld(Origin, Direction) || FMath::IsNearlyZero(Direction.Z))
	{
		return false;
	}
	const double T = -Origin.Z / Direction.Z;
	const FVector Hit = Origin + Direction * T;
	Out = FVector2D(Hit.X, Hit.Y);
	return true;
}

void AGeomTestPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	AGeomTestPawn* P = GetGeomPawn();
	if (!P || !Visualizer) return;

	Visualizer->PixelSize = GetPixelSize();

	// Déplacement de la caméra au clic molette
	if (bPanning)
	{
		float MX, MY;
		if (GetMousePosition(MX, MY))
		{
			const double Px = GetPixelSize();
			// écran : droite = +X, bas = -Y
			P->SetViewCenter(PanStartCenter + FVector2D(-(MX - PanStartMouse.X) * Px, (MY - PanStartMouse.Y) * Px));
		}
		return;
	}

	FVector2D W;
	if (!GetMouseOnPlane(W)) return;
	LastCursor = W;

	if (Visualizer->DragId != INDEX_NONE)
	{
		// Glisser : recalcul à chaque frame si l'algorithme est rapide, sinon 10 fois par seconde
		// pour garder le jeu fluide. Le recalcul final a lieu au relâchement.
		SinceDragRecompute += DeltaTime;
		const float Interval = Visualizer->GetLastMs() > 4.0 ? 0.1f : 0.f;
		const bool bNow = SinceDragRecompute >= Interval;
		Visualizer->MovePoint(Visualizer->DragId, W, bNow);
		if (bNow) SinceDragRecompute = 0.f;
		return;
	}
	Visualizer->HoverId = Visualizer->FindNearest(W, PickRadius());
}

void AGeomTestPlayerController::OnLeftPressed()
{
	FVector2D W;
	if (!Visualizer || !GetMouseOnPlane(W)) return;
	const int32 Hit = Visualizer->FindNearest(W, PickRadius());

	switch (Tool)
	{
	case EGeomTool::Add:
		if (Hit != INDEX_NONE) { Visualizer->DragId = Hit; }   // glisser un point existant marche aussi
		else { Visualizer->AddPoint(W); }
		break;
	case EGeomTool::Move:
		if (Hit != INDEX_NONE) { Visualizer->DragId = Hit; }
		break;
	case EGeomTool::Delete:
		if (Hit != INDEX_NONE) { Visualizer->RemovePoint(Hit); }
		break;
	}
}

void AGeomTestPlayerController::OnLeftReleased()
{
	if (Visualizer && Visualizer->DragId != INDEX_NONE)
	{
		FVector2D W;
		if (GetMouseOnPlane(W)) Visualizer->MovePoint(Visualizer->DragId, W, true);
		Visualizer->DragId = INDEX_NONE;
	}
}

void AGeomTestPlayerController::OnRightPressed()
{
	FVector2D W;
	if (!Visualizer || !GetMouseOnPlane(W)) return;
	const int32 Hit = Visualizer->FindNearest(W, PickRadius());
	if (Hit != INDEX_NONE) Visualizer->RemovePoint(Hit);
}

void AGeomTestPlayerController::OnMiddlePressed()
{
	AGeomTestPawn* P = GetGeomPawn();
	float MX, MY;
	if (!P || !GetMousePosition(MX, MY)) return;
	bPanning = true;
	PanStartMouse = FVector2D(MX, MY);
	PanStartCenter = P->GetViewCenter();
}

void AGeomTestPlayerController::OnMiddleReleased()
{
	bPanning = false;
}

void AGeomTestPlayerController::Zoom(float Factor)
{
	AGeomTestPawn* P = GetGeomPawn();
	if (!P) return;

	// Zoom centré sur le curseur. Calcul direct (projection ortho) : la déprojection
	// utilise les matrices de la frame précédente et ne verrait pas le nouveau zoom.
	const double OldWidth = P->GetOrthoWidth();
	P->SetOrthoWidth(OldWidth * Factor);
	const double Ratio = P->GetOrthoWidth() / OldWidth;

	FVector2D Cursor;
	if (GetMouseOnPlane(Cursor))
	{
		const FVector2D Center = P->GetViewCenter();
		P->SetViewCenter(Cursor - (Cursor - Center) * Ratio);
	}
}

void AGeomTestPlayerController::FrameAll()
{
	AGeomTestPawn* P = GetGeomPawn();
	if (!P || !Visualizer) return;

	int32 SizeX = 0, SizeY = 0;
	GetViewportSize(SizeX, SizeY);
	if (SizeX <= 0 || SizeY <= 0) return;

	FBox2D Box = Visualizer->GetBounds();
	if (!Box.bIsValid || Visualizer->NumPoints() < 2)
	{
		Box = FBox2D(FVector2D(-2000.0, -1500.0), FVector2D(2000.0, 1500.0));
	}
	const FVector2D Size = Box.GetSize().ComponentMax(FVector2D(400.0, 400.0)) * 1.25;
	const double FreeX = FMath::Max(200.0, SizeX - PanelWidthPx);

	// Largeur ortho telle que la boîte tienne dans la zone libre, en X comme en Y
	const double Width = FMath::Max(Size.X * SizeX / FreeX, Size.Y * SizeX / SizeY);
	P->SetOrthoWidth(Width);

	// Le centre de la zone libre est décalé de PanelWidthPx / 2 vers la droite
	const double Px = Width / SizeX;
	P->SetViewCenter(Box.GetCenter() - FVector2D(PanelWidthPx * 0.5 * Px, 0.0));
}

void AGeomTestPlayerController::OnKeyUndo()
{
	if (IsInputKeyDown(EKeys::LeftControl) || IsInputKeyDown(EKeys::RightControl))
	{
		if (OnShortcut) OnShortcut(TEXT("undo"));
	}
}

void AGeomTestPlayerController::OnKeyRedo()
{
	if (IsInputKeyDown(EKeys::LeftControl) || IsInputKeyDown(EKeys::RightControl))
	{
		if (OnShortcut) OnShortcut(TEXT("redo"));
	}
}
