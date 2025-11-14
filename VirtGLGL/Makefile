# Makefile for VirtGLGL.dylib - Userspace OpenGL library for VirtIO GPU

CC = clang++
CFLAGS = -O2 -Wall -Wextra -fPIC -mmacosx-version-min=10.6 -arch x86_64
LDFLAGS = -dynamiclib -framework IOKit -framework CoreFoundation -arch x86_64
INSTALL_NAME = @rpath/VirtGLGL.dylib

SOURCES = VirtGLGL.cpp VirtGLGLClient.cpp
OBJECTS = $(SOURCES:.cpp=.o)
TARGET = VirtGLGL.dylib

.PHONY: all clean install test

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) -install_name $(INSTALL_NAME) -o $@ $^
	@echo "✅ Built $@"
	@otool -L $@

%.o: %.cpp
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJECTS) $(TARGET)
	@echo "🧹 Cleaned build artifacts"

install: $(TARGET)
	@echo "📦 Installing VirtGLGL.dylib to /usr/local/lib"
	sudo cp $(TARGET) /usr/local/lib/
	@echo "✅ Installed to /usr/local/lib/VirtGLGL.dylib"

test: $(TARGET)
	@echo "🧪 Building test program..."
	$(CC) -arch x86_64 -o test_virtglgl test_virtglgl.cpp $(TARGET) -framework IOKit
	@echo "✅ Test program built: test_virtglgl"
	@echo "Run with: ./test_virtglgl"

help:
	@echo "VirtGLGL.dylib Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all      - Build VirtGLGL.dylib (default)"
	@echo "  clean    - Remove build artifacts"
	@echo "  install  - Install library to /usr/local/lib"
	@echo "  test     - Build test program"
	@echo "  help     - Show this help"
	@echo ""
	@echo "Usage:"
	@echo "  make          # Build library"
	@echo "  make test     # Build test program"
	@echo "  make install  # Install (requires sudo)"
