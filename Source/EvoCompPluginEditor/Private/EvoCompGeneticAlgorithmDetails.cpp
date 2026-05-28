#include "EvoCompGeneticAlgorithmDetails.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "EvoCompGeneticAlgorithm.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "FEvoCompGeneticAlgorithmDetails"

TSharedRef<IDetailCustomization> FEvoCompGeneticAlgorithmDetails::MakeInstance()
{
	return MakeShared<FEvoCompGeneticAlgorithmDetails>();
}

void FEvoCompGeneticAlgorithmDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);

	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory("GA | Actions");

	Category.AddCustomRow(LOCTEXT("GARunSearch", "Run Genetic Algorithm"))
	.WholeRowContent()
	[
		SNew(SButton)
		.Text(LOCTEXT("GARunButton", "Run Genetic Algorithm"))
		.ToolTipText(LOCTEXT("GARunTooltip", "Execute the full GA run using current configuration values."))
		.OnClicked_Lambda([Objects]()
		{
			for (const TWeakObjectPtr<UObject>& Object : Objects)
			{
				if (AEvoCompGeneticAlgorithm* GAActor = Cast<AEvoCompGeneticAlgorithm>(Object.Get()))
				{
					GAActor->RunGeneticAlgorithm();
				}
			}

			return FReply::Handled();
		})
	];

	Category.AddCustomRow(LOCTEXT("GAStepSearch", "Step One Generation"))
	.WholeRowContent()
	[
		SNew(SButton)
		.Text(LOCTEXT("GAStepButton", "Step One Generation"))
		.ToolTipText(LOCTEXT("GAStepTooltip", "Advance the GA by one generation and print progress logs."))
		.OnClicked_Lambda([Objects]()
		{
			for (const TWeakObjectPtr<UObject>& Object : Objects)
			{
				if (AEvoCompGeneticAlgorithm* GAActor = Cast<AEvoCompGeneticAlgorithm>(Object.Get()))
				{
					GAActor->StepOneGeneration();
				}
			}

			return FReply::Handled();
		})
	];

	Category.AddCustomRow(LOCTEXT("GAResetSearch", "Reset Population"))
	.WholeRowContent()
	[
		SNew(SButton)
		.Text(LOCTEXT("GAResetButton", "Reset Population"))
		.ToolTipText(LOCTEXT("GAResetTooltip", "Clear GA state and reset generation/fitness counters."))
		.OnClicked_Lambda([Objects]()
		{
			for (const TWeakObjectPtr<UObject>& Object : Objects)
			{
				if (AEvoCompGeneticAlgorithm* GAActor = Cast<AEvoCompGeneticAlgorithm>(Object.Get()))
				{
					GAActor->ResetPopulation();
				}
			}

			return FReply::Handled();
		})
	];
}

#undef LOCTEXT_NAMESPACE