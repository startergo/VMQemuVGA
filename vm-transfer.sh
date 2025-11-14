#!/bin/bash

# VMQemuVGA File Transfer Script
# Usage: ./vm-transfer.sh <ip> <username> <action> [file]

if [ $# -lt 3 ]; then
    echo "Usage: $0 <ip> <username> <upload|download> [file]"
    echo "Examples:"
    echo "  $0 192.168.12.101 qemucat upload"
    echo "  $0 192.168.12.101 qemucat upload check-vram-status.sh"
    exit 1
fi

IP="$1"
USERNAME="$2"
ACTION="$3"
FILE="$4"

SSH_KEY="vm-ssh-key"
if [ ! -f "$SSH_KEY" ]; then
    echo "Error: SSH key file '$SSH_KEY' not found"
    exit 1
fi

case "$ACTION" in
    "upload")
        echo "📤 Uploading files to VM..."
        
        if [ -n "$FILE" ]; then
            if [ "$FILE" = "all" ]; then
                # Upload everything
                echo "Uploading all files..."
            else
                # Upload specific file only
                echo "Uploading specific file: $FILE..."
                scp -i "$SSH_KEY" "$FILE" "$USERNAME@$IP:~/"
                echo "✅ Upload complete!"
                exit 0
            fi
        fi
        
        # Upload default files (when no file specified or "all" specified)
        echo "Uploading VMQemuVGA.kext..."
        if [ -d "build/Release/VMQemuVGA.kext" ]; then
            scp -i "$SSH_KEY" -r "build/Release/VMQemuVGA.kext" "$USERNAME@$IP:~/"
        fi
        
        echo "Uploading test script..."
        if [ -f "check-vram-status.sh" ]; then
            scp -i "$SSH_KEY" "check-vram-status.sh" "$USERNAME@$IP:~/"
        fi

        echo "Uploading test script..."
        if [ -f "debug_vram_properties.sh" ]; then
            scp -i "$SSH_KEY" "debug_vram_properties.sh" "$USERNAME@$IP:~/"
        fi
        
        echo "Uploading package..."
        PKG_FILE=$(ls VMQemuVGA-v8.0-Private-*.pkg 2>/dev/null | head -1)
        if [ -n "$PKG_FILE" ]; then
            scp -i "$SSH_KEY" "$PKG_FILE" "$USERNAME@$IP:~/"
        fi
        echo "✅ Upload complete!"
        ;;
    
    "download")
        echo "📥 Downloading from VM..."
        if [ -n "$FILE" ]; then
            scp -i "$SSH_KEY" "$USERNAME@$IP:~/$FILE" .
        else
            echo "Please specify a file to download"
            exit 1
        fi
        ;;
    
    *)
        echo "Invalid action: $ACTION"
        echo "Use 'upload' or 'download'"
        exit 1
        ;;
esac
