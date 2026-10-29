#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class AboutComponent : public juce::Component
{
public:
	AboutComponent();
	~AboutComponent() override = default;

	void paint (juce::Graphics& g) override;
	void resized() override;

	static void show();
	static void applyAppIcon (juce::Component* comp);

private:
	juce::Image appIcon_;
	juce::TextButton closeButton_ { "Close" };

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AboutComponent)
};
