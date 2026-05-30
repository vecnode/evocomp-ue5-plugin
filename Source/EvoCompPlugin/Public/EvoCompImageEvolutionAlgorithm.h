#pragma once

#include "EvoCompAlgorithmActor.h"
#include "Containers/Ticker.h"
#include "EvoCompImageEvolutionAlgorithm.generated.h"

class UTexture2D;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/**
 * Image-evolution actor that evolves a grayscale patch genome toward a target texture.
 */
UCLASS(BlueprintType, Blueprintable, meta = (DisplayName = "Image Evolution Algorithm Actor"))
class EVOCOMPPLUGIN_API AEvoCompImageEvolutionAlgorithm : public AEvoCompAlgorithmActor
{
	GENERATED_BODY()

public:
	AEvoCompImageEvolutionAlgorithm();

	virtual void Tick(float DeltaTime) override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual bool ShouldTickIfViewportsOnly() const override;

	// Configuration
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Image Evo | Configuration",
		meta = (ClampMin = "1", ToolTip = "Population size used for image evolution."))
	int32 PopulationSize = 64;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Image Evo | Configuration",
		meta = (ClampMin = "1", ToolTip = "Maximum number of generations."))
	int32 MaxGenerations = 2000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Image Evo | Configuration",
		meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Per-gene mutation probability."))
	float MutationRate = 0.03f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Image Evo | Configuration",
		meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Probability to reset a mutated gene instead of perturbing it."))
	float MutationResetChance = 0.20f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Image Evo | Configuration",
		meta = (ClampMin = "0.001", ClampMax = "1.0", ToolTip = "Magnitude of additive mutation when reset is not used."))
	float MutationStepSize = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Image Evo | Configuration",
		meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Uniform crossover rate: probability that a gene is copied from the second-best individual."))
	float SecondBestGeneBorrowRate = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Image Evo | Configuration",
		meta = (ClampMin = "2", ToolTip = "Number of top-ranked individuals considered during rank-based parent sampling."))
	int32 TopRankParentPoolSize = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Image Evo | Configuration",
		meta = (ToolTip = "Keep the best individual unchanged when building the next generation."))
	bool bEnableElitism = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Image Evo | Configuration",
		meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Stop threshold for best fitness."))
	float FitnessThreshold = 0.98f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Image Evo | Configuration",
		meta = (ClampMin = "1", ToolTip = "Minimum generations before threshold-based stop is allowed."))
	int32 MinGenerationsBeforeStop = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Image Evo | Configuration",
		meta = (ClampMin = "1", ToolTip = "Consecutive generations at or above threshold required to stop."))
	int32 RequiredStableGenerations = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Image Evo | Target",
		meta = (ToolTip = "Target color image used for fitness evaluation. The algorithm compares grayscale patch means."))
	TObjectPtr<UTexture2D> TargetTexture = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Image Evo | Target",
		meta = (ClampMin = "2", ToolTip = "Number of horizontal patches in the genome."))
	int32 PatchColumns = 32;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Image Evo | Target",
		meta = (ClampMin = "2", ToolTip = "Number of vertical patches in the genome."))
	int32 PatchRows = 32;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Image Evo | Preview",
		meta = (ClampMin = "64", ClampMax = "2048", ToolTip = "Preview texture output resolution."))
	int32 PreviewResolution = 512;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Image Evo | Viewer",
		meta = (ClampMin = "50.0", ToolTip = "Distance between target and evolving-image planes."))
	float ViewerSeparation = 340.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Image Evo | Viewer",
		meta = (ClampMin = "10.0", ToolTip = "Uniform world scale applied to each viewer plane."))
	float ViewerPlaneScale = 220.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Image Evo | Viewer",
		meta = (ToolTip = "Material template used for both viewer planes. It should expose a texture parameter named by ViewerTextureParameter."))
	TObjectPtr<UMaterialInterface> ViewerMaterialTemplate = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Image Evo | Viewer",
		meta = (ToolTip = "Texture parameter name in ViewerMaterialTemplate used to display target and evolving textures."))
	FName ViewerTextureParameter = TEXT("SpriteTexture");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Image Evo | Viewer",
		meta = (ToolTip = "When enabled, the evolving plane always uses an internal engine unlit pass-through material to avoid black output from custom/overridden templates."))
	bool bForceEvolvingUnlitFallbackMaterial = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Image Evo | Viewer",
		meta = (ToolTip = "Auto initialize one generation preview when a target texture is assigned in editor."))
	bool bAutoInitializePreviewOnConstruction = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Image Evo | Realtime",
		meta = (ToolTip = "When true, one or more generations run every tick."))
	bool bRealtimeEvolution = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Image Evo | Realtime",
		meta = (ClampMin = "1", ClampMax = "60", ToolTip = "Number of generations to execute per actor tick when realtime mode is enabled."))
	int32 GenerationsPerTick = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Image Evo | Random",
		meta = (ToolTip = "Use deterministic random seed for reproducible runs."))
	bool bUseDeterministicSeed = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Image Evo | Random",
		meta = (ClampMin = "0", ToolTip = "Seed used when deterministic mode is enabled."))
	int32 RandomSeed = 90210;

	// Results
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Image Evo | Results",
		meta = (ToolTip = "Best fitness found in the current generation."))
	float GenerationBestFitness = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Image Evo | Results",
		meta = (ToolTip = "Average fitness in the current generation."))
	float AverageFitness = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Image Evo | Results",
		meta = (ToolTip = "Current generation MAE error against the target patch map."))
	float GenerationError = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Image Evo | Results",
		meta = (ToolTip = "Consecutive generations with best fitness above threshold."))
	int32 StableGenerationCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Image Evo | Results",
		meta = (ToolTip = "Transient preview texture of the current generation best individual."))
	TObjectPtr<UTexture2D> BestGenerationTexture = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Image Evo | Results",
		meta = (ToolTip = "Whether target preprocessing and population initialization are ready."))
	bool bEvolutionInitialized = false;

	// Actions
	UFUNCTION(BlueprintCallable, Category = "Image Evo | Actions",
		meta = (ToolTip = "Preprocess target image and initialize a random population."))
	void InitializeEvolution();

	UFUNCTION(BlueprintCallable, Category = "Image Evo | Actions",
		meta = (ToolTip = "Advance the algorithm by one generation."))
	void StepOneGeneration();

	UFUNCTION(BlueprintCallable, Category = "Image Evo | Actions",
		meta = (ToolTip = "Run evolution until stopping criteria are met."))
	void RunEvolution();

	UFUNCTION(BlueprintCallable, Category = "Image Evo | Actions",
		meta = (ToolTip = "Reset generated state and clear preview."))
	void ResetEvolution();

	UFUNCTION(BlueprintCallable, Category = "Image Evo | Actions",
		meta = (ToolTip = "Toggle realtime tick-driven generation updates."))
	void SetRealtimeEvolutionEnabled(bool bEnabled);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Image Evo | Viewer",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> TargetImagePlane = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Image Evo | Viewer",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> EvolvingImagePlane = nullptr;

private:
	struct FImageIndividual
	{
		TArray<float> Genome;
		float Fitness = 0.0f;
	};

	int32 GenomeLength = 0;
	TArray<float> TargetPatchValues;
	TArray<FImageIndividual> ImagePopulation;
	TArray<int32> RankedIndices;
	TArray<float> CurrentBestGenome;
	FRandomStream DeterministicRandom;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> TargetPlaneMaterial = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> EvolvingPlaneMaterial = nullptr;

	bool PrepareTargetPatchMap();
	void InitializeRandomPopulation();
	void EvaluatePopulation();
	void BuildNextGeneration();
	float EvaluateFitness(const TArray<float>& Genome) const;
	void UpdatePreviewTexture(const TArray<float>& Genome);
	void UpdateTargetPlaneTexture();
	void UpdateEvolvingPlaneTexture();
	void UpdateViewerLayout();
	void EnsureViewerMaterials();
	void EnsurePreviewTexture();
	void ClearPreviewTexture(uint8 GrayByte);
	void StartRealtimeTickerFallback();
	void StopRealtimeTickerFallback();
	bool HandleRealtimeTickerFallback(float DeltaTime);
	float DrawUnitRandom();
	int32 DrawRandomInt(int32 MinInclusive, int32 MaxInclusive);
	int32 DrawRankBasedIndex(int32 Count);
	bool ShouldStopEarly() const;
	void ResetEvolutionRuntimeState();
	void StopAndLogRunComplete();

	FTSTicker::FDelegateHandle RealtimeTickerHandle;
};
