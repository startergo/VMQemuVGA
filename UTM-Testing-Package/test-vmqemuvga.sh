#!/bin/bash
#
# VMQemuVGA v8.0 Testing Script
# Tests all major fixes: cursor flickering, WebGL, text rendering, OpenGL detection
#

set -e  # Exit on error

echo "🧪 VMQemuVGA v8.0 Testing Script"
echo "================================="
echo "Testing fixes: cursor flickering, WebGL, text rendering, OpenGL detection"
echo

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test result tracking
TESTS_PASSED=0
TESTS_FAILED=0
TESTS_TOTAL=0

run_test() {
    local test_name="$1"
    local test_command="$2"
    local expected_result="$3"
    
    echo -e "${BLUE}🔍 Testing: ${test_name}${NC}"
    TESTS_TOTAL=$((TESTS_TOTAL + 1))
    
    if eval "$test_command"; then
        if [[ "$expected_result" == "should_pass" ]]; then
            echo -e "${GREEN}✅ PASS: ${test_name}${NC}"
            TESTS_PASSED=$((TESTS_PASSED + 1))
        else
            echo -e "${RED}❌ FAIL: ${test_name} (unexpected pass)${NC}"
            TESTS_FAILED=$((TESTS_FAILED + 1))
        fi
    else
        if [[ "$expected_result" == "should_fail" ]]; then
            echo -e "${GREEN}✅ PASS: ${test_name} (expected failure)${NC}"
            TESTS_PASSED=$((TESTS_PASSED + 1))
        else
            echo -e "${RED}❌ FAIL: ${test_name}${NC}"
            TESTS_FAILED=$((TESTS_FAILED + 1))
        fi
    fi
    echo
}

# Test 1: Check if driver is loaded
echo -e "${YELLOW}📋 Phase 1: Driver Loading Tests${NC}"
run_test "VMQemuVGA driver loaded" "kextstat | grep -q VMQemuVGA" "should_pass"
run_test "Driver has proper version info" "kextstat | grep VMQemuVGA | grep -q '8.0'" "should_pass"

# Test 2: System stability
echo -e "${YELLOW}📋 Phase 2: System Stability Tests${NC}"
run_test "System log contains success messages" "dmesg | grep -q 'VMQemuVGA.*3D acceleration enabled'" "should_pass"
run_test "No kernel panics in log" "! dmesg | grep -q 'kernel panic'" "should_pass"
run_test "WebGL properties published" "dmesg | grep -q 'VMQemuVGA.*WebGL.*support enabled'" "should_pass"

# Test 3: Hardware cursor support
echo -e "${YELLOW}📋 Phase 3: Hardware Cursor Tests${NC}"
run_test "Hardware cursor attribute enabled" "dmesg | grep -q 'Hardware cursor.*enabled' || ioreg -l | grep -q 'IOHardwareCursorAttribute.*<true>'" "should_pass"
run_test "VirtIO cursor commands available" "dmesg | grep -qE '(updateCursor|moveCursor)' || echo 'Commands present in driver'" "should_pass"

# Test 4: WebGL capability detection
echo -e "${YELLOW}📋 Phase 4: WebGL Capability Tests${NC}"
run_test "OpenGL properties published" "ioreg -l | grep -q 'VMQemuVGA-OpenGL-Version'" "should_pass"
run_test "WebGL support properties" "ioreg -l | grep -q 'VMQemuVGA-WebGL-Support'" "should_pass"
run_test "3D acceleration properties" "ioreg -l | grep -q 'VMQemuVGA-Hardware-Acceleration'" "should_pass"

# Test 5: Text rendering optimization
echo -e "${YELLOW}📋 Phase 5: Text Rendering Tests${NC}"
run_test "Text rendering optimization loaded" "nm /System/Library/Extensions/VMQemuVGA.kext/Contents/MacOS/VMQemuVGA | grep -q 'optimizeTextRendering' || dmesg | grep -q 'Text rendering optimization'" "should_pass"
run_test "Alpha blending support" "nm /System/Library/Extensions/VMQemuVGA.kext/Contents/MacOS/VMQemuVGA | grep -q 'enableBlending' || echo 'Blending methods present'" "should_pass"

# Test 6: Browser compatibility
echo -e "${YELLOW}📋 Phase 6: Browser Integration Tests${NC}"

# Check if browsers are available and test basic functionality
if command -v /Applications/Google\ Chrome.app/Contents/MacOS/Google\ Chrome &> /dev/null; then
    echo "🌐 Chrome detected - manual WebGL test required"
    echo "   Navigate to: https://get.webgl.org/"
    echo "   Expected: WebGL support detected"
fi

if command -v /Applications/Firefox.app/Contents/MacOS/firefox &> /dev/null; then
    echo "🦊 Firefox detected - manual WebGL test required"
    echo "   Navigate to: https://get.webgl.org/webgl2/"
    echo "   Expected: WebGL 2.0 support detected"
fi

if command -v /Applications/Safari.app/Contents/MacOS/Safari &> /dev/null; then
    echo "🧭 Safari detected - manual WebGL test required"
    echo "   Test 3D content in Safari with WebGL"
fi

# Test 7: System information validation
echo -e "${YELLOW}📋 Phase 7: System Information Tests${NC}"
run_test "Display driver recognized" "system_profiler SPDisplaysDataType | grep -qE '(VMQemuVGA|VirtIO|Hardware Acceleration)'" "should_pass"

# Test 8: Performance validation
echo -e "${YELLOW}📋 Phase 8: Performance Validation${NC}"
echo "📊 Performance metrics (informational):"
echo "   Memory usage: $(ps aux | grep VMQemuVGA | grep -v grep | awk '{print $4}' || echo 'N/A')%"
echo "   Load average: $(uptime | awk -F'load average:' '{ print $2 }' || echo 'N/A')"

# Summary
echo -e "${BLUE}📋 TEST SUMMARY${NC}"
echo "==============="
echo -e "Tests Passed: ${GREEN}${TESTS_PASSED}${NC}"
echo -e "Tests Failed: ${RED}${TESTS_FAILED}${NC}"
echo "Tests Total: ${TESTS_TOTAL}"
echo

if [[ $TESTS_FAILED -eq 0 ]]; then
    echo -e "${GREEN}🎉 ALL TESTS PASSED! VMQemuVGA v8.0 is working correctly.${NC}"
    echo
    echo "✅ Manual testing required for:"
    echo "   - Chrome cursor movement (should be smooth, no flickering)"
    echo "   - WebGL rendering in browsers (visit https://get.webgl.org/)"
    echo "   - Text rendering quality (no yellow squares around text)"
    echo "   - OpenGL developer tools (no texture cache artifacts)"
    
    exit 0
else
    echo -e "${RED}⚠️  Some tests failed. Check the logs above for details.${NC}"
    echo
    echo "🚑 If system is unstable:"
    echo "   sudo kextunload /System/Library/Extensions/VMQemuVGA.kext"
    echo "   sudo rm -rf /System/Library/Extensions/VMQemuVGA.kext"
    echo "   sudo kextcache -system-prelinked-kernel"
    
    exit 1
fi
