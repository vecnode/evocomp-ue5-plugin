#include "EvoCompAlgorithmActor.h"

#include "Math/UnrealMathUtility.h"

AEvoCompAlgorithmActor::AEvoCompAlgorithmActor()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AEvoCompAlgorithmActor::ResetAlgorithmState()
{
	CurrentGeneration = 0;
	BestFitness = 0.0f;
	bPopulationInitialized = false;
	Population.Reset();
	ConsecutiveGenerationsAboveThreshold = 0;
}

int32 AEvoCompAlgorithmActor::SanitizePositiveInt(int32 Value)
{
	return FMath::Max(Value, 1);
}

float AEvoCompAlgorithmActor::SanitizeUnitFloat(float Value)
{
	return FMath::Clamp(Value, 0.0f, 1.0f);
}