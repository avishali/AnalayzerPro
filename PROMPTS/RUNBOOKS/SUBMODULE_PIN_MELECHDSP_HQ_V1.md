MISSION_ID: SUBMODULE_PIN_MELECHDSP_HQ_V1

TITLE
Commit the melechdsp-hq submodule WIP (glassy-motion render + triangular smoothing) on a branch, push it, and bump the parent repo's submodule pointer — so the beta build is reproducible and the ~660-line WIP can't be lost.

STATE (verified 2026-06-01)
- Submodule third_party/melechdsp-hq is in DETACHED HEAD ("## HEAD (no branch)") at f272a92 (== parent pin), working tree dirty.
- 27 modified TRACKED files (RTADisplay, SnapshotPump, AnalyzerDisplayWidget, AnalyzerEngine.cpp [the new triangular smoothing], mdsp_ui/rta/*, etc.).
- 4 UNTRACKED dirs that are NOT clearly part of the analyzer work — DECIDE before adding:
    cmake/cmake/                                   (looks generated — likely DO NOT commit)
    shared/dsp_bench/                              (benchmark tooling; __pycache__/ + runs/ already gitignored)
    shared/mdsp_dsp/include/mdsp_dsp/dynamics/     (new DSP source — unrelated to analyzer?)
    shared/mdsp_dsp/src/dynamics/                  (new DSP source — unrelated to analyzer?)
- .DS_Store / __pycache__ / runs/ are gitignored (safe).
- Remote: origin = https://github.com/avishali/melechdsp-hq.git

HARD RULES
- DETACHED HEAD: create a branch BEFORE committing, or the commit is unreachable.
- Do NOT `git add -A` blindly — review the 4 untracked dirs first (build junk vs real source).
- Do NOT change code in this mission. Git-only.
- The parent pointer commit must reference a PUSHED submodule commit (not local-only).

============================================================
IMPLEMENTER
============================================================
SUB=third_party/melechdsp-hq

STEP 1 — Branch the submodule (fix detached HEAD)
    git -C $SUB switch -c analyzerpro/glassy-motion-and-smoothing
(Confirm the working-tree changes are still present after switching; -c keeps them.)
STOP and report the branch name + that the 27 modified files survived.

STEP 2 — Untracked dirs (ALREADY ANALYZED 2026-06-01)
- CONFIRMED: the modified shared/mdsp_dsp/CMakeLists.txt explicitly lists
    src/dynamics/LimiterEnvelope.cpp, src/dynamics/TruePeakDetector.cpp, src/dynamics/IspTrimStage.cpp
  => shared/mdsp_dsp/src/dynamics/ AND shared/mdsp_dsp/include/mdsp_dsp/dynamics/ MUST be committed, or the pinned commit will fail to configure/build (missing sources). (These are limiter/dynamics files, unrelated to the analyzer, but the shared hq lib needs them to compile.)
- cmake/cmake/  → NOT referenced by the build; looks generated → EXCLUDE.
- shared/dsp_bench/ → benchmark tooling, not referenced by the lib build; __pycache__/ + runs/ already gitignored → EXCLUDE (owner may commit separately later if desired).
- So STEP 3 includes: all 27 tracked mods + shared/mdsp_dsp/src/dynamics + shared/mdsp_dsp/include/mdsp_dsp/dynamics. Exclude cmake/cmake and dsp_bench.
STOP only if the owner disagrees with the above; otherwise proceed to STEP 3.

STEP 3 — Stage + commit (untracked decision resolved in STEP 2)
    git -C $SUB add -u                        # all 27 tracked modifications
    git -C $SUB add shared/mdsp_dsp/include/mdsp_dsp/dynamics shared/mdsp_dsp/src/dynamics   # build-required
    git -C $SUB status                        # verify: NO .DS_Store, NO cmake/cmake/, NO dsp_bench/ staged
    git -C $SUB commit -m "hq WIP: VBlank glassy-motion render + triangular (cascaded-box) spectral smoothing; add dynamics sources"
STOP and report the new submodule commit SHA + the staged file list.

STEP 4 — Push the submodule branch (so the pin is fetchable)
    git -C $SUB push -u origin analyzerpro/glassy-motion-and-smoothing
STOP and report push result.

STEP 5 — Bump the parent submodule pointer
    # from repo root:
    git add third_party/melechdsp-hq
    git status                                # should show the submodule pointer change staged
    git commit -m "Bump melechdsp-hq: glassy-motion render + triangular smoothing"
    git push
STOP and report the parent commit SHA.

STEP 6 — Verify reproducibility
    git submodule status                      # SHA should match the new submodule commit, no '+'/'-' prefix surprises
    # optional sanity: fresh checkout test
    #   git -C $SUB fetch origin && git -C $SUB rev-parse HEAD  (matches parent pin)
- Confirm parent `git status` shows the submodule clean (or only the deliberately-excluded untracked dirs remain).
STOP and write PROMPTS/MISSIONS/SUBMODULE_PIN_RESULT.md (branch, submodule SHA, parent SHA, what was included/excluded). End with STOP.

============================================================
VERIFIER
============================================================
CHECK 1 — Submodule commit is on a BRANCH (not detached) and PUSHED to origin.
CHECK 2 — Parent pin references the pushed submodule SHA (git submodule status agrees).
CHECK 3 — No build junk committed (.DS_Store, __pycache__, runs/, cmake/cmake unless explicitly approved).
CHECK 4 — If the build compiles dynamics/, it was committed (reproducible). If excluded, the build still configures/links without it.
CHECK 5 — Code unchanged by this mission (git-only).
OUTPUT: PROMPTS/MISSIONS/SUBMODULE_PIN_VERIFIER.md table. End with STOP.
