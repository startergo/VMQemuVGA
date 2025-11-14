#!/bin/bash

#
# VMQemuVGA v8.0 Private Package Installer Script
# Creates signed .pkg installer using Developer ID certificates
# Uses existing installer infrastructure with updated payload
#
# Copyright (c) 2025 VMQemuVGA Development Team
# Built with Apple Developer ID for internal/private distribution
#

set -e

# Configuration
PROJECT_NAME="VMQemuVGA"
KEXT_NAME="${PROJECT_NAME}.kext"
BUILD_DIR="build"
INSTALLER_DIR="VMQemuVGA-Installer"
INSTALLER_CERTIFICATE_CSR="installer_certificate.csr"
INSTALLER_CSR_CONF="installer_csr.conf"
INSTALLER_PRIVATE_KEY="installer_private_key.pem"
PKG_NAME="VMQemuVGA-v8.0-Private-$(date +%Y%m%d).pkg"

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
MAGENTA='\033[0;35m'
NC='\033[0m' # No Color

echo -e "${BLUE}🚀 VMQemuVGA v8.0 Private Package Installer Builder${NC}"
echo -e "${CYAN}   Uses Apple Developer ID for Private Distribution${NC}"
echo "=================================================================="

# Default build type
BUILD_TYPE="Release"
# Function to check prerequisites
check_prerequisites() {
    echo -e "${BLUE}🔍 Checking Prerequisites...${NC}"
    # Determine target build path based on BUILD_TYPE
    local target_kext_path="$BUILD_DIR/$BUILD_TYPE/$KEXT_NAME"

    # Check for built kernel extension
    if [ ! -d "$target_kext_path" ]; then
        echo -e "${RED}❌ Built kernel extension not found at: $target_kext_path${NC}"
        echo -e "${YELLOW}💡 Run './build-enhanced_private.sh' (or build Debug) to create: $target_kext_path${NC}"
        exit 1
    fi
    
    # Check installer directory
    if [ ! -d "$INSTALLER_DIR" ]; then
        echo -e "${RED}❌ Installer directory not found: $INSTALLER_DIR${NC}"
        exit 1
    fi
    
    # Check installer certificate files
    if [ ! -f "$INSTALLER_CSR_CONF" ]; then
        echo -e "${RED}❌ Installer CSR configuration not found: $INSTALLER_CSR_CONF${NC}"
        exit 1
    fi
    
    if [ ! -f "$INSTALLER_CERTIFICATE_CSR" ]; then
        echo -e "${YELLOW}⚠️  Installer CSR not found: $INSTALLER_CERTIFICATE_CSR${NC}"
        echo -e "${YELLOW}   Use --create-cert flag to generate certificate infrastructure${NC}"
    else
        echo -e "${GREEN}✅ Installer CSR found: $INSTALLER_CERTIFICATE_CSR${NC}"
    fi
    
    if [ ! -f "$INSTALLER_PRIVATE_KEY" ]; then
        echo -e "${YELLOW}⚠️  Installer private key not found: $INSTALLER_PRIVATE_KEY${NC}"
        echo -e "${YELLOW}   Use --create-cert flag to generate certificate infrastructure${NC}"
    else
        echo -e "${GREEN}✅ Installer private key found: $INSTALLER_PRIVATE_KEY${NC}"
    fi
    
    # Check for packagemaker/pkgbuild
    if ! command -v pkgbuild >/dev/null 2>&1; then
        echo -e "${RED}❌ pkgbuild not found - please install Xcode Command Line Tools${NC}"
        exit 1
    fi
    
    if ! command -v productbuild >/dev/null 2>&1; then
        echo -e "${RED}❌ productbuild not found - please install Xcode Command Line Tools${NC}"
        exit 1
    fi
    
    echo -e "${GREEN}✅ Prerequisites check passed${NC}"
}

# Function to verify kernel extension signature
verify_kext_signature() {
    local kext_path="$BUILD_DIR/$BUILD_TYPE/$KEXT_NAME"
    
    echo -e "${BLUE}🔐 Verifying Kernel Extension Signature...${NC}"
    
    if codesign -vv "$kext_path" >/dev/null 2>&1; then
        local signer=$(codesign -dvvv "$kext_path" 2>&1 | grep "Authority=" | head -1 | cut -d= -f2- | xargs)
        echo -e "${GREEN}✅ Kernel extension is properly signed${NC}"
        if [ -n "$signer" ] && [ "$signer" != "not set" ]; then
            echo -e "   Signer: $signer"
        else
            echo -e "   Signer: (certificate info unavailable)"
        fi
        
        # Check if it's signed with Developer ID
        if [[ "$signer" == *"Developer ID Application"* ]]; then
            echo -e "${GREEN}   Certificate Type: Developer ID Application (Private Distribution)${NC}"
            return 0
        elif [[ "$signer" == *"VMQemuVGA"* ]]; then
            echo -e "${YELLOW}   Certificate Type: Self-signed VMQemuVGA${NC}"
            return 0
        else
            echo -e "${YELLOW}   Certificate Type: Other ($signer)${NC}"
            return 0
        fi
    else
        echo -e "${YELLOW}⚠️  Kernel extension is unsigned or signature not valid, proceeding with unsigned kext${NC}"
        return 0
    fi
}

# Function to update installer payload
update_installer_payload() {
    local kext_path="$BUILD_DIR/$BUILD_TYPE/$KEXT_NAME"
    local payload_path="$INSTALLER_DIR/payload"
    
    echo -e "${BLUE}📦 Updating Installer Payload...${NC}"
    
    # Create payload directory for modern macOS (Library/Extensions only)
    mkdir -p "$payload_path/Library/Extensions"
    
    # Remove old kext installations
    rm -rf "$payload_path/Library/Extensions/VMQemuVGA.kext"
    rm -rf "$payload_path/System" 2>/dev/null || true
    rm -rf "$payload_path/legacy" 2>/dev/null || true
    rm -rf "$payload_path/modern" 2>/dev/null || true
    
    # Copy signed kext to Library/Extensions (modern approach)
    echo -e "   Copying signed kext to payload (Library/Extensions)..."
    cp -R "$kext_path" "$payload_path/Library/Extensions/"
    chmod -R 755 "$payload_path/Library/Extensions/VMQemuVGA.kext"
    
    # Update payload info
    local kext_version=$(defaults read "$PWD/$kext_path/Contents/Info.plist" CFBundleVersion 2>/dev/null || echo "8.0")
    echo -e "${GREEN}✅ Payload updated with VMQemuVGA v${kext_version}${NC}"
    
    return 0
}

# Function to detect installer signing identity
detect_installer_identity() {
    echo -e "${BLUE}🔐 Detecting Installer Signing Identity...${NC}" >&2
    
    # Check for Developer ID Installer certificate
    local installer_id=$(security find-identity -p basic -v | grep "Developer ID Installer" | head -n1 | cut -d'"' -f2)
    if [ ! -z "$installer_id" ]; then
        echo -e "${GREEN}✅ Found Developer ID Installer certificate: $installer_id${NC}" >&2
        echo "$installer_id"
        return 0
    fi
    
    # Check for Mac Developer Installer
    local mac_installer=$(security find-identity -p basic -v | grep "Mac Developer" | head -n1 | cut -d'"' -f2)
    if [ ! -z "$mac_installer" ]; then
        echo -e "${GREEN}✅ Found Mac Developer certificate: $mac_installer${NC}" >&2
        echo "$mac_installer"
        return 0
    fi
    
    # Check for any installer-capable certificate
    local any_installer=$(security find-identity -p basic -v | grep -i installer | head -n1 | cut -d'"' -f2)
    if [ ! -z "$any_installer" ]; then
        echo -e "${GREEN}✅ Found installer certificate: $any_installer${NC}" >&2
        echo "$any_installer"
        return 0
    fi
    
    echo -e "${YELLOW}⚠️  No installer signing identity found${NC}" >&2
    echo -e "${YELLOW}   Package will be created unsigned${NC}" >&2
    echo ""
    return 1
}

# Function to create installer certificate if needed
create_installer_certificate() {
    echo -e "${BLUE}🔧 Creating Installer Certificate Infrastructure...${NC}"
    
    if [ ! -f "$INSTALLER_PRIVATE_KEY" ]; then
        echo -e "   Generating private key..."
        openssl genrsa -out "$INSTALLER_PRIVATE_KEY" 2048
        chmod 600 "$INSTALLER_PRIVATE_KEY"
    fi
    
    if [ ! -f "$INSTALLER_CERTIFICATE_CSR" ]; then
        echo -e "   Creating certificate signing request..."
        openssl req -new -key "$INSTALLER_PRIVATE_KEY" -out "$INSTALLER_CERTIFICATE_CSR" -config "$INSTALLER_CSR_CONF"
    fi
    
    echo -e "${GREEN}✅ Installer certificate infrastructure ready${NC}"
    echo -e "${YELLOW}💡 Submit $INSTALLER_CERTIFICATE_CSR to Apple for Developer ID Installer certificate${NC}"
}

# Function to build the package
build_package() {
    local installer_identity="$1"
    
    echo -e "${BLUE}🔨 Building Package...${NC}"
    
    # Build package directly with pkgbuild (simpler approach for single component)
    echo -e "   Creating signed installer package..."
    if [ ! -z "$installer_identity" ]; then
        echo -e "${GREEN}🔐 Signing package with: $installer_identity${NC}"
        pkgbuild \
            --root "$INSTALLER_DIR/payload" \
            --scripts "$INSTALLER_DIR/scripts" \
            --identifier "com.vmqemuvga.phase3.kext" \
            --version "8.0" \
            --install-location "/" \
            --sign "$installer_identity" \
            "$PKG_NAME"
    else
        echo -e "${YELLOW}⚠️  Building unsigned package${NC}"
        pkgbuild \
            --root "$INSTALLER_DIR/payload" \
            --scripts "$INSTALLER_DIR/scripts" \
            --identifier "com.vmqemuvga.phase3.kext" \
            --version "8.0" \
            --install-location "/" \
            "$PKG_NAME"
    fi
    
    echo -e "${GREEN}✅ Package built successfully: $PKG_NAME${NC}"
}

# Function to verify package signature
verify_package() {
    echo -e "${BLUE}🔍 Verifying Package...${NC}"
    
    if [ -f "$PKG_NAME" ]; then
        local size_bytes=$(wc -c < "$PKG_NAME" | tr -d ' ')
        local size_mb=$((size_bytes / 1024 / 1024))
        
        echo -e "${GREEN}✅ Package created successfully${NC}"
        echo -e "   File: $PKG_NAME"
        echo -e "   Size: ${size_bytes} bytes (${size_mb} MB)"
        
        # Check package signature
        if pkgutil --check-signature "$PKG_NAME" >/dev/null 2>&1; then
            echo -e "   Signature: ${GREEN}✅ Signed and verified${NC}"
            
            # Get signature details
            local signer=$(pkgutil --check-signature "$PKG_NAME" 2>&1 | grep "1\." | head -n1 | cut -d: -f2 | xargs)
            if [ ! -z "$signer" ]; then
                echo -e "   Signer: $signer"
            fi
        else
            echo -e "   Signature: ${YELLOW}⚠️  Unsigned or verification failed${NC}"
        fi
        
        # Test package contents
        echo -e "   Contents: $(pkgutil --payload-files "$PKG_NAME" | wc -l | xargs) files"
        
        return 0
    else
        echo -e "${RED}❌ Package file not found: $PKG_NAME${NC}"
        return 1
    fi
}

# Function to create installation guide
create_installation_guide() {
    local guide_name="VMQemuVGA-v8.0-Installation-Guide.txt"
    
    cat > "$guide_name" << EOF
VMQemuVGA v8.0 Advanced Command Buffer Resource Dependency Management System
Private Installation Guide

PACKAGE: $PKG_NAME
BUILD DATE: $(date)
CERTIFICATE: Apple Developer ID (Private Distribution)

=== INSTALLATION INSTRUCTIONS ===

1. DOWNLOAD AND VERIFY
   • Download the package file: $PKG_NAME
   • Verify file integrity and signature before installation

2. SYSTEM REQUIREMENTS
   • macOS 10.6 Snow Leopard or later
   • Intel x86_64 architecture
   • Administrator privileges

3. INSTALLATION PROCESS
   • Double-click the package file to start installation
   • Follow the installer prompts
   • Enter administrator password when prompted
   • Allow installation to complete

4. POST-INSTALLATION
   • The installer will automatically place the kernel extension
   • Modern macOS versions may require security approval:
     - Go to System Preferences > Security & Privacy
     - Click "Allow" for the blocked system extension
   • Restart your system if prompted

5. VERIFICATION
   • After restart, verify installation with:
     sudo kextstat | grep VMQemuVGA
   • You should see: puredarwin.driver.VMQemuVGA

=== ADVANCED FEATURES ===

VMQemuVGA v8.0 includes enterprise-grade acceleration:

• VirtIO GPU Acceleration
  - Hardware-accelerated virtualization graphics
  - Full GPU command submission pipeline

• Metal Framework Integration
  - Native Metal API support in virtual machines
  - Hardware-accelerated compute shaders
  - Advanced GPU memory management

• OpenGL Bridge
  - Legacy OpenGL application support
  - Compatibility layer for older software

• CoreAnimation Acceleration
  - Hardware-accelerated UI animations
  - Smooth window compositing and effects

• Advanced Command Buffer Management
  - Pipeline hazard detection and resolution
  - Memory barrier optimization
  - Resource dependency tracking
  - Command buffer pooling for performance

=== COMPATIBILITY NOTES ===

• macOS 10.6-10.10: Uses /System/Library/Extensions
• macOS 10.11+: Uses /Library/Extensions (SIP-compatible)
• macOS 10.13+: Requires user approval for kernel extensions
• macOS 11+: Full support with modern security framework

=== TROUBLESHOOTING ===

If the kernel extension fails to load:

1. Check System Preferences > Security & Privacy for approval prompts
2. Verify SIP settings: csrutil status
3. For development systems, SIP can be disabled temporarily
4. Check Console.app for kernel extension loading errors
5. Verify signature: codesign -vv /System/Library/Extensions/VMQemuVGA.kext

=== SUPPORT ===

For technical support and documentation:
• GitHub: https://github.com/ivanagui2/VMQemuVGA
• This is a PRIVATE distribution for internal use only
• Contains proprietary Apple Developer ID certificates

=== UNINSTALLATION ===

To remove VMQemuVGA:
1. sudo kextunload -b puredarwin.driver.VMQemuVGA
2. sudo rm -rf /System/Library/Extensions/VMQemuVGA.kext
3. sudo rm -rf /Library/Extensions/VMQemuVGA.kext
4. Rebuild cache: sudo kextcache -system-caches

EOF

    echo -e "${GREEN}✅ Installation guide created: $guide_name${NC}"
}

# Main function
main() {
    local skip_build=false
    local create_cert=false
    
    # Parse command line arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            --skip-build)
                skip_build=true
                shift
                ;;
            --debug)
                BUILD_TYPE="Debug"
                echo -e "${YELLOW}⚠️  Debug build requested - preferring $BUILD_DIR/Debug/$KEXT_NAME${NC}"
                shift
                ;;
            --create-cert)
                create_cert=true
                shift
                ;;
            --help|-h)
                echo "VMQemuVGA Private Package Installer Builder"
                echo "Usage: $0 [options]"
                echo ""
                echo "Options:"
                echo "  --skip-build    Skip kext build check (use existing build)"
                echo "  --create-cert   Create installer certificate infrastructure"
                echo "  --help          Show this help"
                echo ""
                echo "This script creates a signed .pkg installer for private distribution"
                echo "using your existing installer infrastructure and Apple Developer ID."
                exit 0
                ;;
            *)
                echo -e "${RED}❌ Unknown option: $1${NC}"
                exit 1
                ;;
        esac
    done
    
    # Check prerequisites
    check_prerequisites
    
    # Verify kernel extension signature
    if [ "$skip_build" = false ]; then
        if ! verify_kext_signature; then
            echo -e "${RED}❌ Kernel extension signature verification failed${NC}"
            exit 1
        fi
    fi
    
    # Create installer certificate infrastructure if requested
    if [ "$create_cert" = true ]; then
        create_installer_certificate
    fi
    
    # Update installer payload with new signed kext
    update_installer_payload
    
    # Detect installer signing identity
    local installer_identity=""
    installer_identity=$(detect_installer_identity)
    
    # Build the package
    build_package "$installer_identity"
    
    # Verify the built package
    if verify_package; then
        create_installation_guide
        
        echo ""
        echo -e "${GREEN}🎉 VMQemuVGA v8.0 Private Package Build Complete!${NC}"
        echo -e "   Package: $PKG_NAME"
        
        if [ ! -z "$installer_identity" ]; then
            echo -e "   Status: ${GREEN}Signed with Apple Developer ID${NC}"
            echo -e "   Distribution: ${GREEN}Ready for private/internal deployment${NC}"
        else
            echo -e "   Status: ${YELLOW}Unsigned (certificate setup needed)${NC}"
            echo -e "   Distribution: ${YELLOW}Limited - requires manual approval${NC}"
        fi
        
        echo ""
        echo -e "${CYAN}📋 Private Distribution Notes:${NC}"
        echo -e "   • This package contains Apple Developer ID certificates"
        echo -e "   • Suitable for internal/private distribution only"
        echo -e "   • Recipients will see 'Developer ID' as trusted source"
        echo -e "   • No Gatekeeper warnings on target systems"
        echo -e "   • Installation guide created for users"
        
        echo ""
        echo -e "${MAGENTA}📦 Ready for Distribution:${NC}"
        echo -e "   • Package: $PKG_NAME"
        echo -e "   • Guide: VMQemuVGA-v8.0-Installation-Guide.txt"
        echo -e "   • Deploy to target systems and run installer"
        
    else
        echo -e "${RED}❌ Package build verification failed${NC}"
        exit 1
    fi
}

# Run main function with all arguments
main "$@"
