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
#include "Widgets/Layout/SDPIScaler.h"
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
	TWeakObjectPtr<AGeomTestGameMode> WeakSelf(this);
	Bridge->OnTogglePanel = [WeakSelf]() { if (WeakSelf.IsValid()) WeakSelf->TogglePanel(); };
	LoadPage();
	TryWireController();
}

void AGeomTestGameMode::LoadPage()
{
	if (!Browser.IsValid() || !Bridge) return;
	++LoadAttempts;
	SinceLoad = 0.f;

	// Liaison permanente AVANT le chargement : window.ue.geom pour tous les scripts de la page.
	// On la refait à chaque tentative : si la fenêtre CEF n'existait pas encore, elle a pu être ignorée.
	Browser->UnbindUObject(TEXT("geom"), Bridge, true);
	Browser->BindUObject(TEXT("geom"), Bridge, true);

	// ?host=ue : la page sait qu'elle est dans Unreal et attend le pont au lieu de simuler
	const FString File = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / PagePath);
	const FString Url = TEXT("file:///") + File + TEXT("?host=ue");
	UE_LOG(LogMaths_T1, Log, TEXT("Chargement du panneau (essai %d) : %s"), LoadAttempts, *Url);
	Browser->LoadURL(Url);
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

	// Seul le rectangle du panneau capte la souris : le reste du viewport reste au jeu.
	// SDPIScaler annule l'échelle DPI de Slate pour le navigateur : sinon CEF met la page en page
	// à la taille non mise à l'échelle et le bord droit est rogné quand la fenêtre est petite.
	// L'échelle est transmise à la page à la place (--ui-scale), voir UpdatePanelScale.
	SAssignNew(Root, SOverlay)
		.Visibility(EVisibility::SelfHitTestInvisible)
		+ SOverlay::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Fill)
		.Padding(PanelMargin)
		[
			SAssignNew(Scaler, SDPIScaler)
			.DPIScale(1.f)
			[
				SAssignNew(PanelBox, SBox)
				.WidthOverride(PanelWidth)
				[
					Browser.ToSharedRef()
				]
			]
		];

	Viewport->AddViewportWidgetContent(Root.ToSharedRef(), 10);
}

void AGeomTestGameMode::UpdatePanelScale()
{
	const float ViewportScale = UWidgetLayoutLibrary::GetViewportScale(this);
	if (ViewportScale <= 0.f || FMath::IsNearlyEqual(ViewportScale, AppliedViewportScale)) return;
	AppliedViewportScale = ViewportScale;

	// Dans le SDPIScaler, 1 unité = 1 pixel. La taille du panneau suit l'échelle du jeu,
	// bornée pour rester lisible dans une petite fenêtre de PIE.
	PanelScale = FMath::Clamp(ViewportScale, MinPanelScale, 2.f);
	if (Scaler.IsValid()) Scaler->SetDPIScale(1.f / ViewportScale);
	if (PanelBox.IsValid()) PanelBox->SetWidthOverride(PanelWidth * PanelScale);
	if (Bridge) Bridge->SetUiScale(PanelScale);
	UE_LOG(LogMaths_T1, Log, TEXT("Échelle du viewport %.2f : panneau à %.0f px (ui-scale %.2f)."), ViewportScale, PanelWidth * PanelScale, PanelScale);
}

float AGeomTestGameMode::GetPanelWidthPx() const
{
	if (!bPanelVisible) return 0.f;
	const float ViewportScale = UWidgetLayoutLibrary::GetViewportScale(this);
	return PanelWidth * PanelScale + 2.f * PanelMargin * ViewportScale;
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
	UpdatePanelScale();
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

		// La page appelle ready() au chargement. Sans nouvelles, on relie et on recharge.
		if (!Bridge->IsReady() && Browser.IsValid())
		{
			SinceLoad += DeltaSeconds;
			if (SinceLoad > 6.f)
			{
				if (LoadAttempts < 3)
				{
					UE_LOG(LogMaths_T1, Warning, TEXT("Le panneau n'a pas appelé ready() : nouvelle liaison et rechargement."));
					LoadPage();
				}
				else if (LoadAttempts == 3)
				{
					++LoadAttempts;
					const FString Msg = TEXT("Panneau web : pont ue.geom non joignable après 3 essais (voir LogMaths_T1).");
					UE_LOG(LogMaths_T1, Error, TEXT("%s"), *Msg);
					if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 30.f, FColor::Red, Msg);
				}
			}
		}
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
	Scaler.Reset();
	Root.Reset();
	Super::EndPlay(Reason);
}
