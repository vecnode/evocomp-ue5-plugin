#pragma once

#include "GameFramework/Actor.h"
#include "EvoCompMainActor.generated.h"

/**
 * Place this actor in any level to configure and drive the Genetic Algorithm
 * from the Details panel. Properties appear as editable fields; functions
 * marked CallInEditor appear as clickable buttons.
 */
UCLASS(BlueprintType, Blueprintable, meta = (DisplayName = "GA Main Actor"))
class EVOCOMPPLUGIN_API AEvoCompMainActor : public AActor
{
	GENERATED_BODY()

public:
	AEvoCompMainActor();

	// ── Configuration ────────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GA | Configuration",
		meta = (ClampMin = "1", ToolTip = "Number of candidate solutions in each generation. Larger values improve search quality but increase compute cost."))
	int32 PopulationSize = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GA | Configuration",
		meta = (ClampMin = "1", ToolTip = "Maximum number of generations the algorithm can run before stopping."))
	int32 MaxGenerations = 500;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GA | Configuration",
		meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Chance that a gene changes during mutation. Typical values are small, like 0.01 to 0.10."))
	float MutationRate = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GA | Configuration",
		meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Probability that two selected parents perform crossover to produce offspring."))
	float CrossoverRate = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GA | Configuration",
		meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Stop target for solution quality. The run can finish early when best fitness reaches this value."))
	float FitnessThreshold = 0.95f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GA | Configuration",
		meta = (ClampMin = "1", ToolTip = "Minimum number of generations to run before threshold-based early stopping is allowed."))
	int32 MinGenerationsBeforeStop = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GA | Configuration",
		meta = (ClampMin = "1", ToolTip = "Number of consecutive generations at/above threshold required before stopping."))
	int32 RequiredStableGenerations = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GA | Configuration",
		meta = (ToolTip = "Preserve top-performing individuals between generations to improve convergence stability."))
	bool bEnableElitism = false;

	// ── Results (read-only display) ───────────────────────────────────────────

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GA | Results",
		meta = (ToolTip = "Current generation index reached by the run."))
	int32 CurrentGeneration = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GA | Results",
		meta = (ToolTip = "Best fitness value found so far during the run."))
	float BestFitness = 0.0f;

	// ── Actions (appear as buttons in the Details panel) ─────────────────────

	UFUNCTION(BlueprintCallable, Category = "GA | Actions",
		meta = (ToolTip = "Run the genetic algorithm using the current configuration values."))
	void RunGeneticAlgorithm();

	UFUNCTION(BlueprintCallable, Category = "GA | Actions",
		meta = (ToolTip = "Advance the algorithm by one generation for step-by-step testing."))
	void StepOneGeneration();

	UFUNCTION(BlueprintCallable, Category = "GA | Actions",
		meta = (ToolTip = "Reset generation counters and clear current best result values."))
	void ResetPopulation();

private:
	void InitializePopulationIfNeeded();
	float EvaluateFitness(float GeneValue) const;
	float SelectParentGene(const TArray<float>& InPopulation, const TArray<float>& InFitnessValues) const;
	void ExecuteOneGeneration();

	TArray<float> Population;
	bool bPopulationInitialized = false;
	int32 ConsecutiveGenerationsAboveThreshold = 0;
};
