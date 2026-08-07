#!/usr/bin/env python3
"""
Adds msg_send() calls after command blocks that don't have them.
Fixes the TCP fragmentation issue by ensuring complete messages are sent atomically.
"""

import re
import sys

def fix_msg_send(content):
    """Add msg_send() after blocks that call send_metal_command() but don't call msg_send()"""
    
    # Pattern: send_metal_command followed by some send operations, but NO msg_send() before closing brace
    # We need to add msg_send() before the closing brace
    
    lines = content.split('\n')
    output = []
    i = 0
    
    while i < len(lines):
        line = lines[i]
        output.append(line)
        
        # Check if this line contains send_metal_command
        if 'send_metal_command(' in line and '//' not in line.split('send_metal_command')[0]:
            # Look ahead to find the closing brace of this if block
            indent_level = len(line) - len(line.lstrip())
            j = i + 1
            has_msg_send = False
            closing_brace_idx = -1
            
            # Scan forward until we find the matching closing brace
            brace_count = line.count('{') - line.count('}')
            
            while j < len(lines) and (brace_count > 0 or closing_brace_idx == -1):
                scan_line = lines[j]
                
                # Check if msg_send() already exists
                if 'msg_send()' in scan_line:
                    has_msg_send = True
                
                # Count braces
                brace_count += scan_line.count('{') - scan_line.count('}')
                
                if brace_count == 0 and closing_brace_idx == -1:
                    closing_brace_idx = j
                    break
                
                j += 1
            
            # If we found the closing brace and there's no msg_send(), add it
            if closing_brace_idx > 0 and not has_msg_send:
                # Insert msg_send() before the closing brace
                # Copy all lines up to (but not including) the closing brace
                for k in range(i + 1, closing_brace_idx):
                    output.append(lines[k])
                
                # Add msg_send() with proper indentation
                closing_line = lines[closing_brace_idx]
                closing_indent = len(closing_line) - len(closing_line.lstrip())
                output.append(' ' * (closing_indent + 4) + 'msg_send();')
                output.append(closing_line)
                
                # Skip to after the closing brace
                i = closing_brace_idx + 1
                continue
        
        i += 1
    
    return '\n'.join(output)

if __name__ == '__main__':
    input_file = 'gl_to_metal_client.c'
    
    with open(input_file, 'r') as f:
        content = f.read()
    
    fixed_content = fix_msg_send(content)
    
    with open(input_file, 'w') as f:
        f.write(fixed_content)
    
    print(f"✅ Fixed {input_file} - added msg_send() calls where needed")
