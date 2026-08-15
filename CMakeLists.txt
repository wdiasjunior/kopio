cmake_minimum_required(VERSION 3.21)
project(kopio VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Gui Concurrent)
qt_standard_project_setup()

enable_testing()

# ---- pure C analysis core (no Qt, no POSIX, no paths) ----------------------
add_library(kopio_core STATIC
    src/core/kop_meta.c
    src/core/kop_hash.c
    src/core/kop_metrics.c
    src/core/kop_classify.c
)
target_include_directories(kopio_core PUBLIC src/core)
if(NOT MSVC)
    target_compile_options(kopio_core PRIVATE -Wall -Wextra)
endif()

# ---- shared C++ scan pipeline (QtGui only) ---------------------------------
add_library(kopio_pipeline STATIC
    src/pipeline/Pipeline.cpp
)
target_include_directories(kopio_pipeline PUBLIC src/pipeline third_party/xxhash)
target_link_libraries(kopio_pipeline PUBLIC kopio_core Qt6::Gui)

# ---- headless CLI ----------------------------------------------------------
qt_add_executable(kopio-scan
    src/cli/kopio-scan.cpp
)
target_link_libraries(kopio-scan PRIVATE kopio_pipeline)

# ---- GUI app ---------------------------------------------------------------
qt_add_executable(kopio
    src/app/main.cpp
    src/app/MainWindow.cpp
    src/app/GreetingPage.cpp
    src/app/ScanPage.cpp
    src/app/ResultsPage.cpp
    src/app/ResultsModel.cpp
    src/app/ResultsDelegate.cpp
    src/app/PreviewDialog.cpp
    src/app/ScanController.cpp
    src/app/TrashOps.cpp
)
target_include_directories(kopio PRIVATE src/app)
target_link_libraries(kopio PRIVATE kopio_pipeline Qt6::Widgets Qt6::Concurrent)

# ---- tests -----------------------------------------------------------------
add_executable(test_core tests/test_core.c)
target_link_libraries(test_core PRIVATE kopio_core)
add_test(NAME core COMMAND test_core)
