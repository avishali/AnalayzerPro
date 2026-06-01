# UI_OVERHAUL_03 — Commit & Submodule-Bump Handoff (for Cursor)

Stage 3 (LookAndFeel generalization into shared `mdsp_ui`) is **implemented and
visually confirmed** in AnalyzerPro. All builds are green against canonical HQ.
What remains is purely git plumbing so that **default builds** (without
`-DMELECHDSP_HQ_ROOT`) inherit the new look, plus two low-priority follow-ups.

Three repos are involved:
- Canonical: `/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq` (branch `master`, remote `avishali/melechdsp-hq`)
- `/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/AnalyzerPro` (branch `master`)
- `/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter` (branch `main`)

Both products consume HQ as a git **submodule** at `third_party/melechdsp-hq`
pointing at the same GitHub repo. So the canonical commit must be **pushed**
before the submodule pointers can be bumped.

---

## ⚠️ Scoping landmines — read before staging anything

1. **Canonical HQ has a stray `M MCP` change** unrelated to mdsp_ui. Do NOT fold
   it into the LookAndFeel commit unless you confirm it's intended. Stage only
   the mdsp_ui files listed below.
2. **MasterLimiter has 3 unrelated modified scripts**
   (`scripts/create_installer.sh`, `scripts/release_sign_macos.sh`,
   `scripts/sign_and_notarize.sh`). These are NOT part of Stage 3 — exclude them
   from the Stage-3 commit.
3. **AnalyzerPro mixes Stage-02 and Stage-03 in the same working tree, and both
   touch `HeaderBar.{cpp,h}`** — a clean per-file split is impossible.
   - Stage-02 (Control IA reorg): `MainView.{cpp,h}`, `ControlRail.{cpp,h}`, parts of `HeaderBar.*`
   - Stage-03 (LookAndFeel adoption): the HeaderBar ad-hoc LnF removal in `HeaderBar.*`
   - **Recommended:** commit them together as one "UI overhaul 02+03" commit rather
     than risk a broken intermediate state from `git add -p` surgery. If a split
     is required, use `git add -p` on `HeaderBar.*` and build between commits.
4. Keep the untracked result docs: `PROMPTS/MISSIONS/UI_OVERHAUL_02_RESULT.md`,
   `UI_OVERHAUL_03_RESULT.md`, and this handoff file.

---

## Step A — Commit & push canonical HQ

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq
git add shared/mdsp_ui/include/mdsp_ui/LookAndFeel.h \
        shared/mdsp_ui/include/mdsp_ui/Metrics.h \
        shared/mdsp_ui/include/mdsp_ui/Theme.h \
        shared/mdsp_ui/src/ButtonPaint.cpp \
        shared/mdsp_ui/src/ButtonStyle.cpp \
        shared/mdsp_ui/src/LookAndFeel.cpp \
        shared/mdsp_ui/src/Theme.cpp \
        shared/mdsp_ui/src/ThemeTokens.generated.cpp \
        shared/mdsp_ui/ui_tokens/scripts/gen_theme_tokens.py \
        shared/mdsp_ui/ui_tokens/tokens.json
# (decide separately whether to commit the stray `MCP` change)
git commit -m "mdsp_ui: generalize MasterLimiter LookAndFeel (soft buttons/combos/rotary/fader), adopt palette as Dark default, add tokens"
git push origin master
HQ_SHA=$(git rev-parse HEAD)
echo "Canonical HQ commit: $HQ_SHA   (use this SHA for both submodule bumps)"
```

> The push is mandatory: the product submodules fetch from GitHub, so an unpushed
> local commit cannot be checked out as a submodule pointer on a clean clone/CI.

---

## Step B — AnalyzerPro: bump submodule + commit

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/AnalyzerPro
git -C third_party/melechdsp-hq fetch origin
git -C third_party/melechdsp-hq checkout "$HQ_SHA"
git add third_party/melechdsp-hq
# product changes (Stage 02 + 03 together — see landmine #3) + result docs
git add Source/ui/MainView.cpp Source/ui/MainView.h \
        Source/ui/layout/ControlRail.cpp Source/ui/layout/ControlRail.h \
        Source/ui/layout/HeaderBar.cpp Source/ui/layout/HeaderBar.h \
        AnalyzerPro.code-workspace \
        PROMPTS/MISSIONS/UI_OVERHAUL_02_RESULT.md \
        PROMPTS/MISSIONS/UI_OVERHAUL_03_RESULT.md \
        PROMPTS/MISSIONS/UI_OVERHAUL_03_COMMIT_HANDOFF.md
git commit -m "ui: control IA reorg (02) + adopt shared mdsp_ui LookAndFeel (03); bump mdsp_ui submodule"
```

---

## Step C — MasterLimiter: bump submodule + commit (Stage-3 only)

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git -C third_party/melechdsp-hq fetch origin
git -C third_party/melechdsp-hq checkout "$HQ_SHA"
git add third_party/melechdsp-hq \
        Source/ui/MasterLimiterLookAndFeel.cpp Source/ui/MasterLimiterLookAndFeel.h
# DO NOT add scripts/create_installer.sh, release_sign_macos.sh, sign_and_notarize.sh (unrelated)
git commit -m "ui: slim MasterLimiterLookAndFeel to product-specific glyphs; bump mdsp_ui submodule to shared LookAndFeel"
```

---

## Step D — Verify default builds inherit the look (no MELECHDSP_HQ_ROOT)

The whole point of the submodule bump: a build configured **without**
`-DMELECHDSP_HQ_ROOT` must now use the updated submodule and show the new look.

```bash
# AnalyzerPro — fresh default-path build
cmake -S /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/AnalyzerPro -B build-default-verify \
      -DANALYZERPRO_COPY_AFTER_BUILD=OFF
cmake --build build-default-verify --target AnalyzerPro_Standalone
# Confirm CMake log prints HQ_DIR = .../third_party/melechdsp-hq (NOT the canonical path)

# MasterLimiter — same idea with its standalone target
```

Both must be green and resolve HQ from `third_party/melechdsp-hq`.

---

## Follow-ups (low priority, not blockers)

1. **Confirm the Standalone debug HUD is flag-gated.** The overlay
   `"[Standalone UI] tick=VBlank paint/s=… render_fps=… scale=1.00"` is printed
   over the spectrum. Find that format string (search AnalyzerPro `Source/` for
   `"tick=VBlank"` / `render_jitter_avg`) and confirm it is compiled out / gated
   behind a debug-only flag so it cannot reach a release/beta build.
2. **Meter scale-label spacing (IN faders).** The `-120 / -0.7 dB / -11.0 dB`
   labels at the bottom of the IN faders sit tight against the fader fill. Nudge
   spacing for pixel-clean layout. Purely cosmetic.
3. **Already tracked separately (chip):** delete the stale orphaned
   `melechdsp-hq/shared/mdsp_ui/src/ui/theme/ThemeTokens.generated.{h,cpp}`
   (unreferenced, divergent from the built copy). Optional to fold into Step A.

## Done criteria
- Canonical HQ committed + pushed; both products' submodule pointers reference `$HQ_SHA`.
- AnalyzerPro and MasterLimiter both commit cleanly with NO unrelated files included.
- Default-path builds (no `MELECHDSP_HQ_ROOT`) are green and show the new look.
