#include "StepSequencerComponent.hpp"

// ==============================================================================
// StepButtonComponent Implementation
// ==============================================================================

StepButtonComponent::StepButtonComponent()
{
	setOpaque (false);
}

void StepButtonComponent::setStepIndex (int index)
{
	stepIndex_ = index;
	repaint();
}

void StepButtonComponent::setStepData (bool active, int midiNote, float velocity)
{
	active_ = active;
	midiNote_ = midiNote;
	velocity_ = std::clamp (velocity, 0.05f, 1.0f);
	repaint();
}

void StepButtonComponent::setPlayhead (bool isPlayhead)
{
	if (isPlayhead_ != isPlayhead)
	{
		isPlayhead_ = isPlayhead;
		repaint();
	}
}

void StepButtonComponent::setInLoopRange (bool inRange)
{
	if (isInLoopRange_ != inRange)
	{
		isInLoopRange_ = inRange;
		repaint();
	}
}

juce::String StepButtonComponent::getDynamicLayerName() const
{
	if (velocity_ < 0.28f)
		return "GH";
	if (velocity_ < 0.48f)
		return "P";
	if (velocity_ < 0.68f)
		return "MP";
	if (velocity_ < 0.86f)
		return "F";
	return "FF";
}

void StepButtonComponent::mouseDown (const juce::MouseEvent& event)
{
	if (event.mods.isRightButtonDown())
	{
		active_ = false;
		velocity_ = 0.85f;
		if (onStepChanged != nullptr)
			onStepChanged (stepIndex_, active_, midiNote_, velocity_);
		repaint();
		return;
	}

	active_ = ! active_;
	if (active_)
	{
		if (getActivePadNote != nullptr)
			midiNote_ = getActivePadNote();

		if (velocity_ < 0.1f)
			velocity_ = 0.8f;

		if (onPreviewTrigger != nullptr)
			onPreviewTrigger (midiNote_, velocity_);
	}

	dragStartVelocity_ = velocity_;

	if (onStepChanged != nullptr)
		onStepChanged (stepIndex_, active_, midiNote_, velocity_);

	repaint();
}

void StepButtonComponent::mouseDrag (const juce::MouseEvent& event)
{
	if (! active_)
		return;

	const float deltaY = static_cast<float> (event.getDistanceFromDragStartY());
	velocity_ = std::clamp (dragStartVelocity_ - (deltaY * 0.005f), 0.05f, 1.0f);

	if (onStepChanged != nullptr)
		onStepChanged (stepIndex_, active_, midiNote_, velocity_);

	repaint();
}

void StepButtonComponent::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
	if (! active_)
		return;

	velocity_ = std::clamp (velocity_ + (wheel.deltaY * 0.05f), 0.05f, 1.0f);

	if (onStepChanged != nullptr)
		onStepChanged (stepIndex_, active_, midiNote_, velocity_);

	repaint();
}

void StepButtonComponent::paint (juce::Graphics& g)
{
	juce::Graphics::ScopedSaveState state (g);

	// Dim steps outside active loop range
	if (! isInLoopRange_)
		g.setOpacity (0.30f);

	auto bounds = getLocalBounds().toFloat().reduced (1.5f);
	const float cornerSize = 4.0f;
	const bool isBeatStart = (stepIndex_ % 4 == 0);

	// 1. Base step container
	const auto baseBg = isBeatStart ? juce::Colour (0xff252930) : juce::Colour (0xff1b1e23);
	g.setColour (baseBg);
	g.fillRoundedRectangle (bounds, cornerSize);

	// 2. Velocity Bar (drawn from bottom up if active)
	if (active_)
	{
		const float barMargin = 2.0f;
		const float availableHeight = bounds.getHeight() - (barMargin * 2.0f);
		const float barHeight = availableHeight * velocity_;
		const auto barRect = juce::Rectangle<float> (
			bounds.getX() + barMargin,
			bounds.getBottom() - barMargin - barHeight,
			bounds.getWidth() - (barMargin * 2.0f),
			barHeight);

		// Dynamic color tint based on velocity layer
		juce::Colour topColor;
		juce::Colour botColor;

		if (velocity_ < 0.30f)
		{
			// Ghost: subtle purple/indigo
			topColor = juce::Colour (0xff7986cb);
			botColor = juce::Colour (0xff3949ab);
		}
		else if (velocity_ > 0.85f)
		{
			// Fortissimo / Accent: vibrant orange/gold
			topColor = juce::Colour (0xffff7043);
			botColor = juce::Colour (0xfff4511e);
		}
		else
		{
			// Normal: Electric cyan
			topColor = juce::Colour (0xff00e5ff);
			botColor = juce::Colour (0xff0097a7);
		}

		juce::ColourGradient grad (topColor, barRect.getCentreX(), barRect.getY(),
		                          botColor, barRect.getCentreX(), barRect.getBottom(), false);
		g.setGradientFill (grad);
		g.fillRoundedRectangle (barRect, 3.0f);

		// Dynamic layer label inside bar (e.g. GH, P, MP, F, FF)
		g.setColour (juce::Colours::white);
		g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
		const auto dynName = getDynamicLayerName();
		g.drawText (dynName,
		            bounds.reduced (2.0f, 2.0f),
		            juce::Justification::centred,
		            false);
	}

	// 3. Step Number Label at Top
	g.setColour (active_ ? juce::Colours::white.withAlpha (0.9f) :
	             (isBeatStart ? juce::Colour (0xff9aa0a6) : juce::Colour (0xff555d68)));
	g.setFont (juce::FontOptions (9.0f, isBeatStart ? juce::Font::bold : juce::Font::plain));
	g.drawText (juce::String (stepIndex_ + 1),
	            bounds.removeFromTop (12.0f),
	            juce::Justification::centred,
	            false);

	// 4. Border outline
	const auto borderCol = active_ ? juce::Colour (0xff4fc3f7).withAlpha (0.6f) :
	                       (isBeatStart ? juce::Colour (0xff373f4d) : juce::Colour (0xff282e38));
	g.setColour (borderCol);
	g.drawRoundedRectangle (bounds, cornerSize, 1.0f);

	// 5. Playhead Highlight (Glowing cursor / light bar)
	if (isPlayhead_)
	{
		auto headBounds = getLocalBounds().toFloat().reduced (1.0f);
		g.setColour (juce::Colours::white);
		g.drawRoundedRectangle (headBounds, cornerSize, 2.0f);

		// Top cursor pill
		auto topPill = juce::Rectangle<float> (headBounds.getCentreX() - 8.0f, headBounds.getY() + 1.0f, 16.0f, 3.0f);
		g.setColour (juce::Colour (0xff00e5ff));
		g.fillRoundedRectangle (topPill, 1.5f);
	}
}

// ==============================================================================
// StepSequencerComponent Implementation
// ==============================================================================

StepSequencerComponent::StepSequencerComponent()
{
	setOpaque (false);

	// Play / Stop button
	playButton.setButtonText ("PLAY");
	playButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff212429));
	playButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff00d4ff));
	playButton.onClick = [this]() {
		if (sequencer_ != nullptr)
		{
			const bool playing = ! sequencer_->isPlaying();
			sequencer_->setPlaying (playing);
			playButton.setButtonText (playing ? "STOP" : "PLAY");
			playButton.setColour (juce::TextButton::textColourOffId,
			                      playing ? juce::Colour (0xffff7043) : juce::Colour (0xff00d4ff));
		}
	};
	addAndMakeVisible (playButton);

	// BPM Label & Slider
	bpmLabel.setText ("BPM", juce::dontSendNotification);
	bpmLabel.setFont (juce::FontOptions (10.0f, juce::Font::bold));
	bpmLabel.setColour (juce::Label::textColourId, juce::Colour (0xff9aa0a6));
	bpmLabel.setJustificationType (juce::Justification::centredRight);
	addAndMakeVisible (bpmLabel);

	bpmSlider.setSliderStyle (juce::Slider::LinearBar);
	bpmSlider.setRange (40.0, 240.0, 1.0);
	bpmSlider.setValue (120.0, juce::dontSendNotification);
	bpmSlider.setTextValueSuffix (" BPM");
	bpmSlider.setColour (juce::Slider::trackColourId, juce::Colour (0xff00d4ff));
	bpmSlider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
	bpmSlider.onValueChange = [this]() {
		const float newBpm = static_cast<float> (bpmSlider.getValue());
		if (sequencer_ != nullptr)
			sequencer_->setBpm (newBpm);
		if (onBpmChanged != nullptr)
			onBpmChanged (newBpm);
	};
	addAndMakeVisible (bpmSlider);

	// Tempo Sync / Free mode button
	tempoSyncBtn.setButtonText ("SYNC");
	tempoSyncBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1b2631));
	tempoSyncBtn.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff00d4ff));
	tempoSyncBtn.onClick = [this]() {
		if (sequencer_ != nullptr)
		{
			const bool newSync = ! sequencer_->isSyncToHost();
			sequencer_->setSyncToHost (newSync);
			lastObservedSync_ = newSync;
			updateTempoSyncUI();
			if (onSyncModeChanged != nullptr)
				onSyncModeChanged (newSync);
		}
	};
	addAndMakeVisible (tempoSyncBtn);

	// Loop Start UI
	loopStartLabel.setText ("START", juce::dontSendNotification);
	loopStartLabel.setFont (juce::FontOptions (9.5f, juce::Font::bold));
	loopStartLabel.setColour (juce::Label::textColourId, juce::Colour (0xff81c784)); // Greenish
	loopStartLabel.setJustificationType (juce::Justification::centredRight);
	addAndMakeVisible (loopStartLabel);

	loopStartSlider.setSliderStyle (juce::Slider::LinearBar);
	loopStartSlider.setRange (1.0, 32.0, 1.0);
	loopStartSlider.setValue (1.0, juce::dontSendNotification);
	loopStartSlider.setColour (juce::Slider::trackColourId, juce::Colour (0xff81c784));
	loopStartSlider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
	loopStartSlider.onValueChange = [this]() {
		if (sequencer_ != nullptr)
		{
			const int start = static_cast<int> (loopStartSlider.getValue());
			const int end = static_cast<int> (loopEndSlider.getValue());
			sequencer_->setLoopRange (start - 1, end - 1);
			updateLoopVisuals();
		}
	};
	addAndMakeVisible (loopStartSlider);

	// Loop End UI
	loopEndLabel.setText ("END", juce::dontSendNotification);
	loopEndLabel.setFont (juce::FontOptions (9.5f, juce::Font::bold));
	loopEndLabel.setColour (juce::Label::textColourId, juce::Colour (0xffe57373)); // Reddish
	loopEndLabel.setJustificationType (juce::Justification::centredRight);
	addAndMakeVisible (loopEndLabel);

	loopEndSlider.setSliderStyle (juce::Slider::LinearBar);
	loopEndSlider.setRange (1.0, 32.0, 1.0);
	loopEndSlider.setValue (16.0, juce::dontSendNotification);
	loopEndSlider.setColour (juce::Slider::trackColourId, juce::Colour (0xffe57373));
	loopEndSlider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
	loopEndSlider.onValueChange = [this]() {
		if (sequencer_ != nullptr)
		{
			const int start = static_cast<int> (loopStartSlider.getValue());
			const int end = static_cast<int> (loopEndSlider.getValue());
			sequencer_->setLoopRange (start - 1, end - 1);
			updateLoopVisuals();
		}
	};
	addAndMakeVisible (loopEndSlider);

	// Quick Loop Preset Buttons
	loop1BarBtn.setButtonText ("1-16");
	loop1BarBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff252930));
	loop1BarBtn.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff9aa0a6));
	loop1BarBtn.onClick = [this]() {
		loopStartSlider.setValue (1.0, juce::dontSendNotification);
		loopEndSlider.setValue (16.0, juce::dontSendNotification);
		if (sequencer_ != nullptr)
			sequencer_->setLoopRange (0, 15);
		updateLoopVisuals();
	};
	addAndMakeVisible (loop1BarBtn);

	loop2BarsBtn.setButtonText ("1-32");
	loop2BarsBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff252930));
	loop2BarsBtn.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff9aa0a6));
	loop2BarsBtn.onClick = [this]() {
		loopStartSlider.setValue (1.0, juce::dontSendNotification);
		loopEndSlider.setValue (32.0, juce::dontSendNotification);
		if (sequencer_ != nullptr)
			sequencer_->setLoopRange (0, 31);
		updateLoopVisuals();
	};
	addAndMakeVisible (loop2BarsBtn);

	// Track Info Label
	trackInfoLabel.setText ("TRACK: SNARE 1  •  38 / D1", juce::dontSendNotification);
	trackInfoLabel.setFont (juce::FontOptions (11.5f, juce::Font::bold));
	trackInfoLabel.setColour (juce::Label::textColourId, juce::Colour (0xff00d4ff));
	trackInfoLabel.setJustificationType (juce::Justification::centredLeft);
	addAndMakeVisible (trackInfoLabel);

	// Clear Track Button
	clearTrackButton.setButtonText ("CLEAR TRK");
	clearTrackButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff212429));
	clearTrackButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff9aa0a6));
	clearTrackButton.onClick = [this]() {
		if (sequencer_ != nullptr)
		{
			sequencer_->clearTrack (selectedTrackIndex_);
			refreshSteps();
		}
	};
	addAndMakeVisible (clearTrackButton);

	// Clear All Button
	clearAllButton.setButtonText ("CLEAR ALL");
	clearAllButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff212429));
	clearAllButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff9aa0a6));
	clearAllButton.onClick = [this]() {
		if (sequencer_ != nullptr)
		{
			sequencer_->clearAll();
			refreshSteps();
		}
	};
	addAndMakeVisible (clearAllButton);

	// Setup 32 Step Buttons
	for (size_t s = 0; s < 32; ++s)
	{
		stepButtons[s].setStepIndex (static_cast<int> (s));
		stepButtons[s].getActivePadNote = [this]() { return selectedMidiNote_; };
		stepButtons[s].onPreviewTrigger = [this] (int note, float vel) {
			if (onPreviewTrigger != nullptr)
				onPreviewTrigger (note, vel);
		};
		stepButtons[s].onStepChanged = [this] (int stepIdx, bool active, int note, float vel) {
			if (sequencer_ != nullptr)
			{
				sequencer_->setStep (selectedTrackIndex_, stepIdx, active, note, vel);
			}
		};
		addAndMakeVisible (stepButtons[s]);
	}
}

void StepSequencerComponent::setSequencer (StepSequencer* sequencer)
{
	sequencer_ = sequencer;
	if (sequencer_ != nullptr)
	{
		loopStartSlider.setValue (sequencer_->getLoopStart() + 1, juce::dontSendNotification);
		loopEndSlider.setValue (sequencer_->getLoopEnd() + 1, juce::dontSendNotification);
		playButton.setButtonText (sequencer_->isPlaying() ? "STOP" : "PLAY");
		lastObservedSync_ = sequencer_->isSyncToHost();
		updateTempoSyncUI();
		refreshSteps();
	}
}

void StepSequencerComponent::updateTempoSyncUI()
{
	if (sequencer_ == nullptr)
		return;

	const bool sync = sequencer_->isSyncToHost();
	lastObservedSync_ = sync;
	if (sync)
	{
		tempoSyncBtn.setButtonText ("SYNC");
		tempoSyncBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1b2631));
		tempoSyncBtn.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff00d4ff));
		bpmSlider.setEnabled (false);
		bpmSlider.setTextValueSuffix (" (SYNC)");
		bpmSlider.setValue (sequencer_->getEffectiveBpm(), juce::dontSendNotification);
	}
	else
	{
		tempoSyncBtn.setButtonText ("FREE");
		tempoSyncBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2d241e));
		tempoSyncBtn.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffffb74d));
		bpmSlider.setEnabled (true);
		bpmSlider.setTextValueSuffix (" BPM");
		bpmSlider.setValue (sequencer_->getBpm(), juce::dontSendNotification);
	}
}

void StepSequencerComponent::setSelectedTrack (int trackIndex, const juce::String& drumName, int midiNote, const juce::String& noteLabel)
{
	selectedTrackIndex_ = trackIndex;
	selectedDrumName_ = drumName;
	selectedMidiNote_ = midiNote;
	selectedNoteLabel_ = noteLabel;

	trackInfoLabel.setText ("EDITING: " + drumName + "  •  " + noteLabel, juce::dontSendNotification);
	refreshSteps();
}

void StepSequencerComponent::updateLoopVisuals()
{
	if (sequencer_ == nullptr)
		return;

	const int start = sequencer_->getLoopStart();
	const int end = sequencer_->getLoopEnd();

	for (size_t s = 0; s < 32; ++s)
	{
		const int idx = static_cast<int> (s);
		stepButtons[s].setInLoopRange (idx >= start && idx <= end);
	}
}

void StepSequencerComponent::refreshSteps()
{
	if (sequencer_ == nullptr)
		return;

	for (size_t s = 0; s < 32; ++s)
	{
		const auto& step = sequencer_->getStep (selectedTrackIndex_, static_cast<int> (s));
		stepButtons[s].setStepData (
			step.active.load (std::memory_order_relaxed),
			step.midiNote.load (std::memory_order_relaxed),
			step.velocity.load (std::memory_order_relaxed));
	}

	updateLoopVisuals();
}

void StepSequencerComponent::updatePlayhead (int currentStep)
{
	for (size_t s = 0; s < 32; ++s)
	{
		stepButtons[s].setPlayhead (static_cast<int> (s) == currentStep);
	}

	if (sequencer_ != nullptr)
	{
		const bool currentSync = sequencer_->isSyncToHost();
		if (currentSync != lastObservedSync_)
		{
			lastObservedSync_ = currentSync;
			updateTempoSyncUI();
		}

		if (currentSync)
		{
			const double effBpm = static_cast<double> (sequencer_->getEffectiveBpm());
			if (std::abs (bpmSlider.getValue() - effBpm) > 0.05)
				bpmSlider.setValue (effBpm, juce::dontSendNotification);
		}

		const bool playing = sequencer_->isPlaying();
		const juce::String targetText = playing ? "STOP" : "PLAY";
		if (playButton.getButtonText() != targetText)
		{
			playButton.setButtonText (targetText);
			playButton.setColour (juce::TextButton::textColourOffId,
			                      playing ? juce::Colour (0xffff7043) : juce::Colour (0xff00d4ff));
		}
	}
}

void StepSequencerComponent::paint (juce::Graphics& g)
{
	auto bounds = getLocalBounds().toFloat();
	const float corner = 8.0f;

	// Background panel
	g.setColour (juce::Colour (0xff14171c));
	g.fillRoundedRectangle (bounds, corner);

	g.setColour (juce::Colour (0xff252a32));
	g.drawRoundedRectangle (bounds, corner, 1.0f);
}

void StepSequencerComponent::resized()
{
	auto area = getLocalBounds().reduced (8, 5);

	// 1. Top Transport Bar
	auto topBar = area.removeFromTop (26);

	playButton.setBounds (topBar.removeFromLeft (60));
	topBar.removeFromLeft (5);

	bpmLabel.setBounds (topBar.removeFromLeft (26));
	topBar.removeFromLeft (3);
	bpmSlider.setBounds (topBar.removeFromLeft (64));
	topBar.removeFromLeft (3);
	tempoSyncBtn.setBounds (topBar.removeFromLeft (56));
	topBar.removeFromLeft (6);

	loopStartLabel.setBounds (topBar.removeFromLeft (34));
	topBar.removeFromLeft (2);
	loopStartSlider.setBounds (topBar.removeFromLeft (38));
	topBar.removeFromLeft (4);

	loopEndLabel.setBounds (topBar.removeFromLeft (24));
	topBar.removeFromLeft (2);
	loopEndSlider.setBounds (topBar.removeFromLeft (38));
	topBar.removeFromLeft (5);

	loop1BarBtn.setBounds (topBar.removeFromLeft (32));
	topBar.removeFromLeft (2);
	loop2BarsBtn.setBounds (topBar.removeFromLeft (32));
	topBar.removeFromLeft (6);

	clearAllButton.setBounds (topBar.removeFromRight (62));
	topBar.removeFromRight (3);
	clearTrackButton.setBounds (topBar.removeFromRight (70));
	topBar.removeFromRight (6);

	trackInfoLabel.setBounds (topBar);

	area.removeFromTop (5); // Space between transport and step rows

	// 2. Two rows of 16 steps (Row 1: Bar 1 / 1-16, Row 2: Bar 2 / 17-32)
	const int rowGap = 3;
	const int availableHeight = area.getHeight() - rowGap;
	const int rowHeight = std::max (1, availableHeight / 2);

	auto row1Area = area.removeFromTop (rowHeight);
	area.removeFromTop (rowGap);
	auto row2Area = area;

	const int numGroups = 4;
	const int stepsPerGroup = 4;
	const int groupGap = 6;
	const int stepGap = 2;

	auto layoutRow = [&] (juce::Rectangle<int> rArea, size_t stepOffset) {
		const int totalGroupGaps = groupGap * (numGroups - 1);
		const int totalStepGaps = stepGap * (16 - numGroups);
		const int totalAvailableWidth = rArea.getWidth() - totalGroupGaps - totalStepGaps;
		const int stepWidth = std::max (1, totalAvailableWidth / 16);
		const int h = rArea.getHeight();

		int currentX = rArea.getX();
		const int y = rArea.getY();

		for (int gIdx = 0; gIdx < numGroups; ++gIdx)
		{
			for (int sIdx = 0; sIdx < stepsPerGroup; ++sIdx)
			{
				const size_t overallStep = stepOffset + static_cast<size_t> (gIdx * stepsPerGroup + sIdx);
				stepButtons[overallStep].setBounds (currentX, y, stepWidth, h);
				currentX += stepWidth + stepGap;
			}
			currentX += (groupGap - stepGap);
		}
	};

	layoutRow (row1Area, 0);  // Bar 1 (0..15)
	layoutRow (row2Area, 16); // Bar 2 (16..31)
}
