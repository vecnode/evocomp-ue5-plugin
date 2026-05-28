#pragma once

#include "IDetailCustomization.h"

class IDetailLayoutBuilder;

class FEvoCompMainActorDetails : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};
