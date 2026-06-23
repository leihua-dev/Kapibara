NAME = kapibara
DPF_PATH = ../../../third_party/DPF
DPF_BUILD_DIR = ../../../build/dpf/$(NAME)
DPF_TARGET_DIR = ../../../build/dpf/bin
UI_TYPE = opengl3

FILES_DSP = \
	KapibaraPlugin.cpp \
	../../engine/SynthCore.cpp \
	../../engine/Voice.cpp \
	../../dsp/Generators.cpp \
	../../engine/MatrixEngine.cpp \
	../../dsp/Operators.cpp \
	../../dsp/Effects.cpp

FILES_UI = KapibaraUI.cpp

include $(DPF_PATH)/Makefile.plugins.mk

BUILD_CXX_FLAGS += -I$(CURDIR)/../..
BUILD_CXX_FLAGS := $(filter-out -std=gnu++11,$(BUILD_CXX_FLAGS)) -std=gnu++17

all: jack
