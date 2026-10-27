#include <iostream>
#include <cassert>
#include <array>
#include "engine/SampleBank.hpp"
#include "engine/DrumVoicePool.hpp"
#include "engine/StepSequencer.hpp"

int main()
{
	std::cout << "=== Running Dreamdrums Engine & 16-Pad Matrix Tests ===" << std::endl;

	SampleBank bank;
	auto assetDir = SampleBank::findAssetsDirectory();
	std::cout << "Assets directory: " << assetDir.getFullPathName().toStdString() << std::endl;
	assert(assetDir.isDirectory());

	// All 16 pads: Launchkey GM notes 36 to 51
	struct PadDefinition { int note; const char* prefix; const char* name; };
	static constexpr std::array<PadDefinition, 16> k16Pads = {{
		// Row 1 (Notes 36 - 43)
		{ 36, "kick_OH", "KICK 1" },
		{ 37, "snareStick_OH", "SIDE STICK" },
		{ 38, "snare2_OH", "SNARE 1" },
		{ 39, "cowbell", "CLAP / PERC" },
		{ 40, "snare_OH", "SNARE 2" },
		{ 41, "loTom_OH", "LOW FL TOM" },
		{ 42, "hihatClosed_OH", "CLOSED HH" },
		{ 43, "loTom_OH", "HI FL TOM" },

		// Row 2 (Notes 44 - 51)
		{ 44, "hihatFoot_OH", "PEDAL HH" },
		{ 45, "loTom_OH", "LOW TOM" },
		{ 46, "hihatOpen_OH", "OPEN HH" },
		{ 47, "hiTom_OH", "MID TOM" },
		{ 48, "hiTom_OH", "HI-MID TOM" },
		{ 49, "crash1_OH", "CRASH 1" },
		{ 50, "hiTom_OH", "HIGH TOM" },
		{ 51, "ride1_OH", "RIDE" }
	}};

	int totalVariationsLoaded = 0;
	for (const auto& def : k16Pads)
	{
		bool loaded = bank.loadInstrument(def.note, assetDir, def.prefix, def.name);
		assert(loaded);
		auto* pad = bank.getPad(def.note);
		assert(pad != nullptr);
		assert(!pad->layers.empty());

		int variationsForPad = 0;
		for (const auto& l : pad->layers)
			variationsForPad += static_cast<int>(l.variations.size());

		std::cout << "  Pad " << def.note << " (" << def.name << "): "
		          << pad->layers.size() << " layers, "
		          << variationsForPad << " variations." << std::endl;

		totalVariationsLoaded += variationsForPad;
	}

	std::cout << "Total variations loaded across 16 pads: " << totalVariationsLoaded << std::endl;
	assert(totalVariationsLoaded > 100);

	// Test Velocity Layers for Snare
	auto* snarePad = bank.getPad(38);
	const auto* sampleGhost = snarePad->findSampleForVelocity(15);
	assert(sampleGhost != nullptr && sampleGhost->name.contains("Ghost"));

	const auto* sampleMP = snarePad->findSampleForVelocity(50);
	assert(sampleMP != nullptr && sampleMP->name.contains("MP"));

	const auto* sampleF = snarePad->findSampleForVelocity(90);
	assert(sampleF != nullptr && sampleF->name.contains("F_"));

	const auto* sampleFF = snarePad->findSampleForVelocity(120);
	assert(sampleFF != nullptr && sampleFF->name.contains("FF_"));

	// Test Round-Robin variation switching
	juce::String prevName = sampleF->name;
	for (int i = 0; i < 5; ++i)
	{
		const auto* nextSample = snarePad->findSampleForVelocity(90);
		assert(nextSample != nullptr);
		assert(nextSample->name != prevName);
		prevName = nextSample->name;
	}

	// Test Polyphonic playback in DrumVoicePool
	DrumVoicePool voicePool;
	juce::AudioBuffer<float> outputBuffer(2, 512);
	outputBuffer.clear();

	// Trigger multiple pads simultaneously (e.g. Kick + Snare + Hi-Hat)
	voicePool.triggerSample(bank.getPad(36)->findSampleForVelocity(100), 0.9f, 48000.0);
	voicePool.triggerSample(bank.getPad(38)->findSampleForVelocity(110), 0.95f, 48000.0);
	voicePool.triggerSample(bank.getPad(42)->findSampleForVelocity(80), 0.7f, 48000.0);

	voicePool.render(outputBuffer, 0, 512);

	float rms = outputBuffer.getRMSLevel(0, 0, 512);
	std::cout << "Rendered 512 samples with 3 polyphonic voices. RMS level: " << rms << std::endl;
	assert(rms > 0.05f);

	// ==========================================================================
	// 16-Step Sequencer Tests
	// ==========================================================================
	std::cout << "=== Running 16-Step Sequencer Tests ===" << std::endl;

	// 1. Step struct test
	Step testStep;
	assert(! testStep.active.load());
	assert(testStep.midiNote.load() == 38);
	assert(std::abs(testStep.velocity.load() - 0.85f) < 0.001f);
	assert(testStep.getMidiVelocity() == 107); // 0.85 * 127 = 107.95 -> 107

	testStep.velocity.store(0.15f);
	assert(testStep.getMidiVelocity() == 19);

	testStep.velocity.store(1.0f);
	assert(testStep.getMidiVelocity() == 127);

	testStep.velocity.store(0.0f);
	assert(testStep.getMidiVelocity() == 1); // clamped to 1

	// 2. StepSequencer pattern storage & defaults
	StepSequencer sequencer;
	assert(sequencer.getTrackDefaultNote(0) == 36);  // Kick 1
	assert(sequencer.getTrackDefaultNote(2) == 38);  // Snare 1
	assert(sequencer.getTrackDefaultNote(6) == 42);  // Closed HH
	assert(sequencer.getTrackDefaultNote(15) == 51); // Ride

	for (int t = 0; t < StepSequencer::kNumTracks; ++t)
	{
		for (int s = 0; s < StepSequencer::kNumSteps; ++s)
		{
			const auto& st = sequencer.getStep(t, s);
			assert(! st.active.load());
			assert(st.midiNote.load() == sequencer.getTrackDefaultNote(t));
		}
	}

	// 3. User Scenario Test:
	// Set Snare 1 (track 2) steps 5 and 13 (indices 4 and 12):
	// Step 5: velocity ~0.2 (Ghost)
	// Step 13: velocity 1.0 (FF)
	sequencer.setStep(2, 4, true, 38, 0.20f);
	sequencer.setStep(2, 12, true, 38, 1.0f);

	assert(sequencer.getStep(2, 4).active.load());
	assert(sequencer.getStep(2, 4).velocity.load() == 0.20f);
	assert(sequencer.getStep(2, 4).getMidiVelocity() == 25);

	assert(sequencer.getStep(2, 12).active.load());
	assert(sequencer.getStep(2, 12).velocity.load() == 1.0f);
	assert(sequencer.getStep(2, 12).getMidiVelocity() == 127);

	// Verify sample resolution for Ghost vs FF
	auto* snare = bank.getPad(38);
	const auto* sampleGhostStep = snare->findSampleForVelocity(sequencer.getStep(2, 4).getMidiVelocity());
	assert(sampleGhostStep != nullptr);
	assert(sampleGhostStep->name.contains("Ghost"));
	std::cout << "  Step 5 (Ghost vel 0.20) resolved to sample: " << sampleGhostStep->name.toStdString() << std::endl;

	const auto* sampleFFStep = snare->findSampleForVelocity(sequencer.getStep(2, 12).getMidiVelocity());
	assert(sampleFFStep != nullptr);
	assert(sampleFFStep->name.contains("FF_"));
	std::cout << "  Step 13 (FF vel 1.00) resolved to sample: " << sampleFFStep->name.toStdString() << std::endl;

	// 4. Per-step Note Override test
	// Change step 8 on track 2 to Crash 1 (note 49)
	sequencer.setStep(2, 7, true, 49, 0.9f);
	assert(sequencer.getStep(2, 7).midiNote.load() == 49);
	auto* crashPad = bank.getPad(sequencer.getStep(2, 7).midiNote.load());
	assert(crashPad != nullptr);
	assert(crashPad->padName == "CRASH 1");
	std::cout << "  Per-step note override verified: Step 8 on Snare track plays " << crashPad->padName.toStdString() << std::endl;

	// 5. Playback & Step progression test in processBlock
	sequencer.setBpm(120.0f);
	sequencer.setPlaying(true);
	assert(sequencer.isPlaying());

	int triggeredCount = 0;
	int lastTriggeredNote = -1;
	float lastTriggeredVel = 0.0f;

	auto triggerCb = [&](int note, float vel) {
		triggeredCount++;
		lastTriggeredNote = note;
		lastTriggeredVel = vel;
	};

	// At 48000 Hz, 120 BPM:
	// 1 step (16th note) = 48000 * 60 / (120 * 4) = 6000 samples
	// We simulate blocks of 500 samples
	outputBuffer.clear();
	for (int b = 0; b < 240; ++b) // 240 blocks * 500 = 120000 samples = 20 steps (more than 1 full pattern loop)
	{
		sequencer.processBlock(bank, voicePool, 48000.0, 500, nullptr, triggerCb);
	}

	std::cout << "  Sequencer ran 120,000 samples. Triggered events count: " << triggeredCount << std::endl;
	assert(triggeredCount >= 3); // Steps 4, 7, 12 triggered at least once
	assert(sequencer.getCurrentStep() >= 0 && sequencer.getCurrentStep() < 16);

	// 6. ValueTree Serialization / Deserialization test
	auto savedTree = sequencer.serializeToValueTree();
	assert(savedTree.isValid());
	assert(savedTree.getNumChildren() == 16);

	StepSequencer restoredSequencer;
	restoredSequencer.deserializeFromValueTree(savedTree);
	assert(std::abs(restoredSequencer.getBpm() - 120.0f) < 0.01f);
	assert(restoredSequencer.getStep(2, 4).active.load());
	assert(std::abs(restoredSequencer.getStep(2, 4).velocity.load() - 0.20f) < 0.01f);
	assert(restoredSequencer.getStep(2, 12).active.load());
	assert(std::abs(restoredSequencer.getStep(2, 12).velocity.load() - 1.0f) < 0.01f);
	assert(restoredSequencer.getStep(2, 7).active.load());
	assert(restoredSequencer.getStep(2, 7).midiNote.load() == 49);
	std::cout << "  Pattern serialization and deserialization verified." << std::endl;

	std::cout << "=== ALL 16-PAD ENGINE & STEP SEQUENCER TESTS PASSED! ===" << std::endl;
	return 0;
}
