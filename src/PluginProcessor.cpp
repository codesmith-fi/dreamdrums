#include <algorithm>
#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessor::BusesProperties PluginProcessor::createBusesProperties()
{
	auto props = BusesProperties().withOutput ("Master", juce::AudioChannelSet::stereo(), true);
	for (int i = 1; i <= 15; ++i)
	{
		props = props.withOutput ("Aux " + juce::String (i), juce::AudioChannelSet::stereo(), false);
	}
	return props;
}

PluginProcessor::PluginProcessor()
	: AudioProcessor (createBusesProperties()),
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
	sequencerSyncParam_ = apvts.getRawParameterValue ("sequencer_sync");
	if (sequencerBpmParam_ != nullptr)
		lastSequencerBpm_ = sequencerBpmParam_->load (std::memory_order_relaxed);
	if (sequencerSyncParam_ != nullptr)
		lastSequencerSync_ = sequencerSyncParam_->load (std::memory_order_relaxed) > 0.5f;

	for (int i = 0; i < kNumPads; ++i)
		padBusParams_[static_cast<size_t> (i)] = apvts.getRawParameterValue ("pad_bus_" + juce::String (i));
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

	params.push_back (std::make_unique<juce::AudioParameterBool> (
		juce::ParameterID { "sequencer_sync", 1 },
		"Sequencer Sync to Host",
		true));

	// 16 Pad Routing parameters (0 = Master, 1..15 = Aux 1..15)
	for (int i = 0; i < kNumPads; ++i)
	{
		params.push_back (std::make_unique<juce::AudioParameterInt> (
			juce::ParameterID { "pad_bus_" + juce::String (i), 1 },
			"Pad " + juce::String (i) + " Routing",
			0, 15, 0));
	}

	return { params.begin(), params.end() };
}

void PluginProcessor::setPadOutputBus (int padIndex, int busIndex)
{
	if (padIndex >= 0 && padIndex < kNumPads)
	{
		busIndex = std::clamp (busIndex, 0, 15);
		if (auto* param = apvts.getParameter ("pad_bus_" + juce::String (padIndex)))
		{
			param->setValueNotifyingHost (param->convertTo0to1 (static_cast<float> (busIndex)));
		}
		updatePadRoutingInEngine (padIndex, busIndex);
	}
}

int PluginProcessor::getPadOutputBus (int padIndex) const
{
	if (padIndex >= 0 && padIndex < kNumPads && padBusParams_[static_cast<size_t> (padIndex)] != nullptr)
		return std::clamp (static_cast<int> (padBusParams_[static_cast<size_t> (padIndex)]->load (std::memory_order_relaxed)), 0, 15);
	return 0;
}

void PluginProcessor::updatePadRoutingInEngine (int padIndex, int busIndex)
{
	if (padIndex >= 0 && padIndex < kNumPads)
	{
		const int note = kPadNotes[padIndex];
		if (auto* pad = sampleBank_.getPad (note))
			pad->outputBusIndex = busIndex;
	}
}

const juce::String PluginProcessor::getName() const
{
	return "Paukku";
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
			sampleBank_.loadInstrument (41, assetDir, "loTom_OH", "LOW FL TOM", -3.0f);
			sampleBank_.loadInstrument (42, assetDir, "hihatClosed_OH", "CLOSED HH");
			sampleBank_.loadInstrument (43, assetDir, "loTom_OH", "HI FL TOM", 0.0f);

			// Launchkey Row 2 (Notes 44 - 51)
			sampleBank_.loadInstrument (44, assetDir, "hihatFoot_OH", "PEDAL HH");
			sampleBank_.loadInstrument (45, assetDir, "loTom_OH", "LOW TOM", 3.0f);
			sampleBank_.loadInstrument (46, assetDir, "hihatOpen_OH", "OPEN HH");
			sampleBank_.loadInstrument (47, assetDir, "hiTom_OH", "MID TOM", -3.0f);
			sampleBank_.loadInstrument (48, assetDir, "hiTom_OH", "HI-MID TOM", 0.0f);
			sampleBank_.loadInstrument (49, assetDir, "crash1_OH", "CRASH 1");
			sampleBank_.loadInstrument (50, assetDir, "hiTom_OH", "HIGH TOM", 3.0f);
			sampleBank_.loadInstrument (51, assetDir, "ride1_OH", "RIDE");

			// General MIDI Extended Cymbals & Percussion (Notes 52 - 59)
			sampleBank_.loadInstrument (52, assetDir, "china1_OH", "CHINESE CYMBAL");
			sampleBank_.loadInstrument (53, assetDir, "ride1Bell_OH", "RIDE BELL");
			sampleBank_.loadInstrument (54, assetDir, "bellchime", "TAMBOURINE", 4.0f);
			sampleBank_.loadInstrument (55, assetDir, "splash1_OH", "SPLASH CYMBAL");
			sampleBank_.loadInstrument (56, assetDir, "cowbell", "COWBELL");
			sampleBank_.loadInstrument (57, assetDir, "crash2_OH", "CRASH 2");
			sampleBank_.loadInstrument (58, assetDir, "cowbell", "VIBRASLAP", 7.0f);
			sampleBank_.loadInstrument (59, assetDir, "ride2_OH", "RIDE 2");

			// Configure Hi-Hat Choke Group (Closed HH 42, Pedal HH 44, Open HH 46)
			if (auto* pad = sampleBank_.getPad (42)) pad->chokeGroup = DrumPad::kHiHatChokeGroup;
			if (auto* pad = sampleBank_.getPad (44)) pad->chokeGroup = DrumPad::kHiHatChokeGroup;
			if (auto* pad = sampleBank_.getPad (46)) pad->chokeGroup = DrumPad::kHiHatChokeGroup;
		}

		for (int i = 0; i < kNumPads; ++i)
			updatePadRoutingInEngine (i, getPadOutputBus (i));
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

	for (int i = 1; i < layouts.outputBuses.size(); ++i)
	{
		const auto& bus = layouts.outputBuses[i];
		if (! bus.isDisabled() && bus != juce::AudioChannelSet::stereo())
			return false;
	}

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

	const int numOutputBuses = getBusCount (false);
	std::vector<juce::AudioBuffer<float>> busBuffers;
	busBuffers.reserve (static_cast<size_t> (numOutputBuses));
	for (int b = 0; b < numOutputBuses; ++b)
	{
		const auto* bus = getBus (false, b);
		if (bus != nullptr && bus->isEnabled() && getChannelCountOfBus (false, b) > 0)
			busBuffers.push_back (getBusBuffer (buffer, false, b));
		else
			busBuffers.push_back ({});
	}

	auto getBufferForBus = [&busBuffers, numOutputBuses] (int busIndex) -> juce::AudioBuffer<float>* {
		int targetBus = (busIndex >= 0 && busIndex < numOutputBuses && busBuffers[static_cast<size_t> (busIndex)].getNumChannels() > 0)
		                ? busIndex : 0;
		if (targetBus >= 0 && targetBus < static_cast<int> (busBuffers.size()) && busBuffers[static_cast<size_t> (targetBus)].getNumChannels() > 0)
			return &busBuffers[static_cast<size_t> (targetBus)];
		return nullptr;
	};

	if (sequencerBpmParam_ != nullptr)
	{
		const float bpm = sequencerBpmParam_->load (std::memory_order_relaxed);
		if (std::abs (bpm - lastSequencerBpm_) > 0.01f)
		{
			lastSequencerBpm_ = bpm;
			stepSequencer_.setBpm (bpm);
		}
	}

	if (sequencerSyncParam_ != nullptr)
	{
		const bool sync = sequencerSyncParam_->load (std::memory_order_relaxed) > 0.5f;
		if (sync != lastSequencerSync_)
		{
			lastSequencerSync_ = sync;
			stepSequencer_.setSyncToHost (sync);
		}
	}

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
			voicePool_.render (getBufferForBus, currentSample, messagePosition - currentSample);
			currentSample = messagePosition;
		}

		if (message.isNoteOn (false))
		{
			const int rawNote = message.getNoteNumber();
			const float velocity = message.getFloatVelocity();
			const int velInt = message.getVelocity();

			// Map standard GM drum notes to available pads / instruments
			int targetNote = rawNote;
			int uiTriggerNote = rawNote;
			float pitchOffsetSemitones = 0.0f;

			switch (rawNote)
			{
				case 35: targetNote = 36; uiTriggerNote = 36; break; // Acoustic Bass Drum -> Kick 1
				case 36: targetNote = 36; uiTriggerNote = 36; break; // Bass Drum 1 (Kick 1)
				case 37: targetNote = 37; uiTriggerNote = 37; break; // Side Stick
				case 38: targetNote = 38; uiTriggerNote = 38; break; // Acoustic Snare (Snare 1)
				case 39: targetNote = 39; uiTriggerNote = 39; break; // Hand Clap / Perc
				case 40: targetNote = 40; uiTriggerNote = 40; break; // Electric Snare (Snare 2)
				case 41: targetNote = 41; uiTriggerNote = 41; break; // Low Floor Tom
				case 42: targetNote = 42; uiTriggerNote = 42; break; // Closed Hi-Hat
				case 43: targetNote = 43; uiTriggerNote = 43; break; // High Floor Tom
				case 44: targetNote = 44; uiTriggerNote = 44; break; // Pedal Hi-Hat
				case 45: targetNote = 45; uiTriggerNote = 45; break; // Low Tom
				case 46: targetNote = 46; uiTriggerNote = 46; break; // Open Hi-Hat
				case 47: targetNote = 47; uiTriggerNote = 47; break; // Low-Mid Tom
				case 48: targetNote = 48; uiTriggerNote = 48; break; // Hi-Mid Tom
				case 49: targetNote = 49; uiTriggerNote = 49; break; // Crash Cymbal 1
				case 50: targetNote = 50; uiTriggerNote = 50; break; // High Tom
				case 51: targetNote = 51; uiTriggerNote = 51; break; // Ride Cymbal 1

				// GM Extended Cymbals & Percussion (Notes 52 - 59)
				case 52: targetNote = 52; uiTriggerNote = 49; break; // 52 (E2) Chinese Cymbal -> Crash 1 pad
				case 53: targetNote = 53; uiTriggerNote = 51; break; // 53 (F2) Ride Bell -> Ride pad
				case 54: targetNote = 54; uiTriggerNote = 39; break; // 54 (F#2) Tambourine -> Clap/Perc pad
				case 55: targetNote = 55; uiTriggerNote = 49; break; // 55 (G2) Splash Cymbal -> Crash 1 pad
				case 56: targetNote = 56; uiTriggerNote = 39; break; // 56 (G#2) Cowbell -> Clap/Perc pad
				case 57: targetNote = 57; uiTriggerNote = 49; break; // 57 (A2) Crash Cymbal 2 -> Crash 1 pad
				case 58: targetNote = 58; uiTriggerNote = 39; break; // 58 (A#2) Vibraslap -> Clap/Perc pad
				case 59: targetNote = 59; uiTriggerNote = 51; break; // 59 (B2) Ride Cymbal 2 -> Ride pad

				// GM Latin & World Percussion (Notes 60 - 81)
				// Bongos
				case 60: targetNote = 50; uiTriggerNote = 50; pitchOffsetSemitones = 5.0f; break; // High Bongo -> High Tom +5st
				case 61: targetNote = 48; uiTriggerNote = 48; pitchOffsetSemitones = 4.0f; break; // Low Bongo -> Hi-Mid Tom +4st
				// Congas
				case 62: targetNote = 37; uiTriggerNote = 37; pitchOffsetSemitones = 4.0f; break; // Mute Hi Conga -> Side Stick +4st
				case 63: targetNote = 47; uiTriggerNote = 47; pitchOffsetSemitones = 3.0f; break; // Open Hi Conga -> Mid Tom +3st
				case 64: targetNote = 45; uiTriggerNote = 45; pitchOffsetSemitones = 2.0f; break; // Low Conga -> Low Tom +2st
				// Timbales
				case 65: targetNote = 50; uiTriggerNote = 50; pitchOffsetSemitones = 2.0f; break; // High Timbale -> High Tom +2st
				case 66: targetNote = 47; uiTriggerNote = 47; pitchOffsetSemitones = 1.0f; break; // Low Timbale -> Mid Tom +1st
				// Agogos
				case 67: targetNote = 56; uiTriggerNote = 39; pitchOffsetSemitones = 5.0f; break; // High Agogo -> Cowbell +5st
				case 68: targetNote = 56; uiTriggerNote = 39; pitchOffsetSemitones = 0.0f; break; // Low Agogo -> Cowbell
				// Cabasa & Maracas (Shakers)
				case 69: targetNote = 42; uiTriggerNote = 42; pitchOffsetSemitones = 4.0f; break; // Cabasa -> Closed HH +4st
				case 70: targetNote = 42; uiTriggerNote = 42; pitchOffsetSemitones = 2.0f; break; // Maracas -> Closed HH +2st
				// Whistle
				case 71: targetNote = 54; uiTriggerNote = 39; pitchOffsetSemitones = 7.0f; break; // Short Whistle -> Bellchime +7st
				case 72: targetNote = 54; uiTriggerNote = 39; pitchOffsetSemitones = 4.0f; break; // Long Whistle -> Bellchime +4st
				// Guiro
				case 73: targetNote = 37; uiTriggerNote = 37; pitchOffsetSemitones = 6.0f; break; // Short Guiro -> Side Stick +6st
				case 74: targetNote = 37; uiTriggerNote = 37; pitchOffsetSemitones = 2.0f; break; // Long Guiro -> Side Stick +2st
				// Claves & Wood Blocks
				case 75: targetNote = 37; uiTriggerNote = 37; pitchOffsetSemitones = 7.0f; break; // Claves -> Side Stick +7st
				case 76: targetNote = 37; uiTriggerNote = 37; pitchOffsetSemitones = 5.0f; break; // Hi Wood Block -> Side Stick +5st
				case 77: targetNote = 37; uiTriggerNote = 37; pitchOffsetSemitones = 1.0f; break; // Low Wood Block -> Side Stick +1st
				// Cuica
				case 78: targetNote = 45; uiTriggerNote = 45; pitchOffsetSemitones = 7.0f; break; // Mute Cuica -> Low Tom +7st
				case 79: targetNote = 45; uiTriggerNote = 45; pitchOffsetSemitones = 4.0f; break; // Open Cuica -> Low Tom +4st
				// Triangle
				case 80: targetNote = 54; uiTriggerNote = 39; pitchOffsetSemitones = 3.0f; break; // Mute Triangle -> Bellchime +3st
				case 81: targetNote = 54; uiTriggerNote = 39; pitchOffsetSemitones = 0.0f; break; // Open Triangle -> Bellchime
				default: break;
			}

			if (uiTriggerNote >= 0 && uiTriggerNote < 128)
				padTriggers_[static_cast<size_t> (uiTriggerNote)].store (velocity, std::memory_order_relaxed);

			if (auto* pad = sampleBank_.getPad (targetNote))
			{
				if (const auto* sample = pad->findSampleForVelocity (velInt))
				{
					float effectivePitch = pad->pitchRatio;
					if (std::abs (pitchOffsetSemitones) > 1e-4f)
						effectivePitch *= std::pow (2.0f, pitchOffsetSemitones / 12.0f);

					voicePool_.triggerSample (sample, velocity, currentSampleRate_, pad->chokeGroup, effectivePitch, pad->outputBusIndex);
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
		voicePool_.render (getBufferForBus, currentSample, numSamples - currentSample);
	}

	// 1. Master Stereo Delay on Master Bus 0
	if (numOutputBuses > 0 && busBuffers[0].getNumChannels() > 0)
	{
		auto& masterBuf = busBuffers[0];
		const int masterChannels = masterBuf.getNumChannels();

		const float delayTimeMs = delayTimeParam_->load (std::memory_order_relaxed);
		const float feedback = std::clamp (delayFeedbackParam_->load (std::memory_order_relaxed), 0.0f, 0.95f);
		const float delayWet = delayWetParam_->load (std::memory_order_relaxed);
		const float delayInSamples = (delayTimeMs * 0.001f) * static_cast<float> (currentSampleRate_);

		auto* left = masterBuf.getWritePointer (0);
		auto* right = masterChannels > 1 ? masterBuf.getWritePointer (1) : nullptr;

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

		// 2. Master Studio Reverb on Master Bus 0
		juce::dsp::Reverb::Parameters params;
		params.roomSize = reverbRoomSizeParam_->load (std::memory_order_relaxed);
		params.damping = reverbDampingParam_->load (std::memory_order_relaxed);
		params.width = reverbWidthParam_->load (std::memory_order_relaxed);
		params.wetLevel = reverbWetParam_->load (std::memory_order_relaxed);
		params.dryLevel = 1.0f;
		reverb_.setParameters (params);

		juce::dsp::AudioBlock<float> block (masterBuf);
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

		for (int i = 0; i < kNumPads; ++i)
			updatePadRoutingInEngine (i, getPadOutputBus (i));
	}
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
	return new PluginProcessor();
}
