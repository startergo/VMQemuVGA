#!/bin/bash
# Enhanced VMQemuVGA Build Script with Flexible Code Signing
# Addresses GitHub Copilot AI feedback for better development workflow

set -e

# Configuration
PROJECT_NAME="VMQemuVGA"
KEXT_NAME="${PROJECT_NAME}.kext"
BUILD_DIR="build"
ENTITLEMENTS_FILE="VMQemuVGA.entitlements"

# Color output

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}🚀 VMQemuVGA Phase 3 Advanced Build System${NC}"
echo "=========================================="

# Function to detect available code signing identities
detect_signing_identity() {
    echo -e "${BLUE}🔐 Detecting Code Signing Identities...${NC}" >&2
    
    # Check for Developer ID Application (production certificate - highest priority)
    local dev_id=$(security find-identity -p codesigning -v | grep "Developer ID Application" | head -n1 | cut -d'"' -f2)
    if [ ! -z "$dev_id" ]; then
        echo -e "${GREEN}✅ Found Developer ID certificate: $dev_id${NC}" >&2
        echo "$dev_id"
        return 0
    fi
    
    # Check for Apple Development certificate
    local apple_dev=$(security find-identity -p codesigning -v | grep "Apple Development" | head -n1 | cut -d'"' -f2)
    if [ ! -z "$apple_dev" ]; then
        echo -e "${GREEN}✅ Found Apple Development certificate: $apple_dev${NC}" >&2
        echo "$apple_dev"
        return 0
    fi
    
    # Check for VMQemuVGA self-signed certificate (development fallback)
    if security find-identity -p codesigning -v | grep -q "VMQemuVGA"; then
        echo -e "${GREEN}✅ Found VMQemuVGA development certificate${NC}" >&2
        echo "VMQemuVGA Code Signing Certificate"
        return 0
    fi
    
    # No signing identity found
    echo -e "${YELLOW}⚠️  No code signing identity found - building unsigned${NC}" >&2
    echo ""
    return 1
}

# Function to build with proper signing configuration
build_kext() {
    local config="$1"
    local sign_identity="$2"
    local snow_leopard_mode="$3"
    
    echo -e "${BLUE}🔨 Building $config configuration...${NC}"
    
    # Create and properly configure build directory
    mkdir -p "$BUILD_DIR"
    mkdir -p "$BUILD_DIR/obj"
    mkdir -p "$BUILD_DIR/dst"
    
    # Mark build directories as deletable by build system (critical fix)
    xattr -w com.apple.xcode.CreatedByBuildSystem true "$BUILD_DIR" 2>/dev/null || true
    xattr -w com.apple.xcode.CreatedByBuildSystem true "$BUILD_DIR/obj" 2>/dev/null || true
    xattr -w com.apple.xcode.CreatedByBuildSystem true "$BUILD_DIR/dst" 2>/dev/null || true
    
    # Choose xcconfig file based on compatibility mode
    local xcconfig_override=""
    if [ "$snow_leopard_mode" = true ]; then
        if [ -f "VMQemuVGA_10_6.xcconfig" ]; then
            echo -e "${BLUE}🐆 Using Snow Leopard compatibility configuration${NC}"
            xcconfig_override="XCCONFIG_FILE=VMQemuVGA_10_6.xcconfig"
        else
            echo -e "${YELLOW}⚠️  Snow Leopard config not found, using default with code signing disabled${NC}"
        fi
    fi
    
    if [ ! -z "$sign_identity" ] && [ "$snow_leopard_mode" = false ]; then
        echo -e "${GREEN}🔐 Code signing with: $sign_identity${NC}"
        
        xcodebuild -project "${PROJECT_NAME}.xcodeproj" \
                   -configuration "$config" \
                   -target "$PROJECT_NAME" \
                   OBJROOT="$BUILD_DIR/obj" \
                   SYMROOT="$BUILD_DIR" \
                   DSTROOT="$BUILD_DIR/dst" \
                   VMQEMUVGA_CODE_SIGN_IDENTITY="$sign_identity" \
                   VMQEMUVGA_CODE_SIGNING_ALLOWED=YES \
                   VMQEMUVGA_CODE_SIGNING_REQUIRED=YES \
                   CODE_SIGN_ENTITLEMENTS="$ENTITLEMENTS_FILE" \
                   $xcconfig_override \
                   clean build
    else
        if [ "$snow_leopard_mode" = true ]; then
            echo -e "${BLUE}🐆 Building for Snow Leopard (code signing disabled)${NC}"
        else
            echo -e "${YELLOW}⚠️  Building unsigned (development only)${NC}"
        fi
        
        xcodebuild -project "${PROJECT_NAME}.xcodeproj" \
                   -configuration "$config" \
                   -target "$PROJECT_NAME" \
                   OBJROOT="$BUILD_DIR/obj" \
                   SYMROOT="$BUILD_DIR" \
                   DSTROOT="$BUILD_DIR/dst" \
                   VMQEMUVGA_CODE_SIGN_IDENTITY="" \
                   VMQEMUVGA_CODE_SIGNING_ALLOWED=NO \
                   VMQEMUVGA_CODE_SIGNING_REQUIRED=NO \
                   $xcconfig_override \
                   clean build
    fi
}

# Function to fix kext bundle structure
# Bundle structure verification and fix function 
fix_bundle_structure() {
#     local config="$1"
#     local kext_path="$BUILD_DIR/$config/$KEXT_NAME"
    
#     echo -e "${BLUE}🔧 Fixing kext bundle structure...${NC}"
    
    if [ -d "$kext_path" ]; then
        # Check if we have the Contents structure outside the kext (wrong location)
        if [ -d "$BUILD_DIR/$config/Contents" ] && [ ! -d "$kext_path/Contents" ]; then
            echo -e "   Detected misplaced bundle structure, fixing..."
            
            # Move Contents directory inside the kext bundle
            mv "$BUILD_DIR/$config/Contents" "$kext_path/"
            echo -e "   ✅ Moved Contents/ into kext bundle"
            
            echo -e "${GREEN}✅ Bundle structure corrected${NC}"
        elif [ -f "$kext_path/Info.plist" ] && [ -f "$kext_path/$PROJECT_NAME" ]; then
            echo -e "   Detected flat structure, converting to Contents/MacOS..."
            
            # Create proper Contents directory structure
            mkdir -p "$kext_path/Contents/MacOS"
            
            # Move Info.plist to correct location
            if [ -f "$kext_path/Info.plist" ]; then
                mv "$kext_path/Info.plist" "$kext_path/Contents/"
                echo -e "   ✅ Moved Info.plist to Contents/"
            fi
            
            # Move executable to correct location
            if [ -f "$kext_path/$PROJECT_NAME" ]; then
                mv "$kext_path/$PROJECT_NAME" "$kext_path/Contents/MacOS/"
                echo -e "   ✅ Moved executable to Contents/MacOS/"
            fi
            
            # Remove old signature directory since structure changed
            if [ -d "$kext_path/_CodeSignature" ]; then
                rm -rf "$kext_path/_CodeSignature"
                echo -e "   ✅ Removed old signature for re-signing"
            fi
            
            echo -e "${GREEN}✅ Bundle structure corrected${NC}"
        else
            echo -e "   Bundle structure already correct"
        fi
    else
        echo -e "${RED}❌ Kext not found at: $kext_path${NC}"
        return 1
    fi
}

# Function to re-sign kext when necessary
re_sign_kext_if_needed() {
    local config="$1"
    local sign_identity="$2"
    local structure_was_fixed="$3"
    local kext_path="$BUILD_DIR/$config/$KEXT_NAME"
    
    # Only re-sign if we have a signing identity and either:
    # 1. The bundle structure was fixed (invalidating original signature)
    # 2. The kext is not properly signed
    if [ -d "$kext_path" ] && [ ! -z "$sign_identity" ]; then
        local needs_resigning=false
        
        if [ "$structure_was_fixed" = "true" ]; then
            echo -e "${BLUE}🔐 Re-signing kext after structure correction...${NC}"
            needs_resigning=true
        else
            # Check if kext is properly signed
            if ! codesign --verify --verbose "$kext_path" >/dev/null 2>&1; then
                echo -e "${BLUE}🔐 Re-signing kext due to signature issues...${NC}"
                needs_resigning=true
            fi
        fi
        
        if [ "$needs_resigning" = "true" ]; then
            if codesign --force --sign "$sign_identity" "$kext_path" >/dev/null 2>&1; then
                echo -e "${GREEN}✅ Kext re-signed successfully${NC}"
            else
                echo -e "${YELLOW}⚠️  Re-signing failed, but kext may still work${NC}"
            fi
        else
            echo -e "${GREEN}✅ Kext signature is valid, no re-signing needed${NC}"
        fi
    fi
}

# Function to verify build result
verify_build() {
    local config="$1"
    local kext_path="$BUILD_DIR/$config/$KEXT_NAME"
    
    if [ -d "$kext_path" ]; then
        # Check for proper kext bundle structure (Contents/MacOS/)
        local binary_path="$kext_path/Contents/MacOS/$PROJECT_NAME"
        if [ -f "$binary_path" ]; then
            # Use portable method for file size (addresses Copilot feedback)
            local size_bytes=$(wc -c < "$binary_path" | tr -d ' ')
            local size_kb=$((size_bytes / 1024))
            
            echo -e "${GREEN}✅ Build successful: $kext_path${NC}"
            echo -e "   Binary size: ${size_bytes} bytes (${size_kb} KB)"
            echo -e "   Architecture: $(file "$binary_path" | cut -d: -f2 | xargs)"
            
            # Verify Info.plist is in correct location
            if [ -f "$kext_path/Contents/Info.plist" ]; then
                echo -e "   Bundle structure: ${GREEN}✅ Proper kext structure${NC}"

                # Personality integrity check. Catches stale-DerivedData and accidental-removal
                # regressions where the personality goes missing from the built Info.plist.
                # Without this, a missing personality is invisible until the kext fails to
                # match at boot — costing days of misdirected effort.
                local info_plist="$kext_path/Contents/Info.plist"
                local personalities_ok=1
                # VMVirtIOFramebufferPCI is the only personality that should match the
                # virtio-gpu PCI device on SL 10.6.8. The VMVirtIOGPU personality was
                # removed because it instantiated a second VMVirtIOGPU that raced
                # VMVirtIOFramebuffer's internal helper for the single device virtqueue.
                if ! grep -q "<key>VMVirtIOFramebufferPCI</key>" "$info_plist" || \
                   ! grep -q "<string>VMVirtIOFramebuffer</string>" "$info_plist"; then
                    echo -e "   ${RED}❌ Personality 'VMVirtIOFramebufferPCI' (IOClass=VMVirtIOFramebuffer) missing or malformed${NC}"
                    personalities_ok=0
                fi
                # Active (non-commented) VMVirtIOGPU personality must NOT be present.
                # The string appears in a comment block explaining the removal, so a
                # bare grep is insufficient — check for the active key form.
                if grep -E "^[[:space:]]*<key>VMVirtIOGPU</key>[[:space:]]*$" "$info_plist" >/dev/null; then
                    echo -e "   ${RED}❌ Active VMVirtIOGPU personality present — will conflict with VMVirtIOFramebuffer helper${NC}"
                    personalities_ok=0
                fi
                if [ "$personalities_ok" -eq 1 ]; then
                    echo -e "   Personalities: ${GREEN}✅ VMVirtIOFramebufferPCI present; VMVirtIOGPU personality correctly absent (avoids dual-instance virtqueue race)${NC}"
                else
                    echo -e "   ${YELLOW}   Personality regression suspected. Check Info-FB.plist.${NC}"
                    return 1
                fi
            else
                echo -e "   Bundle structure: ${RED}❌ Missing Contents/Info.plist${NC}"
            fi
            
            # Check code signing status
            if codesign -dv "$kext_path" >/dev/null 2>&1; then
                local signer=$(codesign -dvvv "$kext_path" 2>&1 | grep "Authority=" | head -1 | cut -d= -f2)
                if [ ! -z "$signer" ]; then
                    echo -e "   Code signing: ${GREEN}✅ Signed by:$signer${NC}"
                else
                    local team_id=$(codesign -dv "$kext_path" 2>&1 | grep "TeamIdentifier=" | cut -d= -f2)
                    echo -e "   Code signing: ${GREEN}✅ Signed by TeamID: $team_id${NC}"
                fi
            else
                echo -e "   Code signing: ${YELLOW}⚠️  Unsigned (development only)${NC}"
            fi
            
            return 0
        fi
    fi
    
    echo -e "${RED}❌ Build failed: $kext_path not found${NC}"
    return 1
}

# Main build process
main() {
    # Parse command line arguments
    local config="Release"
    local force_unsigned=false
    local snow_leopard_mode=false
    
    while [[ $# -gt 0 ]]; do
        case $1 in
            --debug)
                config="Debug"
                shift
                ;;
            --unsigned)
                force_unsigned=true
                shift
                ;;
            --snow-leopard)
                snow_leopard_mode=true
                force_unsigned=true
                echo -e "${BLUE}🐆 Snow Leopard compatibility mode enabled${NC}"
                echo -e "   Code signing: DISABLED (incompatible with 10.6 kernel)"
                echo -e "   Target: macOS 10.6+ x86_64"
                shift
                ;;
            --help|-h)
                echo "Usage: $0 [--debug] [--unsigned] [--snow-leopard] [--help]"
                echo "  --debug         Build Debug configuration"
                echo "  --unsigned      Force unsigned build"
                echo "  --snow-leopard  Build for Snow Leopard compatibility (disables code signing)"
                echo "  --help          Show this help"
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
        echo -e "   Phase 3 features may not work properly${NC}"
    else
        echo -e "${GREEN}✅ Phase 3 entitlements found${NC}"
    fi
    
    # Detect signing identity
    local sign_identity=""
    if [ "$force_unsigned" = false ]; then
        sign_identity=$(detect_signing_identity)
    else
        echo -e "${YELLOW}🔓 Forced unsigned build${NC}"
    fi
    
    # Build the kernel extension
    build_kext "$config" "$sign_identity" "$snow_leopard_mode"
    
    # # Fix bundle structure if needed
    # local structure_fixed="false"
    # if fix_bundle_structure "$config"; then
    #     structure_fixed="true"
    # fi
    
    # Re-sign only when necessary
    if [ ! -z "$sign_identity" ]; then
        re_sign_kext_if_needed "$config" "$sign_identity" "false"
    fi
    
    # Verify the build
    if verify_build "$config"; then
        echo ""
        echo -e "${GREEN}🎉 VMQemuVGA Phase 3 Build Complete!${NC}"
        echo -e "   Configuration: $config"
        echo -e "   Location: $BUILD_DIR/$config/$KEXT_NAME"
        
        if [ ! -z "$sign_identity" ]; then
            echo -e "   Status: ${GREEN}Signed and ready for deployment${NC}"
        else
            echo -e "   Status: ${YELLOW}Unsigned - development only${NC}"
            echo -e "   Note: SIP must be disabled to load unsigned kernel extensions"
        fi
    else
        echo -e "${RED}❌ Build verification failed${NC}"
        exit 1
    fi
}

# Run main function with all arguments
main "$@"
