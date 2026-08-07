# Makefile for VMVirtIOGLEngine OpenGL Plugin Bundle
# Builds a loadable bundle for /System/Library/Extensions/

BUNDLE_NAME = VMVirtIOGLEngine
BUNDLE_DIR = $(BUNDLE_NAME).bundle
CONTENTS = $(BUNDLE_DIR)/Contents
MACOS = $(CONTENTS)/MacOS
RESOURCES = $(CONTENTS)/Resources

CC = g++
SDK = /Users/macbookpro/SDKs/MacOSX.platform/Developer/SDKs/MacOSX10.6.sdk
CFLAGS = -arch x86_64 -mmacosx-version-min=10.6 -isysroot $(SDK) -stdlib=libstdc++ -O2 -Wall -Wl,-no_function_starts -Wl,-no_data_in_code_info
LDFLAGS = -bundle -isysroot $(SDK) -stdlib=libstdc++ -Wl,-no_function_starts -Wl,-no_data_in_code_info -framework OpenGL -framework IOKit -framework CoreFoundation -framework ApplicationServices

SOURCES = VMVirtIOGLEngine.cpp
OBJECTS = $(SOURCES:.cpp=.o)
EXECUTABLE = $(MACOS)/$(BUNDLE_NAME)

.PHONY: all clean install

all: $(BUNDLE_DIR)

$(BUNDLE_DIR): $(EXECUTABLE) $(CONTENTS)/Info.plist
	@echo "✅ Bundle created: $(BUNDLE_DIR)"

$(CONTENTS):
	@mkdir -p $(CONTENTS)

$(MACOS): $(CONTENTS)
	@mkdir -p $(MACOS)

$(RESOURCES): $(CONTENTS)
	@mkdir -p $(RESOURCES)

$(EXECUTABLE): $(OBJECTS) | $(MACOS)
	@echo "🔗 Linking bundle..."
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $(OBJECTS)
	@echo "✅ Bundle executable created"

%.o: %.cpp
	@echo "🔨 Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

$(CONTENTS)/Info.plist: Info.plist | $(CONTENTS)
	@echo "📋 Copying Info.plist..."
	cp Info.plist $(CONTENTS)/Info.plist

clean:
	@echo "🧹 Cleaning..."
	rm -rf $(BUNDLE_DIR)
	rm -f $(OBJECTS)
	@echo "✅ Clean complete"

install: $(BUNDLE_DIR)
	@echo "📦 Installing bundle to /System/Library/Extensions/..."
	@echo "⚠️  Note: This requires SIP to be disabled and root access"
	@echo "Run manually: sudo cp -r $(BUNDLE_DIR) /System/Library/Extensions/"
	@echo "Then: sudo kextcache -system-prelinked-kernel"
	@echo "Then: sudo kextcache -system-caches"

# Create a test program to verify the plugin
test: test_glengine.c
	@echo "🔨 Building test program..."
	$(CC) -arch x86_64 -o test_glengine test_glengine.c -framework OpenGL -framework GLUT -framework ApplicationServices
	@echo "✅ Test program built: test_glengine"

.DEFAULT_GOAL := all
