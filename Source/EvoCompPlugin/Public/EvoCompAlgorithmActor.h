#pragma once

#include "GameFramework/Actor.h"
#include "EvoCompAlgorithmActor.generated.h"

/**
 * Shared base for evolutionary-computation actors.
 * Concrete algorithm actors own their specific configuration and execution flow.
 */
UCLASS(Abstract, BlueprintType, Blueprintable, meta = (DisplayName = "Evolutionary Algorithm Actor"))
class EVOCOMPPLUGIN_API AEvoCompAlgorithmActor : public AActor
{
	GENERATED_BODY()

public:
	AEvoCompAlgorithmActor();

protected:
	void ResetAlgorithmState();
	static int32 SanitizePositiveInt(int32 Value);
	static float SanitizeUnitFloat(float Value);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Algorithm | Results",
		meta = (ToolTip = "Current generation index reached by the run."))
	int32 CurrentGeneration = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Algorithm | Results",
		meta = (ToolTip = "Best fitness value found so far during the run."))
	float BestFitness = 0.0f;

	TArray<float> Population;
	bool bPopulationInitialized = false;
	int32 ConsecutiveGenerationsAboveThreshold = 0;
};