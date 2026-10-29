#include "AboutComponent.hpp"
#include <BinaryData.h>

AboutComponent::AboutComponent()
{
	setSize (380, 360);
	setOpaque (true);

	appIcon_ = juce::ImageCache::getFromMemory (
		BinaryData::dreamdrums_appicon_256_png,
		BinaryData::dreamdrums_appicon_256_pngSize);

	closeButton_.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff212429));
	closeButton_.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff323842));
	closeButton_.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff00d4ff));
	closeButton_.onClick = [this] {
		if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
			dw->exitModalState (0);
	};
	addAndMakeVisible (closeButton_);
}

void AboutComponent::paint (juce::Graphics& g)
{
	auto bounds = getLocalBounds();

	// 1. Sleek dark background matching Dreamdrums
	g.setColour (juce::Colour (0xff14171b));
	g.fillRect (bounds);

	// 2. Inner card container
	auto cardBounds = bounds.reduced (8).toFloat();
	g.setColour (juce::Colour (0xff181b21));
	g.fillRoundedRectangle (cardBounds, 8.0f);
	g.setColour (juce::Colour (0xff262b35));
	g.drawRoundedRectangle (cardBounds, 8.0f, 1.0f);

	// 3. Application icon (centered at top)
	const int iconSize = 96;
	const int iconX = (getWidth() - iconSize) / 2;
	const int iconY = 24;

	if (appIcon_.isValid())
	{
		g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
		g.drawImageWithin (appIcon_, iconX, iconY, iconSize, iconSize,
		                   juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
	}

	// 4. Product Title "DREAMDRUMS"
	g.setColour (juce::Colours::white);
	g.setFont (juce::FontOptions (20.0f, juce::Font::bold));
	g.drawText ("DREAMDRUMS", 0, 130, getWidth(), 26, juce::Justification::centred, false);

	// 5. Version badge
	g.setColour (juce::Colour (0xff00d4ff)); // Cyan accent
	g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
	g.drawText ("v0.5.1", 0, 158, getWidth(), 18, juce::Justification::centred, false);

	// 6. Subtitles / Feature Summary
	g.setColour (juce::Colour (0xffa0a8b4));
	g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
	g.drawText ("SALAMANDER DRUMKIT  •  16 VELOCITY PADS",
	            0, 184, getWidth(), 18, juce::Justification::centred, false);

	g.setColour (juce::Colour (0xff7a8290));
	g.setFont (juce::FontOptions (10.5f));
	g.drawText ("16 Stereo Multi-Out Buses  •  32-Step Sequencer",
	            0, 204, getWidth(), 18, juce::Justification::centred, false);

	// 7. Divider line
	g.setColour (juce::Colour (0xff252a32));
	g.drawHorizontalLine (232, 40.0f, static_cast<float> (getWidth() - 40));

	// 8. Copyright & Contact
	g.setColour (juce::Colour (0xffeeeff2));
	g.setFont (juce::FontOptions (11.5f));
	g.drawText ("Copyright © 2026 by Erno Pakarinen",
	            0, 246, getWidth(), 18, juce::Justification::centred, false);

	g.setColour (juce::Colour (0xff00d4ff));
	g.setFont (juce::FontOptions (11.0f));
	g.drawText ("sanctumsignal@gmail.com",
	            0, 266, getWidth(), 18, juce::Justification::centred, false);
}

void AboutComponent::resized()
{
	closeButton_.setBounds ((getWidth() - 100) / 2, 304, 100, 28);
}

void AboutComponent::show()
{
	auto* content = new AboutComponent();
	juce::DialogWindow::LaunchOptions opt;
	opt.content.setOwned (content);
	opt.dialogTitle = "About Dreamdrums";
	opt.dialogBackgroundColour = juce::Colour (0xff181b21);
	opt.escapeKeyTriggersCloseButton = true;
	opt.useNativeTitleBar = true;
	opt.resizable = false;

	auto* dw = opt.launchAsync();
	if (dw != nullptr)
	{
		auto icon = juce::ImageCache::getFromMemory (
			BinaryData::dreamdrums_appicon_256_png,
			BinaryData::dreamdrums_appicon_256_pngSize);
		if (icon.isValid())
			dw->setIcon (icon);
	}
}

void AboutComponent::applyAppIcon (juce::Component* comp)
{
	if (comp == nullptr)
		return;

	auto icon = juce::ImageCache::getFromMemory (
		BinaryData::dreamdrums_appicon_256_png,
		BinaryData::dreamdrums_appicon_256_pngSize);

	if (icon.isValid())
	{
		if (auto* docWin = dynamic_cast<juce::DocumentWindow*> (comp))
			docWin->setIcon (icon);
		else if (auto* parentDocWin = comp->findParentComponentOfClass<juce::DocumentWindow>())
			parentDocWin->setIcon (icon);

		if (auto* peer = comp->getPeer())
			peer->setIcon (icon);
	}
}
