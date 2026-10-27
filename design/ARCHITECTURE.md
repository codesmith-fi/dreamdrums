
# ARCHITECTURE.md - Project Audio Architecture & Guidelines

## 1. Project Overview
This project is an exploratory, modular audio synthesizer developed using **C++20** and the **JUCE** framework.
The target deployment starts as a native Linux **Standalone** executable (utilizing ALSA/PipeWire/JACK directly) and is architected to seamlessly cross-compile into **VST3** and **CLAP** plugin formats without refactoring core DSP or UI layers.

### Project & Implementation Milestones
Phase 1 (Current): Standalone shell, basic MVC skeleton, MIDI listener & interactive keyboard component (visual-only, muted output).
Phase 2: Monophonic core DSP (Phase accumulator, band-limited Sine oscillator, Note-on/off triggers).
Phase 3: Envelope generation (Analog-modeled ADSR state machine).
Phase 4: Polyphony management & Voice pooling.
Phase 5: Filter & Waveshaping stages.
Phase 6: VST3 / CLAP target configuration and plugin testing.

---
## 2. Architectural Pattern: Strict MVC Separation

The codebase strictly enforces Model-View-Controller (MVC) separation to ensure stability, thread safety, and modularity:

* **Model (DSP Engine & State):**
  * Houses the signal generation (oscillators, envelopes, filters, voice management).
  * Manages audio state, parameter tree, and MIDI buffers.
  * **Strict Isolation:** Zero knowledge of UI components, rendering contexts, or graphics headers.

* **View (User Interface):**
  * Visualizes the synthesizer state (keys, scopes, envelopes, custom controls).
  * Does not perform audio processing or heavy business logic.
  * Driven asynchronously from the processor state via thread-safe abstractions.
  * Avoids native OS-level popup windows to maintain full compatibility across Linux windowing systems (X11 / Wayland).

* **Controller (Processor Glue):**
  * Bridges host events, MIDI input, audio buffer dispatch, and parameter modulation.
  * Implements `juce::AudioProcessor` lifecycle methods (`prepareToPlay`, `processBlock`, `releaseResources`).

---

## 3. Real-Time Audio Thread Guardrails (Golden Rules)

The `processBlock()` audio render loop runs on a high-priority, real-time audio thread. To prevent audio dropouts, glitches, and priority inversions, the following operations are **strictly forbidden** within the audio thread:

1. **No Dynamic Memory Allocation:** No `malloc()`, `free()`, `new`, `delete`, or resizing of STL containers (`std::vector::push_back`, etc.). All buffers and voices must be pre-allocated during `prepareToPlay()`.
2. **No Blocking Primitives:** No standard mutex locks, condition variables, or sleep calls (`std::mutex`, `std::condition_variable`, `std::this_thread::sleep_for`). Use lock-free primitives, atomic values (`std::atomic`), or wait-free FIFO rings where needed.
3. **No I/O Operations:** No file system access, network calls, terminal logging (`std::cout`), or format printing.
4. **Deterministic Execution:** Keep DSP algorithmic paths bounded and mathematically predictable.

### Exceptions 
1. Dynamic memory allocations can be used outside the time critical DSP code.

---

## 4. Coding Conventions

* **Standard:** Modern C++20.
* **Naming:**
  * Classes and Structs: `PascalCase` (e.g., `VoiceManager`, `SineOscillator`).
  * Member Variables: `camelCase` with trailing underscore (e.g., `sampleRate_`, `currentPhase_`).
  * Functions and Methods: `camelCase` (e.g., `setFrequency()`, `processBlock()`).
  * Constants / Enums: `kPascalCase` or `UPPER_SNAKE_CASE` (e.g., `kMaxPolyphony = 16`).
* **Header Strategy:** Clean separation into `src/dsp/`, `src/gui/`, and `src/common/`. Heavy templated math in headers; complex logic in translation units (`.cpp`).
* Indentation: Use tabs for indentation, spaces for alignment.

---

## 5. Build System & Toolchain (Linux)

The project leverages **CMake** combined with `FetchContent` to pull JUCE automatically without requiring global SDK installations.

### Prerequisites (Ubuntu/Debian-based):
```bash
sudo apt update
sudo apt install build-essential cmake git libasound2-dev libx11-dev \
    libxinerama-dev libxext-dev libfreetype6-dev libgl1-mesa-dev libxcursor-dev
    
### Build & Run Commands:
```bash
# Configure build directory
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# Compile executable
cmake --build build -j$(nproc)

# Run native standalone executable
./build/bin/dreamdrums

