#include "GeomTestGameMode.h"
#include "GeomBridge.h"
#include "GeomTestPawn.h"
#include "GeomTestPlayerController.h"
#include "GeomVisualizer.h"
#include "Maths_T1.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "Misc/Paths.h"
#include "SWebBrowser.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"

AGeomTestGameMode::AGeomTestGameMode()
{
	DefaultPawnClass = AGeomTestPawn::StaticClass();
	PlayerControllerClass = AGeomTestPlayerController::StaticClass();
	PrimaryActorTick.bCanEverTick = true;
}

void AGeomTestGameMode::BeginPlay()
{
	Super::BeginPlay();
	UWorld* World = GetWorld();

	// Un visualiseur déjà posé dans le niveau est réutilisé, sinon on en crée un
	for (TActorIterator<AGeomVisualizer> It(World); It; ++It)
	{
		Visualizer = *It;
		break;
	}
	if (!Visualizer)
	{
		Visualizer = World->SpawnActor<AGeomVisualizer>();
	}

	CreatePanel();

	Bridge = NewObject<UGeomBridge>(this);
	Bridge->Init(Visualizer, nullptr, Browser);
	if (Browser.IsValid())
	{
		// Liaison permanente AVANT le chargement : window.ue.geom existe dès le premier script
		Browser->BindUObject(TEXT("geom"), Bridge, true);
		const FString File = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / PagePath);
		Browser->LoadURL(TEXT("file:///") + File);
	}

	TryWireController();
}

bool AGeomTestGameMode::TryWireController()
{
	if (WiredController.IsValid()) return true;
	AGeomTestPlayerController* PC = Cast<AGeomTestPlayerController>(GetWorld()->GetFirstPlayerController());
	if (!PC) return false;

	PC->SetVisualizer(Visualizer);
	if (Bridge) Bridge->SetController(PC);

	TWeakObjectPtr<AGeomTestGameMode> WeakThis(this);
	PC->OnShortcut = [WeakThis](const FString& Name)
	{
		if (!WeakThis.IsValid()) return;
		if (Name == TEXT("panel")) { WeakThis->TogglePanel(); return; }
		if (WeakThis->Bridge) WeakThis->Bridge->PushShortcut(Name);
	};
	WiredController = PC;
	return true;
}

void AGeomTestGameMode::CreatePanel()
{
	UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
	if (!Viewport)
	{
		UE_LOG(LogMaths_T1, Error, TEXT("Pas de viewport de jeu : panneau web non créé."));
		return;
	}

	const FString File = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / PagePath);
	if (!FPaths::FileExists(File))
	{
		const FString Msg = FString::Printf(TEXT("Page introuvable : %s"), *File);
		UE_LOG(LogMaths_T1, Error, TEXT("%s"), *Msg);
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 30.f, FColor::Red, Msg);
	}

	SAssignNew(Browser, SWebBrowser)
		.InitialURL(TEXT("about:blank"))
		.ShowControls(false)
		.ShowAddressBar(false)
		.SupportsTransparency(false)
		.BrowserFrameRate(BrowserFrameRate);

	// Seul le rectangle du panneau capte la souris : le reste du viewport reste au jeu
	SAssignNew(Root, SOverlay)
		.Visibility(EVisibility::SelfHitTestInvisible)
		+ SOverlay::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Fill)
		.Padding(PanelMargin)
		[
			SAssignNew(PanelBox, SBox)
			.WidthOverride(PanelWidth)
			[
				Browser.ToSharedRef()
			]
		];

	Viewport->AddViewportWidgetContent(Root.ToSharedRef(), 10);
}

float AGeomTestGameMode::GetPanelWidthPx() const
{
	if (!bPanelVisible) return 0.f;
	const float Scale = UWidgetLayoutLibrary::GetViewportScale(this);
	return (PanelWidth + 2.f * PanelMargin) * Scale;
}

void AGeomTestGameMode::TogglePanel()
{
	bPanelVisible = !bPanelVisible;
	if (PanelBox.IsValid())
	{
		PanelBox->SetVisibility(bPanelVisible ? EVisibility::Visible : EVisibility::Collapsed);
	}
}

void AGeomTestGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	TryWireController();
	if (AGeomTestPlayerController* PC = WiredController.Get())
	{
		PC->PanelWidthPx = GetPanelWidthPx();
		if (!bFramedOnce)
		{
			// Première frame avec un viewport dimensionné : cadrage initial
			int32 X = 0, Y = 0;
			PC->GetViewportSize(X, Y);
			if (X > 0) { PC->FrameAll(); bFramedOnce = true; }
		}
	}
	if (Bridge)
	{
		Bridge->Flush(DeltaSeconds);
	}
}

void AGeomTestGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Root.IsValid())
	{
		if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
		{
			Viewport->RemoveViewportWidgetContent(Root.ToSharedRef());
		}
	}
	if (Browser.IsValid())
	{
		Browser->UnbindUObject(TEXT("geom"), Bridge, true);
	}
	Browser.Reset();
	PanelBox.Reset();
	Root.Reset();
	Super::EndPlay(Reason);
}
