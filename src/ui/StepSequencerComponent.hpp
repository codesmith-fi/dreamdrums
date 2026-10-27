#pragma once

#include <array>
#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../engine/StepSequencer.hpp"

class StepButtonComponent : public juce::Component
{
public:
	StepButtonComponent();
	~StepButtonComponent() override = default;

	void setStepIndex (int index);
	int getStepIndex() const noexcept { return stepIndex_; }

	void setStepData (bool active, int midiNote, float velocity);
	bool isActive() const noexcept { return active_; }
	float getVelocity() const noexcept { return velocity_; }
	int getMidiNote() const noexcept { return midiNote_; }

	void setPlayhead (bool isPlayhead);
	bool isPlayhead() const noexcept { return isPlayhead_; }

	void mouseDown (const juce::MouseEvent& event) override;
	void mouseDrag (const juce::MouseEvent& event) override;
	void mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;
	void paint (juce::Graphics& g) override;

	std::function<void (int stepIndex, bool active, int midiNote, float velocity)> onStepChanged;
	std::function<int()> getActivePadNote;
	std::function<void (int note, float velocity)> onPreviewTrigger;

private:
	void showNoteContextMenu();
	juce::String getDynamicLayerName() const;

	int stepIndex_ { 0 };
	bool active_ { false };
	int midiNote_ { 38 };
	float velocity_ { 0.85f };
	bool isPlayhead_ { false };

	float dragStartVelocity_ { 0.85f };

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepButtonComponent)
};

class StepSequencerComponent : public juce::Component
{
public:
	StepSequencerComponent();
	~StepSequencerComponent() override = default;

	void setSequencer (StepSequencer* sequencer);
	void setSelectedTrack (int trackIndex, const juce::String& drumName, int midiNote, const juce::String& noteLabel);
	int getSelectedTrack() const noexcept { return selectedTrackIndex_; }

	void updatePlayhead (int currentStep);
	void refreshSteps();

	void resized() override;
	void paint (juce::Graphics& g) override;

	std::function<void (int note, float vel)> onPreviewTrigger;

private:
	StepSequencer* sequencer_ { nullptr };
	int selectedTrackIndex_ { 2 }; // Default: Snare 1 (track index 2, note 38)
	int selectedMidiNote_ { 38 };
	juce::String selectedDrumName_ { "SNARE 1" };
	juce::String selectedNoteLabel_ { "38 / D1" };

	// Transport UI
	juce::TextButton playButton;
	juce::Label bpmLabel;
	juce::Slider bpmSlider;
	juce::Label trackInfoLabel;
	juce::TextButton clearTrackButton;
	juce::TextButton clearAllButton;

	// 16 Step Buttons
	std::array<StepButtonComponent, 16> stepButtons;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepSequencerComponent)
};
