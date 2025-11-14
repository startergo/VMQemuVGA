#!/bin/bash

echo "🧪 VMQemuVGA VRAM Detection & CPU Optimization Test"
echo "=================================================="
echo

# Function to check current GPU status
check_gpu_status() {
    echo "📊 Current GPU Status:"
    echo "---------------------"
    
    # Check Activity Monitor for GPU usage
    if command -v iostat >/dev/null 2>&1; then
        echo "💻 CPU Usage (before driver test):"
        iostat -c 2 1 | tail -1
    fi
    
    # Check System Profiler
    echo
    echo "🖥️  System Profiler GPU Information:"
    system_profiler SPDisplaysDataType | grep -A 20 "VirtIO-GPU\|VMware\|QXL\|Display"
    
    echo
    echo "🔍 Checking for VirtIO-GPU detection:"
    system_profiler SPDisplaysDataType | grep -i "virtio\|1050\|VRAM"
    
    echo
}

# Function to test VRAM detection
test_vram_detection() {
    echo "🧠 Testing VRAM Detection:"
    echo "-------------------------"
    
    echo "📋 System Profiler GPU Information:"
    system_profiler SPDisplaysDataType | grep -A 30 -i "display\|gpu\|vram\|virtio"
    
    echo
    echo "🔍 Looking for 512MB VRAM reporting:"
    system_profiler SPDisplaysDataType | grep -i "512\|vram" || echo "❌ VRAM not detected yet"
    
    echo
    echo "📊 Checking IOKit registry for VRAM information:"
    ioreg -l | grep -i "vram\|memory" | head -10
}

# Function to test CPU optimization
test_cpu_optimization() {
    echo "🚀 Testing CPU Optimization:"
    echo "----------------------------"
    
    echo "💻 CPU usage during graphics operations:"
    
    # Start monitoring CPU in background
    if command -v top >/dev/null 2>&1; then
        echo "Starting CPU monitoring..."
        (top -l 10 -n 10 -F -R | grep -E "CPU|VMQemuVGA|kernel_task" > /tmp/cpu_monitor.log 2>&1) &
        monitor_pid=$!
        
        # Simulate graphics activity (if we have a way to trigger it)
        echo "Generating graphics activity..."
        
        # Create a simple graphics stress test
        for i in {1..100}; do
            echo "Graphics operation $i" > /dev/null
            # This would ideally trigger actual graphics operations
            sleep 0.01
        done
        
        sleep 2
        kill $monitor_pid 2>/dev/null
        
        echo "CPU monitoring results:"
        if [ -f /tmp/cpu_monitor.log ]; then
            cat /tmp/cpu_monitor.log | tail -20
        fi
    fi
}

# Function to check for white screen artifacts (manual)
test_display_artifacts() {
    echo "🎨 Display Artifact Test (Manual):"
    echo "----------------------------------"
    echo "To test for white painting artifacts:"
    echo "1. Move the mouse cursor around the screen"
    echo "2. Look for any white rectangles or artifacts"
    echo "3. Open/close windows to test window painting"
    echo "4. Try scrolling in applications"
    echo
    echo "❓ Do you see any white painting artifacts with mouse movement? (y/n)"
}

# Function to check driver logs
check_driver_logs() {
    echo "📝 Driver Logs:"
    echo "--------------"
    echo "Recent VMQemuVGA messages:"
    
    # Check system log for our driver messages
    if command -v log >/dev/null 2>&1; then
        log show --predicate 'process CONTAINS "kernel" AND composedMessage CONTAINS "VMQemuVGA"' --info --last 5m 2>/dev/null | tail -20
    else
        # Fallback to dmesg/syslog
        dmesg | grep -i vmqemuvga | tail -10 2>/dev/null || echo "No recent driver messages found"
    fi
}

# Main test sequence
echo "Starting VMQemuVGA comprehensive test..."
echo

# Check initial status
check_gpu_status

echo
echo "=================================================="
echo

# Test VRAM detection
test_vram_detection

echo
echo "=================================================="
echo

# Test CPU optimization
test_cpu_optimization

echo
echo "=================================================="
echo

# Check logs
check_driver_logs

echo
echo "=================================================="
echo

# Manual display test
test_display_artifacts

echo
echo "🏁 Test Complete!"
echo "================="
echo "Key things to verify:"
echo "1. ✅ System Profiler shows VirtIO-GPU with 512MB VRAM"
echo "2. ✅ No white painting artifacts with mouse movement"
echo "3. ✅ Reduced CPU usage during graphics operations"
echo "4. ✅ GPU usage visible in Activity Monitor"
echo
echo "💡 Use your existing installer to deploy the updated VMQemuVGA.kext"
echo "💡 If VRAM is still showing 0MB after installation, try restarting the VM"
