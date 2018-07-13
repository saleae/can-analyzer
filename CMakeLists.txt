cmake_minimum_required (VERSION 3.11)
project(can_analyzer)

# custom CMake Modules are located in the cmake directory.
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

include(ExternalAnalyzerSDK)

set(SOURCES 
src/CanAnalyzer.cpp
src/CanAnalyzer.h
src/CanAnalyzerResults.cpp
src/CanAnalyzerResults.h
src/CanAnalyzerSettings.cpp
src/CanAnalyzerSettings.h
src/CanSimulationDataGenerator.cpp
src/CanSimulationDataGenerator.h
)

add_analyzer_plugin(can_analyzer SOURCES ${SOURCES})
