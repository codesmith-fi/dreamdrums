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
	assert(sequencer.getCurrentStep() >= 0 && sequencer.getCurrentStep() <= 15);

	// 5b. 32-step & Loop Range progression tests
	std::cout << "  Testing 32-step full range and loop bounds..." << std::endl;
	sequencer.setLoopRange(0, 31); // 2 full bars
	assert(sequencer.getLoopStart() == 0);
	assert(sequencer.getLoopEnd() == 31);

	// Program a snare fill at step 28 (Bar 2, 4th beat)
	sequencer.setStep(2, 28, true, 38, 0.95f);
	int step28Triggered = 0;
	auto triggerFillCb = [&](int note, float) {
		if (note == 38 && sequencer.getCurrentStep() == 28)
			step28Triggered++;
	};

	// Advance 2 full 32-step bars (64 steps * 6000 samples = 384,000 samples)
	for (int b = 0; b < 768; ++b)
	{
		sequencer.processBlock(bank, voicePool, 48000.0, 500, nullptr, triggerFillCb);
	}
	assert(step28Triggered >= 1);
	std::cout << "  32-step progression verified: Bar 2 step 28 triggered " << step28Triggered << " times." << std::endl;

	// Test custom loop range: Bar 2 only (16..31)
	sequencer.setLoopRange(16, 31);
	for (int b = 0; b < 100; ++b)
	{
		sequencer.processBlock(bank, voicePool, 48000.0, 500, nullptr, nullptr);
		assert(sequencer.getCurrentStep() >= 16 && sequencer.getCurrentStep() <= 31);
	}
	std::cout << "  Loop range Bar 2 (16..31) boundary verified." << std::endl;

	// Test polyrhythmic loop range: 12 steps (0..11)
	sequencer.setLoopRange(0, 11);
	for (int b = 0; b < 100; ++b)
	{
		sequencer.processBlock(bank, voicePool, 48000.0, 500, nullptr, nullptr);
		assert(sequencer.getCurrentStep() >= 0 && sequencer.getCurrentStep() <= 11);
	}
	std::cout << "  Polyrhythmic loop range (0..11) boundary verified." << std::endl;

	// 6. ValueTree Serialization / Deserialization test
	sequencer.setLoopRange(4, 27);
	auto savedTree = sequencer.serializeToValueTree();
	assert(savedTree.isValid());
	assert(savedTree.getNumChildren() == 16);

	StepSequencer restoredSequencer;
	restoredSequencer.deserializeFromValueTree(savedTree);
	assert(std::abs(restoredSequencer.getBpm() - 120.0f) < 0.01f);
	assert(restoredSequencer.getLoopStart() == 4);
	assert(restoredSequencer.getLoopEnd() == 27);
	assert(restoredSequencer.getStep(2, 4).active.load());
	assert(std::abs(restoredSequencer.getStep(2, 4).velocity.load() - 0.20f) < 0.01f);
	assert(restoredSequencer.getStep(2, 12).active.load());
	assert(std::abs(restoredSequencer.getStep(2, 12).velocity.load() - 1.0f) < 0.01f);
	assert(restoredSequencer.getStep(2, 7).active.load());
	assert(restoredSequencer.getStep(2, 7).midiNote.load() == 49);
	assert(restoredSequencer.getStep(2, 28).active.load());

	// Tempo Sync (SYNC vs FREE) test
	sequencer.setSyncToHost(false);
	assert(!sequencer.isSyncToHost());
	auto freeSyncTree = sequencer.serializeToValueTree();
	StepSequencer freeRestoredSeq;
	freeRestoredSeq.deserializeFromValueTree(freeSyncTree);
	assert(!freeRestoredSeq.isSyncToHost());

	sequencer.setSyncToHost(true);
	assert(sequencer.isSyncToHost());
	auto syncTree = sequencer.serializeToValueTree();
	StepSequencer syncRestoredSeq;
	syncRestoredSeq.deserializeFromValueTree(syncTree);
	assert(syncRestoredSeq.isSyncToHost());

	std::cout << "  Pattern, loop range and tempo sync (SYNC/FREE) serialization verified." << std::endl;
	// 7. Hi-Hat Choke Group Test
	std::cout << "=== Running Hi-Hat Choke Group Tests ===" << std::endl;
	bank.getPad(42)->chokeGroup = DrumPad::kHiHatChokeGroup; // Closed HH
	bank.getPad(44)->chokeGroup = DrumPad::kHiHatChokeGroup; // Pedal HH
	bank.getPad(46)->chokeGroup = DrumPad::kHiHatChokeGroup; // Open HH
	assert(bank.getPad(38)->chokeGroup == 0); // Snare has no choke group

	DrumVoicePool chokeVoicePool;
	chokeVoicePool.reset();

	// Step 1: Trigger Open HH (Note 46)
	const auto* openHHSample = bank.getPad(46)->findSampleForVelocity(100);
	assert(openHHSample != nullptr);
	chokeVoicePool.triggerSample(openHHSample, 0.9f, 48000.0, bank.getPad(46)->chokeGroup);

	juce::AudioBuffer<float> testChokeBuffer(2, 512);
	testChokeBuffer.clear();
	chokeVoicePool.render(testChokeBuffer, 0, 100);
	float openHHLevel = testChokeBuffer.getRMSLevel(0, 0, 100);
	assert(openHHLevel > 0.01f);

	const auto& voices = chokeVoicePool.getVoices();
	assert(voices[0].isActive());
	assert(! voices[0].isFadingOutVoice());
	assert(voices[0].getChokeGroup() == DrumPad::kHiHatChokeGroup);
	std::cout << "  Open HH active and producing sound (RMS: " << openHHLevel << ")" << std::endl;

	// Step 2: Trigger Closed HH (Note 42) -> Must initiate quick fade-out of Open HH
	const auto* closedHHSample = bank.getPad(42)->findSampleForVelocity(100);
	assert(closedHHSample != nullptr);
	chokeVoicePool.triggerSample(closedHHSample, 0.95f, 48000.0, bank.getPad(42)->chokeGroup);

	// Open HH voice must immediately enter fade-out state
	assert(voices[0].isFadingOutVoice());
	// Closed HH voice must be active in another slot and not fading out
	assert(voices[1].isActive());
	assert(! voices[1].isFadingOutVoice());
	assert(voices[1].getChokeGroup() == DrumPad::kHiHatChokeGroup);
	std::cout << "  Closed HH triggered: Open HH entered quick fade-out state." << std::endl;

	// Step 3: Render 5ms (240 samples at 48kHz) to complete fade-out
	testChokeBuffer.clear();
	chokeVoicePool.render(testChokeBuffer, 0, 240);

	// Open HH voice must now be completely deactivated and finished
	assert(! voices[0].isActive());
	// Closed HH is still happily playing
	assert(voices[1].isActive());
	std::cout << "  5ms fade-out elapsed: Open HH voice deactivated cleanly, Closed HH continues playing." << std::endl;

	// Step 4: Retest with Pedal HH (Note 44) choking Open HH (Note 46)
	chokeVoicePool.reset();
	chokeVoicePool.triggerSample(openHHSample, 0.85f, 48000.0, bank.getPad(46)->chokeGroup);
	assert(voices[0].isActive() && ! voices[0].isFadingOutVoice());

	const auto* pedalHHSample = bank.getPad(44)->findSampleForVelocity(90);
	assert(pedalHHSample != nullptr);
	chokeVoicePool.triggerSample(pedalHHSample, 0.9f, 48000.0, bank.getPad(44)->chokeGroup);
	assert(voices[0].isFadingOutVoice());

	testChokeBuffer.clear();
	chokeVoicePool.render(testChokeBuffer, 0, 240);
	assert(! voices[0].isActive());
	assert(voices[1].isActive());
	std::cout << "  Pedal HH choking Open HH verified successfully!" << std::endl;

	std::cout << "=== ALL 16-PAD ENGINE, SEQUENCER & CHOKE TESTS PASSED! ===" << std::endl;
	return 0;
}
