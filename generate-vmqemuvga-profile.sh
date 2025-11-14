#!/bin/bash
#
# VMQemuVGA Configuration Profile Generator
# Creates Apple MDM profile to pre-approve VMQemuVGA kernel extension
# Based on Richard Purves' kext whitelisting script
#

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}🔧 VMQemuVGA Configuration Profile Generator${NC}"
echo -e "${BLUE}==============================================${NC}"
echo

# Check if running as root
if [[ $EUID -ne 0 ]]; then
    echo -e "${RED}❌ This script must be run as root${NC}"
    echo "Please run: sudo $0"
    exit 1
fi

KEXT_PATH="/Library/Extensions/VMQemuVGA.kext"
PROFILE_NAME="VMQemuVGA-KextApproval.mobileconfig"
OUTPUT_DIR="$HOME/Desktop"

# Check if VMQemuVGA is installed
if [[ ! -d "$KEXT_PATH" ]]; then
    echo -e "${RED}❌ VMQemuVGA.kext not found at $KEXT_PATH${NC}"
    echo "Please install VMQemuVGA first, then run this script"
    exit 1
fi

echo -e "${BLUE}🔍 Analyzing VMQemuVGA kernel extension...${NC}"

# Get the Team Identifier from code signature
echo "Extracting Team ID from code signature..."
TEAM_ID=$(codesign -d -vvvv "$KEXT_PATH" 2>&1 | grep "Authority=Developer ID Application:" | cut -d"(" -f2 | tr -d ")" || echo "")

if [[ -z "$TEAM_ID" ]]; then
    echo -e "${RED}❌ Could not extract Team ID from code signature${NC}"
    echo "Checking if kext is signed..."
    if ! codesign -dv "$KEXT_PATH" >/dev/null 2>&1; then
        echo -e "${RED}❌ VMQemuVGA.kext is not code signed${NC}"
        exit 1
    else
        echo -e "${YELLOW}⚠️  Kext is signed but Team ID extraction failed${NC}"
        # Try alternative method
        TEAM_ID=$(codesign -dvvv "$KEXT_PATH" 2>&1 | grep "TeamIdentifier=" | cut -d= -f2 || echo "")
        if [[ -z "$TEAM_ID" ]]; then
            echo -e "${RED}❌ Unable to determine Team ID${NC}"
            exit 1
        fi
    fi
fi

# Get the CFBundleIdentifier from Info.plist
echo "Extracting Bundle ID from Info.plist..."
BUNDLE_ID=$(defaults read "$KEXT_PATH/Contents/Info.plist" CFBundleIdentifier 2>/dev/null || echo "")

if [[ -z "$BUNDLE_ID" ]]; then
    echo -e "${RED}❌ Could not extract Bundle ID from Info.plist${NC}"
    exit 1
fi

echo -e "${GREEN}✅ Team ID: $TEAM_ID${NC}"
echo -e "${GREEN}✅ Bundle ID: $BUNDLE_ID${NC}"
echo

# Generate unique profile UUID
PROFILE_UUID=$(uuidgen)
PAYLOAD_UUID=$(uuidgen)

echo -e "${BLUE}📝 Generating configuration profile...${NC}"

# Create the configuration profile
cat > "/tmp/$PROFILE_NAME" << EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>PayloadContent</key>
    <array>
        <dict>
            <key>AllowUserOverrides</key>
            <false/>
            <key>AllowedTeamIdentifiers</key>
            <array>
                <string>$TEAM_ID</string>
            </array>
            <key>AllowedKernelExtensions</key>
            <dict>
                <key>$TEAM_ID</key>
                <array>
                    <string>$BUNDLE_ID</string>
                </array>
            </dict>
            <key>PayloadDisplayName</key>
            <string>VMQemuVGA Kernel Extension Policy</string>
            <key>PayloadIdentifier</key>
            <string>com.vmqemuvga.kext.policy</string>
            <key>PayloadType</key>
            <string>com.apple.syspolicy.kernel-extension-policy</string>
            <key>PayloadUUID</key>
            <string>$PAYLOAD_UUID</string>
            <key>PayloadVersion</key>
            <integer>1</integer>
        </dict>
    </array>
    <key>PayloadDescription</key>
    <string>Allows VMQemuVGA kernel extension to load without user approval</string>
    <key>PayloadDisplayName</key>
    <string>VMQemuVGA Kernel Extension Approval</string>
    <key>PayloadIdentifier</key>
    <string>com.vmqemuvga.profile</string>
    <key>PayloadRemovalDisallowed</key>
    <false/>
    <key>PayloadType</key>
    <string>Configuration</string>
    <key>PayloadUUID</key>
    <string>$PROFILE_UUID</string>
    <key>PayloadVersion</key>
    <integer>1</integer>
</dict>
</plist>
EOF

# Format the profile nicely and move to output location
xmllint --format "/tmp/$PROFILE_NAME" > "$OUTPUT_DIR/$PROFILE_NAME"
rm "/tmp/$PROFILE_NAME"

echo -e "${GREEN}✅ Configuration profile created: $OUTPUT_DIR/$PROFILE_NAME${NC}"
echo

# Show the profile contents
echo -e "${BLUE}📋 Profile Contents:${NC}"
echo "Team ID: $TEAM_ID"
echo "Bundle ID: $BUNDLE_ID"
echo "Allow User Overrides: No"
echo

# Provide installation instructions
echo -e "${YELLOW}📋 Installation Instructions${NC}"
echo -e "${YELLOW}===========================${NC}"
echo
echo "1. Install the configuration profile:"
echo "   sudo profiles install -path '$OUTPUT_DIR/$PROFILE_NAME'"
echo
echo "2. Verify the profile is installed:"
echo "   profiles list | grep vmqemuvga"
echo
echo "3. Reboot the system:"
echo "   sudo reboot"
echo
echo "4. After reboot, VMQemuVGA should load automatically without approval popup"
echo "   Verify with: kextstat | grep VMQemuVGA"
echo
echo "5. To remove the profile later (if needed):"
echo "   sudo profiles remove -identifier 'com.vmqemuvga.profile'"
echo
echo -e "${GREEN}🎉 Profile generation completed!${NC}"
echo
echo -e "${BLUE}Note: This profile pre-approves VMQemuVGA and bypasses the user approval requirement.${NC}"
echo -e "${BLUE}This is especially useful for VM environments where popups may not appear.${NC}"

exit 0
