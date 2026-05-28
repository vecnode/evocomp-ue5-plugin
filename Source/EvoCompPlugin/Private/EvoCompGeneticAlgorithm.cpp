#include "EvoCompGeneticAlgorithm.h"

#include "EvoCompPluginBPLibrary.h"

#include "Math/UnrealMathUtility.h"

AEvoCompGeneticAlgorithm::AEvoCompGeneticAlgorithm()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AEvoCompGeneticAlgorithm::InitializePopulationIfNeeded()
{
	const int32 SafePopulationSize = SanitizePositiveInt(PopulationSize);
	if (bPopulationInitialized && Population.Num() == SafePopulationSize)
	{
		return;
	}

	Population.SetNum(SafePopulationSize);
	for (float& Gene : Population)
	{
		Gene = FMath::FRand();
	}

	bPopulationInitialized = true;
	CurrentGeneration = 0;
	BestFitness = 0.0f;
	ConsecutiveGenerationsAboveThreshold = 0;

	UE_LOG(LogTemp, Display, TEXT("[GA] Population initialized with %d random genes."), SafePopulationSize);
}

float AEvoCompGeneticAlgorithm::EvaluateFitness(float GeneValue) const
{
	// Demonstration objective in [0,1]: a narrow global peak near 0.78 with local ripples.
	// The ripple term is phase-aligned with the peak so the true global optimum remains reachable.
	const float Delta = GeneValue - 0.78f;
	const float Peak = FMath::Exp(-32.0f * Delta * Delta);
	const float Ripple = 0.5f * (1.0f + FMath::Cos(22.0f * Delta));
	const float Combined = Peak * (0.78f + (0.22f * Ripple));
	return FMath::Clamp(Combined, 0.0f, 1.0f);
}

float AEvoCompGeneticAlgorithm::SelectParentGene(const TArray<float>& InPopulation, const TArray<float>& InFitnessValues) const
{
	if (InPopulation.IsEmpty())
	{
		return 0.0f;
	}

	const int32 A = FMath::RandRange(0, InPopulation.Num() - 1);
	const int32 B = FMath::RandRange(0, InPopulation.Num() - 1);
	return InFitnessValues[A] >= InFitnessValues[B] ? InPopulation[A] : InPopulation[B];
}

void AEvoCompGeneticAlgorithm::ExecuteOneGeneration()
{
	InitializePopulationIfNeeded();

	const int32 NextGeneration = CurrentGeneration + 1;
	UE_LOG(LogTemp, Display, TEXT("[GA] Start Generation %d"), NextGeneration);

	TArray<float> FitnessValues;
	FitnessValues.SetNum(Population.Num());

	int32 BestIndex = 0;
	float CurrentBestFitness = 0.0f;
	for (int32 Index = 0; Index < Population.Num(); ++Index)
	{
		FitnessValues[Index] = EvaluateFitness(Population[Index]);
		if (FitnessValues[Index] > CurrentBestFitness)
		{
			CurrentBestFitness = FitnessValues[Index];
			BestIndex = Index;
		}
	}

	const float PreviousBest = BestFitness;
	BestFitness = FMath::Max(BestFitness, CurrentBestFitness);

	TArray<float> NextPopulation;
	NextPopulation.Reserve(Population.Num());

	// Keep the strongest individual untouched so progress is not lost between generations.
	if (bEnableElitism)
	{
		NextPopulation.Add(Population[BestIndex]);
	}

	const float SafeMutationRate = SanitizeUnitFloat(MutationRate);
	const float SafeCrossoverRate = SanitizeUnitFloat(CrossoverRate);

	// Build the rest of the generation via tournament selection, crossover, and mutation.
	while (NextPopulation.Num() < Population.Num())
	{
		const float ParentA = SelectParentGene(Population, FitnessValues);
		const float ParentB = SelectParentGene(Population, FitnessValues);

		float Child = ParentA;
		if (FMath::FRand() < SafeCrossoverRate)
		{
			const float Alpha = FMath::FRandRange(0.2f, 0.8f);
			Child = (Alpha * ParentA) + ((1.0f - Alpha) * ParentB);
		}

		if (FMath::FRand() < SafeMutationRate)
		{
			Child += FMath::FRandRange(-0.1f, 0.1f);
		}

		NextPopulation.Add(FMath::Clamp(Child, 0.0f, 1.0f));
	}

	Population = MoveTemp(NextPopulation);
	CurrentGeneration = NextGeneration;

	UE_LOG(LogTemp, Display,
		TEXT("[GA] End Generation %d | BestFitness: %.4f -> %.4f"),
		CurrentGeneration,
		PreviousBest,
		BestFitness);
}

void AEvoCompGeneticAlgorithm::RunGeneticAlgorithm()
{
	ResetAlgorithmState();

	const int32 SafeMaxGenerations = SanitizePositiveInt(MaxGenerations);
	const int32 SafeMinGenerationsBeforeStop = SanitizePositiveInt(MinGenerationsBeforeStop);
	const int32 SafeRequiredStableGenerations = SanitizePositiveInt(RequiredStableGenerations);
	const float SafeMutationRate = SanitizeUnitFloat(MutationRate);
	const float SafeCrossoverRate = SanitizeUnitFloat(CrossoverRate);
	const float SafeFitnessThreshold = SanitizeUnitFloat(FitnessThreshold);

	UE_LOG(LogTemp, Display,
		TEXT("[GA] Run button clicked | PopSize=%d | MaxGen=%d | MutRate=%.3f | Crossover=%.3f | Threshold=%.3f | MinStopGen=%d | StableGen=%d | Elitism=%s"),
		SanitizePositiveInt(PopulationSize),
		SafeMaxGenerations,
		SafeMutationRate,
		SafeCrossoverRate,
		SafeFitnessThreshold,
		SafeMinGenerationsBeforeStop,
		SafeRequiredStableGenerations,
		bEnableElitism ? TEXT("true") : TEXT("false"));

	for (int32 Index = 0; Index < SafeMaxGenerations; ++Index)
	{
		ExecuteOneGeneration();
		const float CurrentBest = BestFitness;

		if (CurrentBest >= SafeFitnessThreshold)
		{
			++ConsecutiveGenerationsAboveThreshold;
		}
		else
		{
			ConsecutiveGenerationsAboveThreshold = 0;
		}

		// Stop only after the threshold is sustained long enough to reduce random lucky spikes.
		const bool bReachedMinGeneration = CurrentGeneration >= SafeMinGenerationsBeforeStop;
		const bool bReachedStability = ConsecutiveGenerationsAboveThreshold >= SafeRequiredStableGenerations;

		if (bReachedMinGeneration && bReachedStability)
		{
			UE_LOG(LogTemp, Display,
				TEXT("[GA] Target reached at generation %d (BestFitness=%.4f, Threshold=%.4f, StableCount=%d)"),
				CurrentGeneration,
				BestFitness,
				SafeFitnessThreshold,
				ConsecutiveGenerationsAboveThreshold);
			break;
		}
	}

	UE_LOG(LogTemp, Display,
		TEXT("[GA] Run complete | FinalGeneration=%d | FinalBestFitness=%.4f"),
		CurrentGeneration,
		BestFitness);

	if (BestFitness < SafeFitnessThreshold)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[GA] Threshold not reached after %d generations (BestFitness=%.4f, Threshold=%.4f). Consider lowering Threshold or increasing MutationRate/PopulationSize for exploratory runs."),
			SafeMaxGenerations,
			BestFitness,
			SafeFitnessThreshold);
	}

	UEvoCompPluginBPLibrary::ExecuteGeneticAlgorithm();
}

void AEvoCompGeneticAlgorithm::StepOneGeneration()
{
	UE_LOG(LogTemp, Display, TEXT("[GA] Step button clicked"));

	const int32 SafeMaxGenerations = SanitizePositiveInt(MaxGenerations);
	const float SafeFitnessThreshold = SanitizeUnitFloat(FitnessThreshold);
	const int32 SafeRequiredStableGenerations = SanitizePositiveInt(RequiredStableGenerations);

	if (CurrentGeneration >= SafeMaxGenerations)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GA] StepOneGeneration | Already reached MaxGenerations (%d)"), SafeMaxGenerations);
		return;
	}

	ExecuteOneGeneration();
	const float CurrentBest = BestFitness;
	if (CurrentBest >= SafeFitnessThreshold)
	{
		++ConsecutiveGenerationsAboveThreshold;
	}
	else
	{
		ConsecutiveGenerationsAboveThreshold = 0;
	}

	if (CurrentBest >= SafeFitnessThreshold)
	{
		UE_LOG(LogTemp, Display,
			TEXT("[GA] Threshold reached after manual step at generation %d (BestFitness=%.4f, StableCount=%d/%d)"),
			CurrentGeneration,
			BestFitness,
			ConsecutiveGenerationsAboveThreshold,
			SafeRequiredStableGenerations);
	}
}

void AEvoCompGeneticAlgorithm::ResetPopulation()
{
	ResetAlgorithmState();
	UE_LOG(LogTemp, Display, TEXT("[GA] Reset button clicked | State cleared."));
}