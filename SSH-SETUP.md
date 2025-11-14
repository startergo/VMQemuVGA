# VMQemuVGA SSH Setup - Quick Reference

## 🔐 SSH Key Files Created

- **Private Key**: `vm-ssh-key` (keep secure on host)
- **Public Key**: `vm-ssh-key.pub` (copy to VM)

## 🚀 Quick Setup Commands

### 1. Get Your Public Key
```bash
cat vm-ssh-key.pub
```

### 2. Copy to VM (Method A - Easiest)
```bash
ssh-copy-id -i vm-ssh-key qemucat@192.168.12.101
```

### 3. Test Passwordless Connection
```bash
ssh -i vm-ssh-key qemucat@192.168.12.101
```

## 📋 Available Scripts

- `./setup-vm-ssh.sh [IP] [USER]` - Show setup instructions
- `./connect-vm.sh [IP] [USER]` - Connect to VM with SSH key
- `./vm-transfer.sh [IP] [USER] [upload|download|install]` - File transfer

## 🔧 SSH Config (Optional)

Add to `~/.ssh/config`:
```
Host vm
HostName 192.168.12.101
User qemucat
IdentityFile /Users/macbookpro/VMQemuVGA/vm-ssh-key
IdentitiesOnly yes
```

Then connect with: `ssh vm`

## 📦 Development Workflow

1. **Build driver**: `./build-enhanced_private.sh`
2. **Upload to VM**: `./vm-transfer.sh 192.168.12.101 qemucat upload`
3. **Install on VM**: `./vm-transfer.sh 192.168.12.101 qemucat install`
4. **Test driver**: `./connect-vm.sh` then run tests

## 🔑 Public Key (for reference)

```
ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAINMrXDW+bVHwUnCCHlMjdt/8rFFEsC+tNSMdkoe/Tjen VMQemuVGA-development-key
```
