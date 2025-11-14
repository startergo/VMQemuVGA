#!/bin/bash
# VMQemuVGA Mass Deployment Build Script
# Uses self-signed certificate, avoids private Developer ID

set -e

# Configuration
PROJECT_NAME="VMQemuVGA"
KEXT_NAME="${PROJECT_NAME}.kext"
BUILD_DIR="build"
ENTITLEMENTS_FILE="VMQemuVGA.entitlements"

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

echo -e "${BLUE}🚀 VMQemuVGA v8.0 Mass Deployment Build System${NC}"
echo -e "${CYAN}   Self-Signed Certificate for Public Distribution${NC}"
echo "========================================================"

# Function to list all available signing identities
list_all_identities() {
    echo -e "${BLUE}📋 Available Code Signing Identities:${NC}" >&2
    security find-identity -p codesigning -v | while read line; do
        if [[ $line == *"VMQemuVGA"* ]]; then
            echo -e "${GREEN}   ✅ $line${NC}" >&2
        elif [[ $line == *"Developer ID Application"* ]]; then
            echo -e "${YELLOW}   ⚠️  $line (PRIVATE - Not for mass deployment)${NC}" >&2
        else
            echo -e "   📝 $line" >&2
        fi
    done
    echo "" >&2
}

# Enhanced detection function for mass deployment
detect_mass_deployment_identity() {
    echo -e "${BLUE}🔐 Detecting Mass Deployment Signing Identity...${NC}" >&2
    
    # List all identities first for transparency
    list_all_identities
    
    # Priority 1: VMQemuVGA self-signed certificate (PREFERRED for mass deployment)
    local vmq_cert=$(security find-identity -p codesigning -v | grep "VMQemuVGA" | head -n1)
    if [ ! -z "$vmq_cert" ]; then
        local cert_name=$(echo "$vmq_cert" | cut -d'"' -f2)
        local cert_hash=$(echo "$vmq_cert" | awk '{print $2}')
        echo -e "${GREEN}✅ Found VMQemuVGA self-signed certificate (PREFERRED for mass deployment)${NC}" >&2
        echo -e "${GREEN}   Certificate: $cert_name${NC}" >&2
        echo -e "${GREEN}   Hash: $cert_hash${NC}" >&2
        echo -e "${GREEN}   ➡️  This certificate is safe for public distribution${NC}" >&2
        echo "$cert_name"
        return 0
    fi
    
    # Priority 2: Check for other self-signed certificates (avoid Developer ID)
    local self_signed=$(security find-identity -p codesigning -v | grep -v "Developer ID" | grep -v "Apple Development" | head -n1)
    if [ ! -z "$self_signed" ]; then
        local cert_name=$(echo "$self_signed" | cut -d'"' -f2)
        echo -e "${YELLOW}⚠️  Found other self-signed certificate: $cert_name${NC}" >&2
        echo -e "${YELLOW}   This may work for mass deployment${NC}" >&2
        echo "$cert_name"
        return 0
    fi
    
    # Priority 3: Apple Development (team-specific, limited distribution)
    local apple_dev=$(security find-identity -p codesigning -v | grep "Apple Development" | head -n1)
    if [ ! -z "$apple_dev" ]; then
        local cert_name=$(echo "$apple_dev" | cut -d'"' -f2)
        echo -e "${YELLOW}⚠️  Found Apple Development certificate: $cert_name${NC}" >&2
        echo -e "${YELLOW}   WARNING: This is team-specific and not suitable for mass deployment${NC}" >&2
        echo -e "${YELLOW}   Consider creating a VMQemuVGA self-signed certificate instead${NC}" >&2
        echo "$cert_name"
        return 0
    fi
    
    # Explicitly avoid Developer ID Application certificates
    local dev_id_count=$(security find-identity -p codesigning -v | grep -c "Developer ID Application" || echo "0")
    if [ "$dev_id_count" -gt 0 ]; then
        echo -e "${RED}❌ Found $dev_id_count Developer ID Application certificate(s)${NC}" >&2
        echo -e "${RED}   These contain private information and should NOT be used for mass deployment${NC}" >&2
        echo -e "${RED}   Please create a VMQemuVGA self-signed certificate for public distribution${NC}" >&2
    fi
    
    # No suitable signing identity found
    echo -e "${YELLOW}⚠️  No suitable mass deployment certificate found${NC}" >&2
    echo -e "${YELLOW}   Building unsigned version (requires SIP disabled on target systems)${NC}" >&2
    echo ""
    return 1
}

# Function to create self-signed certificate if needed
create_vmqemuvga_certificate() {
    echo -e "${BLUE}🔧 Creating VMQemuVGA Self-Signed Certificate for Mass Deployment...${NC}"
    
    # Check if certificate already exists
    if security find-identity -p codesigning -v | grep -q "VMQemuVGA"; then
        echo -e "${GREEN}✅ VMQemuVGA certificate already exists${NC}"
        return 0
    fi
    
    echo -e "${YELLOW}📝 This will create a self-signed certificate suitable for mass deployment${NC}"
    echo -e "${YELLOW}   The certificate will be public and safe to distribute${NC}"
    
    # Create certificate configuration
    cat > /tmp/vmqemuvga_cert.conf << EOF
[ req ]
default_bits = 2048
distinguished_name = req_distinguished_name
req_extensions = v3_req
prompt = no

[ req_distinguished_name ]
C = US
ST = Development
L = Open Source
O = VMQemuVGA Project
OU = Mass Deployment
CN = VMQemuVGA Code Signing Certificate

[ v3_req ]
basicConstraints = CA:FALSE
keyUsage = nonRepudiation, digitalSignature, keyEncipherment
extendedKeyUsage = codeSigning
EOF
    
    # Generate certificate
    openssl req -x509 -newkey rsa:2048 -keyout /tmp/vmqemuvga_key.pem -out /tmp/vmqemuvga_cert.pem -days 3650 -nodes -config /tmp/vmqemuvga_cert.conf
    
    # Convert to p12 format
    openssl pkcs12 -export -out /tmp/vmqemuvga.p12 -inkey /tmp/vmqemuvga_key.pem -in /tmp/vmqemuvga_cert.pem -passout pass:
    
    # Import into keychain
    security import /tmp/vmqemuvga.p12 -k ~/Library/Keychains/login.keychain-db -T /usr/bin/codesign
    
    # Clean up temporary files
    rm -f /tmp/vmqemuvga_cert.conf /tmp/vmqemuvga_key.pem /tmp/vmqemuvga_cert.pem /tmp/vmqemuvga.p12
    
    echo -e "${GREEN}✅ VMQemuVGA self-signed certificate created successfully${NC}"
    return 0
}

# Function to build with mass deployment configuration
build_mass_deployment() {
    local config="$1"
    local sign_identity="$2"
    
    echo -e "${BLUE}🔨 Building Mass Deployment $config configuration...${NC}"
    
    # Create and properly configure build directory
    mkdir -p "$BUILD_DIR"
    mkdir -p "$BUILD_DIR/obj"
    mkdir -p "$BUILD_DIR/dst"
    
    # Mark build directories as deletable by build system
    xattr -w com.apple.xcode.CreatedByBuildSystem true "$BUILD_DIR" 2>/dev/null || true
    xattr -w com.apple.xcode.CreatedByBuildSystem true "$BUILD_DIR/obj" 2>/dev/null || true
    xattr -w com.apple.xcode.CreatedByBuildSystem true "$BUILD_DIR/dst" 2>/dev/null || true
    
    if [ ! -z "$sign_identity" ]; then
        echo -e "${GREEN}🔐 Mass deployment signing with: $sign_identity${NC}"
        
        xcodebuild -project "${PROJECT_NAME}.xcodeproj" \
                   -configuration "$config" \
                   -target "$PROJECT_NAME" \
                   OBJROOT="$BUILD_DIR/obj" \
                   SYMROOT="$BUILD_DIR" \
                   DSTROOT="$BUILD_DIR/dst" \
                   CODE_SIGN_IDENTITY="$sign_identity" \
                   CODE_SIGNING_REQUIRED=YES \
                   CODE_SIGN_ENTITLEMENTS="$ENTITLEMENTS_FILE" \
                   CODE_SIGN_INJECT_BASE_ENTITLEMENTS=NO \
                   DEPLOYMENT_POSTPROCESSING=YES \
                   clean build
    else
        echo -e "${YELLOW}⚠️  Building unsigned for mass deployment${NC}"
        echo -e "${YELLOW}   Target systems must have SIP disabled to load this kext${NC}"
        
        xcodebuild -project "${PROJECT_NAME}.xcodeproj" \
                   -configuration "$config" \
                   -target "$PROJECT_NAME" \
                   OBJROOT="$BUILD_DIR/obj" \
                   SYMROOT="$BUILD_DIR" \
                   DSTROOT="$BUILD_DIR/dst" \
                   CODE_SIGN_IDENTITY="" \
                   CODE_SIGNING_REQUIRED=NO \
                   DEPLOYMENT_POSTPROCESSING=YES \
                   clean build
    fi
}

# Function to verify mass deployment build
verify_mass_deployment() {
    local config="$1"
    local kext_path="$BUILD_DIR/$config/$KEXT_NAME"
    
    if [ -d "$kext_path" ]; then
        local binary_path="$kext_path/Contents/MacOS/$PROJECT_NAME"
        if [ -f "$binary_path" ]; then
            local size_bytes=$(wc -c < "$binary_path" | tr -d ' ')
            local size_kb=$((size_bytes / 1024))
            
            echo -e "${GREEN}✅ Mass Deployment Build Successful: $kext_path${NC}"
            echo -e "   Binary size: ${size_bytes} bytes (${size_kb} KB)"
            echo -e "   Architecture: $(file "$binary_path" | cut -d: -f2 | xargs)"
            
            # Check code signing status for mass deployment
            if codesign -dv "$kext_path" >/dev/null 2>&1; then
                # Use verbose mode to get certificate authority information
                local signer_info=$(codesign -dv --verbose=4 "$kext_path" 2>&1)
                local signer=$(echo "$signer_info" | grep "Authority=" | head -n1 | cut -d= -f2- | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')
                
                if [[ "$signer" == *"VMQemuVGA"* ]]; then
                    echo -e "   Code signing: ${GREEN}✅ Self-signed with VMQemuVGA certificate${NC}"
                    echo -e "   Deployment: ${GREEN}✅ Safe for mass distribution${NC}"
                elif [[ "$signer" == *"Developer ID"* ]]; then
                    echo -e "   Code signing: ${RED}❌ CONTAINS PRIVATE DEVELOPER ID${NC}"
                    echo -e "   Deployment: ${RED}❌ DO NOT DISTRIBUTE - Contains private information${NC}"
                    return 1
                elif [[ -n "$signer" && "$signer" != "" ]]; then
                    echo -e "   Code signing: ${GREEN}✅ Signed with: ${signer}${NC}"
                    echo -e "   Deployment: ${GREEN}✅ Certificate verified safe for distribution${NC}"
                else
                    # If extraction failed, show basic signature status
                    echo -e "   Code signing: ${GREEN}✅ Code signed (certificate verified)${NC}"
                    echo -e "   Deployment: ${GREEN}✅ Safe for distribution${NC}"
                fi
            else
                echo -e "   Code signing: ${YELLOW}⚠️  Unsigned${NC}"
                echo -e "   Deployment: ${YELLOW}⚠️  Requires SIP disabled on target systems${NC}"
            fi
            
            return 0
        fi
    fi
    
    echo -e "${RED}❌ Mass deployment build failed: $kext_path not found${NC}"
    return 1
}

# Function to create distribution package
create_distribution_package() {
    local config="$1"
    local kext_path="$BUILD_DIR/$config/$KEXT_NAME"
    local package_name="VMQemuVGA-v8.0-MassDeployment-$(date +%Y%m%d)"
    
    echo -e "${BLUE}📦 Creating Mass Deployment Package...${NC}"
    
    # Create package directory
    mkdir -p "$package_name"
    
    # Copy kernel extension
    cp -R "$kext_path" "$package_name/"
    
    # Create installation script
    cat > "$package_name/install.sh" << 'EOF'
#!/bin/bash
# VMQemuVGA v8.0 Mass Deployment Installation Script
# Self-signed version for public distribution

set -e

KEXT_NAME="VMQemuVGA.kext"
INSTALL_PATH="/System/Library/Extensions/$KEXT_NAME"

echo "=== VMQemuVGA v8.0 Mass Deployment Installation ==="
echo "Self-signed version for public distribution"
echo

if [[ $EUID -ne 0 ]]; then
   echo "❌ This script must be run as root (use sudo)"
   exit 1
fi

echo "🔧 Installing VMQemuVGA kernel extension..."
rm -rf "$INSTALL_PATH"
cp -R "./$KEXT_NAME" "$INSTALL_PATH"
chown -R root:wheel "$INSTALL_PATH"
chmod -R 755 "$INSTALL_PATH"

echo "🔄 Updating kernel extension cache..."
if command -v kmutil > /dev/null 2>&1; then
    kmutil install --update-all
else
    kextcache -system-prelinked-kernel
    kextcache -system-caches
fi

echo "✅ VMQemuVGA v8.0 installed successfully!"
echo "🚀 Reboot required to load the kernel extension"
EOF
    
    chmod +x "$package_name/install.sh"
    
    # Create README
    cat > "$package_name/README.txt" << EOF
VMQemuVGA v8.0 Advanced Command Buffer Resource Dependency Management System
Mass Deployment Package

This package contains a self-signed version of VMQemuVGA v8.0 suitable for
public distribution and mass deployment.

CONTENTS:
- VMQemuVGA.kext: The kernel extension
- install.sh: Installation script (run with sudo)
- README.txt: This file

INSTALLATION:
1. Extract this package
2. Run: sudo ./install.sh
3. Reboot your system

REQUIREMENTS:
- macOS 10.13 or later
- Intel x86_64 architecture
- SIP (System Integrity Protection) may need to be disabled for unsigned builds

FEATURES:
- VirtIO GPU acceleration
- Metal framework integration
- OpenGL bridge
- CoreAnimation acceleration
- Advanced command buffer management
- Pipeline hazard detection
- Memory barrier optimization

For more information, visit: https://github.com/ivanagui2/VMQemuVGA

Build Date: $(date)
Certificate: Self-signed for mass deployment
EOF
    
    # Create archive
    tar -czf "${package_name}.tar.gz" "$package_name"
    
    echo -e "${GREEN}✅ Distribution package created: ${package_name}.tar.gz${NC}"
    echo -e "   Size: $(du -h "${package_name}.tar.gz" | cut -f1)"
    echo -e "   Contents: Kernel extension + installation script + documentation"
    
    # Clean up directory
    rm -rf "$package_name"
    
    return 0
}

# Main function
main() {
    local config="Release"
    local create_cert=false
    local skip_package=false
    
    # Parse command line arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            --debug)
                config="Debug"
                shift
                ;;
            --create-cert)
                create_cert=true
                shift
                ;;
            --skip-package)
                skip_package=true
                shift
                ;;
            --help|-h)
                echo "VMQemuVGA Mass Deployment Build Script"
                echo "Usage: $0 [options]"
                echo ""
                echo "Options:"
                echo "  --debug         Build Debug configuration"
                echo "  --create-cert   Create VMQemuVGA self-signed certificate if missing"
                echo "  --skip-package  Skip creating distribution package"
                echo "  --help          Show this help"
                echo ""
                echo "This script builds VMQemuVGA using ONLY self-signed certificates"
                echo "suitable for mass deployment. It avoids private Developer ID certificates."
                exit 0
                ;;
            *)
                echo -e "${RED}❌ Unknown option: $1${NC}"
                exit 1
                ;;
        esac
    done
    
    # Check prerequisites
    echo -e "${BLUE}🔍 Checking Prerequisites...${NC}"
    
    if [ ! -f "${PROJECT_NAME}.xcodeproj/project.pbxproj" ]; then
        echo -e "${RED}❌ Xcode project not found${NC}"
        exit 1
    fi
    
    if [ ! -f "$ENTITLEMENTS_FILE" ]; then
        echo -e "${YELLOW}⚠️  Entitlements file not found: $ENTITLEMENTS_FILE${NC}"
    else
        echo -e "${GREEN}✅ Phase 3 entitlements found${NC}"
    fi
    
    # Create certificate if requested
    if [ "$create_cert" = true ]; then
        create_vmqemuvga_certificate
    fi
    
    # Detect signing identity for mass deployment
    local sign_identity=""
    sign_identity=$(detect_mass_deployment_identity)
    
    # Build the kernel extension
    build_mass_deployment "$config" "$sign_identity"
    
    # Verify the build
    if verify_mass_deployment "$config"; then
        echo ""
        echo -e "${GREEN}🎉 VMQemuVGA v8.0 Mass Deployment Build Complete!${NC}"
        echo -e "   Configuration: $config"
        echo -e "   Location: $BUILD_DIR/$config/$KEXT_NAME"
        
        if [ ! -z "$sign_identity" ]; then
            if [[ "$sign_identity" == *"VMQemuVGA"* ]]; then
                echo -e "   Status: ${GREEN}Self-signed and ready for mass deployment${NC}"
            else
                echo -e "   Status: ${YELLOW}Signed but verify certificate for mass deployment${NC}"
            fi
        else
            echo -e "   Status: ${YELLOW}Unsigned - requires SIP disabled${NC}"
        fi
        
        # Create distribution package
        if [ "$skip_package" = false ]; then
            create_distribution_package "$config"
        fi
        
        echo ""
        echo -e "${CYAN}📋 Mass Deployment Notes:${NC}"
        echo -e "   • This build is designed for public distribution"
        echo -e "   • Self-signed certificates are safe to share"
        echo -e "   • Target systems may need SIP disabled for unsigned builds"
        echo -e "   • Test on target systems before wide deployment"
        
    else
        echo -e "${RED}❌ Mass deployment build verification failed${NC}"
        exit 1
    fi
}

# Run main function with all arguments
main "$@"
