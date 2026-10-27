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
	if (event.mods.isPopupMenu())
	{
		showNoteContextMenu();
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

void StepButtonComponent::showNoteContextMenu()
{
	struct NoteItem { int note; const char* name; };
	static constexpr NoteItem kDrums[16] = {
		{ 36, "36: KICK 1" },
		{ 37, "37: SIDE STICK" },
		{ 38, "38: SNARE 1" },
		{ 39, "39: CLAP / PERC" },
		{ 40, "40: SNARE 2" },
		{ 41, "41: LOW FL TOM" },
		{ 42, "42: CLOSED HH" },
		{ 43, "43: HI FL TOM" },
		{ 44, "44: PEDAL HH" },
		{ 45, "45: LOW TOM" },
		{ 46, "46: OPEN HH" },
		{ 47, "47: MID TOM" },
		{ 48, "48: HI-MID TOM" },
		{ 49, "49: CRASH 1" },
		{ 50, "50: HIGH TOM" },
		{ 51, "51: RIDE" }
	};

	juce::PopupMenu menu;
	menu.addSectionHeader ("Assign Drum to Step " + juce::String (stepIndex_ + 1));

	for (size_t i = 0; i < 16; ++i)
	{
		menu.addItem (static_cast<int> (i + 1),
		              kDrums[i].name,
		              true,
		              midiNote_ == kDrums[i].note);
	}

	menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
		[this] (int result) {
			if (result > 0 && result <= 16)
			{
				midiNote_ = kDrums[static_cast<size_t> (result - 1)].note;
				active_ = true;
				if (onPreviewTrigger != nullptr)
					onPreviewTrigger (midiNote_, velocity_);
				if (onStepChanged != nullptr)
					onStepChanged (stepIndex_, active_, midiNote_, velocity_);
				repaint();
			}
		});
}

void StepButtonComponent::paint (juce::Graphics& g)
{
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
		g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
		const auto dynName = getDynamicLayerName();
		g.drawText (dynName,
		            bounds.reduced (2.0f, 4.0f),
		            juce::Justification::centred,
		            false);
	}

	// 3. Step Number Label at Top
	g.setColour (active_ ? juce::Colours::white.withAlpha (0.9f) :
	             (isBeatStart ? juce::Colour (0xff9aa0a6) : juce::Colour (0xff555d68)));
	g.setFont (juce::FontOptions (9.5f, isBeatStart ? juce::Font::bold : juce::Font::plain));
	g.drawText (juce::String (stepIndex_ + 1),
	            bounds.removeFromTop (13.0f),
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
	playButton.setButtonText ("PLAY ▶");
	playButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff212429));
	playButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff00d4ff));
	playButton.onClick = [this]() {
		if (sequencer_ != nullptr)
		{
			const bool playing = ! sequencer_->isPlaying();
			sequencer_->setPlaying (playing);
			playButton.setButtonText (playing ? "STOP ⏹" : "PLAY ▶");
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
		if (sequencer_ != nullptr)
			sequencer_->setBpm (static_cast<float> (bpmSlider.getValue()));
	};
	addAndMakeVisible (bpmSlider);

	// Track Info Label
	trackInfoLabel.setText ("TRACK: SNARE 1  •  38 / D1", juce::dontSendNotification);
	trackInfoLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));
	trackInfoLabel.setColour (juce::Label::textColourId, juce::Colour (0xff00d4ff));
	trackInfoLabel.setJustificationType (juce::Justification::centredLeft);
	addAndMakeVisible (trackInfoLabel);

	// Clear Track Button
	clearTrackButton.setButtonText ("CLEAR TRACK");
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

	// Setup 16 Step Buttons
	for (size_t s = 0; s < 16; ++s)
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
		bpmSlider.setValue (sequencer_->getBpm(), juce::dontSendNotification);
		playButton.setButtonText (sequencer_->isPlaying() ? "STOP ⏹" : "PLAY ▶");
		refreshSteps();
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

void StepSequencerComponent::refreshSteps()
{
	if (sequencer_ == nullptr)
		return;

	for (size_t s = 0; s < 16; ++s)
	{
		const auto& step = sequencer_->getStep (selectedTrackIndex_, static_cast<int> (s));
		stepButtons[s].setStepData (
			step.active.load (std::memory_order_relaxed),
			step.midiNote.load (std::memory_order_relaxed),
			step.velocity.load (std::memory_order_relaxed));
	}
}

void StepSequencerComponent::updatePlayhead (int currentStep)
{
	for (size_t s = 0; s < 16; ++s)
	{
		stepButtons[s].setPlayhead (static_cast<int> (s) == currentStep);
	}

	if (sequencer_ != nullptr)
	{
		const bool playing = sequencer_->isPlaying();
		const juce::String targetText = playing ? "STOP ⏹" : "PLAY ▶";
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
	auto area = getLocalBounds().reduced (8, 6);

	// 1. Top Transport Bar
	auto topBar = area.removeFromTop (28);

	playButton.setBounds (topBar.removeFromLeft (90));
	topBar.removeFromLeft (12);

	bpmLabel.setBounds (topBar.removeFromLeft (32));
	topBar.removeFromLeft (4);
	bpmSlider.setBounds (topBar.removeFromLeft (86));
	topBar.removeFromLeft (16);

	clearAllButton.setBounds (topBar.removeFromRight (80));
	topBar.removeFromRight (6);
	clearTrackButton.setBounds (topBar.removeFromRight (95));
	topBar.removeFromRight (12);

	trackInfoLabel.setBounds (topBar);

	area.removeFromTop (6); // Space between transport and step buttons

	// 2. 16 Step Buttons divided into 4 groups of 4
	const int numGroups = 4;
	const int stepsPerGroup = 4;
	const int groupGap = 8;
	const int stepGap = 3;

	const int totalGroupGaps = groupGap * (numGroups - 1);
	const int totalStepGaps = stepGap * (16 - numGroups);
	const int totalAvailableWidth = area.getWidth() - totalGroupGaps - totalStepGaps;
	const int stepWidth = std::max (1, totalAvailableWidth / 16);
	const int stepHeight = area.getHeight();

	int currentX = area.getX();
	const int y = area.getY();

	for (int gIdx = 0; gIdx < numGroups; ++gIdx)
	{
		for (int sIdx = 0; sIdx < stepsPerGroup; ++sIdx)
		{
			const size_t overallStep = static_cast<size_t> (gIdx * stepsPerGroup + sIdx);
			stepButtons[overallStep].setBounds (currentX, y, stepWidth, stepHeight);
			currentX += stepWidth + stepGap;
		}
		currentX += (groupGap - stepGap);
	}
}
