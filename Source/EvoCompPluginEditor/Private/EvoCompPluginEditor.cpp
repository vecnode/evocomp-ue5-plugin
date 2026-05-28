#include "EvoCompPluginEditor.h"

#include "EvoCompMainActorDetails.h"

#include "Subsystems/AssetEditorSubsystem.h"
#include "Editor.h"
#include "PropertyEditorModule.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "FEvoCompPluginEditorModule"

void FEvoCompPluginEditorModule::StartupModule()
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyEditorModule.RegisterCustomClassLayout(
		TEXT("EvoCompMainActor"),
		FOnGetDetailCustomizationInstance::CreateStatic(&FEvoCompMainActorDetails::MakeInstance));

	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FEvoCompPluginEditorModule::RegisterMenus));
}

void FEvoCompPluginEditorModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyEditorModule.UnregisterCustomClassLayout(TEXT("EvoCompMainActor"));
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
		LOCTEXT("OpenMainBlueprintLabel", "Open Genetic Algorithm Main Blueprint"),
		LOCTEXT("OpenMainBlueprintTooltip", "Open or create the plugin main blueprint widget."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateRaw(this, &FEvoCompPluginEditorModule::OpenOrCreateMainBlueprint)));
}

void FEvoCompPluginEditorModule::OpenOrCreateMainBlueprint()
{
	// Open the BP_GA_Main actor blueprint created by the user in the Content Browser.
	static const FString AssetObjectPath = TEXT("/EvoCompPlugin/BP_GA_Main.BP_GA_Main");
	if (UObject* ExistingAsset = StaticLoadObject(UObject::StaticClass(), nullptr, *AssetObjectPath))
	{
		if (UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
		{
			AssetEditorSubsystem->OpenEditorForAsset(ExistingAsset);
		}
		return;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[GA] BP_GA_Main not found. Create a Blueprint derived from AEvoCompMainActor in the Content Browser and name it BP_GA_Main."));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FEvoCompPluginEditorModule, EvoCompPluginEditor)
