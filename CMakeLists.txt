cmake_minimum_required(VERSION 3.22)

if (POLICY CMP0169)
	cmake_policy(SET CMP0169 OLD)
endif()

project(dreamdrums VERSION 0.5.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ==============================================================================
# Build Target Configuration: Linux Native vs Windows Native vs Windows WINE
option(ENABLE_WINE_FIXES "Enable Wine/Bottles specific rendering and focus workarounds" OFF)
option(DREAMDRUMS_WINE_BUILD "Enable Wine/Bottles specific build mode" OFF)

if (DREAMDRUMS_WINE_BUILD)
	set(ENABLE_WINE_FIXES ON)
endif()

if (WIN32)
	if (ENABLE_WINE_FIXES)
		message(STATUS "Build Target: Linux WINE / Bottles Compatibility Mode")
		add_compile_definitions(
			JUCE_DIRECT2D=0
			JUCE_USE_DIRECTWRITE=0
			JUCE_USE_OPENGL=0
			ENABLE_WINE_BUILD=1
		)
	else()
		message(STATUS "Build Target: Windows Native (Direct2D / Standard Windows Messaging)")
		add_compile_definitions(
			JUCE_DIRECT2D=1
			JUCE_USE_DIRECTWRITE=1
			JUCE_USE_OPENGL=0
			ENABLE_WINE_BUILD=0
		)
	endif()
endif()

# ==============================================================================
# JUCE 8.0.15 & Wine Compatibility Patching
# ==============================================================================
include(FetchContent)

FetchContent_Declare(
	JUCE
	GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
	GIT_TAG 8.0.15
	GIT_SHALLOW TRUE
)

FetchContent_GetProperties(JUCE)
if (NOT juce_POPULATED)
	FetchContent_Populate(JUCE)

	if (ENABLE_WINE_FIXES)
		message(STATUS "Applying Wine / Bottles compatibility patches to JUCE...")

		# A. modules/juce_gui_basics/native/juce_Windowing_windows.cpp
		set(_JUCE_WIN_FILE "${juce_SOURCE_DIR}/modules/juce_gui_basics/native/juce_Windowing_windows.cpp")
		if (EXISTS "${_JUCE_WIN_FILE}")
			file(READ "${_JUCE_WIN_FILE}" _JUCE_WIN_CONTENT)

			# 1. Pakota oletusrenderöijäksi GDI (engine 0) Direct2D:n sijaan
			string(REPLACE "const auto renderer = juce_isRunningInWine() ? 0 : 1;"
			               "const auto renderer = 0;"
			               _JUCE_WIN_CONTENT "${_JUCE_WIN_CONTENT}")

			# 2. Pakota molemmat renderöintikontekstit GDI:ksi
			string(REPLACE "contextDescriptorList<GDIRenderContext, D2DRenderContext>"
			               "contextDescriptorList<GDIRenderContext, GDIRenderContext>"
			               _JUCE_WIN_CONTENT "${_JUCE_WIN_CONTENT}")

			# 3. Disabloi VBlankDispatcher-päivityskutsut
			string(REPLACE "void updateCurrentMonitorAndRefreshVBlankDispatcher (ForceRefreshDispatcher force = ForceRefreshDispatcher::no)"
			               "void updateCurrentMonitorAndRefreshVBlankDispatcher (ForceRefreshDispatcher force = ForceRefreshDispatcher::no) { ignoreUnused (force); return; } void unused_updateCurrentMonitorAndRefreshVBlankDispatcher (ForceRefreshDispatcher force = ForceRefreshDispatcher::no)"
			               _JUCE_WIN_CONTENT "${_JUCE_WIN_CONTENT}")

			string(REPLACE "VBlankDispatcher::getInstance()->removeListener (*this);"
			               "/* VBlankDispatcher removeListener disabled */"
			               _JUCE_WIN_CONTENT "${_JUCE_WIN_CONTENT}")

			string(REPLACE "auto* dispatcher = VBlankDispatcher::getInstance();"
			               "/* auto* dispatcher = nullptr; */"
			               _JUCE_WIN_CONTENT "${_JUCE_WIN_CONTENT}")

			string(REPLACE "dispatcher->reconfigureDisplays();"
			               "/* dispatcher->reconfigureDisplays(); */"
			               _JUCE_WIN_CONTENT "${_JUCE_WIN_CONTENT}")

			# 4. Lisää 60Hz vblankFallbackTimer korvaamaan VSync-kutsut
			string(REPLACE "std::optional<TimedCallback> monitorUpdateTimer;"
			               "std::optional<TimedCallback> monitorUpdateTimer; std::optional<TimedCallback> vblankFallbackTimer;"
			               _JUCE_WIN_CONTENT "${_JUCE_WIN_CONTENT}")

			string(REPLACE "setCurrentRenderingEngine (engine);"
			               "setCurrentRenderingEngine (engine); vblankFallbackTimer.emplace ([this] { onVBlank (Time::getMillisecondCounterHiRes() * 0.001); }); vblankFallbackTimer->startTimerHz (60);"
			               _JUCE_WIN_CONTENT "${_JUCE_WIN_CONTENT}")

			string(REPLACE "suspendResumeRegistration = {};"
			               "vblankFallbackTimer.reset(); suspendResumeRegistration = {};"
			               _JUCE_WIN_CONTENT "${_JUCE_WIN_CONTENT}")

			# 5. Vankempi BitBlt / StretchDIBits GDI-piirto (estää mustat ikkunat)
			string(REPLACE "void blitToDC (HDC dc, int x, int y) const noexcept"
			               "void blitToDC (HDC dc, int x, int y) const noexcept { SetMapMode (dc, MM_TEXT); GdiFlush(); if (! BitBlt (dc, x, y, width, height, hdc, 0, 0, SRCCOPY)) StretchDIBits (dc, x, y, width, height, 0, 0, width, height, bitmapData, (const BITMAPINFO*) &bitmapInfo, DIB_RGB_COLORS, SRCCOPY); } void unused_blitToDC (HDC dc, int x, int y) const noexcept"
			               _JUCE_WIN_CONTENT "${_JUCE_WIN_CONTENT}")

			# 6. Pakkorepaint ikkunan avautuessa
			string(REPLACE "void handleShowWindow() override {}"
			               "void handleShowWindow() override { peer.getComponent().repaint(); }"
			               _JUCE_WIN_CONTENT "${_JUCE_WIN_CONTENT}")

			file(WRITE "${_JUCE_WIN_FILE}" "${_JUCE_WIN_CONTENT}")
		endif()

		# B. modules/juce_gui_basics/native/juce_VBlank_windows.cpp
		set(_JUCE_VBLANK_FILE "${juce_SOURCE_DIR}/modules/juce_gui_basics/native/juce_VBlank_windows.cpp")
		if (EXISTS "${_JUCE_VBLANK_FILE}")
			file(READ "${_JUCE_VBLANK_FILE}" _JUCE_VBLANK_CONTENT)

			string(REPLACE "void updateDisplay (ComponentPeer::VBlankListener& listener, HMONITOR monitor)"
			               "void updateDisplay (ComponentPeer::VBlankListener& listener, HMONITOR monitor) { ignoreUnused (listener, monitor); return; } void unused_updateDisplay (ComponentPeer::VBlankListener& listener, HMONITOR monitor)"
			               _JUCE_VBLANK_CONTENT "${_JUCE_VBLANK_CONTENT}")

			string(REPLACE "void reconfigureDisplays()"
			               "void reconfigureDisplays() { return; } void unused_reconfigureDisplays()"
			               _JUCE_VBLANK_CONTENT "${_JUCE_VBLANK_CONTENT}")

			file(WRITE "${_JUCE_VBLANK_FILE}" "${_JUCE_VBLANK_CONTENT}")
		endif()

		# C. modules/juce_graphics/native/juce_DirectWriteTypeface_windows.cpp
		set(_JUCE_DWRITE_FILE "${juce_SOURCE_DIR}/modules/juce_graphics/native/juce_DirectWriteTypeface_windows.cpp")
		if (EXISTS "${_JUCE_DWRITE_FILE}")
			file(READ "${_JUCE_DWRITE_FILE}" _JUCE_DWRITE_CONTENT)

			string(REPLACE "auto factory = factories->getDWriteFactory().getInterface<IDWriteFactory2>();"
			               "ignoreUnused (c, language); return {}; auto factory = factories->getDWriteFactory().getInterface<IDWriteFactory2>();"
			               _JUCE_DWRITE_CONTENT "${_JUCE_DWRITE_CONTENT}")

			file(WRITE "${_JUCE_DWRITE_FILE}" "${_JUCE_DWRITE_CONTENT}")
		endif()

		message(STATUS "JUCE Wine / Bottles patches applied successfully.")
	endif()

	# Always auto-open connected MIDI input devices and set title to "Dreamdrums v0.5.1"
	set(_JUCE_STANDALONE_APP_FILE "${juce_SOURCE_DIR}/modules/juce_audio_plugin_client/juce_audio_plugin_client_Standalone.cpp")
	if (EXISTS "${_JUCE_STANDALONE_APP_FILE}")
		file(READ "${_JUCE_STANDALONE_APP_FILE}" _JUCE_STANDALONE_APP_CONTENT)
		string(REPLACE "autoOpenMidiDevices);"
		               "true /* autoOpenMidiDevices */);"
		               _JUCE_STANDALONE_APP_CONTENT "${_JUCE_STANDALONE_APP_CONTENT}")
		string(REPLACE "return new StandaloneFilterWindow (getApplicationName(),"
		               "return new StandaloneFilterWindow (\"Dreamdrums v0.5.1\","
		               _JUCE_STANDALONE_APP_CONTENT "${_JUCE_STANDALONE_APP_CONTENT}")
		file(WRITE "${_JUCE_STANDALONE_APP_FILE}" "${_JUCE_STANDALONE_APP_CONTENT}")
	endif()

	set(_JUCE_STANDALONE_WIN_FILE "${juce_SOURCE_DIR}/modules/juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h")
	if (EXISTS "${_JUCE_STANDALONE_WIN_FILE}")
		file(READ "${_JUCE_STANDALONE_WIN_FILE}" _JUCE_STANDALONE_WIN_CONTENT)
		string(REPLACE "bool shouldAutoOpenMidiDevices = false"
		               "bool shouldAutoOpenMidiDevices = true"
		               _JUCE_STANDALONE_WIN_CONTENT "${_JUCE_STANDALONE_WIN_CONTENT}")
		string(REPLACE "m.addItem (4, TRANS (\"Reset to default state\"));"
		               "m.addItem (4, TRANS (\"Reset to default state\"));\n        m.addSeparator();\n        m.addItem (5, TRANS (\"About...\"));"
		               _JUCE_STANDALONE_WIN_CONTENT "${_JUCE_STANDALONE_WIN_CONTENT}")
		string(REPLACE "case 4:  resetToDefaultState(); break;"
		               "case 4:  resetToDefaultState(); break;\n            case 5:  { DialogWindow::LaunchOptions opt; auto* l = new Label (\"\", \"Dreamdrums v0.5.1\\n\\nCopyright (C) 2026 by Erno Pakarinen\\nContact: sanctumsignal(at)gmail.com\"); l->setFont (FontOptions (13.5f, Font::bold)); l->setJustificationType (Justification::centred); l->setSize (360, 130); l->setColour (Label::textColourId, Colour (0xffeeeff2)); l->setColour (Label::backgroundColourId, Colour (0xff181b21)); opt.content.setOwned (l); opt.dialogTitle = \"About Dreamdrums\"; opt.dialogBackgroundColour = Colour (0xff181b21); opt.escapeKeyTriggersCloseButton = true; opt.useNativeTitleBar = true; opt.resizable = false; opt.launchAsync(); break; }"
		               _JUCE_STANDALONE_WIN_CONTENT "${_JUCE_STANDALONE_WIN_CONTENT}")
		file(WRITE "${_JUCE_STANDALONE_WIN_FILE}" "${_JUCE_STANDALONE_WIN_CONTENT}")
	endif()

	add_subdirectory(${juce_SOURCE_DIR} ${juce_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "Don't check for remote updates on every reconfigure")

# ==============================================================================
# CLAP JUCE Extensions (FetchContent)
# ==============================================================================
FetchContent_Declare(
	clap-juce-extensions
	GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
	GIT_TAG 7adee3a1bd4684d4caa5601100e364abccff4b4d
	GIT_SUBMODULES_RECURSE TRUE
)
FetchContent_MakeAvailable(clap-juce-extensions)

# ==============================================================================
# Plugin Targets (Standalone, VST3, CLAP)
# ==============================================================================
juce_add_plugin(dreamdrums
	PRODUCT_NAME "Dreamdrums"
	COMPANY_NAME "dreamdrums"
	VERSION "0.5.1"
	IS_SYNTH TRUE
	NEEDS_MIDI_INPUT TRUE
	NEEDS_MIDI_OUTPUT TRUE
	IS_MIDI_EFFECT FALSE
	FORMATS Standalone VST3
)

clap_juce_extensions_plugin(
	TARGET dreamdrums
	CLAP_ID "com.dreamdrums.dreamdrums"
	CLAP_FEATURES instrument drum synthesizer
)

target_sources(dreamdrums
	PRIVATE
		src/PluginProcessor.h
		src/PluginProcessor.cpp
		src/PluginEditor.h
		src/PluginEditor.cpp
		src/engine/DrumSample.hpp
		src/engine/DrumPad.hpp
		src/engine/SampleBank.hpp
		src/engine/SampleBank.cpp
		src/engine/DrumVoice.hpp
		src/engine/DrumVoicePool.hpp
		src/engine/StepSequencer.hpp
		src/ui/DrumPadComponent.hpp
		src/ui/DrumPadComponent.cpp
		src/ui/StepSequencerComponent.hpp
		src/ui/StepSequencerComponent.cpp
)

target_include_directories(dreamdrums
	PRIVATE
		src
)

target_compile_features(dreamdrums PRIVATE cxx_std_20)

target_compile_definitions(dreamdrums
	PUBLIC
		JUCE_WEB_BROWSER=0
		JUCE_USE_CURL=0
		JUCE_VST3_CAN_REPLACE_VST2=0
		DREAMDRUMS_DEFAULT_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets/salamanderDrums"
)

target_link_libraries(dreamdrums
	PRIVATE
		juce::juce_dsp
		juce::juce_audio_utils
		juce::juce_recommended_config_flags
		juce::juce_recommended_warning_flags
		juce::juce_recommended_lto_flags
)

if (TARGET dreamdrums_Standalone)
	set_target_properties(dreamdrums_Standalone PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
		OUTPUT_NAME "dreamdrums"
	)
endif()

# ==============================================================================
# Engine Unit & Verification Test
# ==============================================================================
add_executable(engine_test
	tests/test_engine.cpp
	src/engine/SampleBank.cpp
)
target_include_directories(engine_test PRIVATE src)
target_compile_features(engine_test PRIVATE cxx_std_20)
target_compile_definitions(engine_test PRIVATE
	JUCE_WEB_BROWSER=0
	JUCE_USE_CURL=0
	DREAMDRUMS_DEFAULT_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets/salamanderDrums"
)
target_link_libraries(engine_test PRIVATE
	juce::juce_audio_formats
	juce::juce_audio_basics
	juce::juce_audio_processors
	juce::juce_core
)
set_target_properties(engine_test PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
