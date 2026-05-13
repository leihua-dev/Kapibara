cmake_minimum_required(VERSION 3.20)

project(JuceAdditiveSynth VERSION 0.3.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
)

FetchContent_MakeAvailable(JUCE)

juce_add_gui_app(JuceAdditiveSynth
    PRODUCT_NAME "MotifForge v0.3"
)

target_sources(JuceAdditiveSynth
    PRIVATE
        src/Main.cpp
        src/SynthCore.cpp
        src/SynthCore.h
        src/ui/SpectrumView.cpp
        src/ui/SpectrumView.h
        src/Voice.cpp
        src/Voice.h
        src/MatrixEngine.cpp
        src/MatrixEngine.h
        src/Operators.cpp
        src/Operators.h
        src/Effects.cpp
        src/Effects.h
        src/ResamplingEngine.cpp
        src/ResamplingEngine.h
        src/SamplePlaybackEngine.cpp
        src/SamplePlaybackEngine.h
        src/CompositionModel.h
        src/Generators.cpp
        src/Generators.h
        src/FunctionalSampleSource.cpp
        src/SpectralFrame.h
)

target_include_directories(JuceAdditiveSynth
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(JuceAdditiveSynth
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
)

# SpectralTimeline holds up to kMaxTimelineFrames (32) StaticSpectralFrame's, each
# carrying kMaxPartials (256) per-partial arrays. A single SpectralTimeline is
# ~460 kB, so any function that constructs one on the stack pushes us close to
# the 1 MB Windows default. Bump the linker stack to 8 MB as defense in depth.
if(MSVC)
    target_link_options(JuceAdditiveSynth PRIVATE /STACK:8388608)
else()
    target_link_options(JuceAdditiveSynth PRIVATE -Wl,-z,stacksize=8388608)
endif()

target_link_libraries(JuceAdditiveSynth
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
