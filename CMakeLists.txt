cmake_minimum_required(VERSION 3.22)

# Universal binary su Mac (Intel + Apple Silicon)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "" FORCE)

# Windows: runtime C++ statico, così il plugin non richiede il "Visual C++ Redistributable"
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(GroovyRack VERSION 0.1.0)

# Attiva con -DGROOVY_COPY_AFTER_BUILD=ON per copiare il plugin nelle cartelle VST3/AU di sistema
# (su Windows serve avviare Visual Studio come amministratore)
option(GROOVY_COPY_AFTER_BUILD "Copia il plugin nelle cartelle di sistema dopo la build" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(GroovyRack
    COMPANY_NAME "Groovy"
    PLUGIN_MANUFACTURER_CODE Grvy
    PLUGIN_CODE Grck
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "G-POCKET"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD ${GROOVY_COPY_AFTER_BUILD}
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx Distortion)

target_sources(GroovyRack PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Presets.cpp)

# Grafica definitiva: se esiste Resources/skin.png viene incorporata nel plugin e usata come sfondo
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Resources/skin.png")
    juce_add_binary_data(GroovyRackAssets SOURCES Resources/skin.png)
    target_compile_definitions(GroovyRack PUBLIC GROOVY_HAS_SKIN=1)
    target_link_libraries(GroovyRack PRIVATE GroovyRackAssets)
endif()

# Suoni dell'easter egg (entrata, sigla, salto, morte): incorporati nel plugin
juce_add_binary_data(GroovyRackSfx
    HEADER_NAME GrvSfxData.h
    NAMESPACE GrvSfx
    SOURCES
        Resources/sfx/sfx_intro.wav
        Resources/sfx/sfx_theme.ogg
        Resources/sfx/sfx_jump.wav
        Resources/sfx/sfx_death.wav)
target_link_libraries(GroovyRack PRIVATE GroovyRackSfx)

option(GROOVY_LIMITED_EDITION "Versione privata con la skin CIRCUIT" OFF)
if(GROOVY_LIMITED_EDITION)
    target_compile_definitions(GroovyRack PUBLIC GROOVY_LIMITED_EDITION=1)
endif()

target_compile_definitions(GroovyRack PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(GroovyRack
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
