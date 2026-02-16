# Phase-2 / Slice B — Verification

Verify AnalyzerSnapshot is defined only in mdsp_dsp; AnalyzerPro uses the shared header (via adapter) and has no local definition.

---

## 1. Build steps

### melechdsp-hq — mdsp_dsp target

AnalyzerPro builds melechdsp-hq via add_subdirectory; no separate build required. To build mdsp_dsp standalone (e.g. from melechdsp-hq repo):

```bash
cd /path/to/melechdsp-hq
cmake -B build -DJUCE_PATH="$JUCE_PATH"
cmake --build build --target mdsp_dsp
```

Expect: build completes. No new .cpp was added for Slice B; the new AnalyzerSnapshot.h is header-only.

### AnalyzerPro target

```bash
cd /path/to/AnalyzerPro
cmake -B build -DJUCE_PATH="$JUCE_PATH"
cmake --build build --target AnalyzerPro
```

Expect: build completes with no errors. Plugin artefact in build/AnalyzerPro_artefacts/.

---

## 2. Static checks (ripgrep)

Run from AnalyzerPro repo root.

**Prove AnalyzerSnapshot is not defined anywhere under AnalyzerPro Source/:**

```bash
rg 'struct AnalyzerSnapshot|class AnalyzerSnapshot' Source/
```

Expected: no matches. (Definition lives in mdsp_dsp only.)

```bash
rg 'struct PublishedAnalyzerSnapshot|class PublishedAnalyzerSnapshot' Source/
```

Expected: no matches.

**Prove AnalyzerPro includes mdsp_dsp/analyzer/AnalyzerSnapshot.h (or adapter that includes it):**

```bash
rg 'AnalyzerSnapshot|AnalyzerSnapshotAdapter|analyzer/AnalyzerSnapshot' Source/
```

Expected: only references via the adapter or the type name in use. No local `#include ".*AnalyzerSnapshot\.h"` under Source/ (old path removed).

```bash
rg '#include.*AnalyzerSnapshot|#include.*analyzer/AnalyzerSnapshot' Source/
```

Expected: no match for a local analyzer/AnalyzerSnapshot.h. Adapter is allowed.

```bash
rg 'dsp_adapters/AnalyzerSnapshotAdapter|mdsp_dsp/analyzer/AnalyzerSnapshot' Source/
```

Expected: at least two matches — AnalyzerEngine.h and AnalyzerDisplayView.h include the adapter; adapter file contains `#include <mdsp_dsp/analyzer/AnalyzerSnapshot.h>`.

---

## 3. ODR checks

**No duplicate AnalyzerSnapshot / PublishedAnalyzerSnapshot definitions:**

After a clean build, ensure only one definition of the snapshot types is linked. Header-only structs typically do not emit external symbols; if the plugin links mdsp_dsp and no local .cpp/.h defines the same struct, there is no ODR violation.

Check that no object file in AnalyzerPro Source/ was compiled from a file that defined AnalyzerSnapshot:

```bash
cd build
grep -l AnalyzerSnapshot CMakeFiles/AnalyzerPro.dir/Source/*.o 2>/dev/null || true
```

Expected: no .o under Source/ that corresponds to a definition (AnalyzerSnapshot.h is deleted; adapter only has using declarations). If your build preserves .o paths, ensure no Source/analyzer/AnalyzerSnapshot.*.o exists.

Linker expectation: Build must complete without "multiple definition of" or "redefinition of" errors for AnalyzerSnapshot or PublishedAnalyzerSnapshot. Single definition in mdsp_dsp header, included only via adapter in AnalyzerPro.

---

## 4. Runtime smoke

1. Open the plugin (DAW or AnalyzerPro Standalone from build).
2. Feed a continuous signal (e.g. tone or music) to the input.
3. Confirm FFT works: spectrum/RTA display updates; bins show level; no blank or frozen trace.
4. Confirm no crashes when switching view (FFT / Bands / Log), changing FFT size, or toggling hold.
5. Confirm multi-trace (L/R/Mono/Mid/Side) and peak hold display as before; no missing data or wrong scaling.

Pass: behavior matches pre–Slice B (FFT and snapshot path unchanged in character).

---

## 5. Rollback plan

To revert Slice B safely in both repos.

**AnalyzerPro:**

Restore local AnalyzerSnapshot definition and includes; remove adapter and use of shared header:

```bash
cd /path/to/AnalyzerPro
git checkout HEAD -- Source/analyzer/AnalyzerSnapshot.h
git checkout HEAD -- Source/analyzer/AnalyzerEngine.h Source/ui/analyzer/AnalyzerDisplayView.h
git rm -f Source/dsp_adapters/AnalyzerSnapshotAdapter.h 2>/dev/null || rm -f Source/dsp_adapters/AnalyzerSnapshotAdapter.h
```

If Source/analyzer/AnalyzerSnapshot.h was deleted in a commit, restore it from the commit before Slice B:

```bash
git show <pre-slice-B-commit>:Source/analyzer/AnalyzerSnapshot.h > Source/analyzer/AnalyzerSnapshot.h
```

Revert Engine and DisplayView includes to point at the local header:

- AnalyzerEngine.h: `#include "AnalyzerSnapshot.h"`
- AnalyzerDisplayView.h: `#include "../../analyzer/AnalyzerSnapshot.h"`

Remove the adapter file if present. If third_party/melechdsp-hq was given a copy of the new header, remove it or reset the submodule:

```bash
rm -f third_party/melechdsp-hq/shared/mdsp_dsp/include/mdsp_dsp/analyzer/AnalyzerSnapshot.h
# or, to reset submodule to previous commit:
git submodule update --init third_party/melechdsp-hq
```

Rebuild:

```bash
cmake --build build --target AnalyzerPro
```

**melechdsp-hq:**

Remove the shared snapshot header so no other consumer relies on it until Slice B is re-applied:

```bash
cd /path/to/melechdsp-hq
git rm shared/mdsp_dsp/include/mdsp_dsp/analyzer/AnalyzerSnapshot.h
# commit the removal, or:
git checkout HEAD -- shared/mdsp_dsp/include/mdsp_dsp/analyzer/
```

If Slice B was a single commit, revert it:

```bash
git revert --no-edit <slice-B-commit-hash>
```

Rebuild mdsp_dsp if built standalone:

```bash
cmake --build build --target mdsp_dsp
```

After rollback, AnalyzerPro must again compile with its local Source/analyzer/AnalyzerSnapshot.h and no dependency on mdsp_dsp for AnalyzerSnapshot.
