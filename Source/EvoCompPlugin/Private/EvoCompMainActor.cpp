#include "EvoCompMainActor.h"
#include "EvoCompPluginBPLibrary.h"

#include "Math/UnrealMathUtility.h"
#include "Algo/MaxElement.h"

void AEvoCompMainActor::InitializePopulationIfNeeded()
{
	if (bPopulationInitialized && Population.Num() == PopulationSize)
	{
		return;
	}

	Population.SetNum(PopulationSize);
	for (float& Gene : Population)
	{
		Gene = FMath::FRand();
	}

	bPopulationInitialized = true;
	CurrentGeneration = 0;
	BestFitness = 0.0f;
	ConsecutiveGenerationsAboveThreshold = 0;

	UE_LOG(LogTemp, Display, TEXT("[GA] Population initialized with %d random genes."), PopulationSize);
}

float AEvoCompMainActor::EvaluateFitness(float GeneValue) const
{
	// Fitness function in [0,1]: peak around 0.8, smooth slope.
	const float Delta = GeneValue - 0.8f;
	return FMath::Clamp(1.0f - (Delta * Delta), 0.0f, 1.0f);
}

float AEvoCompMainActor::SelectParentGene(const TArray<float>& InPopulation, const TArray<float>& InFitnessValues) const
{
	const int32 A = FMath::RandRange(0, InPopulation.Num() - 1);
	const int32 B = FMath::RandRange(0, InPopulation.Num() - 1);
	return InFitnessValues[A] >= InFitnessValues[B] ? InPopulation[A] : InPopulation[B];
}

void AEvoCompMainActor::ExecuteOneGeneration()
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

	// Build the rest of the generation via tournament selection, crossover, and mutation.
	while (NextPopulation.Num() < Population.Num())
	{
		const float ParentA = SelectParentGene(Population, FitnessValues);
		const float ParentB = SelectParentGene(Population, FitnessValues);

		float Child = ParentA;
		if (FMath::FRand() < CrossoverRate)
		{
			const float Alpha = FMath::FRandRange(0.2f, 0.8f);
			Child = (Alpha * ParentA) + ((1.0f - Alpha) * ParentB);
		}

		if (FMath::FRand() < MutationRate)
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

AEvoCompMainActor::AEvoCompMainActor()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AEvoCompMainActor::RunGeneticAlgorithm()
{
	CurrentGeneration = 0;
	BestFitness = 0.0f;
	bPopulationInitialized = false;
	Population.Reset();
	ConsecutiveGenerationsAboveThreshold = 0;

	UE_LOG(LogTemp, Display,
		TEXT("[GA] Run button clicked | PopSize=%d | MaxGen=%d | MutRate=%.3f | Crossover=%.3f | Threshold=%.3f | MinStopGen=%d | StableGen=%d | Elitism=%s"),
		PopulationSize, MaxGenerations, MutationRate, CrossoverRate,
		FitnessThreshold,
		MinGenerationsBeforeStop,
		RequiredStableGenerations,
		bEnableElitism ? TEXT("true") : TEXT("false"));

	for (int32 Index = 0; Index < MaxGenerations; ++Index)
	{
		ExecuteOneGeneration();
		const float CurrentBest = BestFitness;

		if (CurrentBest >= FitnessThreshold)
		{
			++ConsecutiveGenerationsAboveThreshold;
		}
		else
		{
			ConsecutiveGenerationsAboveThreshold = 0;
		}

		// Stop only after the threshold is sustained long enough to reduce random lucky spikes.
		const bool bReachedMinGeneration = CurrentGeneration >= MinGenerationsBeforeStop;
		const bool bReachedStability = ConsecutiveGenerationsAboveThreshold >= RequiredStableGenerations;

		if (bReachedMinGeneration && bReachedStability)
		{
			UE_LOG(LogTemp, Display,
				TEXT("[GA] Target reached at generation %d (BestFitness=%.4f, Threshold=%.4f, StableCount=%d)"),
				CurrentGeneration,
				BestFitness,
				FitnessThreshold,
				ConsecutiveGenerationsAboveThreshold);
			break;
		}
	}

	UE_LOG(LogTemp, Display,
		TEXT("[GA] Run complete | FinalGeneration=%d | FinalBestFitness=%.4f"),
		CurrentGeneration,
		BestFitness);

	UEvoCompPluginBPLibrary::ExecuteGeneticAlgorithm();
}

void AEvoCompMainActor::StepOneGeneration()
{
	UE_LOG(LogTemp, Display, TEXT("[GA] Step button clicked"));

	if (CurrentGeneration >= MaxGenerations)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GA] StepOneGeneration | Already reached MaxGenerations (%d)"), MaxGenerations);
		return;
	}

	ExecuteOneGeneration();
	const float CurrentBest = BestFitness;
	if (CurrentBest >= FitnessThreshold)
	{
		++ConsecutiveGenerationsAboveThreshold;
	}
	else
	{
		ConsecutiveGenerationsAboveThreshold = 0;
	}

	if (CurrentBest >= FitnessThreshold)
	{
		UE_LOG(LogTemp, Display,
			TEXT("[GA] Threshold reached after manual step at generation %d (BestFitness=%.4f, StableCount=%d/%d)"),
			CurrentGeneration,
			BestFitness,
			ConsecutiveGenerationsAboveThreshold,
			RequiredStableGenerations);
	}
}

void AEvoCompMainActor::ResetPopulation()
{
	CurrentGeneration = 0;
	BestFitness = 0.0f;
	bPopulationInitialized = false;
	Population.Reset();
	ConsecutiveGenerationsAboveThreshold = 0;
	UE_LOG(LogTemp, Display, TEXT("[GA] Reset button clicked | State cleared."));
}
