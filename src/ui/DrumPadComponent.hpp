#pragma once

#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>

class DrumPadComponent : public juce::Component
{
public:
	DrumPadComponent();
	~DrumPadComponent() override = default;

	void setPadInfo (int note, const juce::String& name, const juce::String& noteLabel);
	int getMidiNote() const noexcept { return midiNote_; }

	void trigger (float velocity = 1.0f);
	void updateAnimation (float decayFactor = 0.85f);

	void setSelected (bool selected);
	bool isSelected() const noexcept { return isSelected_; }

	void mouseDown (const juce::MouseEvent& event) override;
	void paint (juce::Graphics& g) override;

	std::function<void (int note, float velocity)> onTrigger;
	std::function<void (int note)> onSelect;

private:
	int midiNote_ { 36 };
	juce::String padName_ { "Kick" };
	juce::String noteName_ { "36 / C1" };

	float triggerIntensity_ { 0.0f };
	bool isTriggered_ { false };
	bool isSelected_ { false };

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumPadComponent)
};
