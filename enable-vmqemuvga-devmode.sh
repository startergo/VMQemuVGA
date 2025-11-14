#!/bin/bash
#
# VMQemuVGA Developer Mode Installation Script
# Uses kext-dev-mode to bypass approval requirements
# Suitable for development and testing environments
#

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}🔧 VMQemuVGA Developer Mode Setup${NC}"
echo -e "${BLUE}=================================${NC}"
echo

# Check if running as root
if [[ $EUID -ne 0 ]]; then
    echo -e "${RED}❌ This script must be run as root${NC}"
    echo "Please run: sudo $0"
    exit 1
fi

# Get macOS version
MACOS_VERSION=$(sw_vers -productVersion)
echo -e "macOS Version: ${GREEN}$MACOS_VERSION${NC}"
echo

KEXT_PATH="/Library/Extensions/VMQemuVGA.kext"

# Check if VMQemuVGA is installed
if [[ ! -d "$KEXT_PATH" ]]; then
    echo -e "${RED}❌ VMQemuVGA.kext not found at $KEXT_PATH${NC}"
    echo "Please install VMQemuVGA first using simple-install-catalina.sh"
    exit 1
fi

echo -e "${BLUE}🔍 Checking current boot arguments...${NC}"
CURRENT_BOOT_ARGS=$(nvram boot-args 2>/dev/null | cut -d$'\t' -f2 || echo "")
echo "Current boot-args: $CURRENT_BOOT_ARGS"

# Check if kext-dev-mode is already enabled
if echo "$CURRENT_BOOT_ARGS" | grep -q "kext-dev-mode=1"; then
    echo -e "${GREEN}✅ kext-dev-mode is already enabled${NC}"
    KEXT_DEV_MODE_SET=true
else
    echo -e "${YELLOW}⚠️  kext-dev-mode is not enabled${NC}"
    KEXT_DEV_MODE_SET=false
fi

# Enable kext-dev-mode if not already set
if [[ "$KEXT_DEV_MODE_SET" == "false" ]]; then
    echo -e "${BLUE}🔧 Enabling kext-dev-mode...${NC}"
    
    # Preserve existing boot args and add kext-dev-mode
    if [[ -z "$CURRENT_BOOT_ARGS" ]]; then
        NEW_BOOT_ARGS="kext-dev-mode=1"
    else
        NEW_BOOT_ARGS="$CURRENT_BOOT_ARGS kext-dev-mode=1"
    fi
    
    echo "Setting boot-args to: $NEW_BOOT_ARGS"
    nvram boot-args="$NEW_BOOT_ARGS"
    
    echo -e "${GREEN}✅ kext-dev-mode enabled${NC}"
    REBOOT_REQUIRED=true
else
    REBOOT_REQUIRED=false
fi

# Check if SIP needs adjustment
echo -e "${BLUE}🔍 Checking System Integrity Protection (SIP)...${NC}"
SIP_STATUS=$(csrutil status 2>/dev/null || echo "unknown")
echo "SIP Status: $SIP_STATUS"

if echo "$SIP_STATUS" | grep -q "enabled"; then
    echo -e "${YELLOW}⚠️  SIP is fully enabled${NC}"
    echo -e "${YELLOW}   This may prevent kext loading even with kext-dev-mode${NC}"
    echo -e "${YELLOW}   Consider partially disabling SIP if kext still won't load${NC}"
    SIP_ISSUE=true
else
    echo -e "${GREEN}✅ SIP is disabled or partially disabled${NC}"
    SIP_ISSUE=false
fi

# Try to load the kext immediately if no reboot is required
if [[ "$REBOOT_REQUIRED" == "false" ]]; then
    echo -e "${BLUE}🔄 Attempting to load VMQemuVGA...${NC}"
    
    if kextload "$KEXT_PATH" 2>/dev/null; then
        echo -e "${GREEN}✅ VMQemuVGA loaded successfully!${NC}"
        KEXT_LOADED=true
    else
        echo -e "${YELLOW}⚠️  Failed to load VMQemuVGA${NC}"
        echo "This is normal - a reboot may still be required"
        KEXT_LOADED=false
    fi
else
    KEXT_LOADED=false
fi

# Provide final instructions
echo
echo -e "${BLUE}📋 Setup Summary${NC}"
echo -e "${BLUE}================${NC}"
echo -e "VMQemuVGA: ${GREEN}✅ Installed${NC}"
echo -e "kext-dev-mode: ${GREEN}✅ Enabled${NC}"

if [[ "$SIP_ISSUE" == "true" ]]; then
    echo -e "SIP Status: ${YELLOW}⚠️  May need adjustment${NC}"
else
    echo -e "SIP Status: ${GREEN}✅ Compatible${NC}"
fi

if [[ "$KEXT_LOADED" == "true" ]]; then
    echo -e "Current Status: ${GREEN}✅ VMQemuVGA is loaded and ready${NC}"
else
    echo -e "Current Status: ${YELLOW}⚠️  Reboot required${NC}"
fi

echo
echo -e "${YELLOW}📋 Next Steps${NC}"
echo -e "${YELLOW}============${NC}"
echo

if [[ "$REBOOT_REQUIRED" == "true" ]] || [[ "$KEXT_LOADED" == "false" ]]; then
    echo "1. REBOOT THE SYSTEM:"
    echo "   sudo reboot"
    echo
    echo "2. After reboot, verify VMQemuVGA is loaded:"
    echo "   kextstat | grep VMQemuVGA"
    echo
    if [[ "$SIP_ISSUE" == "true" ]]; then
        echo "3. If kext still won't load, you may need to adjust SIP:"
        echo "   - Boot into Recovery Mode (Command+R)"
        echo "   - Open Terminal from Utilities menu"
        echo "   - Run: csrutil enable --without kext"
        echo "   - Reboot normally"
        echo
    fi
else
    echo -e "${GREEN}✅ VMQemuVGA is ready to use!${NC}"
    echo "Verify with: kextstat | grep VMQemuVGA"
fi

echo "4. To disable kext-dev-mode later (more secure):"
echo "   sudo nvram boot-args=\"$(nvram boot-args 2>/dev/null | cut -d$'\t' -f2 | sed 's/kext-dev-mode=1//' | sed 's/  / /')\""
echo "   sudo reboot"
echo
echo -e "${BLUE}Note: kext-dev-mode allows unsigned/untrusted kexts to load.${NC}"
echo -e "${BLUE}This is less secure but suitable for development and testing.${NC}"

exit 0
