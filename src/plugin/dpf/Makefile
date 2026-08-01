NAME = kapibara
DPF_PATH = ../../../third_party/DPF
DPF_BUILD_DIR = ../../../build/dpf/$(NAME)
DPF_TARGET_DIR = ../../../build/dpf/bin
UI_TYPE = opengl3

FILES_DSP = \
	KapibaraPlugin.cpp \
	../../engine/SynthCore.cpp \
	../../engine/Voice.cpp \
	../../dsp/WavetableCore.cpp \
	../../dsp/GeneratorBank.cpp \
	../../dsp/BasicOscDsp.cpp \
	../../dsp/SampleNoiseDsp.cpp \
	../../engine/AdsrEnv.cpp \
	../../engine/ModMatrix.cpp \
	../../dsp/MasterEffects.cpp

FILES_UI = \
	ui/KapibaraUI.cpp \
	$(wildcard ui/sections/core/*.cpp) \
	$(wildcard ui/sections/menus/*.cpp) \
	$(wildcard ui/sections/presets/*.cpp) \
	$(wildcard ui/sections/fx/*.cpp) \
	$(wildcard ui/sections/router/*.cpp) \
	$(wildcard ui/sections/source/*.cpp) \
	$(wildcard ui/sections/pervoice/*.cpp) \
	$(wildcard ui/sections/sync/*.cpp) \
	$(wildcard ui/sections/page/*.cpp) \
	$(wildcard ui/sections/visuals/*.cpp) \
	$(wildcard ui/sections/input/*.cpp) \
	$(wildcard ui/sections/osc/partialbank/*.cpp) \
	$(wildcard ui/sections/osc/meta/*.cpp) \
	$(wildcard ui/sections/osc/basic/*.cpp) \
	$(wildcard ui/sections/osc/noise/*.cpp) \
	$(wildcard ui/sections/osc/shared/*.cpp) \
	$(wildcard ui/sections/matrix/*.cpp)

include $(DPF_PATH)/Makefile.plugins.mk

BUILD_CXX_FLAGS += -I$(CURDIR)/../..
BUILD_CXX_FLAGS := $(filter-out -std=gnu++11,$(BUILD_CXX_FLAGS)) -std=gnu++17

# Plugin formats, not just the standalone. DPF builds whichever of these the
# host toolchain supports; the standalone stays first so a plain `make` still
# produces something runnable.
all: jack lv2 vst3 clap
