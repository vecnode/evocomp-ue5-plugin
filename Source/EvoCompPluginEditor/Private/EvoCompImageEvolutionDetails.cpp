#include "EvoCompImageEvolutionDetails.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "EvoCompImageEvolutionAlgorithm.h"
#include "Widgets/Input/SButton.h"

#define LOCTEXT_NAMESPACE "FEvoCompImageEvolutionDetails"

TSharedRef<IDetailCustomization> FEvoCompImageEvolutionDetails::MakeInstance()
{
	return MakeShared<FEvoCompImageEvolutionDetails>();
}

void FEvoCompImageEvolutionDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);

	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory("Image Evo | Actions");

	Category.AddCustomRow(LOCTEXT("InitImageEvolution", "Initialize Evolution"))
	.WholeRowContent()
	[
		SNew(SButton)
		.Text(LOCTEXT("InitImageEvolutionButton", "Initialize + Run Realtime"))
		.ToolTipText(LOCTEXT("InitImageEvolutionTooltip", "Prepare target patch map, create a random population, and start non-blocking realtime evolution."))
		.OnClicked_Lambda([Objects]()
		{
			for (const TWeakObjectPtr<UObject>& Object : Objects)
			{
				if (AEvoCompImageEvolutionAlgorithm* ImageActor = Cast<AEvoCompImageEvolutionAlgorithm>(Object.Get()))
				{
					ImageActor->InitializeEvolution();
					ImageActor->RunEvolution();
				}
			}

			return FReply::Handled();
		})
	];

	Category.AddCustomRow(LOCTEXT("StepImageEvolution", "Step One Generation"))
	.WholeRowContent()
	[
		SNew(SButton)
		.Text(LOCTEXT("StepImageEvolutionButton", "Step One Generation"))
		.ToolTipText(LOCTEXT("StepImageEvolutionTooltip", "Run one evolution generation and refresh preview."))
		.OnClicked_Lambda([Objects]()
		{
			for (const TWeakObjectPtr<UObject>& Object : Objects)
			{
				if (AEvoCompImageEvolutionAlgorithm* ImageActor = Cast<AEvoCompImageEvolutionAlgorithm>(Object.Get()))
				{
					ImageActor->StepOneGeneration();
				}
			}

			return FReply::Handled();
		})
	];

	Category.AddCustomRow(LOCTEXT("ResetImageEvolution", "Reset Evolution"))
	.WholeRowContent()
	[
		SNew(SButton)
		.Text(LOCTEXT("ResetImageEvolutionButton", "Reset Evolution"))
		.ToolTipText(LOCTEXT("ResetImageEvolutionTooltip", "Clear population and generated preview texture."))
		.OnClicked_Lambda([Objects]()
		{
			for (const TWeakObjectPtr<UObject>& Object : Objects)
			{
				if (AEvoCompImageEvolutionAlgorithm* ImageActor = Cast<AEvoCompImageEvolutionAlgorithm>(Object.Get()))
				{
					ImageActor->ResetEvolution();
				}
			}

			return FReply::Handled();
		})
	];

	Category.AddCustomRow(LOCTEXT("ToggleRealtimeImageEvolution", "Pause Realtime"))
	.WholeRowContent()
	[
		SNew(SButton)
		.Text(LOCTEXT("ToggleRealtimeImageEvolutionButton", "Pause Realtime Evolution"))
		.ToolTipText(LOCTEXT("ToggleRealtimeImageEvolutionTooltip", "Pause tick-driven generation updates without resetting the current population."))
		.OnClicked_Lambda([Objects]()
		{
			for (const TWeakObjectPtr<UObject>& Object : Objects)
			{
				if (AEvoCompImageEvolutionAlgorithm* ImageActor = Cast<AEvoCompImageEvolutionAlgorithm>(Object.Get()))
				{
					ImageActor->SetRealtimeEvolutionEnabled(false);
				}
			}

			return FReply::Handled();
		})
	];
}

#undef LOCTEXT_NAMESPACE
