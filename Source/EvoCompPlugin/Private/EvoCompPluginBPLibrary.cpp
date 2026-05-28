// Copyright Epic Games, Inc. All Rights Reserved.

#include "EvoCompPluginBPLibrary.h"
#include "EvoCompPlugin.h"

UEvoCompPluginBPLibrary::UEvoCompPluginBPLibrary(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{

}

void UEvoCompPluginBPLibrary::ExecuteGeneticAlgorithm()
{
	UE_LOG(LogTemp, Display, TEXT("[GA] Blueprint library execution called."));
}

