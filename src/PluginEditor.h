#pragma once

#include <array>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "ui/DrumPadComponent.hpp"
#include "ui/StepSequencerComponent.hpp"

class PluginEditor : public juce::AudioProcessorEditor,
                     private juce::Timer
{
public:
	enum class LayoutMode
	{
		Launchkey_2x8,
		Grid_4x4
	};

	explicit PluginEditor (PluginProcessor&);
	~PluginEditor() override;

	void paint (juce::Graphics&) override;
	void resized() override;
	void parentHierarchyChanged() override;
	void visibilityChanged() override;

	void setLayoutMode (LayoutMode mode);
	LayoutMode getLayoutMode() const noexcept { return layoutMode_; }

private:
	void timerCallback() override;

	struct RotaryKnob
	{
		juce::Slider slider;
		juce::Label label;
		std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
	};

	void setupKnob (RotaryKnob& knob, const juce::String& paramId, const juce::String& labelText);

	PluginProcessor& processor;
	std::array<DrumPadComponent, 16> drumPads;
	LayoutMode layoutMode_ = LayoutMode::Launchkey_2x8;
	juce::TextButton layoutButton;

	// Master FX Knobs
	RotaryKnob reverbRoomKnob;
	RotaryKnob reverbDampKnob;
	RotaryKnob reverbWidthKnob;
	RotaryKnob reverbMixKnob;

	RotaryKnob delayTimeKnob;
	RotaryKnob delayFeedbackKnob;
	RotaryKnob delayMixKnob;

	// 16-Step Sequencer
	StepSequencerComponent sequencerComponent;
	int selectedPadIndex_ { 2 }; // Snare 1 (index 2)
	void selectPad (int index);

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
