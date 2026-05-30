#pragma once

#include "IDetailCustomization.h"

class IDetailLayoutBuilder;

class FEvoCompImageEvolutionDetails : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};
