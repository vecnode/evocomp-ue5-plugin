#pragma once

#include "Modules/ModuleManager.h"

struct FToolMenuSection;

class FEvoCompPluginEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterMenus();
	void AddMenuEntry(FToolMenuSection& Section);
	void OpenOrCreateMainBlueprint();
	void OpenOrCreateImageEvolutionBlueprint();
};
