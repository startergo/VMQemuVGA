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
    
    echo -e "${BLUE}🔨 Building $config configuration...${NC}"
    
    # Create and properly configure build directory
    mkdir -p "$BUILD_DIR"
    mkdir -p "$BUILD_DIR/obj"
    mkdir -p "$BUILD_DIR/dst"
    
    # Mark build directories as deletable by build system (critical fix)
    xattr -w com.apple.xcode.CreatedByBuildSystem true "$BUILD_DIR" 2>/dev/null || true
    xattr -w com.apple.xcode.CreatedByBuildSystem true "$BUILD_DIR/obj" 2>/dev/null || true
    xattr -w com.apple.xcode.CreatedByBuildSystem true "$BUILD_DIR/dst" 2>/dev/null || true
    
    if [ ! -z "$sign_identity" ]; then
        echo -e "${GREEN}🔐 Code signing with: $sign_identity${NC}"
        
        xcodebuild -project "${PROJECT_NAME}.xcodeproj" \
                   -configuration "$config" \
                   -target "$PROJECT_NAME" \
                   OBJROOT="$BUILD_DIR/obj" \
                   SYMROOT="$BUILD_DIR" \
                   DSTROOT="$BUILD_DIR/dst" \
                   CODE_SIGN_IDENTITY="$sign_identity" \
                   CODE_SIGNING_REQUIRED=YES \
                   CODE_SIGN_ENTITLEMENTS="$ENTITLEMENTS_FILE" \
                   clean build
    else
        echo -e "${YELLOW}⚠️  Building unsigned (development only)${NC}"
        
        xcodebuild -project "${PROJECT_NAME}.xcodeproj" \
                   -configuration "$config" \
                   -target "$PROJECT_NAME" \
                   OBJROOT="$BUILD_DIR/obj" \
                   SYMROOT="$BUILD_DIR" \
                   DSTROOT="$BUILD_DIR/dst" \
                   CODE_SIGN_IDENTITY="" \
                   CODE_SIGNING_REQUIRED=NO \
                   clean build
    fi
}

# Function to fix kext bundle structure
# Bundle structure verification and fix function 
# fix_bundle_structure() {
#     local config="$1"
#     local kext_path="$BUILD_DIR/$config/$KEXT_NAME"
    
#     echo -e "${BLUE}🔧 Fixing kext bundle structure...${NC}"
    
#     if [ -d "$kext_path" ]; then
#         # Check if we have the Contents structure outside the kext (wrong location)
#         if [ -d "$BUILD_DIR/$config/Contents" ] && [ ! -d "$kext_path/Contents" ]; then
#             echo -e "   Detected misplaced bundle structure, fixing..."
            
#             # Move Contents directory inside the kext bundle
#             mv "$BUILD_DIR/$config/Contents" "$kext_path/"
#             echo -e "   ✅ Moved Contents/ into kext bundle"
            
#             echo -e "${GREEN}✅ Bundle structure corrected${NC}"
#         elif [ -f "$kext_path/Info.plist" ] && [ -f "$kext_path/$PROJECT_NAME" ]; then
#             echo -e "   Detected flat structure, converting to Contents/MacOS..."
            
#             # Create proper Contents directory structure
#             mkdir -p "$kext_path/Contents/MacOS"
            
#             # Move Info.plist to correct location
#             if [ -f "$kext_path/Info.plist" ]; then
#                 mv "$kext_path/Info.plist" "$kext_path/Contents/"
#                 echo -e "   ✅ Moved Info.plist to Contents/"
#             fi
            
#             # Move executable to correct location
#             if [ -f "$kext_path/$PROJECT_NAME" ]; then
#                 mv "$kext_path/$PROJECT_NAME" "$kext_path/Contents/MacOS/"
#                 echo -e "   ✅ Moved executable to Contents/MacOS/"
#             fi
            
#             # Remove old signature directory since structure changed
#             if [ -d "$kext_path/_CodeSignature" ]; then
#                 rm -rf "$kext_path/_CodeSignature"
#                 echo -e "   ✅ Removed old signature for re-signing"
#             fi
            
#             echo -e "${GREEN}✅ Bundle structure corrected${NC}"
#         else
#             echo -e "   Bundle structure already correct"
#         fi
#     else
#         echo -e "${RED}❌ Kext not found at: $kext_path${NC}"
#         return 1
#     fi
# }

# Function to embed GLDriver bundle in kext (Snow Leopard CGL architecture)
embed_gldriver_bundle() {
    local config="$1"
    local kext_path="$BUILD_DIR/$config/$KEXT_NAME"
    local gldriver_source="GLPlugin/VMVirtIOGLEngine.bundle"
    local gldriver_dest="$kext_path/Contents/PlugIns"
    
    echo -e "${BLUE}📦 Embedding GLDriver bundle in kext...${NC}"
    
    if [ ! -d "$gldriver_source" ]; then
        echo -e "${YELLOW}⚠️  GLDriver bundle not found at: $gldriver_source${NC}"
        echo -e "   CGL may not discover the OpenGL renderer"
        return 1
    fi
    
    # Create PlugIns directory in kext
    mkdir -p "$gldriver_dest"
    
    # Copy GLDriver bundle into kext
    if cp -R "$gldriver_source" "$gldriver_dest/"; then
        echo -e "${GREEN}✅ GLDriver bundle embedded: $gldriver_dest/VMVirtIOGLEngine.bundle${NC}"
        
        # Verify bundle structure
        if [ -f "$gldriver_dest/VMVirtIOGLEngine.bundle/Contents/MacOS/VMVirtIOGLEngine" ]; then
            local bundle_size=$(wc -c < "$gldriver_dest/VMVirtIOGLEngine.bundle/Contents/MacOS/VMVirtIOGLEngine" | tr -d ' ')
            echo -e "   Bundle size: ${bundle_size} bytes"
        fi
        
        # Set proper permissions
        chmod -R 755 "$gldriver_dest/VMVirtIOGLEngine.bundle"
        
        return 0
    else
        echo -e "${RED}❌ Failed to embed GLDriver bundle${NC}"
        return 1
    fi
}

# Function to embed GLDriver bundle in kext (Snow Leopard architecture)
embed_gldriver_bundle() {
    local config="$1"
    local kext_path="$BUILD_DIR/$config/$KEXT_NAME"
    local gldriver_source="GLPlugin/VMVirtIOGLEngine.bundle"
    local gldriver_dest="$kext_path/Contents/Resources/VMVirtIOGLEngine.bundle"
    
    echo -e "${BLUE}📦 Embedding GLDriver bundle in kext...${NC}"
    
    if [ ! -d "$gldriver_source" ]; then
        echo -e "${YELLOW}⚠️  GLDriver bundle not found at $gldriver_source${NC}"
        echo -e "   Skipping bundle embedding - build GLPlugin first if needed${NC}"
        return 0
    fi
    
    if [ ! -d "$kext_path" ]; then
        echo -e "${RED}❌ Kext not found at $kext_path${NC}"
        return 1
    fi
    
    # Create Resources directory if it doesn't exist
    mkdir -p "$kext_path/Contents/Resources"
    
    # Remove old bundle if it exists
    if [ -d "$gldriver_dest" ]; then
        rm -rf "$gldriver_dest"
    fi
    
    # Copy the GLDriver bundle into the kext
    cp -R "$gldriver_source" "$gldriver_dest"
    
    if [ -d "$gldriver_dest" ]; then
        echo -e "${GREEN}✅ GLDriver bundle embedded successfully${NC}"
        echo -e "   Location: $gldriver_dest"
        
        # Verify the embedded bundle has the binary
        if [ -f "$gldriver_dest/Contents/MacOS/VMVirtIOGLEngine" ]; then
            local size_bytes=$(wc -c < "$gldriver_dest/Contents/MacOS/VMVirtIOGLEngine" | tr -d ' ')
            local size_kb=$((size_bytes / 1024))
            echo -e "   Binary size: ${size_bytes} bytes (${size_kb} KB)"
        else
            echo -e "${RED}❌ GLDriver binary not found in embedded bundle${NC}"
            return 1
        fi
    else
        echo -e "${RED}❌ Failed to embed GLDriver bundle${NC}"
        return 1
    fi
    
    return 0
}

# Function to re-sign kext after structure fix
re_sign_kext() {
    local config="$1"
    local sign_identity="$2"
    local kext_path="$BUILD_DIR/$config/$KEXT_NAME"
    echo -e "${BLUE}🔐 Re-signing kext with embedded GLDriver...${NC}"
    
    if [ -d "$kext_path" ] && [ ! -z "$sign_identity" ]; then
        if codesign --force --sign "$sign_identity" "$kext_path" >/dev/null 2>&1; then
            echo -e "${GREEN}✅ Kext re-signed successfully${NC}"
        else
            echo -e "${YELLOW}⚠️  Re-signing failed, but kext may still work${NC}"
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
            --help|-h)
                echo "Usage: $0 [--debug] [--unsigned] [--help]"
                echo "  --debug     Build Debug configuration"
                echo "  --unsigned  Force unsigned build"
                echo "  --help      Show this help"
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
    build_kext "$config" "$sign_identity"
    
    # # Fix bundle structure if needed
    # fix_bundle_structure "$config"
    
    # Embed GLDriver bundle in kext (Snow Leopard architecture)
    embed_gldriver_bundle "$config"
    
    # Re-sign after structure fix and bundle embedding
    if [ ! -z "$sign_identity" ]; then
        re_sign_kext "$config" "$sign_identity"
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
