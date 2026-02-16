VERIFICATION: FFT trace glow / polished look (AnalyzerRenderer)

Build commands

  From repo root (AnalyzerPro):
    mkdir -p build && cd build && cmake -G "Xcode" -DCMAKE_BUILD_TYPE=Release .. && cmake --build . --config Release

  Or Unix Makefiles:
    mkdir -p build && cd build && cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release .. && make -j

  Target: build the app/plugin so mdsp_ui (AnalyzerRenderer) is linked and run the analyzer view.

Visual smoke checklist

  [ ] RMS has soft glow and depth
  [ ] Peak and Hold have subtle glow, sharper core
  [ ] No new flicker
  [ ] Grid remains sharp (no blur on grid lines)
  [ ] Draw order: grid → RMS fill → RMS outline → Peak outline → Hold outline → overlays

Static rg check

  Confirm no blur/image filter usage introduced in the renderer:

    rg -n "blur|GaussianBlur|DropShadow|Image::|applyBlur" --type-add 'cpp:*.{cpp,h}' -t cpp third_party/melechdsp-hq/shared/mdsp_ui/src/analyzer/AnalyzerRenderer.cpp

  Expected: no matches (or only comments). Glow is implemented with layered Path strokes and alpha only.

  Confirm only Path stroking changes (no new fill logic for traces other than existing RMS gradient):

    rg -n "strokePath|PathStrokeType|fillPath" third_party/melechdsp-hq/shared/mdsp_ui/src/analyzer/AnalyzerRenderer.cpp

  Expected: strokePath/PathStrokeType used for glow/core/main passes; fillPath only for existing RMS fill area.
