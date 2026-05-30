#include "EvoCompPluginEditor.h"

#include "EvoCompGeneticAlgorithmDetails.h"
#include "EvoCompImageEvolutionDetails.h"
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

	static const FEvoCompAlgorithmBlueprintEntry ImageEvolutionBlueprintEntry
	{
		LOCTEXT("OpenImageEvolutionBlueprintLabel", "Open Image Evolution Main Blueprint"),
		LOCTEXT("OpenImageEvolutionBlueprintTooltip", "Open or create the plugin image evolution blueprint."),
		TEXT("/EvoCompPlugin/BP_ImageEvolution_Main.BP_ImageEvolution_Main"),
		TEXT("BP_ImageEvolution_Main")
	};

	static const FEvoCompAlgorithmBlueprintEntry ImageEvolutionBlueprintFallbackEntry
	{
		LOCTEXT("OpenImageEvolutionBlueprintFallbackLabel", "Open Image Evolution Main Blueprint"),
		LOCTEXT("OpenImageEvolutionBlueprintFallbackTooltip", "Open or create the plugin image evolution blueprint."),
		TEXT("/EvoCompPlugin/BP_IE_Main.BP_IE_Main"),
		TEXT("BP_IE_Main")
	};
}

void FEvoCompPluginEditorModule::StartupModule()
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyEditorModule.RegisterCustomClassLayout(
		TEXT("EvoCompGeneticAlgorithm"),
		FOnGetDetailCustomizationInstance::CreateStatic(&FEvoCompGeneticAlgorithmDetails::MakeInstance));

	PropertyEditorModule.RegisterCustomClassLayout(
		TEXT("EvoCompImageEvolutionAlgorithm"),
		FOnGetDetailCustomizationInstance::CreateStatic(&FEvoCompImageEvolutionDetails::MakeInstance));

	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FEvoCompPluginEditorModule::RegisterMenus));
}

void FEvoCompPluginEditorModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyEditorModule.UnregisterCustomClassLayout(TEXT("EvoCompGeneticAlgorithm"));
		PropertyEditorModule.UnregisterCustomClassLayout(TEXT("EvoCompImageEvolutionAlgorithm"));
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

	Section.AddMenuEntry(
		"OpenMainImageEvolutionBlueprint",
		ImageEvolutionBlueprintEntry.MenuLabel,
		ImageEvolutionBlueprintEntry.MenuTooltip,
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateRaw(this, &FEvoCompPluginEditorModule::OpenOrCreateImageEvolutionBlueprint)));
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

void FEvoCompPluginEditorModule::OpenOrCreateImageEvolutionBlueprint()
{
	if (UObject* ExistingAsset = StaticLoadObject(UObject::StaticClass(), nullptr, ImageEvolutionBlueprintEntry.AssetObjectPath))
	{
		if (UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
		{
			AssetEditorSubsystem->OpenEditorForAsset(ExistingAsset);
		}
		return;
	}

	if (UObject* ExistingAsset = StaticLoadObject(UObject::StaticClass(), nullptr, ImageEvolutionBlueprintFallbackEntry.AssetObjectPath))
	{
		if (UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
		{
			AssetEditorSubsystem->OpenEditorForAsset(ExistingAsset);
		}
		return;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[IMAGE-EVO] Neither %s nor %s was found. Create a Blueprint derived from AEvoCompImageEvolutionAlgorithm in /EvoCompPlugin and name it BP_ImageEvolution_Main or BP_IE_Main."),
		ImageEvolutionBlueprintEntry.MissingAssetName,
		ImageEvolutionBlueprintFallbackEntry.MissingAssetName);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FEvoCompPluginEditorModule, EvoCompPluginEditor)
