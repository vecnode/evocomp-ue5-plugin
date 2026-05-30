#include "EvoCompImageEvolutionAlgorithm.h"

#include "Algo/Sort.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "MaterialShared.h"
#include "Math/UnrealMathUtility.h"
#include "HAL/PlatformProcess.h"
#include "Modules/ModuleManager.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UObjectIterator.h"
#include "Containers/Ticker.h"
#include "Containers/Set.h"

#if WITH_EDITORONLY_DATA
#include "Engine/TextureDefines.h"
#endif

namespace
{
	static constexpr float GrayscaleR = 0.299f;

	static constexpr float GrayscaleG = 0.587f;
	static constexpr float GrayscaleB = 0.114f;

		static void GetTextureParameterNames(UMaterialInterface* Material, TSet<FName>& OutNames)
		{
			OutNames.Reset();
			if (!Material)
			{
				return;
			}

			TArray<FMaterialParameterInfo> Infos;
			TArray<FGuid> Ids;
			Material->GetAllTextureParameterInfo(Infos, Ids);
			for (const FMaterialParameterInfo& Info : Infos)
			{
				OutNames.Add(Info.Name);
			}
		}

		static UTexture2D* ResolveDefaultTargetTexture()
		{
			static const TCHAR* CandidatePaths[] =
			{
				TEXT("/EvoCompPlugin/Targets/image1.image1"),
				TEXT("Texture2D'/EvoCompPlugin/Targets/image1.image1'"),
				TEXT("/Game/EvoCompPlugin/Targets/image1.image1")
			};

			for (const TCHAR* Path : CandidatePaths)
			{
				if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, Path))
				{
					return Texture;
				}
			}

			return nullptr;
		}

	static void UploadPreviewPixels(UTexture2D* Texture, const TArray<FColor>& Pixels, int32 Resolution)
	{
		if (!Texture || Pixels.Num() != (Resolution * Resolution) || Resolution <= 0)
		{
			return;
		}

		if (!Texture->GetPlatformData() || Texture->GetPlatformData()->Mips.IsEmpty())
		{
			return;
		}

		FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
		const int32 ByteCount = Pixels.Num() * static_cast<int32>(sizeof(FColor));
		void* MipData = Mip.BulkData.Lock(LOCK_READ_WRITE);
		if (!MipData)
		{
			Mip.BulkData.Unlock();
			return;
		}

		FMemory::Memcpy(MipData, Pixels.GetData(), ByteCount);
		Mip.BulkData.Unlock();

		// Synchronous resource refresh is more reliable for editor preview worlds.
		Texture->UpdateResource();
		Texture->RefreshSamplerStates();
	}

	static int32 ApplyTextureToMaterial(UMaterialInstanceDynamic* Material, UTexture* Texture, const FName& PreferredParameter)
	{
		if (!Material)
		{
			return 0;
		}

		TSet<FName> AvailableTextureParameters;
		GetTextureParameterNames(Material, AvailableTextureParameters);

		// Force common UI/material tint and opacity controls to visible defaults.
		Material->SetVectorParameterValue(TEXT("TintColorAndOpacity"), FLinearColor::White);
		Material->SetVectorParameterValue(TEXT("ColorAndOpacity"), FLinearColor::White);
		Material->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor::White);
		Material->SetScalarParameterValue(TEXT("Opacity"), 1.0f);
		Material->SetScalarParameterValue(TEXT("OpacityFromTexture"), 1.0f);
		Material->SetScalarParameterValue(TEXT("UseTextureAlpha"), 1.0f);

			int32 AppliedCount = 0;
			TSet<FName> AppliedNames;
			auto SetParam = [&](const FName& ParameterName)
		{
				if (ParameterName.IsNone() || AppliedNames.Contains(ParameterName))
			{
				return;
			}

			Material->SetTextureParameterValue(ParameterName, Texture);

				UTexture* BoundTexture = Material->K2_GetTextureParameterValue(ParameterName);
				if (BoundTexture == Texture)
				{
					++AppliedCount;
				}
				AppliedNames.Add(ParameterName);
		};

			SetParam(PreferredParameter);
			SetParam(TEXT("SlateUI"));
			SetParam(TEXT("SpriteTexture"));
			SetParam(TEXT("Texture"));
			SetParam(TEXT("BaseTexture"));
			SetParam(TEXT("BaseColorTexture"));

			for (const FName& RemainingParameter : AvailableTextureParameters)
		{
				SetParam(RemainingParameter);
		}

		return AppliedCount;
	}

	static UMaterialInterface* ResolveValidMaterialParent(UMaterialInterface* Candidate)
	{
		UMaterialInterface* Current = Candidate;
		while (UMaterialInstanceDynamic* DynamicParent = Cast<UMaterialInstanceDynamic>(Current))
		{
			Current = DynamicParent->Parent;
		}

		return Current;
	}

	static bool IsWidgetPassThroughMaterial(UMaterialInterface* Candidate)
	{
		const UMaterialInterface* Material = ResolveValidMaterialParent(Candidate);
		if (!Material)
		{
			return false;
		}

		const FString MaterialName = Material->GetName();
		return MaterialName.Contains(TEXT("Widget3DPassThrough"));
	}

	static void CopyMaterialParameters(UMaterialInstanceDynamic* SourceMID, UMaterialInstanceDynamic* DestMID)
	{
		if (!SourceMID || !DestMID)
		{
			return;
		}

		TArray<FMaterialParameterInfo> ScalarInfos;
		TArray<FGuid> ScalarIds;
		SourceMID->GetAllScalarParameterInfo(ScalarInfos, ScalarIds);
		for (const FMaterialParameterInfo& Info : ScalarInfos)
		{
			DestMID->SetScalarParameterValue(Info.Name, SourceMID->K2_GetScalarParameterValue(Info.Name));
		}

		TArray<FMaterialParameterInfo> VectorInfos;
		TArray<FGuid> VectorIds;
		SourceMID->GetAllVectorParameterInfo(VectorInfos, VectorIds);
		for (const FMaterialParameterInfo& Info : VectorInfos)
		{
			DestMID->SetVectorParameterValue(Info.Name, SourceMID->K2_GetVectorParameterValue(Info.Name));
		}

		TArray<FMaterialParameterInfo> TextureInfos;
		TArray<FGuid> TextureIds;
		SourceMID->GetAllTextureParameterInfo(TextureInfos, TextureIds);
		for (const FMaterialParameterInfo& Info : TextureInfos)
		{
			DestMID->SetTextureParameterValue(Info.Name, SourceMID->K2_GetTextureParameterValue(Info.Name));
		}
	}

	static void ConfigureEvolvingPlaneRendering(UStaticMeshComponent* Plane)
	{
		if (!Plane)
		{
			return;
		}

		// Keep evolving-plane visibility and pass flags deterministic across lifecycle/update calls.
		Plane->SetReverseCulling(false);
		Plane->SetRenderInMainPass(true);
		Plane->SetRenderInDepthPass(true);
		Plane->SetVisibleInSceneCaptureOnly(false);
		Plane->SetHiddenInSceneCapture(false);
		Plane->SetOwnerNoSee(false);
		Plane->SetOnlyOwnerSee(false);
		Plane->SetRenderCustomDepth(false);
		Plane->SetVisibility(true, true);
		Plane->SetHiddenInGame(false, true);
	}

	static void ApplyMaterialToAllSlots(UStaticMeshComponent* MeshComponent, UMaterialInterface* Material)
	{
		if (!MeshComponent || !Material)
		{
			return;
		}

		const int32 SlotCount = FMath::Max(1, MeshComponent->GetNumMaterials());
		for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
		{
			MeshComponent->SetMaterial(SlotIndex, Material);
		}

		MeshComponent->MarkRenderDynamicDataDirty();
		MeshComponent->MarkRenderStateDirty();
	}

	static AEvoCompImageEvolutionAlgorithm* ResolveActionTargetInstance(AEvoCompImageEvolutionAlgorithm* Source, const TCHAR* ActionName)
	{
		if (!Source)
		{
			return nullptr;
		}

		if (!Source->IsTemplate())
		{
			return Source;
		}

		AEvoCompImageEvolutionAlgorithm* BestCandidate = nullptr;
		for (TObjectIterator<AEvoCompImageEvolutionAlgorithm> It; It; ++It)
		{
			AEvoCompImageEvolutionAlgorithm* Candidate = *It;
			if (!Candidate || Candidate == Source || Candidate->IsTemplate())
			{
				continue;
			}

			if (!Candidate->IsA(Source->GetClass()))
			{
				continue;
			}

			UWorld* World = Candidate->GetWorld();
			if (!World)
			{
				continue;
			}

			if (World->WorldType == EWorldType::PIE || World->WorldType == EWorldType::Game)
			{
				UE_LOG(LogTemp, Display,
					TEXT("[IMAGE-EVO] %s called on CDO. Forwarding to instance %s in world %s."),
					ActionName,
					*Candidate->GetPathName(),
					*World->GetName());
				return Candidate;
			}

			if (!BestCandidate && World->WorldType == EWorldType::Editor)
			{
				BestCandidate = Candidate;
			}
		}

		if (BestCandidate)
		{
			UE_LOG(LogTemp, Display,
				TEXT("[IMAGE-EVO] %s called on CDO. Forwarding to editor instance %s in world %s."),
				ActionName,
				*BestCandidate->GetPathName(),
				*BestCandidate->GetWorld()->GetName());
			return BestCandidate;
		}

		UE_LOG(LogTemp, Warning,
			TEXT("[IMAGE-EVO] %s called on CDO for class %s but no actor instance was found in any world. Place an instance of this Blueprint in a level and trigger the action from that instance."),
			ActionName,
			*Source->GetClass()->GetName());
		return nullptr;
	}

	template <typename CallbackType>
	static bool ForwardActionIfTemplate(
		AEvoCompImageEvolutionAlgorithm* Source,
		const TCHAR* ActionName,
		CallbackType&& Callback)
	{
		if (!Source || !Source->IsTemplate())
		{
			return false;
		}

		if (AEvoCompImageEvolutionAlgorithm* Target = ResolveActionTargetInstance(Source, ActionName))
		{
			Callback(Target);
		}

		// Returning true means this call originated from a template object and was handled.
		return true;
	}
}

AEvoCompImageEvolutionAlgorithm::AEvoCompImageEvolutionAlgorithm()
{
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMeshFinder(TEXT("/Engine/BasicShapes/Plane.Plane"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DefaultViewerMaterialFinderSprite(TEXT("/Paper2D/MaskedUnlitSpriteMaterial.MaskedUnlitSpriteMaterial"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DefaultViewerMaterialFinderBasicShape(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DefaultViewerMaterialFinderOpaque(TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Opaque.Widget3DPassThrough_Opaque"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DefaultViewerMaterialFinderTranslucent(TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Translucent.Widget3DPassThrough_Translucent"));

	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(SceneRoot);

	TargetImagePlane = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TargetImagePlane"));
	TargetImagePlane->SetupAttachment(SceneRoot);
	TargetImagePlane->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	TargetImagePlane->SetMobility(EComponentMobility::Movable);
	TargetImagePlane->SetGenerateOverlapEvents(false);
	TargetImagePlane->SetCanEverAffectNavigation(false);
	TargetImagePlane->SetCastShadow(false);

	EvolvingImagePlane = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EvolvingImagePlane"));
	EvolvingImagePlane->SetupAttachment(SceneRoot);
	EvolvingImagePlane->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	EvolvingImagePlane->SetMobility(EComponentMobility::Movable);
	EvolvingImagePlane->SetGenerateOverlapEvents(false);
	EvolvingImagePlane->SetCanEverAffectNavigation(false);
	EvolvingImagePlane->SetCastShadow(false);
	ConfigureEvolvingPlaneRendering(EvolvingImagePlane);

	TargetImagePlane->SetVisibility(true, true);
	TargetImagePlane->SetHiddenInGame(false, true);
	EvolvingImagePlane->SetVisibility(true, true);
	EvolvingImagePlane->SetHiddenInGame(false, true);

	if (PlaneMeshFinder.Succeeded())
	{
		TargetImagePlane->SetStaticMesh(PlaneMeshFinder.Object);
		EvolvingImagePlane->SetStaticMesh(PlaneMeshFinder.Object);
	}

	if (DefaultViewerMaterialFinderSprite.Succeeded())
	{
		ViewerMaterialTemplate = DefaultViewerMaterialFinderSprite.Object;
	}
	else if (DefaultViewerMaterialFinderBasicShape.Succeeded())
	{
		ViewerMaterialTemplate = DefaultViewerMaterialFinderBasicShape.Object;
	}
	else if (DefaultViewerMaterialFinderOpaque.Succeeded())
	{
		ViewerMaterialTemplate = DefaultViewerMaterialFinderOpaque.Object;
	}
	else if (DefaultViewerMaterialFinderTranslucent.Succeeded())
	{
		ViewerMaterialTemplate = DefaultViewerMaterialFinderTranslucent.Object;
	}

	UpdateViewerLayout();
}

void AEvoCompImageEvolutionAlgorithm::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (!TargetTexture)
	{
		TargetTexture = ResolveDefaultTargetTexture();
	}

	if (!BestGenerationTexture && (bEvolutionInitialized || CurrentGeneration > 0 || BestFitness > 0.0f))
	{
		// Clear stale runtime state that may come from previously serialized actor/blueprint defaults.
		ResetAlgorithmState();
		GenerationBestFitness = 0.0f;
		AverageFitness = 0.0f;
		GenerationError = 1.0f;
		StableGenerationCount = 0;
		bEvolutionInitialized = false;
	}

	if (!bEvolutionInitialized && !BestGenerationTexture)
	{
		EnsurePreviewTexture();
		ClearPreviewTexture(127);
	}

	EnsureViewerMaterials();
	UpdateViewerLayout();
	UpdateTargetPlaneTexture();
	UpdateEvolvingPlaneTexture();

	ConfigureEvolvingPlaneRendering(EvolvingImagePlane);

	if (!bAutoInitializePreviewOnConstruction || !TargetTexture || BestGenerationTexture)
	{
		return;
	}

	const bool bWasRealtimeEnabled = bRealtimeEvolution;
	bRealtimeEvolution = false;
	InitializeEvolution();
	bRealtimeEvolution = bWasRealtimeEnabled;
}

void AEvoCompImageEvolutionAlgorithm::BeginPlay()
{
	Super::BeginPlay();

	if (!TargetTexture)
	{
		TargetTexture = ResolveDefaultTargetTexture();
	}

	EnsureViewerMaterials();
	UpdateViewerLayout();
	UpdateTargetPlaneTexture();
	UpdateEvolvingPlaneTexture();

	ConfigureEvolvingPlaneRendering(EvolvingImagePlane);
}

bool AEvoCompImageEvolutionAlgorithm::ShouldTickIfViewportsOnly() const
{
	return true;
}

void AEvoCompImageEvolutionAlgorithm::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bRealtimeEvolution)
	{
		return;
	}

	if (!bEvolutionInitialized)
	{
		return;
	}

	const int32 SafeMaxGenerations = SanitizePositiveInt(MaxGenerations);
	if (CurrentGeneration >= SafeMaxGenerations || ShouldStopEarly())
	{
		StopAndLogRunComplete();
		return;
	}

	const int32 SafeGenerationsPerTick = SanitizePositiveInt(GenerationsPerTick);
	for (int32 Index = 0; Index < SafeGenerationsPerTick; ++Index)
	{
		if (CurrentGeneration >= SafeMaxGenerations || ShouldStopEarly())
		{
			StopAndLogRunComplete();
			break;
		}

		StepOneGeneration();
	}
}

void AEvoCompImageEvolutionAlgorithm::InitializeEvolution()
{
	if (ForwardActionIfTemplate(this, TEXT("InitializeEvolution"), [](AEvoCompImageEvolutionAlgorithm* Target)
	{
		Target->InitializeEvolution();
	}))
	{
		return;
	}

	ResetEvolutionRuntimeState();

	if (!TargetTexture)
	{
		TargetTexture = ResolveDefaultTargetTexture();
		if (TargetTexture)
		{
			UE_LOG(LogTemp, Display, TEXT("[IMAGE-EVO] Recovered missing TargetTexture from default plugin asset: %s"), *TargetTexture->GetPathName());
		}
	}

	UpdateTargetPlaneTexture();

	if (bUseDeterministicSeed)
	{
		DeterministicRandom.Initialize(FMath::Max(0, RandomSeed));
	}

	const bool bPreparedTarget = PrepareTargetPatchMap();
	if (!bPreparedTarget)
	{
		UE_LOG(LogTemp, Warning, TEXT("[IMAGE-EVO] Initialization failed. Assign a valid target texture in plugin content."));
		return;
	}

	InitializeRandomPopulation();
	EvaluatePopulation();
	bEvolutionInitialized = true;

	UE_LOG(LogTemp, Display,
		TEXT("[IMAGE-EVO] Initialized | Pop=%d | Genome=%d | Grid=%dx%d | Preview=%d"),
		ImagePopulation.Num(),
		GenomeLength,
		PatchColumns,
		PatchRows,
		PreviewResolution);
}

void AEvoCompImageEvolutionAlgorithm::StepOneGeneration()
{
	if (ForwardActionIfTemplate(this, TEXT("StepOneGeneration"), [](AEvoCompImageEvolutionAlgorithm* Target)
	{
		Target->StepOneGeneration();
	}))
	{
		return;
	}

	if (!bEvolutionInitialized)
	{
		InitializeEvolution();
		if (!bEvolutionInitialized)
		{
			return;
		}
	}

	if (CurrentGeneration >= SanitizePositiveInt(MaxGenerations))
	{
		UE_LOG(LogTemp, Warning, TEXT("[IMAGE-EVO] MaxGenerations reached (%d)."), SanitizePositiveInt(MaxGenerations));
		return;
	}

	if (ShouldStopEarly())
	{
		UE_LOG(LogTemp, Display,
			TEXT("[IMAGE-EVO] Early stop criteria already met at generation %d (Best=%.5f)."),
			CurrentGeneration,
			BestFitness);
		return;
	}

	BuildNextGeneration();
	++CurrentGeneration;
	EvaluatePopulation();
}

void AEvoCompImageEvolutionAlgorithm::RunEvolution()
{
	if (ForwardActionIfTemplate(this, TEXT("RunEvolution"), [](AEvoCompImageEvolutionAlgorithm* Target)
	{
		Target->RunEvolution();
	}))
	{
		return;
	}

	if (!bEvolutionInitialized)
	{
		InitializeEvolution();
		if (!bEvolutionInitialized)
		{
			return;
		}
	}

	const int32 SafeMaxGenerations = SanitizePositiveInt(MaxGenerations);
	if (CurrentGeneration >= SafeMaxGenerations)
	{
		UE_LOG(LogTemp, Warning, TEXT("[IMAGE-EVO] MaxGenerations reached (%d)."), SafeMaxGenerations);
		return;
	}

	if (ShouldStopEarly())
	{
		UE_LOG(LogTemp, Display,
			TEXT("[IMAGE-EVO] Early stop criteria already met at generation %d (Best=%.5f)."),
			CurrentGeneration,
			BestFitness);
		return;
	}

	if (!bRealtimeEvolution)
	{
		SetRealtimeEvolutionEnabled(true);
	}

	// Run a small burst immediately so the user sees visible change right after pressing Run.
	const int32 WarmStartSteps = FMath::Clamp(SanitizePositiveInt(GenerationsPerTick), 1, 8);
	for (int32 Index = 0; Index < WarmStartSteps; ++Index)
	{
		if (CurrentGeneration >= SafeMaxGenerations || ShouldStopEarly())
		{
			break;
		}

		StepOneGeneration();
	}

	UE_LOG(LogTemp, Display,
		TEXT("[IMAGE-EVO] Realtime run started | Generation=%d | GenerationsPerTick=%d"),
		CurrentGeneration,
		SanitizePositiveInt(GenerationsPerTick));

	StartRealtimeTickerFallback();
}

void AEvoCompImageEvolutionAlgorithm::ResetEvolution()
{
	if (ForwardActionIfTemplate(this, TEXT("ResetEvolution"), [](AEvoCompImageEvolutionAlgorithm* Target)
	{
		Target->ResetEvolution();
	}))
	{
		return;
	}

	ResetEvolutionRuntimeState();

	EnsurePreviewTexture();
	ClearPreviewTexture(127);
	UpdateTargetPlaneTexture();
	UpdateEvolvingPlaneTexture();
	StopRealtimeTickerFallback();

	UE_LOG(LogTemp, Display, TEXT("[IMAGE-EVO] State reset."));
}

void AEvoCompImageEvolutionAlgorithm::SetRealtimeEvolutionEnabled(bool bEnabled)
{
	if (ForwardActionIfTemplate(this, TEXT("SetRealtimeEvolutionEnabled"), [bEnabled](AEvoCompImageEvolutionAlgorithm* Target)
	{
		Target->SetRealtimeEvolutionEnabled(bEnabled);
	}))
	{
		return;
	}

	bRealtimeEvolution = bEnabled;
	if (!bRealtimeEvolution)
	{
		StopRealtimeTickerFallback();
	}
	UE_LOG(LogTemp, Display, TEXT("[IMAGE-EVO] Realtime evolution %s."), bRealtimeEvolution ? TEXT("enabled") : TEXT("disabled"));
}

void AEvoCompImageEvolutionAlgorithm::StartRealtimeTickerFallback()
{
	if (!bRealtimeEvolution || RealtimeTickerHandle.IsValid())
	{
		return;
	}

	const UWorld* World = GetWorld();
	const bool bWorldLikelyTicksActor = (World != nullptr) && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
	if (bWorldLikelyTicksActor)
	{
		return;
	}

	RealtimeTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &AEvoCompImageEvolutionAlgorithm::HandleRealtimeTickerFallback),
		0.0f);
}

void AEvoCompImageEvolutionAlgorithm::StopRealtimeTickerFallback()
{
	if (!RealtimeTickerHandle.IsValid())
	{
		return;
	}

	FTSTicker::GetCoreTicker().RemoveTicker(RealtimeTickerHandle);
	RealtimeTickerHandle.Reset();
}

bool AEvoCompImageEvolutionAlgorithm::HandleRealtimeTickerFallback(float DeltaTime)
{
	// Fallback ticker keeps editor-preview runs responsive when world tick is not driving this actor.
	if (!bRealtimeEvolution)
	{
		return false;
	}

	if (!bEvolutionInitialized)
	{
		return true;
	}

	const int32 SafeMaxGenerations = SanitizePositiveInt(MaxGenerations);
	if (CurrentGeneration >= SafeMaxGenerations || ShouldStopEarly())
	{
		StopAndLogRunComplete();
		return false;
	}

	const int32 SafeGenerationsPerTick = SanitizePositiveInt(GenerationsPerTick);
	for (int32 Index = 0; Index < SafeGenerationsPerTick; ++Index)
	{
		if (CurrentGeneration >= SafeMaxGenerations || ShouldStopEarly())
		{
			StopAndLogRunComplete();
			return false;
		}

		StepOneGeneration();
	}

	return true;
}

void AEvoCompImageEvolutionAlgorithm::ResetEvolutionRuntimeState()
{
	ResetAlgorithmState();
	bEvolutionInitialized = false;
	GenerationBestFitness = 0.0f;
	AverageFitness = 0.0f;
	GenerationError = 1.0f;
	StableGenerationCount = 0;
	ImagePopulation.Reset();
	RankedIndices.Reset();
	CurrentBestGenome.Reset();
	TargetPatchValues.Reset();
}

void AEvoCompImageEvolutionAlgorithm::StopAndLogRunComplete()
{
	bRealtimeEvolution = false;
	UE_LOG(LogTemp, Display,
		TEXT("[IMAGE-EVO] Run complete | Generation=%d | BestFitness=%.5f | Stable=%d"),
		CurrentGeneration,
		BestFitness,
		StableGenerationCount);
}

bool AEvoCompImageEvolutionAlgorithm::PrepareTargetPatchMap()
{
	const int32 SafePatchColumns = FMath::Max(PatchColumns, 2);
	const int32 SafePatchRows = FMath::Max(PatchRows, 2);
	GenomeLength = SafePatchColumns * SafePatchRows;

	if (GenomeLength <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[IMAGE-EVO] Invalid genome length from patch setup."));
		return false;
	}

	if (!TargetTexture)
	{
		TargetTexture = ResolveDefaultTargetTexture();
		if (TargetTexture)
		{
			UE_LOG(LogTemp, Display,
				TEXT("[IMAGE-EVO] Recovered TargetTexture during patch-map prep: %s"),
				*TargetTexture->GetPathName());
			UpdateTargetPlaneTexture();
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[IMAGE-EVO] TargetTexture is null."));
			return false;
		}
	}

#if WITH_EDITORONLY_DATA
	if (!TargetTexture->Source.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[IMAGE-EVO] TargetTexture source data is invalid for %s."), *TargetTexture->GetName());
		return false;
	}

	TArray64<uint8> SourcePixels;
	if (!TargetTexture->Source.GetMipData(SourcePixels, 0))
	{
		UE_LOG(LogTemp, Warning, TEXT("[IMAGE-EVO] Failed reading source mip data for %s."), *TargetTexture->GetName());
		return false;
	}

	const int32 Width = TargetTexture->Source.GetSizeX();
	const int32 Height = TargetTexture->Source.GetSizeY();
	if (Width <= 0 || Height <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[IMAGE-EVO] TargetTexture has invalid dimensions."));
		return false;
	}

	const ETextureSourceFormat SourceFormat = TargetTexture->Source.GetFormat();
	const int32 BytesPerPixel = TargetTexture->Source.GetBytesPerPixel();
	if (BytesPerPixel <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[IMAGE-EVO] Unsupported target bytes-per-pixel."));
		return false;
	}

	auto ReadGrayAt = [&](int32 X, int32 Y) -> float
	{
		const int64 Index = static_cast<int64>((Y * Width) + X) * BytesPerPixel;
		if (Index < 0 || (Index + BytesPerPixel) > SourcePixels.Num())
		{
			return 0.0f;
		}

		const uint8* Pixel = SourcePixels.GetData() + Index;

		switch (SourceFormat)
		{
		case TSF_G8:
			return static_cast<float>(Pixel[0]) / 255.0f;
		case TSF_BGRA8:
		{
			const float B = static_cast<float>(Pixel[0]) / 255.0f;
			const float G = static_cast<float>(Pixel[1]) / 255.0f;
			const float R = static_cast<float>(Pixel[2]) / 255.0f;
			return (GrayscaleR * R) + (GrayscaleG * G) + (GrayscaleB * B);
		}
		default:
			return 0.0f;
		}
	};

	if (SourceFormat != TSF_G8 && SourceFormat != TSF_BGRA8)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[IMAGE-EVO] Unsupported source format for %s. Use G8 or BGRA8 texture source."),
			*TargetTexture->GetName());
		return false;
	}

	TargetPatchValues.SetNumUninitialized(GenomeLength);

	for (int32 PatchY = 0; PatchY < SafePatchRows; ++PatchY)
	{
		const int32 YStart = (PatchY * Height) / SafePatchRows;
		const int32 YEndExclusive = FMath::Max(((PatchY + 1) * Height) / SafePatchRows, YStart + 1);

		for (int32 PatchX = 0; PatchX < SafePatchColumns; ++PatchX)
		{
			const int32 XStart = (PatchX * Width) / SafePatchColumns;
			const int32 XEndExclusive = FMath::Max(((PatchX + 1) * Width) / SafePatchColumns, XStart + 1);

			double Sum = 0.0;
			int32 Samples = 0;
			for (int32 Y = YStart; Y < YEndExclusive; ++Y)
			{
				for (int32 X = XStart; X < XEndExclusive; ++X)
				{
					Sum += static_cast<double>(ReadGrayAt(X, Y));
					++Samples;
				}
			}

			const int32 GenomeIndex = (PatchY * SafePatchColumns) + PatchX;
			TargetPatchValues[GenomeIndex] = Samples > 0 ? static_cast<float>(Sum / static_cast<double>(Samples)) : 0.0f;
		}
	}

	return true;
#else
	UE_LOG(LogTemp, Warning, TEXT("[IMAGE-EVO] Target patch preparation requires editor source texture data."));
	return false;
#endif
}

void AEvoCompImageEvolutionAlgorithm::InitializeRandomPopulation()
{
	const int32 SafePopulationSize = SanitizePositiveInt(PopulationSize);

	ImagePopulation.SetNum(SafePopulationSize);
	for (FImageIndividual& Individual : ImagePopulation)
	{
		Individual.Genome.SetNumUninitialized(GenomeLength);
		for (float& Gene : Individual.Genome)
		{
			Gene = DrawUnitRandom();
		}

		Individual.Fitness = 0.0f;
	}
}

void AEvoCompImageEvolutionAlgorithm::EvaluatePopulation()
{
	if (ImagePopulation.IsEmpty() || TargetPatchValues.Num() != GenomeLength)
	{
		return;
	}

	double FitnessSum = 0.0;
	for (FImageIndividual& Individual : ImagePopulation)
	{
		Individual.Fitness = EvaluateFitness(Individual.Genome);
		FitnessSum += static_cast<double>(Individual.Fitness);
	}

	RankedIndices.SetNumUninitialized(ImagePopulation.Num());
	for (int32 Index = 0; Index < ImagePopulation.Num(); ++Index)
	{
		RankedIndices[Index] = Index;
	}

	Algo::Sort(RankedIndices, [&](int32 LeftIndex, int32 RightIndex)
	{
		return ImagePopulation[LeftIndex].Fitness > ImagePopulation[RightIndex].Fitness;
	});

	const int32 BestIndex = RankedIndices[0];
	GenerationBestFitness = ImagePopulation[BestIndex].Fitness;
	AverageFitness = static_cast<float>(FitnessSum / static_cast<double>(ImagePopulation.Num()));
	GenerationError = 1.0f - GenerationBestFitness;

	if (GenerationBestFitness >= SanitizeUnitFloat(FitnessThreshold))
	{
		++StableGenerationCount;
	}
	else
	{
		StableGenerationCount = 0;
	}

	if (GenerationBestFitness > BestFitness)
	{
		BestFitness = GenerationBestFitness;
	}

	CurrentBestGenome = ImagePopulation[BestIndex].Genome;
	UpdatePreviewTexture(CurrentBestGenome);

	UE_LOG(LogTemp, Display,
		TEXT("[IMAGE-EVO] Gen=%d | Best=%.5f | Avg=%.5f | Err=%.5f"),
		CurrentGeneration,
		GenerationBestFitness,
		AverageFitness,
		GenerationError);
}

void AEvoCompImageEvolutionAlgorithm::BuildNextGeneration()
{
	if (ImagePopulation.IsEmpty() || RankedIndices.IsEmpty())
	{
		return;
	}

	const int32 BestIndex = RankedIndices[0];
	const int32 SecondIndex = RankedIndices.Num() > 1 ? RankedIndices[1] : RankedIndices[0];
	const TArray<float>& BestGenome = ImagePopulation[BestIndex].Genome;
	const TArray<float>& SecondGenome = ImagePopulation[SecondIndex].Genome;

	const int32 SafePopulationSize = SanitizePositiveInt(PopulationSize);
	const int32 SafeTopPool = FMath::Clamp(TopRankParentPoolSize, 2, RankedIndices.Num());
	const float SafeBorrowRate = SanitizeUnitFloat(SecondBestGeneBorrowRate);
	const float SafeMutationRate = SanitizeUnitFloat(MutationRate);
	const float SafeResetChance = SanitizeUnitFloat(MutationResetChance);
	const float SafeMutationStep = FMath::Max(0.001f, MutationStepSize);

	TArray<FImageIndividual> NextPopulation;
	NextPopulation.Reserve(SafePopulationSize);

	if (bEnableElitism)
	{
		FImageIndividual Elite;
		Elite.Genome = BestGenome;
		Elite.Fitness = ImagePopulation[BestIndex].Fitness;
		NextPopulation.Add(MoveTemp(Elite));
	}

	while (NextPopulation.Num() < SafePopulationSize)
	{
		FImageIndividual Child;
		Child.Genome = BestGenome;
		Child.Fitness = 0.0f;

		const int32 RankSampleA = RankedIndices[DrawRankBasedIndex(SafeTopPool)];
		const int32 RankSampleB = RankedIndices[DrawRankBasedIndex(SafeTopPool)];
		const TArray<float>& DonorA = ImagePopulation[RankSampleA].Genome;
		const TArray<float>& DonorB = ImagePopulation[RankSampleB].Genome;

		for (int32 GeneIndex = 0; GeneIndex < Child.Genome.Num(); ++GeneIndex)
		{
			if (DrawUnitRandom() < SafeBorrowRate)
			{
				// Core literature-driven operation requested: inject genes from the next-highest fitness individual.
				Child.Genome[GeneIndex] = SecondGenome[GeneIndex];
			}

			if (DrawUnitRandom() < 0.5f)
			{
				Child.Genome[GeneIndex] = DonorA[GeneIndex];
			}
			if (DrawUnitRandom() < 0.35f)
			{
				Child.Genome[GeneIndex] = DonorB[GeneIndex];
			}

			if (DrawUnitRandom() < SafeMutationRate)
			{
				if (DrawUnitRandom() < SafeResetChance)
				{
					Child.Genome[GeneIndex] = DrawUnitRandom();
				}
				else
				{
					const float Delta = ((DrawUnitRandom() * 2.0f) - 1.0f) * SafeMutationStep;
					Child.Genome[GeneIndex] = FMath::Clamp(Child.Genome[GeneIndex] + Delta, 0.0f, 1.0f);
				}
			}
		}

		NextPopulation.Add(MoveTemp(Child));
	}

	ImagePopulation = MoveTemp(NextPopulation);
}

float AEvoCompImageEvolutionAlgorithm::EvaluateFitness(const TArray<float>& Genome) const
{
	if (Genome.Num() != TargetPatchValues.Num() || Genome.IsEmpty())
	{
		return 0.0f;
	}

	double ErrorSum = 0.0;
	for (int32 Index = 0; Index < Genome.Num(); ++Index)
	{
		ErrorSum += FMath::Abs(Genome[Index] - TargetPatchValues[Index]);
	}

	const float MeanAbsoluteError = static_cast<float>(ErrorSum / static_cast<double>(Genome.Num()));
	return FMath::Clamp(1.0f - MeanAbsoluteError, 0.0f, 1.0f);
}

void AEvoCompImageEvolutionAlgorithm::UpdatePreviewTexture(const TArray<float>& Genome)
{
	if (Genome.Num() != GenomeLength || GenomeLength <= 0)
	{
		return;
	}

	EnsurePreviewTexture();
	if (!BestGenerationTexture || !BestGenerationTexture->GetPlatformData() || BestGenerationTexture->GetPlatformData()->Mips.IsEmpty())
	{
		return;
	}

	const int32 SafeColumns = FMath::Max(PatchColumns, 2);
	const int32 SafeRows = FMath::Max(PatchRows, 2);
	const int32 SafeResolution = FMath::Clamp(PreviewResolution, 64, 2048);
	float MinGene = 1.0f;
	float MaxGene = 0.0f;
	for (const float GeneValue : Genome)
	{
		MinGene = FMath::Min(MinGene, GeneValue);
		MaxGene = FMath::Max(MaxGene, GeneValue);
	}
	const float GeneRange = MaxGene - MinGene;

	TArray<FColor> PreviewPixels;
	PreviewPixels.SetNumUninitialized(SafeResolution * SafeResolution);

	for (int32 Y = 0; Y < SafeResolution; ++Y)
	{
		const int32 PatchY = FMath::Min((Y * SafeRows) / SafeResolution, SafeRows - 1);
		for (int32 X = 0; X < SafeResolution; ++X)
		{
			const int32 PatchX = FMath::Min((X * SafeColumns) / SafeResolution, SafeColumns - 1);
			const int32 GeneIndex = (PatchY * SafeColumns) + PatchX;
			float DisplayGray = Genome[GeneIndex];
			if (GeneRange > KINDA_SMALL_NUMBER)
			{
				// Stretch per-generation intensity range so subtle changes remain visible.
				DisplayGray = (DisplayGray - MinGene) / GeneRange;
			}

			const uint8 GrayByte = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(DisplayGray * 255.0f), 0, 255));
			PreviewPixels[(Y * SafeResolution) + X] = FColor(GrayByte, GrayByte, GrayByte, 255);
		}
	}

	if (CurrentGeneration <= 3 || (CurrentGeneration % 100) == 0)
	{
		UE_LOG(LogTemp, Display,
			TEXT("[IMAGE-EVO] Preview range | Gen=%d | Min=%.4f | Max=%.4f | Range=%.4f"),
			CurrentGeneration,
			MinGene,
			MaxGene,
			GeneRange);
	}

	UploadPreviewPixels(BestGenerationTexture, PreviewPixels, SafeResolution);

	if (EvolvingImagePlane)
	{
		EvolvingImagePlane->MarkRenderDynamicDataDirty();
		EvolvingImagePlane->MarkRenderStateDirty();
	}

	UpdateEvolvingPlaneTexture();
}

void AEvoCompImageEvolutionAlgorithm::UpdateTargetPlaneTexture()
{
	EnsureViewerMaterials();
	if (!TargetPlaneMaterial)
	{
		return;
	}

	const int32 AppliedCount = ApplyTextureToMaterial(TargetPlaneMaterial, TargetTexture, ViewerTextureParameter);

	if (TargetImagePlane)
	{
		ApplyMaterialToAllSlots(TargetImagePlane, TargetPlaneMaterial);
	}

	if (AppliedCount == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[IMAGE-EVO] Target plane material has no texture parameters to bind."));
	}
}

void AEvoCompImageEvolutionAlgorithm::UpdateEvolvingPlaneTexture()
{
	EnsureViewerMaterials();
	if (!EvolvingPlaneMaterial)
	{
		return;
	}

	if (UMaterialInstanceDynamic* TargetMID = Cast<UMaterialInstanceDynamic>(TargetImagePlane ? TargetImagePlane->GetMaterial(0) : nullptr))
	{
		CopyMaterialParameters(TargetMID, EvolvingPlaneMaterial);
	}

	const int32 AppliedCount = ApplyTextureToMaterial(EvolvingPlaneMaterial, BestGenerationTexture, ViewerTextureParameter);

	if (EvolvingImagePlane)
	{
		ApplyMaterialToAllSlots(EvolvingImagePlane, EvolvingPlaneMaterial);
		ConfigureEvolvingPlaneRendering(EvolvingImagePlane);
	}

	if (CurrentGeneration <= 3 || (CurrentGeneration % 100) == 0)
	{
		UMaterialInterface* AssignedMaterial = EvolvingImagePlane ? EvolvingImagePlane->GetMaterial(0) : nullptr;
		const FString ParentMaterialName = EvolvingPlaneMaterial && EvolvingPlaneMaterial->Parent
			? EvolvingPlaneMaterial->Parent->GetName()
			: TEXT("None");
		const FString AssignedMaterialName = AssignedMaterial ? AssignedMaterial->GetName() : TEXT("None");
		const FString ActorName = GetPathName();
		const FString WorldName = GetWorld() ? GetWorld()->GetName() : TEXT("None");

		TSet<FName> TextureParams;
		GetTextureParameterNames(EvolvingPlaneMaterial, TextureParams);
		const bool bSpriteBound = EvolvingPlaneMaterial->K2_GetTextureParameterValue(TEXT("SpriteTexture")) == BestGenerationTexture;
		const bool bSlateBound = EvolvingPlaneMaterial->K2_GetTextureParameterValue(TEXT("SlateUI")) == BestGenerationTexture;
		const bool bTextureBound = EvolvingPlaneMaterial->K2_GetTextureParameterValue(TEXT("Texture")) == BestGenerationTexture;

		UE_LOG(LogTemp, Display,
			TEXT("[IMAGE-EVO v2] Evolving texture bind | Actor=%s | World=%s | Gen=%d | Texture=%s | Param=%s | AppliedParams=%d | MaterialParams=%d | Bound(Sprite=%d,Slate=%d,Tex=%d) | MIDParent=%s | MeshMat=%s | Slots=%d | PlaneValid=%d | PlaneVisible=%d | HiddenInGame=%d"),
			*ActorName,
			*WorldName,
			CurrentGeneration,
			BestGenerationTexture ? *BestGenerationTexture->GetName() : TEXT("None"),
			*ViewerTextureParameter.ToString(),
			AppliedCount,
			TextureParams.Num(),
			bSpriteBound ? 1 : 0,
			bSlateBound ? 1 : 0,
			bTextureBound ? 1 : 0,
			*ParentMaterialName,
			*AssignedMaterialName,
			EvolvingImagePlane ? EvolvingImagePlane->GetNumMaterials() : 0,
			EvolvingImagePlane ? 1 : 0,
			(EvolvingImagePlane && EvolvingImagePlane->IsVisible()) ? 1 : 0,
			(EvolvingImagePlane && EvolvingImagePlane->bHiddenInGame) ? 1 : 0);
	}
}

void AEvoCompImageEvolutionAlgorithm::UpdateViewerLayout()
{
	const float HalfSeparation = 0.5f * FMath::Max(ViewerSeparation, 50.0f);
	const float SafePlaneScale = FMath::Max(ViewerPlaneScale, 10.0f);
	const FRotator TargetFacingRotation(0.0f, -90.0f, 90.0f);
	const FRotator EvolvingFacingRotation = TargetFacingRotation;

	if (TargetImagePlane)
	{
		TargetImagePlane->SetRelativeLocation(FVector(0.0f, -HalfSeparation, 0.0f));
		TargetImagePlane->SetRelativeRotation(TargetFacingRotation);
		TargetImagePlane->SetRelativeScale3D(FVector(SafePlaneScale / 100.0f, SafePlaneScale / 100.0f, 1.0f));
	}

	if (EvolvingImagePlane)
	{
		EvolvingImagePlane->SetRelativeLocation(FVector(0.0f, HalfSeparation, 0.0f));
		EvolvingImagePlane->SetRelativeRotation(EvolvingFacingRotation);
		EvolvingImagePlane->SetRelativeScale3D(FVector(SafePlaneScale / 100.0f, SafePlaneScale / 100.0f, 1.0f));
		ConfigureEvolvingPlaneRendering(EvolvingImagePlane);
	}

}

void AEvoCompImageEvolutionAlgorithm::EnsureViewerMaterials()
{
	UMaterialInterface* EffectiveTargetMaterial = ViewerMaterialTemplate;
	UMaterialInterface* EffectiveEvolvingMaterial = nullptr;
	EffectiveTargetMaterial = ResolveValidMaterialParent(EffectiveTargetMaterial);

#if WITH_EDITOR
	if (!FModuleManager::Get().IsModuleLoaded(TEXT("Paper2D")))
	{
		FModuleManager::Get().LoadModule(TEXT("Paper2D"));
	}

	// Choose a known engine fallback if no material template is assigned.
	static UMaterialInterface* RuntimeSpriteMaterial =
		LoadObject<UMaterialInterface>(nullptr, TEXT("/Paper2D/MaskedUnlitSpriteMaterial.MaskedUnlitSpriteMaterial"));
	static UMaterialInterface* RuntimeOpaquePassThroughMaterial =
		LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Opaque.Widget3DPassThrough_Opaque"));
	static UMaterialInterface* RuntimeTranslucentPassThroughMaterial =
		LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Translucent.Widget3DPassThrough_Translucent"));
	static UMaterialInterface* RuntimeBasicShapeMaterial =
		LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	if (!EffectiveTargetMaterial)
	{
		if (RuntimeOpaquePassThroughMaterial)
		{
			EffectiveTargetMaterial = RuntimeOpaquePassThroughMaterial;
		}
		else if (RuntimeTranslucentPassThroughMaterial)
		{
			EffectiveTargetMaterial = RuntimeTranslucentPassThroughMaterial;
		}
		else if (RuntimeSpriteMaterial)
		{
			EffectiveTargetMaterial = RuntimeSpriteMaterial;
		}
		else if (RuntimeBasicShapeMaterial)
		{
			EffectiveTargetMaterial = RuntimeBasicShapeMaterial;
		}
	}
#endif // WITH_EDITOR

	if (!EffectiveTargetMaterial)
	{
		return;
	}

	EffectiveTargetMaterial = ResolveValidMaterialParent(EffectiveTargetMaterial);
	if (TargetPlaneMaterial && TargetPlaneMaterial->Parent != EffectiveTargetMaterial)
	{
		TargetPlaneMaterial = nullptr;
	}

	if (TargetImagePlane && !TargetPlaneMaterial)
	{
		TargetPlaneMaterial = UMaterialInstanceDynamic::Create(EffectiveTargetMaterial, this);
		if (TargetPlaneMaterial)
		{
			ApplyMaterialToAllSlots(TargetImagePlane, TargetPlaneMaterial);
		}
	}

	// Evolving must follow the actual target mesh material first.
	if (TargetImagePlane && TargetImagePlane->GetMaterial(0))
	{
		EffectiveEvolvingMaterial = ResolveValidMaterialParent(TargetImagePlane->GetMaterial(0));
	}

#if WITH_EDITOR
	// Keep evolving preview out of the widget pass-through path; it can bind but still render white.
	if (IsWidgetPassThroughMaterial(EffectiveEvolvingMaterial))
	{
		if (RuntimeSpriteMaterial)
		{
			EffectiveEvolvingMaterial = RuntimeSpriteMaterial;
		}
		else if (RuntimeTranslucentPassThroughMaterial)
		{
			EffectiveEvolvingMaterial = RuntimeTranslucentPassThroughMaterial;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[IMAGE-EVO] Sprite fallback unavailable; evolving material remains widget pass-through."));
		}
	}
#endif

	if (!EffectiveEvolvingMaterial)
	{
		EffectiveEvolvingMaterial = EffectiveTargetMaterial;
	}

	// Evolving plane must use a material that actually exposes a texture parameter.
	// If the chosen fallback has none (e.g. some sprite paths), force opaque pass-through.
#if WITH_EDITOR
	if (EffectiveEvolvingMaterial)
	{
		TSet<FName> EvolvingTextureParams;
		GetTextureParameterNames(EffectiveEvolvingMaterial, EvolvingTextureParams);
		if (EvolvingTextureParams.Num() == 0)
		{
			if (bForceEvolvingUnlitFallbackMaterial && RuntimeOpaquePassThroughMaterial)
			{
				EffectiveEvolvingMaterial = RuntimeOpaquePassThroughMaterial;
			}
			else
			{
				EffectiveEvolvingMaterial = EffectiveTargetMaterial;
			}
		}
	}
#endif

	if (!EffectiveEvolvingMaterial)
	{
		EffectiveEvolvingMaterial = EffectiveTargetMaterial;
	}
	EffectiveEvolvingMaterial = ResolveValidMaterialParent(EffectiveEvolvingMaterial);

	if (EvolvingPlaneMaterial && EvolvingPlaneMaterial->Parent != EffectiveEvolvingMaterial)
	{
		EvolvingPlaneMaterial = nullptr;
	}

	if (EvolvingImagePlane && !EvolvingPlaneMaterial)
	{
		EvolvingPlaneMaterial = UMaterialInstanceDynamic::Create(EffectiveEvolvingMaterial, this);
		if (EvolvingPlaneMaterial)
		{
			if (UMaterialInstanceDynamic* TargetMID = Cast<UMaterialInstanceDynamic>(TargetImagePlane ? TargetImagePlane->GetMaterial(0) : nullptr))
			{
				// Mirror all runtime parameters from the known-good target setup.
				CopyMaterialParameters(TargetMID, EvolvingPlaneMaterial);
			}
			ApplyMaterialToAllSlots(EvolvingImagePlane, EvolvingPlaneMaterial);
		}
	}
}

void AEvoCompImageEvolutionAlgorithm::EnsurePreviewTexture()
{
	const int32 SafeResolution = FMath::Clamp(PreviewResolution, 64, 2048);
	const bool bNeedsCreate =
		!BestGenerationTexture ||
		!BestGenerationTexture->GetPlatformData() ||
		BestGenerationTexture->GetSizeX() != SafeResolution ||
		BestGenerationTexture->GetSizeY() != SafeResolution;

	if (!bNeedsCreate)
	{
		return;
	}

	BestGenerationTexture = UTexture2D::CreateTransient(SafeResolution, SafeResolution, PF_B8G8R8A8);
	if (!BestGenerationTexture)
	{
		return;
	}

	BestGenerationTexture->Filter = TF_Nearest;
	BestGenerationTexture->SRGB = false;
	BestGenerationTexture->NeverStream = true;
	BestGenerationTexture->LODGroup = TEXTUREGROUP_UI;
	BestGenerationTexture->AddressX = TA_Clamp;
	BestGenerationTexture->AddressY = TA_Clamp;

#if WITH_EDITORONLY_DATA
	BestGenerationTexture->MipGenSettings = TMGS_NoMipmaps;
	BestGenerationTexture->CompressionSettings = TC_EditorIcon;
#endif
	BestGenerationTexture->UpdateResource();
	ClearPreviewTexture(127);
}

void AEvoCompImageEvolutionAlgorithm::ClearPreviewTexture(uint8 GrayByte)
{
	EnsurePreviewTexture();
	if (!BestGenerationTexture || !BestGenerationTexture->GetPlatformData() || BestGenerationTexture->GetPlatformData()->Mips.IsEmpty())
	{
		return;
	}

	const int32 SafeResolution = FMath::Clamp(PreviewResolution, 64, 2048);
	TArray<FColor> PreviewPixels;
	PreviewPixels.Init(FColor(GrayByte, GrayByte, GrayByte, 255), SafeResolution * SafeResolution);
	UploadPreviewPixels(BestGenerationTexture, PreviewPixels, SafeResolution);
}

float AEvoCompImageEvolutionAlgorithm::DrawUnitRandom()
{
	if (bUseDeterministicSeed)
	{
		return DeterministicRandom.FRand();
	}

	return FMath::FRand();
}

int32 AEvoCompImageEvolutionAlgorithm::DrawRandomInt(int32 MinInclusive, int32 MaxInclusive)
{
	if (bUseDeterministicSeed)
	{
		return DeterministicRandom.RandRange(MinInclusive, MaxInclusive);
	}

	return FMath::RandRange(MinInclusive, MaxInclusive);
}

int32 AEvoCompImageEvolutionAlgorithm::DrawRankBasedIndex(int32 Count)
{
	const int32 SafeCount = FMath::Max(Count, 1);
	if (SafeCount <= 1)
	{
		return 0;
	}

	// Linear rank selection: low index (high rank) has higher probability.
	const float U = DrawUnitRandom();
	const float Weighted = 1.0f - FMath::Sqrt(1.0f - U);
	const int32 Result = FMath::Clamp(static_cast<int32>(Weighted * static_cast<float>(SafeCount)), 0, SafeCount - 1);
	return Result;
}

bool AEvoCompImageEvolutionAlgorithm::ShouldStopEarly() const
{
	const bool bReachedMinGenerations = CurrentGeneration >= SanitizePositiveInt(MinGenerationsBeforeStop);
	const bool bReachedStability = StableGenerationCount >= SanitizePositiveInt(RequiredStableGenerations);
	return bReachedMinGenerations && bReachedStability;
}
