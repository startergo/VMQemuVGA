#!/bin/bash

# VMQemuVGA 3D Acceleration Driver
# Build and Deployment Script
# 
# This script builds the enhanced 3D acceleration driver and provides
# deployment options for development and production environments.

set -e  # Exit on any error

# Script configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR"
BUILD_DIR="$PROJECT_DIR/build"
KEXT_NAME="VMQemuVGA.kext"
TEST_APP="vm3dtest"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Logging functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to check prerequisites
check_prerequisites() {
    log_info "Checking build prerequisites..."
    
    # Check for Xcode
    if ! command -v xcodebuild &> /dev/null; then
        log_error "Xcode command line tools not found. Please install Xcode."
        exit 1
    fi
    
    # Check and initialize MacKernelSDK submodule
    if [ ! -d "$PROJECT_DIR/MacKernelSDK" ] || [ ! -f "$PROJECT_DIR/MacKernelSDK/Headers/IOKit/IOKitLib.h" ]; then
        log_info "MacKernelSDK submodule not found or incomplete. Initializing..."
        if [ -d "$PROJECT_DIR/.git" ]; then
            git submodule update --init --recursive
            if [ $? -ne 0 ]; then
                log_error "Failed to initialize MacKernelSDK submodule"
                log_info "Please manually run: git submodule update --init --recursive"
                exit 1
            fi
            log_success "MacKernelSDK submodule initialized successfully"
        else
            log_error "This appears to be a source archive without git. Please download MacKernelSDK manually:"
            log_error "git clone https://github.com/acidanthera/MacKernelSDK.git"
            exit 1
        fi
    else
        log_success "MacKernelSDK submodule found"
    fi
    
    # Check for IOKit headers
    if [ ! -d "/System/Library/Frameworks/IOKit.framework" ]; then
        log_error "IOKit framework not found. This requires macOS development environment."
        exit 1
    fi
    
    # Check macOS version
    macos_version=$(sw_vers -productVersion)
    log_info "macOS version: $macos_version"
    
    # Check SIP status
    sip_status=$(csrutil status 2>/dev/null || echo "unknown")
    log_info "SIP status: $sip_status"
    if [[ $sip_status == *"enabled"* ]]; then
        log_warning "System Integrity Protection is enabled. You may need to disable it for kext loading."
    fi
    
    log_success "Prerequisites check completed"
}

# Function to build the kernel extension
build_kext() {
    log_info "Building VMQemuVGA kernel extension..."
    
    # Create build directory
    mkdir -p "$BUILD_DIR"
    
    # Create build directory structure
    mkdir -p "$BUILD_DIR"
    mkdir -p "$BUILD_DIR/obj"
    mkdir -p "$BUILD_DIR/dst"
    
    # Mark build directory as deletable by build system
    xattr -w com.apple.xcode.CreatedByBuildSystem true "$BUILD_DIR" 2>/dev/null || true
    
    # Build using xcodebuild
    cd "$PROJECT_DIR"
    
    log_info "Running xcodebuild..."
    xcodebuild -project VMQemuVGA.xcodeproj \
               -configuration Release \
               -target VMQemuVGA \
               OBJROOT="$BUILD_DIR/obj" \
               SYMROOT="$BUILD_DIR" \
               DSTROOT="$BUILD_DIR/dst" \
               HEADER_SEARCH_PATHS="$PROJECT_DIR/MacKernelSDK/Headers" \
               LIBRARY_SEARCH_PATHS="$PROJECT_DIR/MacKernelSDK/Library/universal" \
               ARCHS=x86_64 \
               MACOSX_DEPLOYMENT_TARGET=10.6 \
               CODE_SIGN_IDENTITY="" \
               clean build
    
    if [ -d "$BUILD_DIR/Release/$KEXT_NAME" ]; then
        log_success "Kernel extension built successfully"
        
        # Display build information
        kext_version=$(defaults read "$BUILD_DIR/Release/$KEXT_NAME/Contents/Info" CFBundleVersion 2>/dev/null || echo "unknown")
        kext_identifier=$(defaults read "$BUILD_DIR/Release/$KEXT_NAME/Contents/Info" CFBundleIdentifier 2>/dev/null || echo "unknown")
        
        log_info "Built kernel extension:"
        log_info "  Version: $kext_version"
        log_info "  Identifier: $kext_identifier"
        log_info "  Path: $BUILD_DIR/Release/$KEXT_NAME"
        
        return 0
    else
        log_error "Kernel extension build failed"
        return 1
    fi
}

# Function to build test application
build_test() {
    log_info "Building 3D acceleration test application..."
    
    if [ -f "$PROJECT_DIR/test/VM3DTest.cpp" ]; then
        cd "$PROJECT_DIR/test"
        
        # Compile test application
        clang++ -o "$TEST_APP" VM3DTest.cpp \
                -framework IOKit \
                -framework CoreFoundation \
                -framework ApplicationServices \
                -std=c++11 \
                -O2
        
        if [ -f "$TEST_APP" ]; then
            log_success "Test application built successfully: test/$TEST_APP"
            
            # Make executable
            chmod +x "$TEST_APP"
            
            return 0
        else
            log_error "Test application build failed"
            return 1
        fi
    else
        log_warning "Test application source not found, skipping test build"
        return 0
    fi
}

# Function to build Phase 3 advanced test applications
build_phase3_tests() {
    log_info "Building Phase 3 advanced test applications..."
    
    # Check if test directory exists
    if [ ! -d "$PROJECT_DIR/test" ]; then
        log_error "Test directory not found"
        return 1
    fi
    
    cd "$PROJECT_DIR/test"
    
    # Build main VMTest application if it exists
    if [ -f "VMTest.cpp" ]; then
        log_info "Compiling VMTest application..."
        g++ -std=c++17 -framework IOKit -framework CoreFoundation -framework Metal -framework MetalKit \
            -framework CoreAnimation -framework QuartzCore -framework IOSurface \
            -I"$PROJECT_DIR/FB" -I"$PROJECT_DIR/qemu" -I"$PROJECT_DIR/phase2" -I"$PROJECT_DIR/phase3" \
            -DTESTING_MODE=1 -o vmtest \
            VMTest.cpp "../FB/VMQemuVGAClient.cpp" "../phase2/VMQemuVGAAccelerator.cpp"
        
        if [ $? -eq 0 ]; then
            log_success "VMTest application built successfully"
            chmod +x vmtest
        else
            log_error "VMTest application build failed"
        fi
    fi
    
    # Build Phase 3 comprehensive test application
    if [ -f "VMPhase3Test.cpp" ]; then
        log_info "Compiling Phase 3 comprehensive test application..."
        g++ -std=c++17 -framework IOKit -framework CoreFoundation -framework ApplicationServices \
            -I"$PROJECT_DIR/test" -I"$PROJECT_DIR/FB" -I"$PROJECT_DIR/qemu" -I"$PROJECT_DIR/phase2" -I"$PROJECT_DIR/phase3" \
            -DTESTING_MODE=1 -o vmphase3test \
            VMPhase3Test.cpp \
            "../phase3/VMPhase3Manager.cpp" "../phase3/VMMetalBridge.cpp" "../phase3/VMOpenGLBridge.cpp" \
            "../phase3/VMCoreAnimationAccelerator.cpp" "../phase3/VMIOSurfaceManager.cpp" \
            "../phase2/VMQemuVGAAccelerator.cpp" "../FB/VMQemuVGAClient.cpp"
        
        if [ $? -eq 0 ]; then
            log_success "Phase 3 test application built successfully"
            chmod +x vmphase3test
            return 0
        else
            log_error "Phase 3 test application build failed"
            return 1
        fi
    else
        log_warning "Phase 3 test source not found, skipping Phase 3 test build"
        return 0
    fi
}

# Function to validate the built kext
validate_kext() {
    log_info "Validating kernel extension..."
    
    kext_path="$BUILD_DIR/Release/$KEXT_NAME"
    
    if [ ! -d "$kext_path" ]; then
        log_error "Kernel extension not found at $kext_path"
        return 1
    fi
    
    # Check bundle structure
    if [ ! -f "$kext_path/Contents/Info.plist" ]; then
        log_error "Info.plist not found in kernel extension"
        return 1
    fi
    
    if [ ! -f "$kext_path/Contents/MacOS/VMQemuVGA" ]; then
        log_error "Kernel extension binary not found"
        return 1
    fi
    
    # Validate Info.plist
    plutil -lint "$kext_path/Contents/Info.plist" >/dev/null 2>&1
    if [ $? -eq 0 ]; then
        log_success "Info.plist validation passed"
    else
        log_error "Info.plist validation failed"
        return 1
    fi
    
    # Check code signing (if available)
    codesign -dv "$kext_path" >/dev/null 2>&1
    if [ $? -eq 0 ]; then
        log_success "Code signing verification passed"
    else
        log_warning "Code signing verification failed or not signed"
    fi
    
    log_success "Kernel extension validation completed"
    return 0
}

# Function to install the kernel extension
install_kext() {
    log_info "Installing kernel extension..."
    
    kext_path="$BUILD_DIR/Release/$KEXT_NAME"
    install_path="/Library/Extensions/$KEXT_NAME"
    
    # Check if running as root
    if [ "$EUID" -ne 0 ]; then
        log_error "Installation requires root privileges. Run with sudo."
        return 1
    fi
    
    # Backup existing kext if present
    if [ -d "$install_path" ]; then
        log_warning "Existing kernel extension found, creating backup..."
        cp -r "$install_path" "$install_path.backup.$(date +%Y%m%d_%H%M%S)"
        rm -rf "$install_path"
    fi
    
    # Copy new kext
    cp -r "$kext_path" "$install_path"
    
    # Set proper ownership and permissions
    chown -R root:wheel "$install_path"
    chmod -R 755 "$install_path"
    
    log_success "Kernel extension installed to $install_path"
    
    # Update kernel extension cache
    log_info "Updating kernel extension cache..."
    kextcache -update-volume / || log_warning "Failed to update kext cache"
    
    return 0
}

# Function to load the kernel extension
load_kext() {
    log_info "Loading kernel extension..."
    
    install_path="/Library/Extensions/$KEXT_NAME"
    
    # Check if running as root
    if [ "$EUID" -ne 0 ]; then
        log_error "Loading requires root privileges. Run with sudo."
        return 1
    fi
    
    # Unload if already loaded
    kextunload "$install_path" 2>/dev/null && log_info "Unloaded previous version"
    
    # Load the kernel extension
    kextload "$install_path"
    
    if [ $? -eq 0 ]; then
        log_success "Kernel extension loaded successfully"
        
        # Verify loading
        sleep 1
        if kextstat | grep -q "VMQemuVGA"; then
            log_success "Kernel extension is running"
            
            # Display status
            log_info "Kernel extension status:"
            kextstat | grep VMQemuVGA
            
            return 0
        else
            log_error "Kernel extension loaded but not found in kextstat"
            return 1
        fi
    else
        log_error "Failed to load kernel extension"
        return 1
    fi
}

# Function to run tests
run_tests() {
    log_info "Running 3D acceleration tests..."
    
    test_path="$PROJECT_DIR/test/$TEST_APP"
    
    if [ ! -f "$test_path" ]; then
        log_error "Test application not found. Run 'build test' first."
        return 1
    fi
    
    # Check if running as root for kext access
    if [ "$EUID" -ne 0 ]; then
        log_error "Tests require root privileges to access kernel extension. Run with sudo."
        return 1
    fi
    
    # Check if kext is loaded
    if ! kextstat | grep -q "VMQemuVGA"; then
        log_error "VMQemuVGA kernel extension is not loaded. Run 'load' first."
        return 1
    fi
    
    # Run tests
    cd "$PROJECT_DIR/test"
    ./"$TEST_APP"
    
    return $?
}

# Function to run Phase 3 advanced tests
run_phase3_tests() {
    log_info "Running Phase 3 advanced acceleration tests..."
    
    phase3_test_path="$PROJECT_DIR/test/vmphase3test"
    
    if [ ! -f "$phase3_test_path" ]; then
        log_error "Phase 3 test application not found. Run 'phase3-tests' first."
        return 1
    fi
    
    # Check if running as root for kext access
    if [ "$EUID" -ne 0 ]; then
        log_error "Tests require root privileges to access kernel extension. Run with sudo."
        return 1
    fi
    
    # Check if kext is loaded
    if ! kextstat | grep -q "VMQemuVGA"; then
        log_error "VMQemuVGA kernel extension is not loaded. Run 'load' first."
        return 1
    fi
    
    # Run Phase 3 tests
    cd "$PROJECT_DIR/test"
    ./vmphase3test
    result=$?
    
    if [ $result -eq 0 ]; then
        log_success "Phase 3 tests completed successfully"
    else
        log_error "Phase 3 tests failed with code $result"
    fi
    
    return $result
}

# Function to uninstall the kernel extension
uninstall_kext() {
    log_info "Uninstalling kernel extension..."
    
    install_path="/Library/Extensions/$KEXT_NAME"
    
    # Check if running as root
    if [ "$EUID" -ne 0 ]; then
        log_error "Uninstallation requires root privileges. Run with sudo."
        return 1
    fi
    
    # Unload if loaded
    kextunload "$install_path" 2>/dev/null && log_info "Unloaded kernel extension"
    
    # Remove files
    if [ -d "$install_path" ]; then
        rm -rf "$install_path"
        log_success "Kernel extension removed from $install_path"
        
        # Update cache
        kextcache -update-volume / || log_warning "Failed to update kext cache"
    else
        log_warning "Kernel extension not found at $install_path"
    fi
    
    return 0
}

# Function to clean build artifacts
clean() {
    log_info "Cleaning build artifacts..."
    
    # Remove build directory
    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
        log_success "Removed build directory"
    fi
    
    # Remove test executable
    test_path="$PROJECT_DIR/test/$TEST_APP"
    if [ -f "$test_path" ]; then
        rm -f "$test_path"
        log_success "Removed test executable"
    fi
    
    # Remove Phase 3 test executable
    phase3_test_path="$PROJECT_DIR/test/vmphase3test"
    if [ -f "$phase3_test_path" ]; then
        rm -f "$phase3_test_path"
        log_success "Removed Phase 3 test executable"
    fi
    
    # Clean Xcode derived data
    if [ -d ~/Library/Developer/Xcode/DerivedData ]; then
        find ~/Library/Developer/Xcode/DerivedData -name "*VMQemuVGA*" -type d -exec rm -rf {} + 2>/dev/null || true
        log_info "Cleaned Xcode derived data"
    fi
    
    log_success "Clean completed"
}

# Function to display usage information
usage() {
    echo "VMQemuVGA 3D Acceleration Driver - Build & Deployment Script"
    echo ""
    echo "Usage: $0 [command]"
    echo ""
    echo "Commands:"
    echo "  build         Build the kernel extension"
    echo "  test          Build the test application"  
    echo "  phase3-tests  Build Phase 3 advanced test applications"
    echo "  all           Build both kernel extension and test app"
    echo "  validate      Validate the built kernel extension"
    echo "  install       Install kernel extension (requires sudo)"
    echo "  load          Load kernel extension (requires sudo)"
    echo "  unload        Unload kernel extension (requires sudo)"
    echo "  uninstall     Uninstall kernel extension (requires sudo)"
    echo "  run-tests     Run 3D acceleration tests (requires sudo)"
    echo "  run-phase3-tests Run Phase 3 advanced tests (requires sudo)"
    echo "  clean         Clean build artifacts"
    echo "  status        Show kernel extension status"
    echo "  help          Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0 all                    # Build everything"
    echo "  sudo $0 install load      # Install and load kext"
    echo "  sudo $0 run-tests         # Run validation tests"
    echo "  sudo $0 unload uninstall  # Remove kext"
    echo ""
    echo "3D Acceleration Features:"
    echo "  ✓ VirtIO GPU paravirtualization"
    echo "  ✓ Hardware-accelerated 3D rendering"
    echo "  ✓ Metal framework compatibility"
    echo "  ✓ Shader management (GLSL/MSL)"
    echo "  ✓ Advanced texture operations"
    echo "  ✓ Command buffer optimization"
    echo ""
}

# Function to show status
show_status() {
    log_info "VMQemuVGA Driver Status"
    echo ""
    
    # Check if kext exists
    install_path="/Library/Extensions/$KEXT_NAME"
    if [ -d "$install_path" ]; then
        echo "📦 Installation: Installed at $install_path"
        
        # Show version
        kext_version=$(defaults read "$install_path/Contents/Info" CFBundleVersion 2>/dev/null || echo "unknown")
        echo "📋 Version: $kext_version"
    else
        echo "📦 Installation: Not installed"
    fi
    
    # Check if loaded
    if kextstat | grep -q "VMQemuVGA"; then
        echo "🚀 Status: Loaded and running"
        echo ""
        echo "Kernel Extension Details:"
        kextstat | grep VMQemuVGA
    else
        echo "🚀 Status: Not loaded"
    fi
    
    # Check build artifacts
    if [ -d "$BUILD_DIR/Release/$KEXT_NAME" ]; then
        echo "🔨 Build: Available at $BUILD_DIR/Release/$KEXT_NAME"
    else
        echo "🔨 Build: No build artifacts found"
    fi
    
    # Check test app
    if [ -f "$PROJECT_DIR/test/$TEST_APP" ]; then
        echo "🧪 Tests: Test application available"
    else
        echo "🧪 Tests: Test application not built"
    fi
    
    echo ""
}

# Main script logic
main() {
    # Show header
    echo "🎮 VMQemuVGA 3D Acceleration Driver"
    echo "   Build & Deployment Script v2.0"
    echo "   Enhanced 3D Graphics for Virtual Machines"
    echo ""
    
    # Handle no arguments
    if [ $# -eq 0 ]; then
        usage
        exit 0
    fi
    
    # Process commands
    while [ $# -gt 0 ]; do
        case $1 in
            build)
                check_prerequisites
                build_kext
                ;;
            test)
                check_prerequisites
                build_test
                ;;
            phase3-tests)
                check_prerequisites
                build_phase3_tests
                ;;
            all)
                check_prerequisites
                build_kext
                build_test
                build_phase3_tests
                ;;
            validate)
                validate_kext
                ;;
            install)
                install_kext
                ;;
            load)
                load_kext
                ;;
            unload)
                kextunload "/Library/Extensions/$KEXT_NAME" 2>/dev/null && log_success "Unloaded kernel extension" || log_warning "Kernel extension not loaded"
                ;;
            uninstall)
                uninstall_kext
                ;;
            run-tests)
                run_tests
                ;;
            run-phase3-tests)
                run_phase3_tests
                ;;
            clean)
                clean
                ;;
            status)
                show_status
                ;;
            help|--help|-h)
                usage
                exit 0
                ;;
            *)
                log_error "Unknown command: $1"
                usage
                exit 1
                ;;
        esac
        shift
    done
    
    log_success "Script completed successfully!"
}

# Run main function
main "$@"
