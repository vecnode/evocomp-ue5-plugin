#include "EvoCompPluginEditor.h"

#include "EvoCompGeneticAlgorithmDetails.h"
#include "EvoCompGeneticAlgorithm.h"

#include "Subsystems/AssetEditorSubsystem.h"
#include "Editor.h"
#include "PropertyEditorModule.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "FEvoCompPluginEditorModule"

namespace
{
	struct FEvoCompAlgorithmBlueprintEntry
	{
		FText MenuLabel;
		FText MenuTooltip;
		const TCHAR* AssetObjectPath;
		const TCHAR* MissingAssetName;
	};

	static const FEvoCompAlgorithmBlueprintEntry GeneticAlgorithmBlueprintEntry
	{
		LOCTEXT("OpenMainBlueprintLabel", "Open Genetic Algorithm Main Blueprint"),
		LOCTEXT("OpenMainBlueprintTooltip", "Open or create the plugin genetic algorithm blueprint."),
		TEXT("/EvoCompPlugin/BP_GA_Main.BP_GA_Main"),
		TEXT("BP_GA_Main")
	};
}

void FEvoCompPluginEditorModule::StartupModule()
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyEditorModule.RegisterCustomClassLayout(
		TEXT("EvoCompGeneticAlgorithm"),
		FOnGetDetailCustomizationInstance::CreateStatic(&FEvoCompGeneticAlgorithmDetails::MakeInstance));

	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FEvoCompPluginEditorModule::RegisterMenus));
}

void FEvoCompPluginEditorModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyEditorModule.UnregisterCustomClassLayout(TEXT("EvoCompGeneticAlgorithm"));
	}

	UToolMenus::UnregisterOwner(this);
}

void FEvoCompPluginEditorModule::RegisterMenus()
{
	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
	FToolMenuSection& Section = Menu->AddSection("EvoCompPlugin", LOCTEXT("EvoCompPluginSection", "Genetic Algorithm"));
	AddMenuEntry(Section);
}

void FEvoCompPluginEditorModule::AddMenuEntry(FToolMenuSection& Section)
{
	Section.AddMenuEntry(
		"OpenMainGeneticAlgorithmBlueprint",
		GeneticAlgorithmBlueprintEntry.MenuLabel,
		GeneticAlgorithmBlueprintEntry.MenuTooltip,
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateRaw(this, &FEvoCompPluginEditorModule::OpenOrCreateMainBlueprint)));
}

void FEvoCompPluginEditorModule::OpenOrCreateMainBlueprint()
{
	// Open the BP_GA_Main actor blueprint created by the user in the Content Browser.
	if (UObject* ExistingAsset = StaticLoadObject(UObject::StaticClass(), nullptr, GeneticAlgorithmBlueprintEntry.AssetObjectPath))
	{
		if (UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
		{
			AssetEditorSubsystem->OpenEditorForAsset(ExistingAsset);
		}
		return;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[GA] %s not found. Create a Blueprint derived from AEvoCompGeneticAlgorithm in the Content Browser and name it BP_GA_Main."),
		GeneticAlgorithmBlueprintEntry.MissingAssetName);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FEvoCompPluginEditorModule, EvoCompPluginEditor)
