# Metal Render-Path Repro Harness

This harness drives the real `MetalHost` / `MetalHostImpl` render path outside Pro Tools. It is opt-in:

```sh
cmake -B build-metal-repro -DANALYZERPRO_METAL_REPRO=ON -DJUCE_PATH=/Users/avishaylidani/DEV/SDK/JUCE
cmake --build build-metal-repro --target MetalReproHarness
```

Cheapest first, no ASan rebuild, names the zombie:

```sh
NSZombieEnabled=YES MallocScribble=YES MallocStackLogging=YES ./build-metal-repro/MetalReproHarness --empty
```

Expected useful failure shape:

```text
*** -[CAMetalDrawable release]: message sent to deallocated instance 0x...
```

Then capture the allocation/free stacks while the process is still alive:

```sh
malloc_history <pid> <addr>
```

Precise double-free/UAF report:

```sh
cmake -B build-asan -DANALYZERPRO_METAL_REPRO=ON -DANALYZERPRO_ASAN=ON -DJUCE_PATH=/Users/avishaylidani/DEV/SDK/JUCE
cmake --build build-asan --target MetalReproHarness
./build-asan/MetalReproHarness --analyzer
```

Toggles:

- `--empty`: no published chrome or analyzer config; this forces the `drawEmptyClear` drawable-only path.
- `--chrome`: publishes only synthetic `FrameTexturePayload` chrome frames.
- `--analyzer`: publishes chrome plus analyzer config while a synthetic feeder drives `AnalyzerEngine`.
- `--analyzer --multitrace`: also publishes synthetic DB vectors for peak/stereo/mid/side traces.

Interpretation:

- `--empty` reproduces: drawable lifetime / presentation path.
- `--chrome` only reproduces: chrome payload upload or texture-ring path.
- `--analyzer` only reproduces: engine snapshot / RMS analyzer path.
- `--analyzer --multitrace` only reproduces: published DB trace-vector path.

Defaults match the stress brief: `50` cycles and `200000` rendered frames per cycle. For local smoke checks before long zombie/ASan runs, use `--cycles N` and `--frames N`.
