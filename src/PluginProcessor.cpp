#include <algorithm>
#include "PluginProcessor.h"
#include "PluginEditor.h"

PluginProcessor::PluginProcessor()
	: AudioProcessor (BusesProperties()
		.withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
	  apvts (*this, nullptr, "Parameters", createParameterLayout())
{
	reverbRoomSizeParam_ = apvts.getRawParameterValue ("reverb_room_size");
	reverbDampingParam_ = apvts.getRawParameterValue ("reverb_damping");
	reverbWidthParam_ = apvts.getRawParameterValue ("reverb_width");
	reverbWetParam_ = apvts.getRawParameterValue ("reverb_wet");

	delayTimeParam_ = apvts.getRawParameterValue ("delay_time");
	delayFeedbackParam_ = apvts.getRawParameterValue ("delay_feedback");
	delayWetParam_ = apvts.getRawParameterValue ("delay_wet");

	sequencerBpmParam_ = apvts.getRawParameterValue ("sequencer_bpm");
}

PluginProcessor::~PluginProcessor()
{
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
	std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

	// Reverb parameters
	params.push_back (std::make_unique<juce::AudioParameterFloat> (
		juce::ParameterID { "reverb_room_size", 1 },
		"Room Size",
		juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f),
		0.5f));

	params.push_back (std::make_unique<juce::AudioParameterFloat> (
		juce::ParameterID { "reverb_damping", 1 },
		"Damping",
		juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f),
		0.5f));

	params.push_back (std::make_unique<juce::AudioParameterFloat> (
		juce::ParameterID { "reverb_width", 1 },
		"Width",
		juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f),
		1.0f));

	params.push_back (std::make_unique<juce::AudioParameterFloat> (
		juce::ParameterID { "reverb_wet", 1 },
		"Reverb Mix",
		juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f),
		0.15f));

	// Delay parameters
	params.push_back (std::make_unique<juce::AudioParameterFloat> (
		juce::ParameterID { "delay_time", 1 },
		"Delay Time",
		juce::NormalisableRange<float> (10.0f, 1000.0f, 1.0f, 0.5f),
		250.0f,
		juce::AudioParameterFloatAttributes().withLabel ("ms")));

	params.push_back (std::make_unique<juce::AudioParameterFloat> (
		juce::ParameterID { "delay_feedback", 1 },
		"Feedback",
		juce::NormalisableRange<float> (0.0f, 0.95f, 0.01f),
		0.35f));

	params.push_back (std::make_unique<juce::AudioParameterFloat> (
		juce::ParameterID { "delay_wet", 1 },
		"Delay Mix",
		juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f),
		0.0f));

	// Sequencer parameters
	params.push_back (std::make_unique<juce::AudioParameterFloat> (
		juce::ParameterID { "sequencer_bpm", 1 },
		"Sequencer BPM",
		juce::NormalisableRange<float> (40.0f, 240.0f, 1.0f),
		120.0f,
		juce::AudioParameterFloatAttributes().withLabel ("BPM")));

	return { params.begin(), params.end() };
}

const juce::String PluginProcessor::getName() const
{
	return "dreamdrums";
}

bool PluginProcessor::acceptsMidi() const
{
	return true;
}

bool PluginProcessor::producesMidi() const
{
	return true;
}

bool PluginProcessor::isMidiEffect() const
{
	return false;
}

double PluginProcessor::getTailLengthSeconds() const
{
	return 0.0;
}

int PluginProcessor::getNumPrograms()
{
	return 1;
}

int PluginProcessor::getCurrentProgram()
{
	return 0;
}

void PluginProcessor::setCurrentProgram (int)
{
}

const juce::String PluginProcessor::getProgramName (int)
{
	return {};
}

void PluginProcessor::changeProgramName (int, const juce::String&)
{
}

void PluginProcessor::triggerPad (int midiNote, float velocity)
{
	if (midiNote >= 0 && midiNote < 128)
	{
		padTriggers_[static_cast<size_t> (midiNote)].store (velocity, std::memory_order_relaxed);
		keyboardState.noteOn (1, midiNote, velocity);
		keyboardState.noteOff (1, midiNote, 0.0f);
	}
}

float PluginProcessor::popPadTrigger (int midiNote)
{
	if (midiNote >= 0 && midiNote < 128)
		return padTriggers_[static_cast<size_t> (midiNote)].exchange (0.0f, std::memory_order_relaxed);

	return 0.0f;
}

void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
	currentSampleRate_ = sampleRate;
	keyboardState.reset();
	voicePool_.reset();
	stepSequencer_.reset();

	for (auto& trigger : padTriggers_)
		trigger.store (0.0f, std::memory_order_relaxed);

	juce::dsp::ProcessSpec spec;
	spec.sampleRate = sampleRate;
	spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
	spec.numChannels = static_cast<juce::uint32> (getTotalNumOutputChannels() > 0 ? getTotalNumOutputChannels() : 2);

	reverb_.prepare (spec);
	reverb_.reset();

	delayLine_.prepare (spec);
	delayLine_.reset();

	if (sampleBank_.isEmpty())
	{
		const auto assetDir = SampleBank::findAssetsDirectory();
		if (assetDir.isDirectory())
		{
			// Launchkey Row 1 (Notes 36 - 43)
			sampleBank_.loadInstrument (36, assetDir, "kick_OH", "KICK 1");
			sampleBank_.loadInstrument (37, assetDir, "snareStick_OH", "SIDE STICK");
			sampleBank_.loadInstrument (38, assetDir, "snare2_OH", "SNARE 1");
			sampleBank_.loadInstrument (39, assetDir, "cowbell", "CLAP / PERC");
			sampleBank_.loadInstrument (40, assetDir, "snare_OH", "SNARE 2");
			sampleBank_.loadInstrument (41, assetDir, "loTom_OH", "LOW FL TOM");
			sampleBank_.loadInstrument (42, assetDir, "hihatClosed_OH", "CLOSED HH");
			sampleBank_.loadInstrument (43, assetDir, "loTom_OH", "HI FL TOM");

			// Launchkey Row 2 (Notes 44 - 51)
			sampleBank_.loadInstrument (44, assetDir, "hihatFoot_OH", "PEDAL HH");
			sampleBank_.loadInstrument (45, assetDir, "loTom_OH", "LOW TOM");
			sampleBank_.loadInstrument (46, assetDir, "hihatOpen_OH", "OPEN HH");
			sampleBank_.loadInstrument (47, assetDir, "hiTom_OH", "MID TOM");
			sampleBank_.loadInstrument (48, assetDir, "hiTom_OH", "HI-MID TOM");
			sampleBank_.loadInstrument (49, assetDir, "crash1_OH", "CRASH 1");
			sampleBank_.loadInstrument (50, assetDir, "hiTom_OH", "HIGH TOM");
			sampleBank_.loadInstrument (51, assetDir, "ride1_OH", "RIDE");
		}
	}
}

void PluginProcessor::releaseResources()
{
	voicePool_.reset();
	reverb_.reset();
	delayLine_.reset();
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
	if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
		return false;

	return true;
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
	juce::ScopedNoDenormals noDenormals;

	auto totalNumInputChannels = getTotalNumInputChannels();
	auto totalNumOutputChannels = getTotalNumOutputChannels();

	for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
		buffer.clear (i, 0, buffer.getNumSamples());

	keyboardState.processNextMidiBuffer (midiMessages, 0, buffer.getNumSamples(), true);

	if (totalNumOutputChannels == 0)
		return;

	// Clear output buffer before active voices mix into it
	buffer.clear();

	if (sequencerBpmParam_ != nullptr)
		stepSequencer_.setBpm (sequencerBpmParam_->load (std::memory_order_relaxed));

	stepSequencer_.processBlock (sampleBank_, voicePool_, currentSampleRate_, buffer.getNumSamples(), getPlayHead(),
		[this] (int note, float vel) {
			if (note >= 0 && note < 128)
				padTriggers_[static_cast<size_t> (note)].store (vel, std::memory_order_relaxed);
		});

	int currentSample = 0;
	const int numSamples = buffer.getNumSamples();

	for (const auto metadata : midiMessages)
	{
		const auto& message = metadata.getMessage();
		const auto messagePosition = std::clamp (metadata.samplePosition, 0, numSamples);

		if (messagePosition > currentSample)
		{
			voicePool_.render (buffer, currentSample, messagePosition - currentSample);
			currentSample = messagePosition;
		}

		if (message.isNoteOn (false))
		{
			const int rawNote = message.getNoteNumber();
			const float velocity = message.getFloatVelocity();
			const int velInt = message.getVelocity();

			// Map standard GM drum notes to available pads
			int targetNote = rawNote;
			switch (rawNote)
			{
				case 35: targetNote = 36; break; // Acoustic Bass Drum -> Kick 1
				case 36: targetNote = 36; break; // Bass Drum 1 (Kick 1)
				case 37: targetNote = 37; break; // Side Stick
				case 38: targetNote = 38; break; // Acoustic Snare (Snare 1)
				case 39: targetNote = 39; break; // Hand Clap / Cowbell
				case 40: targetNote = 40; break; // Electric Snare (Snare 2)
				case 41: targetNote = 41; break; // Low Floor Tom
				case 42: targetNote = 42; break; // Closed Hi-Hat
				case 43: targetNote = 43; break; // High Floor Tom
				case 44: targetNote = 44; break; // Pedal Hi-Hat
				case 45: targetNote = 45; break; // Low Tom
				case 46: targetNote = 46; break; // Open Hi-Hat
				case 47: targetNote = 47; break; // Low-Mid Tom
				case 48: targetNote = 48; break; // Hi-Mid Tom
				case 49: targetNote = 49; break; // Crash Cymbal 1
				case 50: targetNote = 50; break; // High Tom
				case 51: targetNote = 51; break; // Ride Cymbal 1
				case 52: targetNote = 49; break; // Chinese Cymbal -> Crash 1
				case 53: targetNote = 51; break; // Ride Bell -> Ride 1
				case 54: targetNote = 39; break; // Tambourine -> Clap / Perc
				case 55: targetNote = 49; break; // Splash Cymbal -> Crash 1
				case 56: targetNote = 39; break; // Cowbell -> Clap / Perc
				case 57: targetNote = 49; break; // Crash Cymbal 2 -> Crash 1
				case 58: targetNote = 39; break; // Vibraslap -> Clap / Perc
				case 59: targetNote = 51; break; // Ride Cymbal 2 -> Ride 1
				default: break;
			}

			if (targetNote >= 0 && targetNote < 128)
				padTriggers_[static_cast<size_t> (targetNote)].store (velocity, std::memory_order_relaxed);

			if (auto* pad = sampleBank_.getPad (targetNote))
			{
				if (const auto* sample = pad->findSampleForVelocity (velInt))
				{
					voicePool_.triggerSample (sample, velocity, currentSampleRate_);
				}
			}
		}
		else if (message.isAllNotesOff() || message.isAllSoundOff())
		{
			voicePool_.reset();
		}
	}

	if (currentSample < numSamples)
	{
		voicePool_.render (buffer, currentSample, numSamples - currentSample);
	}

	// 1. Master Stereo Delay
	if (totalNumOutputChannels > 0)
	{
		const float delayTimeMs = delayTimeParam_->load (std::memory_order_relaxed);
		const float feedback = std::clamp (delayFeedbackParam_->load (std::memory_order_relaxed), 0.0f, 0.95f);
		const float delayWet = delayWetParam_->load (std::memory_order_relaxed);
		const float delayInSamples = (delayTimeMs * 0.001f) * static_cast<float> (currentSampleRate_);

		auto* left = buffer.getWritePointer (0);
		auto* right = totalNumOutputChannels > 1 ? buffer.getWritePointer (1) : nullptr;

		for (int i = 0; i < numSamples; ++i)
		{
			const float inL = left[i];
			const float delayedL = delayLine_.popSample (0, delayInSamples, true);
			delayLine_.pushSample (0, inL + delayedL * feedback);
			left[i] = inL + delayedL * delayWet;

			if (right != nullptr)
			{
				const float inR = right[i];
				const float delayedR = delayLine_.popSample (1, delayInSamples, true);
				delayLine_.pushSample (1, inR + delayedR * feedback);
				right[i] = inR + delayedR * delayWet;
			}
		}
	}

	// 2. Master Studio Reverb
	{
		juce::dsp::Reverb::Parameters params;
		params.roomSize = reverbRoomSizeParam_->load (std::memory_order_relaxed);
		params.damping = reverbDampingParam_->load (std::memory_order_relaxed);
		params.width = reverbWidthParam_->load (std::memory_order_relaxed);
		params.wetLevel = reverbWetParam_->load (std::memory_order_relaxed);
		params.dryLevel = 1.0f;
		reverb_.setParameters (params);

		juce::dsp::AudioBlock<float> block (buffer);
		juce::dsp::ProcessContextReplacing<float> context (block);
		reverb_.process (context);
	}
}

bool PluginProcessor::hasEditor() const
{
	return true;
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
	return new PluginEditor (*this);
}

void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
	auto state = apvts.copyState();
	state.appendChild (stepSequencer_.serializeToValueTree(), nullptr);
	std::unique_ptr<juce::XmlElement> xml (state.createXml());
	copyXmlToBinary (*xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
	std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
	if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
	{
		auto state = juce::ValueTree::fromXml (*xmlState);
		auto seqChild = state.getChildWithName ("StepSequencer");
		if (seqChild.isValid())
			stepSequencer_.deserializeFromValueTree (seqChild);
		apvts.replaceState (state);
	}
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
	return new PluginProcessor();
}
