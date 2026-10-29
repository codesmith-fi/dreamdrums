#pragma once

#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "engine/SampleBank.hpp"
#include "engine/DrumVoicePool.hpp"
#include "engine/StepSequencer.hpp"

class PluginProcessor : public juce::AudioProcessor
{
public:
	PluginProcessor();
	~PluginProcessor() override;

	void prepareToPlay (double sampleRate, int samplesPerBlock) override;
	void releaseResources() override;

	bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

	using juce::AudioProcessor::processBlock;
	void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

	juce::AudioProcessorEditor* createEditor() override;
	bool hasEditor() const override;

	const juce::String getName() const override;

	bool acceptsMidi() const override;
	bool producesMidi() const override;
	bool isMidiEffect() const override;
	double getTailLengthSeconds() const override;

	int getNumPrograms() override;
	int getCurrentProgram() override;
	void setCurrentProgram (int index) override;
	const juce::String getProgramName (int index) override;
	void changeProgramName (int index, const juce::String& newName) override;

	void getStateInformation (juce::MemoryBlock& destData) override;
	void setStateInformation (const void* data, int sizeInBytes) override;

	juce::MidiKeyboardState keyboardState;
	juce::AudioProcessorValueTreeState apvts;

	SampleBank& getSampleBank() noexcept { return sampleBank_; }
	const SampleBank& getSampleBank() const noexcept { return sampleBank_; }

	StepSequencer& getStepSequencer() noexcept { return stepSequencer_; }
	const StepSequencer& getStepSequencer() const noexcept { return stepSequencer_; }

	void triggerPad (int midiNote, float velocity);
	float popPadTrigger (int midiNote);

	void setSequencerSync (bool sync)
	{
		stepSequencer_.setSyncToHost (sync);
		lastSequencerSync_ = sync;
		if (auto* param = apvts.getParameter ("sequencer_sync"))
			param->setValueNotifyingHost (sync ? 1.0f : 0.0f);
	}

	void setSequencerBpm (float bpm)
	{
		stepSequencer_.setBpm (bpm);
		lastSequencerBpm_ = bpm;
		if (auto* param = apvts.getParameter ("sequencer_bpm"))
			param->setValueNotifyingHost (param->convertTo0to1 (bpm));
	}
	static constexpr int kNumPads = 16;
	static constexpr int kPadNotes[kNumPads] = {
		40, 41, 42, 43, 48, 49, 50, 51,
		36, 37, 38, 39, 44, 45, 46, 47
	};

	void setPadOutputBus (int padIndex, int busIndex);
	int getPadOutputBus (int padIndex) const;
	void updatePadRoutingInEngine (int padIndex, int busIndex);

private:
	static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
	static juce::AudioProcessor::BusesProperties createBusesProperties();

	std::array<std::atomic<float>*, kNumPads> padBusParams_ {};

	SampleBank sampleBank_;
	DrumVoicePool voicePool_;
	StepSequencer stepSequencer_;
	double currentSampleRate_ { 48000.0 };

	std::array<std::atomic<float>, 128> padTriggers_ {};

	// Master FX
	juce::dsp::Reverb reverb_;
	juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLine_ { 192000 };

	// Reverb APVTS raw parameter pointers
	std::atomic<float>* reverbRoomSizeParam_ { nullptr };
	std::atomic<float>* reverbDampingParam_ { nullptr };
	std::atomic<float>* reverbWidthParam_ { nullptr };
	std::atomic<float>* reverbWetParam_ { nullptr };

	// Delay APVTS raw parameter pointers
	std::atomic<float>* delayTimeParam_ { nullptr };
	std::atomic<float>* delayFeedbackParam_ { nullptr };
	std::atomic<float>* delayWetParam_ { nullptr };

	// Sequencer APVTS raw parameter pointer
	std::atomic<float>* sequencerBpmParam_ { nullptr };
	std::atomic<float>* sequencerSyncParam_ { nullptr };
	float lastSequencerBpm_ { 120.0f };
	bool lastSequencerSync_ { true };

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
