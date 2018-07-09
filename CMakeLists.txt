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

add_library(can_analyzer SHARED ${SOURCES})
target_link_libraries(can_analyzer PRIVATE Saleae::AnalyzerSDK)

if(MSVC)
    install(TARGETS can_analyzer RUNTIME DESTINATION ".")
else()
    install(TARGETS can_analyzer LIBRARY DESTINATION ".")
endif()

set_post_build_destination(can_analyzer)
