# Include Path Fix — mdsp_ui Headers

## Problem

Build errors occurred because `#include <mdsp_ui/...>` headers couldn't be found. The include path wasn't properly configured in CMake.

## Solution Applied

### 1. Fixed CMakeLists.txt Include Path

**Before** (incorrect):
```cmake
target_include_directories(${PLUGIN_NAME} PUBLIC
    ${PROJECT_DIR}/third_party/melechdsp-hq/shared/mdsp_ui/include  # ❌ PROJECT_DIR is not a CMake variable
)
```

**After** (correct):
```cmake
target_include_directories(${PLUGIN_NAME}
    PRIVATE
        Source
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/melechdsp-hq/shared/mdsp_ui/include  # ✅ Correct CMake variable
)
```

### 2. Verified mdsp_ui PUBLIC Include Directories

The `mdsp_ui` library already sets PUBLIC include directories in its CMakeLists.txt:
```cmake
target_include_directories(mdsp_ui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
```

This means linking to `mdsp_ui` should automatically propagate the include path. The explicit addition in AnalyzerPro's CMakeLists.txt ensures it's available even if propagation doesn't work in edge cases.

## Files Using Angle Bracket Includes

All files correctly use angle bracket includes (39 files found):
- `Source/PluginEditor.h` - `#include <mdsp_ui/UiContext.h>`
- `Source/ui/MainView.h` - `#include <mdsp_ui/UiContext.h>`
- `Source/ui/layout/HeaderBar.h` - `#include <mdsp_ui/UiContext.h>`
- `Source/ui/layout/FooterBar.h` - `#include <mdsp_ui/UiContext.h>`
- `Source/ui/layout/ControlRail.h` - `#include <mdsp_ui/UiContext.h>`
- `Source/ui/layout/control_primitives/*.h` - All use `<mdsp_ui/UiContext.h>`
- `Source/ui/meters/*.h` - All use `<mdsp_ui/UiContext.h>`
- `Source/ui/analyzer/rta1_import/RTADisplay.*` - Multiple `<mdsp_ui/...>` includes
- And many more...

**All of these should now work correctly** with the fixed include path.

## Verification

After regenerating the Xcode project or rebuilding:
1. All `#include <mdsp_ui/...>` includes should resolve
2. No "file not found" errors for mdsp_ui headers
3. Build should succeed

## If Errors Persist

If you still get "file not found" errors after this fix:

1. **Regenerate Xcode project**:
   ```bash
   cmake -S . -B build-xcode -G Xcode -DJUCE_PATH=$JUCE_PATH
   ```

2. **Check the specific error**: The error message will show which header is missing. Common issues:
   - Missing header in mdsp_ui (file doesn't exist)
   - Include path not propagated to Xcode (regenerate project)
   - Circular dependency (check include order)

3. **Verify include path in Xcode**:
   - Open Xcode project
   - Check Build Settings → Header Search Paths
   - Should include: `$(PROJECT_DIR)/third_party/melechdsp-hq/shared/mdsp_ui/include`

## Next Steps

If a specific header still fails, provide:
- The exact error message
- The file that's failing
- The include line that's failing

Then we can either:
- Fix the missing header
- Adjust the include path
- Fix the include statement

---

**Status**: ✅ CMakeLists.txt fixed, all angle bracket includes should work
