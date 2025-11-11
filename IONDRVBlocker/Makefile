# IONDRVVirtIOGPUBlocker Makefile
# Builds a standalone kext to prevent IONDRVFramebuffer from matching VirtIO GPU devices

KEXT_NAME = IONDRVVirtIOGPUBlocker
BUNDLE_ID = com.vmqemuvga.IONDRVBlocker
VERSION = 1.0.0

# Source files
SOURCES = IONDRVVirtIOGPUBlocker.cpp

# Build configuration
ARCH = x86_64
MACOSX_VERSION_MIN = 10.6
SDK_PATH = ../MacKernelSDK

# Compiler flags
CXXFLAGS = -Wall -Wextra -mmacosx-version-min=$(MACOSX_VERSION_MIN) \
           -arch $(ARCH) -fno-builtin -fno-stack-protector \
           -mkernel -nostdinc -I$(SDK_PATH)/Headers \
           -DKERNEL -DKERNEL_PRIVATE -DDRIVER_PRIVATE -DAPPLE -DNeXT

# Linker flags  
LDFLAGS = -Xlinker -kext -nostdlib -lkmodc++ -lkmod -lcc_kext

# Build directories
BUILD_DIR = build
KEXT_DIR = $(BUILD_DIR)/$(KEXT_NAME).kext
CONTENTS_DIR = $(KEXT_DIR)/Contents
MACOS_DIR = $(CONTENTS_DIR)/MacOS

.PHONY: all clean install

all: $(KEXT_DIR)

$(KEXT_DIR): $(SOURCES) Info.plist
	@echo "Building $(KEXT_NAME).kext..."
	@mkdir -p $(MACOS_DIR)
	@cp Info.plist $(CONTENTS_DIR)/
	@clang++ $(CXXFLAGS) $(LDFLAGS) -o $(MACOS_DIR)/$(KEXT_NAME) $(SOURCES)
	@echo "✅ $(KEXT_NAME).kext built successfully"

clean:
	@rm -rf $(BUILD_DIR)
	@echo "🧹 Cleaned build directory"

install: $(KEXT_DIR)
	@echo "Installing $(KEXT_NAME).kext..."
	@sudo cp -r $(KEXT_DIR) /Library/Extensions/
	@sudo chown -R root:wheel /Library/Extensions/$(KEXT_NAME).kext
	@sudo chmod -R 755 /Library/Extensions/$(KEXT_NAME).kext
	@echo "✅ $(KEXT_NAME).kext installed to /Library/Extensions/"
	@echo "⚠️  Run 'sudo kextcache -i /' to update kernel cache"

uninstall:
	@echo "Uninstalling $(KEXT_NAME).kext..."
	@sudo rm -rf /Library/Extensions/$(KEXT_NAME).kext
	@echo "✅ $(KEXT_NAME).kext removed"
	@echo "⚠️  Run 'sudo kextcache -i /' to update kernel cache"

load: install
	@echo "Loading $(KEXT_NAME).kext..."
	@sudo kextload /Library/Extensions/$(KEXT_NAME).kext

unload:
	@echo "Unloading $(KEXT_NAME).kext..."
	@sudo kextunload /Library/Extensions/$(KEXT_NAME).kext

test: $(KEXT_DIR)
	@echo "Testing $(KEXT_NAME).kext..."
	@kextutil -print-diagnostics $(KEXT_DIR)