1. avishaylidani@avishays-MacBook-Pro-MAIN AnalyzerPro % rg -n "Source/ui/rta/|\.\./rta/|\"RTAGeometry\.h\"|\"RTADisplayModel\.h\"|\"RTADisplayController\.h\"|\"RTADisplayRenderer\.h\"|\"RTACurveHelpers\.h\"|\"RTAEnums\.h\"" Source
empty.

2.avishaylidani@avishays-MacBook-Pro-MAIN AnalyzerPro % rg -n "#include\s*<mdsp_ui/rta/" Source/ui/analyzer/RTADisplay\.(h|cpp)
Source/ui/analyzer/RTADisplay.cpp
2:#include <mdsp_ui/rta/RTADisplayRenderer.h>
3:#include <mdsp_ui/rta/RTACurveHelpers.h>
4:#include <mdsp_ui/rta/RTADisplayModel.h>

Source/ui/analyzer/RTADisplay.h
6:#include <mdsp_ui/rta/RTAEnums.h>
7:#include <mdsp_ui/rta/RTAGeometry.h>
8:#include <mdsp_ui/rta/RTADisplayModel.h>
9:#include <mdsp_ui/rta/RTADisplayController.h>
10:#include <mdsp_ui/rta/RTADisplayRenderer.h>

3.
avishaylidani@avishays-MacBook-Pro-MAIN AnalyzerPro % rg -n "RTADisplayModel\.cpp|RTADisplayController\.cpp|RTADisplayRenderer\.cpp|RTAGeometry\.cpp|RTACurveHelpers\.cpp" -S .

avishaylidani@avishays-MacBook-Pro-MAIN melechdsp-hq % rg -n "RTADisplayModel\.cpp|RTADisplayController\.cpp|RTADisplayRenderer\.cpp|RTAGeometry\.cpp|RTACurveHelpers\.cpp" -S .
./shared/mdsp_ui/CMakeLists.txt
157:    src/rta/RTACurveHelpers.cpp
158:    src/rta/RTAGeometry.cpp
159:    src/rta/RTADisplayModel.cpp
160:    src/rta/RTADisplayController.cpp
161:    src/rta/RTADisplayRenderer.cpp

4.build succeed. 

5.
avishaylidani@avishays-MacBook-Pro-MAIN melechdsp-hq % rg -n "src/rta/RTACurveHelpers\.cpp|src/rta/RTAGeometry\.cpp|src/rta/RTADisplayModel\.cpp|src/rta/RTADisplayController\.cpp|src/rta/RTADisplayRenderer\.cpp" shared/mdsp_ui/CMakeLists.txt
157:    src/rta/RTACurveHelpers.cpp
158:    src/rta/RTAGeometry.cpp
159:    src/rta/RTADisplayModel.cpp
160:    src/rta/RTADisplayController.cpp
161:    src/rta/RTADisplayRenderer.cpp

6. 
avishaylidani@avishays-MacBook-Pro-MAIN melechdsp-hq % rg -n "include/mdsp_ui/rta" -S shared/mdsp_ui/CMakeLists.txt

empty .