#include "PluginEditor.h"

struct PadConfig
{
	int note;
	const char* name;
	const char* noteLabel;
};

static constexpr PadConfig kConfigs[16] = {
	// Launchkey Row 1 (Notes 36 - 43)
	{ 36, "KICK 1", "36 / C1" },
	{ 37, "SIDE STICK", "37 / C#1" },
	{ 38, "SNARE 1", "38 / D1" },
	{ 39, "CLAP / PERC", "39 / D#1" },
	{ 40, "SNARE 2", "40 / E1" },
	{ 41, "LOW FL TOM", "41 / F1" },
	{ 42, "CLOSED HH", "42 / F#1" },
	{ 43, "HI FL TOM", "43 / G1" },

	// Launchkey Row 2 (Notes 44 - 51)
	{ 44, "PEDAL HH", "44 / G#1" },
	{ 45, "LOW TOM", "45 / A1" },
	{ 46, "OPEN HH", "46 / A#1" },
	{ 47, "MID TOM", "47 / B1" },
	{ 48, "HI-MID TOM", "48 / C2" },
	{ 49, "CRASH 1", "49 / C#2" },
	{ 50, "HIGH TOM", "50 / D2" },
	{ 51, "RIDE", "51 / D#2" }
};

PluginEditor::PluginEditor (PluginProcessor& p)
	: AudioProcessorEditor (&p),
	  processor (p)
{
	setOpaque (true);

#if JUCE_WINDOWS && ENABLE_WINE_BUILD
	if (auto* peer = getPeer())
	{
		peer->setCurrentRenderingEngine (0);
		repaint();
	}
#endif

	for (size_t i = 0; i < 16; ++i)
	{
		drumPads[i].setPadInfo (kConfigs[i].note, kConfigs[i].name, kConfigs[i].noteLabel);
		drumPads[i].onTrigger = [this] (int note, float vel) {
			processor.triggerPad (note, vel);
		};
		drumPads[i].onSelect = [this, i] (int /*note*/) {
			selectPad (static_cast<int> (i));
		};
		addAndMakeVisible (drumPads[i]);
	}

	sequencerComponent.setSequencer (&processor.getStepSequencer());
	sequencerComponent.onPreviewTrigger = [this] (int note, float vel) {
		processor.triggerPad (note, vel);
	};
	sequencerComponent.onSyncModeChanged = [this] (bool sync) {
		processor.setSequencerSync (sync);
	};
	sequencerComponent.onBpmChanged = [this] (float bpm) {
		processor.setSequencerBpm (bpm);
	};
	addAndMakeVisible (sequencerComponent);

	// Master FX Knobs
	setupKnob (reverbRoomKnob, "reverb_room_size", "ROOM");
	setupKnob (reverbDampKnob, "reverb_damping", "DAMP");
	setupKnob (reverbWidthKnob, "reverb_width", "WIDTH");
	setupKnob (reverbMixKnob, "reverb_wet", "MIX");

	setupKnob (delayTimeKnob, "delay_time", "TIME");
	setupKnob (delayFeedbackKnob, "delay_feedback", "F.BACK");
	setupKnob (delayMixKnob, "delay_wet", "MIX");

	layoutButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff212429));
	layoutButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff323842));
	layoutButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff00d4ff));
	layoutButton.onClick = [this]() {
		if (layoutMode_ == LayoutMode::Launchkey_2x8)
			setLayoutMode (LayoutMode::Grid_4x4);
		else
			setLayoutMode (LayoutMode::Launchkey_2x8);
	};
	addAndMakeVisible (layoutButton);

	selectPad (2); // Default to Snare 1
	startTimerHz (60);
	setLayoutMode (LayoutMode::Launchkey_2x8);
}

void PluginEditor::setupKnob (RotaryKnob& knob, const juce::String& paramId, const juce::String& labelText)
{
	knob.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
	knob.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 16);
	knob.slider.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xff00d4ff));
	knob.slider.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff212429));
	knob.slider.setColour (juce::Slider::thumbColourId, juce::Colour (0xff00d4ff));
	knob.slider.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffeeeff2));
	knob.slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
	knob.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);

	knob.label.setText (labelText, juce::dontSendNotification);
	knob.label.setJustificationType (juce::Justification::centred);
	knob.label.setFont (juce::FontOptions (10.5f, juce::Font::bold));
	knob.label.setColour (juce::Label::textColourId, juce::Colour (0xff9aa0a6));

	addAndMakeVisible (knob.slider);
	addAndMakeVisible (knob.label);

	knob.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
		processor.apvts, paramId, knob.slider);
}

PluginEditor::~PluginEditor()
{
	stopTimer();
}

void PluginEditor::timerCallback()
{
	for (auto& pad : drumPads)
	{
		const float triggerVel = processor.popPadTrigger (pad.getMidiNote());
		if (triggerVel > 0.0f)
			pad.trigger (triggerVel);

		pad.updateAnimation (0.85f);
	}

	sequencerComponent.updatePlayhead (processor.getStepSequencer().getCurrentStep());
}

void PluginEditor::selectPad (int index)
{
	if (index < 0 || index >= 16)
		return;

	selectedPadIndex_ = index;
	for (size_t i = 0; i < 16; ++i)
		drumPads[i].setSelected (static_cast<int> (i) == selectedPadIndex_);

	sequencerComponent.setSelectedTrack (
		selectedPadIndex_,
		kConfigs[selectedPadIndex_].name,
		kConfigs[selectedPadIndex_].note,
		kConfigs[selectedPadIndex_].noteLabel);
}

void PluginEditor::setLayoutMode (LayoutMode mode)
{
	layoutMode_ = mode;
	if (layoutMode_ == LayoutMode::Launchkey_2x8)
	{
		layoutButton.setButtonText ("Layout: 2x8 (Launchkey)");
		setSize (980, 650);
	}
	else
	{
		layoutButton.setButtonText ("Layout: 4x4 (MPC)");
		setSize (680, 890);
	}
	resized();
	repaint();
}

void PluginEditor::paint (juce::Graphics& g)
{
	auto bounds = getLocalBounds();

	// 1. Sleek dark background
	g.setColour (juce::Colour (0xff14171b));
	g.fillRect (bounds);

	// 2. Header banner
	const int headerHeight = 60;
	auto headerBounds = bounds.removeFromTop (headerHeight);

	// Subtle header bottom divider
	g.setColour (juce::Colour (0xff252a32));
	g.drawHorizontalLine (headerHeight - 1, 0.0f, static_cast<float> (getWidth()));

	// Brand title "DREAMDRUMS"
	g.setColour (juce::Colours::white);
	g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
	g.drawText ("DREAMDRUMS",
	            headerBounds.removeFromLeft (200).translated (20, -4),
	            juce::Justification::centredLeft,
	            false);

	// Subtitle / Kit Info
	headerBounds.removeFromRight (190);
	g.setColour (juce::Colour (0xff00d4ff)); // Cyan accent
	g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
	g.drawText ("SALAMANDER DRUMKIT  •  16 VELOCITY PADS",
	            headerBounds.translated (-10, -4),
	            juce::Justification::centredRight,
	            false);

	// 3. Master FX Panels (Boxes)
	const int fxTop = 66;
	const int fxHeight = 98;
	const int marginX = 16;
	const int gapX = 12;
	const int availableFxWidth = getWidth() - 2 * marginX - gapX;
	const int reverbBoxWidth = (availableFxWidth * 4) / 7;
	const int delayBoxWidth = availableFxWidth - reverbBoxWidth;

	auto reverbBox = juce::Rectangle<int> (marginX, fxTop, reverbBoxWidth, fxHeight).toFloat();
	auto delayBox = juce::Rectangle<int> (marginX + reverbBoxWidth + gapX, fxTop, delayBoxWidth, fxHeight).toFloat();

	// Draw Reverb Box
	g.setColour (juce::Colour (0xff181b21));
	g.fillRoundedRectangle (reverbBox, 6.0f);
	g.setColour (juce::Colour (0xff262b35));
	g.drawRoundedRectangle (reverbBox, 6.0f, 1.0f);

	g.setColour (juce::Colour (0xff00d4ff)); // Cyan
	g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
	g.drawText ("STUDIO REVERB", reverbBox.removeFromTop (18.0f).reduced (10.0f, 0.0f), juce::Justification::centredLeft, false);

	// Draw Delay Box
	g.setColour (juce::Colour (0xff181b21));
	g.fillRoundedRectangle (delayBox, 6.0f);
	g.setColour (juce::Colour (0xff262b35));
	g.drawRoundedRectangle (delayBox, 6.0f, 1.0f);

	g.setColour (juce::Colour (0xff00d4ff)); // Cyan
	g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
	g.drawText ("STEREO DELAY", delayBox.removeFromTop (18.0f).reduced (10.0f, 0.0f), juce::Justification::centredLeft, false);
}

void PluginEditor::resized()
{
	layoutButton.setBounds (getWidth() - 170, 16, 150, 28);

	auto area = getLocalBounds();
	const int headerHeight = 60;
	area.removeFromTop (headerHeight);

	// Master FX Area
	const int fxHeight = 106;
	auto fxArea = area.removeFromTop (fxHeight);
	auto fxInner = fxArea.reduced (16, 4);

	const int fxGap = 12;
	const int totalFxWidth = fxInner.getWidth() - fxGap;
	const int reverbWidth = (totalFxWidth * 4) / 7;

	auto reverbArea = fxInner.removeFromLeft (reverbWidth);
	fxInner.removeFromLeft (fxGap);
	auto delayArea = fxInner;

	// Layout Reverb Knobs (4 knobs)
	{
		reverbArea.removeFromTop (18); // Header text space
		const int knobWidth = reverbArea.getWidth() / 4;
		std::array<RotaryKnob*, 4> rKnobs = { &reverbRoomKnob, &reverbDampKnob, &reverbWidthKnob, &reverbMixKnob };
		for (size_t i = 0; i < 4; ++i)
		{
			auto kArea = reverbArea.removeFromLeft (knobWidth).reduced (2, 0);
			rKnobs[i]->label.setBounds (kArea.removeFromTop (14));
			rKnobs[i]->slider.setBounds (kArea);
		}
	}

	// Layout Delay Knobs (3 knobs)
	{
		delayArea.removeFromTop (18); // Header text space
		const int knobWidth = delayArea.getWidth() / 3;
		std::array<RotaryKnob*, 3> dKnobs = { &delayTimeKnob, &delayFeedbackKnob, &delayMixKnob };
		for (size_t i = 0; i < 3; ++i)
		{
			auto kArea = delayArea.removeFromLeft (knobWidth).reduced (2, 0);
			dKnobs[i]->label.setBounds (kArea.removeFromTop (14));
			dKnobs[i]->slider.setBounds (kArea);
		}
	}

	// Step Sequencer Area at bottom
	const int seqHeight = 145;
	auto seqArea = area.removeFromBottom (seqHeight);
	sequencerComponent.setBounds (seqArea.reduced (16, 2));

	// Pad Matrix Area
	const int marginX = 16;
	const int marginY = 8;
	auto gridArea = area.reduced (marginX, marginY);

	if (layoutMode_ == LayoutMode::Launchkey_2x8)
	{
		const int rows = 2;
		const int cols = 8;
		const int gap = 8;

		const int totalGapX = gap * (cols - 1);
		const int totalGapY = gap * (rows - 1);

		const int padWidth = (gridArea.getWidth() - totalGapX) / cols;
		const int padHeight = (gridArea.getHeight() - totalGapY) / rows;

		for (int row = 0; row < rows; ++row)
		{
			for (int col = 0; col < cols; ++col)
			{
				const size_t index = static_cast<size_t> (row * cols + col);
				const int x = gridArea.getX() + col * (padWidth + gap);
				const int y = gridArea.getY() + row * (padHeight + gap);

				if (index < drumPads.size())
					drumPads[index].setBounds (x, y, padWidth, padHeight);
			}
		}
	}
	else // Grid_4x4 (MPC bottom-up: Row 3 = 36-39, Row 2 = 40-43, Row 1 = 44-47, Row 0 = 48-51)
	{
		const int rows = 4;
		const int cols = 4;
		const int gap = 10;

		const int totalGapX = gap * (cols - 1);
		const int totalGapY = gap * (rows - 1);

		const int padWidth = (gridArea.getWidth() - totalGapX) / cols;
		const int padHeight = (gridArea.getHeight() - totalGapY) / rows;

		for (int row = 0; row < rows; ++row)
		{
			for (int col = 0; col < cols; ++col)
			{
				// Invert row index so row 3 (bottom) gets 0..3 (36-39)
				const size_t index = static_cast<size_t> ((3 - row) * cols + col);
				const int x = gridArea.getX() + col * (padWidth + gap);
				const int y = gridArea.getY() + row * (padHeight + gap);

				if (index < drumPads.size())
					drumPads[index].setBounds (x, y, padWidth, padHeight);
			}
		}
	}
}

void PluginEditor::parentHierarchyChanged()
{
	AudioProcessorEditor::parentHierarchyChanged();
	setOpaque (true);
#if JUCE_WINDOWS && ENABLE_WINE_BUILD
	if (auto* peer = getPeer())
	{
		peer->setCurrentRenderingEngine (0);
		repaint();
	}
#endif
}

void PluginEditor::visibilityChanged()
{
	AudioProcessorEditor::visibilityChanged();
	setOpaque (true);
#if JUCE_WINDOWS && ENABLE_WINE_BUILD
	if (auto* peer = getPeer())
	{
		peer->setCurrentRenderingEngine (0);
		repaint();
	}
#endif
}
