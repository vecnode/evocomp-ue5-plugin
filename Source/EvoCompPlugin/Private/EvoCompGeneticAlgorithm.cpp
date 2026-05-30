#include "EvoCompGeneticAlgorithm.h"

#include "EvoCompPluginBPLibrary.h"

#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "Containers/UnrealString.h"
#include "Math/UnrealMathUtility.h"

namespace
{
	static const FString& GetGAStringAlphabet()
	{
		static const FString Alphabet = TEXT(" abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ.,!?'-");
		return Alphabet;
	}

	int32 CharToAlphabetIndex(TCHAR Character)
	{
		const FString& Alphabet = GetGAStringAlphabet();
		int32 Index = 0;
		if (Alphabet.FindChar(Character, Index))
		{
			return Index;
		}
		return Index;
	}

	TCHAR RandomAlphabetChar()
	{
		const FString& Alphabet = GetGAStringAlphabet();
		const int32 Index = FMath::RandRange(0, Alphabet.Len() - 1);
		return Alphabet[Index];
	}
}

AEvoCompGeneticAlgorithm::AEvoCompGeneticAlgorithm()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	DisplayRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DisplayRoot"));
	SetRootComponent(DisplayRoot);

	TargetTextComponent = CreateDefaultSubobject<UTextRenderComponent>(TEXT("TargetText"));
	TargetTextComponent->SetupAttachment(DisplayRoot);
	TargetTextComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 140.0f));
	TargetTextComponent->SetHorizontalAlignment(EHTA_Left);
	TargetTextComponent->SetWorldSize(30.0f);
	TargetTextComponent->SetTextRenderColor(FColor(110, 210, 255));

	CurrentTextComponent = CreateDefaultSubobject<UTextRenderComponent>(TEXT("CurrentText"));
	CurrentTextComponent->SetupAttachment(DisplayRoot);
	CurrentTextComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 90.0f));
	CurrentTextComponent->SetHorizontalAlignment(EHTA_Left);
	CurrentTextComponent->SetWorldSize(30.0f);
	CurrentTextComponent->SetTextRenderColor(FColor::White);

	CurrentCandidateString = TEXT("Press Run to start evolution...");
	RefreshDisplayText();
}

void AEvoCompGeneticAlgorithm::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bStringEvolutionRunning)
	{
		return;
	}

	const int32 SafeGenerationsPerTick = SanitizePositiveInt(StringGenerationsPerTick);
	for (int32 StepIndex = 0; StepIndex < SafeGenerationsPerTick && bStringEvolutionRunning; ++StepIndex)
	{
		StepStringGeneration();
	}
}

void AEvoCompGeneticAlgorithm::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshDisplayText();
}

bool AEvoCompGeneticAlgorithm::ShouldTickIfViewportsOnly() const
{
	return true;
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
}

void AEvoCompGeneticAlgorithm::RunGeneticAlgorithm()
{
	// Keep the existing blueprint wiring intact, but route the main Run action
	// to the string-evolution demo so the displayed text advances on tick.
	RunStringTargetTest();
}

void AEvoCompGeneticAlgorithm::StepOneGeneration()
{
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
	BestCandidateString.Reset();
	StringPopulation.Reset();
	ActiveTargetString.Reset();
	ActiveTargetLength = 0;
	LastLoggedMatchCount = 0;
	bStringEvolutionRunning = false;
	CurrentCandidateString = TEXT("Press Run to start evolution...");
	RefreshDisplayText();
	UE_LOG(LogTemp, Display, TEXT("[GA-STRING] State reset."));
}

void AEvoCompGeneticAlgorithm::RefreshDisplayText()
{
	if (TargetTextComponent)
	{
		const FString DisplayTarget = ActiveTargetString.IsEmpty() ? SanitizeTargetString(TargetString) : ActiveTargetString;
		TargetTextComponent->SetText(FText::FromString(FString::Printf(TEXT("Target: %s"), *DisplayTarget)));
	}

	if (CurrentTextComponent)
	{
		CurrentTextComponent->SetText(FText::FromString(FString::Printf(TEXT("Current: %s"), *CurrentCandidateString)));
	}
}

FString AEvoCompGeneticAlgorithm::SanitizeTargetString(const FString& InTarget) const
{
	FString Trimmed = InTarget;
	Trimmed.TrimStartAndEndInline();

	FString Sanitized;
	Sanitized.Reserve(Trimmed.Len());
	const FString& Alphabet = GetGAStringAlphabet();

	for (TCHAR Character : Trimmed)
	{
		if (Alphabet.Contains(FString::Chr(Character)))
		{
			Sanitized.AppendChar(Character);
			continue;
		}

		if (FChar::IsWhitespace(Character))
		{
			Sanitized.AppendChar(TEXT(' '));
		}
	}

	Sanitized.TrimStartAndEndInline();

	if (Sanitized.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[GA-STRING] TargetString became empty after sanitization. Falling back to default sentence."));
		return TEXT("This is my target string for the genetic algorithm.");
	}

	return Sanitized;
}

FString AEvoCompGeneticAlgorithm::CreateRandomCandidate(int32 Length) const
{
	FString Candidate;
	Candidate.Reserve(Length);

	for (int32 Index = 0; Index < Length; ++Index)
	{
		Candidate.AppendChar(RandomAlphabetChar());
	}

	return Candidate;
}

FString AEvoCompGeneticAlgorithm::SelectParentCandidate(const TArray<FString>& InPopulation, const TArray<float>& InFitnessValues) const
{
	if (InPopulation.IsEmpty())
	{
		return FString();
	}

	const int32 A = FMath::RandRange(0, InPopulation.Num() - 1);
	const int32 B = FMath::RandRange(0, InPopulation.Num() - 1);
	return InFitnessValues[A] >= InFitnessValues[B] ? InPopulation[A] : InPopulation[B];
}

int32 AEvoCompGeneticAlgorithm::CountExactMatches(const FString& Candidate, const FString& Target) const
{
	if (Candidate.Len() != Target.Len() || Candidate.IsEmpty())
	{
		return 0;
	}

	int32 ExactMatches = 0;
	for (int32 Index = 0; Index < Target.Len(); ++Index)
	{
		if (Candidate[Index] == Target[Index])
		{
			++ExactMatches;
		}
	}

	return ExactMatches;
}

float AEvoCompGeneticAlgorithm::EvaluateStringFitness(const FString& Candidate, const FString& Target) const
{
	const int32 TargetLength = Target.Len();
	if (TargetLength <= 0 || Candidate.Len() != TargetLength || Candidate.IsEmpty())
	{
		return 0.0f;
	}

	int32 ExactMatches = 0;
	float ClosenessAccumulator = 0.0f;

	for (int32 Index = 0; Index < TargetLength; ++Index)
	{
		const TCHAR CandidateChar = Candidate[Index];
		const TCHAR TargetChar = Target[Index];

		if (CandidateChar == TargetChar)
		{
			++ExactMatches;
		}

		const int32 CandidateIndex = CharToAlphabetIndex(CandidateChar);
		const int32 TargetIndex = CharToAlphabetIndex(TargetChar);
		const float Distance = static_cast<float>(FMath::Abs(CandidateIndex - TargetIndex));
		const float MaxIndex = static_cast<float>(GetGAStringAlphabet().Len() - 1);
		const float Closeness = 1.0f - (Distance / MaxIndex);
		ClosenessAccumulator += FMath::Clamp(Closeness, 0.0f, 1.0f);
	}

	if (ExactMatches == TargetLength)
	{
		return 1.0f;
	}

	const float ExactScore = static_cast<float>(ExactMatches) / static_cast<float>(TargetLength);
	const float ClosenessScore = ClosenessAccumulator / static_cast<float>(TargetLength);
	const float Combined = (0.80f * ExactScore) + (0.20f * ClosenessScore);
	return FMath::Clamp(Combined, 0.0f, 1.0f);
}

void AEvoCompGeneticAlgorithm::MutateCandidate(FString& Candidate, const FString& Target, float MutationChance) const
{
	for (int32 Index = 0; Index < Candidate.Len(); ++Index)
	{
		if (Index < Target.Len() && Candidate[Index] == Target[Index])
		{
			continue;
		}

		if (FMath::FRand() < MutationChance)
		{
			Candidate[Index] = RandomAlphabetChar();
		}
	}
}

void AEvoCompGeneticAlgorithm::RunStringTargetTest()
{
	ResetAlgorithmState();
	BestCandidateString.Reset();
	CurrentCandidateString.Reset();
	StringPopulation.Reset();
	bStringEvolutionRunning = false;

	if (bUseDeterministicSeed)
	{
		FMath::RandInit(FMath::Max(RandomSeed, 0));
	}

	ActiveTargetString = SanitizeTargetString(TargetString);
	ActiveTargetLength = ActiveTargetString.Len();

	const int32 SafePopulationSize = SanitizePositiveInt(PopulationSize);
	const float SafeMutationRate = SanitizeUnitFloat(MutationRate);
	const float SafeCrossoverRate = SanitizeUnitFloat(CrossoverRate);
	const float SafeFitnessThreshold = SanitizeUnitFloat(FitnessThreshold);

	UE_LOG(LogTemp, Display,
		TEXT("[GA-STRING] Run started | Target=\"%s\" | PopSize=%d | MaxGen=%d | MutRate=%.3f | Crossover=%.3f | Threshold=%.3f | Deterministic=%s | Seed=%d"),
		*ActiveTargetString,
		SafePopulationSize,
		SanitizePositiveInt(MaxGenerations),
		SafeMutationRate,
		SafeCrossoverRate,
		SafeFitnessThreshold,
		bUseDeterministicSeed ? TEXT("true") : TEXT("false"),
		RandomSeed);

	StringPopulation.Reserve(SafePopulationSize);
	for (int32 Index = 0; Index < SafePopulationSize; ++Index)
	{
		StringPopulation.Add(CreateRandomCandidate(ActiveTargetLength));
	}

	if (!StringPopulation.IsEmpty())
	{
		CurrentCandidateString = StringPopulation[0];
	}

	bStringEvolutionRunning = true;
	RefreshDisplayText();
}

void AEvoCompGeneticAlgorithm::StepStringGeneration()
{
	if (!bStringEvolutionRunning || StringPopulation.IsEmpty() || ActiveTargetLength <= 0)
	{
		return;
	}

	const int32 SafeMaxGenerations = SanitizePositiveInt(MaxGenerations);
	const int32 SafeMinGenerationsBeforeStop = SanitizePositiveInt(MinGenerationsBeforeStop);
	const int32 SafeRequiredStableGenerations = SanitizePositiveInt(RequiredStableGenerations);
	const float SafeMutationRate = SanitizeUnitFloat(MutationRate);
	const float SafeCrossoverRate = SanitizeUnitFloat(CrossoverRate);
	const float SafeFitnessThreshold = SanitizeUnitFloat(FitnessThreshold);

	if (CurrentGeneration >= SafeMaxGenerations)
	{
		bStringEvolutionRunning = false;
		UE_LOG(LogTemp, Warning,
			TEXT("[GA-STRING] Reached MaxGenerations (%d). Best=\"%s\" | BestFitness=%.4f"),
			SafeMaxGenerations,
			*BestCandidateString,
			BestFitness);
		return;
	}

	++CurrentGeneration;

	TArray<float> FitnessValues;
	FitnessValues.SetNum(StringPopulation.Num());

	int32 BestIndex = 0;
	float GenerationBestFitness = -1.0f;

	for (int32 CandidateIndex = 0; CandidateIndex < StringPopulation.Num(); ++CandidateIndex)
	{
		FitnessValues[CandidateIndex] = EvaluateStringFitness(StringPopulation[CandidateIndex], ActiveTargetString);
		if (FitnessValues[CandidateIndex] > GenerationBestFitness)
		{
			GenerationBestFitness = FitnessValues[CandidateIndex];
			BestIndex = CandidateIndex;
		}
	}

	CurrentCandidateString = StringPopulation[BestIndex];
	const int32 CurrentMatchCount = CountExactMatches(CurrentCandidateString, ActiveTargetString);

	if (GenerationBestFitness >= BestFitness)
	{
		BestFitness = GenerationBestFitness;
		BestCandidateString = StringPopulation[BestIndex];
	}

	if (CurrentMatchCount > LastLoggedMatchCount)
	{
		LastLoggedMatchCount = CurrentMatchCount;
		UE_LOG(LogTemp, Display,
			TEXT("[GA-STRING] Progress | Current=\"%s\" | Target=\"%s\" | Matched=%d/%d | Generation=%d"),
			*CurrentCandidateString,
			*ActiveTargetString,
			CurrentMatchCount,
			ActiveTargetLength,
			CurrentGeneration);
	}

	if (GenerationBestFitness >= SafeFitnessThreshold)
	{
		++ConsecutiveGenerationsAboveThreshold;
	}
	else
	{
		ConsecutiveGenerationsAboveThreshold = 0;
	}

	RefreshDisplayText();

	const bool bReachedMinGeneration = CurrentGeneration >= SafeMinGenerationsBeforeStop;
	const bool bReachedStability = ConsecutiveGenerationsAboveThreshold >= SafeRequiredStableGenerations;
	const bool bExactMatchFound = BestCandidateString == ActiveTargetString;

	if (bExactMatchFound || (bReachedMinGeneration && bReachedStability))
	{
		bStringEvolutionRunning = false;

		if (bExactMatchFound)
		{
			BestFitness = 1.0f;
		}

		UE_LOG(LogTemp, Display,
			TEXT("[GA-STRING] Run complete | Solved=%s | Target=\"%s\" | Best=\"%s\" | Generation=%d | BestFitness=%.4f"),
			bExactMatchFound ? TEXT("true") : TEXT("false"),
			*ActiveTargetString,
			*BestCandidateString,
			CurrentGeneration,
			BestFitness);
		return;
	}

	TArray<FString> NextPopulation;
	NextPopulation.Reserve(StringPopulation.Num());

	if (bEnableElitism)
	{
		NextPopulation.Add(StringPopulation[BestIndex]);
	}

	while (NextPopulation.Num() < StringPopulation.Num())
	{
		const FString ParentA = SelectParentCandidate(StringPopulation, FitnessValues);
		const FString ParentB = SelectParentCandidate(StringPopulation, FitnessValues);

		FString Child = ParentA;
		if (ActiveTargetLength > 1 && FMath::FRand() < SafeCrossoverRate)
		{
			const int32 SplitIndex = FMath::RandRange(1, ActiveTargetLength - 1);
			Child = ParentA.Left(SplitIndex) + ParentB.Mid(SplitIndex);
		}

		MutateCandidate(Child, ActiveTargetString, SafeMutationRate);
		NextPopulation.Add(MoveTemp(Child));
	}

	StringPopulation = MoveTemp(NextPopulation);
}