#!/bin/bash
#
# Toggle between custom libGL and original system libGL on VM
#

if [ "$1" == "custom" ]; then
    echo "Installing custom libGL (Metal translator)..."
    scp -i vm-ssh-key build/metal/libGL.1.dylib qemucat@qemucat.local:~/
    ssh -t -i vm-ssh-key qemucat@qemucat.local "sudo cp ~/libGL.1.dylib /opt/X11/lib/libGL.1.dylib && echo '✅ Custom libGL installed'"
elif [ "$1" == "original" ]; then
    echo "Restoring original system libGL..."
    ssh -t -i vm-ssh-key qemucat@qemucat.local "sudo cp /opt/X11/lib/libGL.1.dylib.original /opt/X11/lib/libGL.1.dylib && echo '✅ Original libGL restored'"
else
    echo "Usage: $0 [custom|original]"
    echo ""
    echo "  custom    - Install custom Metal translator libGL"
    echo "  original  - Restore original system libGL"
    exit 1
fi
