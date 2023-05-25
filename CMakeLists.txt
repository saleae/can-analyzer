cmake_minimum_required (VERSION 3.11)
project(can_analyzer)

add_definitions( -DLOGIC2 )

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.14" CACHE STRING "Minimum supported MacOS version" FORCE)

# enable generation of compile_commands.json, helpful for IDEs to locate include files.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
