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

	void setInLoopRange (bool inRange);
	bool isInLoopRange() const noexcept { return isInLoopRange_; }

	void mouseDown (const juce::MouseEvent& event) override;
	void mouseDrag (const juce::MouseEvent& event) override;
	void mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;
	void paint (juce::Graphics& g) override;

	std::function<void (int stepIndex, bool active, int midiNote, float velocity)> onStepChanged;
	std::function<int()> getActivePadNote;
	std::function<void (int note, float velocity)> onPreviewTrigger;

private:
	juce::String getDynamicLayerName() const;

	int stepIndex_ { 0 };
	bool active_ { false };
	int midiNote_ { 38 };
	float velocity_ { 0.85f };
	bool isPlayhead_ { false };
	bool isInLoopRange_ { true };

	float dragStartVelocity_ { 0.85f };

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepButtonComponent)
};

class BpmSlider : public juce::Slider
{
public:
	BpmSlider()
	{
		setScrollWheelEnabled (true);
	}

	void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
	{
		if (! isEnabled())
			return;

		if (std::abs (wheel.deltaY) > 1e-4f)
		{
			const double step = (e.mods.isShiftDown() ? 5.0 : 1.0);
			const double delta = (wheel.deltaY > 0.0f ? step : -step);
			setValue (std::clamp (getValue() + delta, getMinimum(), getMaximum()), juce::sendNotificationSync);
		}
	}
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
	void updateLoopVisuals();
	void updateTempoSyncUI();

	void resized() override;
	void paint (juce::Graphics& g) override;

	std::function<void (int note, float vel)> onPreviewTrigger;
	std::function<void (bool sync)> onSyncModeChanged;
	std::function<void (float bpm)> onBpmChanged;

private:
	StepSequencer* sequencer_ { nullptr };
	int selectedTrackIndex_ { 2 }; // Default: Snare 1 (track index 2, note 38)
	int selectedMidiNote_ { 38 };
	juce::String selectedDrumName_ { "SNARE 1" };
	juce::String selectedNoteLabel_ { "38 / D1" };
	bool lastObservedSync_ { true };

	// Transport UI
	juce::TextButton playButton;
	juce::Label bpmLabel;
	BpmSlider bpmSlider;
	juce::TextButton tempoSyncBtn;

	// Loop boundaries UI
	juce::Label loopStartLabel;
	juce::Slider loopStartSlider;
	juce::Label loopEndLabel;
	juce::Slider loopEndSlider;
	juce::TextButton loop1BarBtn;
	juce::TextButton loop2BarsBtn;

	juce::Label trackInfoLabel;
	juce::TextButton clearTrackButton;
	juce::TextButton clearAllButton;

	// 32 Step Buttons (2 bars of 16 steps)
	std::array<StepButtonComponent, 32> stepButtons;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepSequencerComponent)
};
