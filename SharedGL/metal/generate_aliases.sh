#!/bin/bash
#
# Generate alias list for linker to re-export Mesa symbols
#

echo "Generating alias list for ld..."
ssh -i vm-ssh-key qemucat@qemucat.local "nm -gU /opt/local/lib/libGL.1.dylib.mesa 2>/dev/null | grep ' T _gl' | sed 's/.* _//' | grep -E '^gl[A-Z]' | grep -v '^glX'" | while read sym; do
    echo "_$sym _mesa_$sym"
done > SharedGL/metal/mesa_aliases.txt

echo "✅ Generated alias list with $(wc -l < SharedGL/metal/mesa_aliases.txt) symbols"
