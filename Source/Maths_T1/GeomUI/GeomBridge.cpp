#include "GeomBridge.h"
#include "GeomVisualizer.h"
#include "GeomTestPlayerController.h"
#include "SWebBrowser.h"
#include "Maths_T1.h"

void UGeomBridge::Init(AGeomVisualizer* InVisualizer, AGeomTestPlayerController* InController, TSharedPtr<SWebBrowser> InBrowser)
{
	Visualizer = InVisualizer;
	Controller = InController;
	Browser = InBrowser;

	if (InVisualizer)
	{
		InVisualizer->OnPointsChanged.AddUObject(this, &UGeomBridge::MarkPointsDirty_Internal);
		InVisualizer->OnResultChanged.AddUObject(this, &UGeomBridge::MarkResultDirty_Internal);
	}
}

void UGeomBridge::Exec(const FString& Script) const
{
	if (TSharedPtr<SWebBrowser> B = Browser.Pin())
	{
		B->ExecuteJavascript(Script);
	}
}

void UGeomBridge::Flush(float DeltaSeconds)
{
	if (!bReady) return;
	SinceLastPush += DeltaSeconds;
	SinceLastCursor += DeltaSeconds;

	AGeomVisualizer* V = Visualizer.Get();
	if (V && (bPointsDirty || bResultDirty) && SinceLastPush >= 0.05f)
	{
		if (bPointsDirty) Exec(FString::Printf(TEXT("window.onGeomPoints&&window.onGeomPoints(%s)"), *V->PointsToJson()));
		if (bResultDirty) Exec(FString::Printf(TEXT("window.onGeomResult&&window.onGeomResult(%s)"), *V->ResultToJson()));
		bPointsDirty = bResultDirty = false;
		SinceLastPush = 0.f;
	}

	AGeomTestPlayerController* C = Controller.Get();
	if (C && SinceLastCursor >= 0.1f)
	{
		const FVector2D Cursor = C->GetCursorOnPlane();
		if (!Cursor.Equals(LastSentCursor, 0.5))
		{
			Exec(FString::Printf(TEXT("window.onGeomCursor&&window.onGeomCursor(%.0f,%.0f)"), Cursor.X, Cursor.Y));
			LastSentCursor = Cursor;
		}
		SinceLastCursor = 0.f;
	}
}

void UGeomBridge::PushState()
{
	Exec(FString::Printf(TEXT("window.onGeomState&&window.onGeomState({\"algo\":\"%s\",\"tool\":\"%s\",\"kernel\":%s})"),
		*Algorithm, *Tool, bKernel ? TEXT("true") : TEXT("false")));
}

void UGeomBridge::PushShortcut(const FString& Name)
{
	if (Name.StartsWith(TEXT("tool:"))) Tool = Name.Mid(5);
	Exec(FString::Printf(TEXT("window.onGeomShortcut&&window.onGeomShortcut(\"%s\")"), *Name));
}

// ---------------------------------------------------------------- Appels de la page

void UGeomBridge::Ready()
{
	bReady = true;
	UE_LOG(LogMaths_T1, Log, TEXT("Panneau web prêt."));
	PushState();
	bPointsDirty = bResultDirty = true;
	SinceLastPush = 1.f; // envoi immédiat au prochain Flush
}

void UGeomBridge::SetAlgorithm(const FString& Id)
{
	EGeomAlgorithm A = EGeomAlgorithm::Jarvis;
	if (Id == TEXT("jarvis")) A = EGeomAlgorithm::Jarvis;
	else if (Id == TEXT("graham")) A = EGeomAlgorithm::Graham;
	else if (Id == TEXT("incr")) A = EGeomAlgorithm::Incremental;
	else if (Id == TEXT("delaunay")) A = EGeomAlgorithm::Delaunay;
	else { UE_LOG(LogMaths_T1, Warning, TEXT("Algorithme inconnu : %s"), *Id); return; }

	Algorithm = Id;
	if (AGeomVisualizer* V = Visualizer.Get()) V->SetAlgorithm(A);
}

void UGeomBridge::SetPoints(const FString& Packed)
{
	TArray<int32> Ids;
	TArray<FVector2D> Positions;
	TArray<FString> Items;
	Packed.ParseIntoArray(Items, TEXT(";"));
	for (const FString& Item : Items)
	{
		TArray<FString> V;
		Item.ParseIntoArray(V, TEXT(","));
		if (V.Num() == 3)
		{
			Ids.Add(FCString::Atoi(*V[0]));
			Positions.Emplace(FCString::Atod(*V[1]), FCString::Atod(*V[2]));
		}
	}
	if (AGeomVisualizer* Vis = Visualizer.Get()) Vis->SetAllPoints(Ids, Positions);
}

void UGeomBridge::SetLayer(const FString& Name, bool bVisible)
{
	if (AGeomVisualizer* V = Visualizer.Get()) V->SetLayer(Name, bVisible);
}

void UGeomBridge::SetTool(const FString& InTool)
{
	Tool = InTool;
	AGeomTestPlayerController* C = Controller.Get();
	if (!C) return;
	if (InTool == TEXT("move")) C->SetTool(EGeomTool::Move);
	else if (InTool == TEXT("del")) C->SetTool(EGeomTool::Delete);
	else C->SetTool(EGeomTool::Add);
}

void UGeomBridge::SetKernelMode(bool bEnabled)
{
	bKernel = bEnabled;
	if (AGeomVisualizer* V = Visualizer.Get()) V->SetKernelMode(bEnabled);
}

void UGeomBridge::SetOverlay(const FString& Name, const FString& Packed, const FString& ColorHex)
{
	if (AGeomVisualizer* V = Visualizer.Get()) V->SetOverlay(Name, Packed, FColor::FromHex(ColorHex));
}

void UGeomBridge::FrameAll()
{
	if (AGeomTestPlayerController* C = Controller.Get()) C->FrameAll();
}

void UGeomBridge::Recompute()
{
	if (AGeomVisualizer* V = Visualizer.Get()) V->Recompute();
}

void UGeomBridge::Log(const FString& Message)
{
	UE_LOG(LogMaths_T1, Log, TEXT("[Web] %s"), *Message);
}
