#!/bin/sh
# deploy_gld.sh — build, VERIFY, and ship the GLD stub bundle.
#
# Ends the stale-deploy class: twice a failed build reached the
# guest as an old artifact (once a bundle with no executable,
# once the August probe binary), both caught only by absent
# output. This script refuses to ship when (a) the compile
# failed, (b) the binary is unchanged since the last deploy, or
# (c) the guest-side digest doesn't match after the copy.
#
# Usage: probe/deploy_gld.sh   (from anywhere; paths derived)
set -e

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$REPO_ROOT/probe/gld_stub.c"
STAGE="/tmp/gld_stage/VMVirtIOGLEngine.bundle/Contents/MacOS"
OUT="$STAGE/VMVirtIOGLEngine"
LAST_MD5="/tmp/gld_last_deploy.md5"
# SDK search order: env override, then the Xcode Platforms SDKs dir
# (canonical — a Downloads copy vanished mid-session once), then the
# leopard-webkit cross-toolchain copy. First hit with stdio.h wins.
if [ -z "$MACOSX10_6_SDK" ]; then
  for c in \
    "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.6.sdk" \
    "$HOME/Downloads/MacOSX10.6.sdk" \
    "$HOME/leopard-webkit-build/sdk/MacOSX-SDKs/MacOSX10.6.sdk"; do
    if [ -f "$c/usr/include/stdio.h" ]; then MACOSX10_6_SDK="$c"; break; fi
  done
fi
SDK="$MACOSX10_6_SDK"
[ -f "$SDK/usr/include/stdio.h" ] || {
  echo "DEPLOY ABORT: no MacOSX10.6 SDK found (set MACOSX10_6_SDK)"; exit 1; }

rm -rf /tmp/gld_stage
mkdir -p "$STAGE"
# RUNG 75: the bundle carries its own Mesa pair — libOSMesa's one
# @rpath dep (libglapi) is rewritten to @loader_path so the stub's
# dlopen needs NO dyld env (runtime setenv can't reach dyld).
if [ -f "$REPO_ROOT/probe/gld-libs/libOSMesa.8.dylib" ]; then
  cp "$REPO_ROOT/probe/gld-libs/libOSMesa.8.dylib" \
     "$REPO_ROOT/probe/gld-libs/libglapi.0.dylib" "$STAGE/"
  install_name_tool -change "@rpath/libglapi.0.dylib" \
      "@loader_path/libglapi.0.dylib" "$STAGE/libOSMesa.8.dylib" 2>/dev/null
else
  echo "NOTE: probe/gld-libs absent — bundle ships without Mesa (GPU path off)"
fi
# The bundle's Info.plist lives in the repo (source of record —
# /tmp staging dies at boot).
cp "$REPO_ROOT/probe/VMVirtIOGLEngine.Info.plist" "$STAGE/../Info.plist"

# 1. Build — a failure aborts BEFORE anything ships (set -e).
# RUNG 59: the bundle is SELF-CONTAINED again — the hard OSMesa link
# made it UNLOADABLE in any process without the rpath (GLMark's gate:
# no renderer at all). The rung-55 "dlopen crashes" was the ARITY
# bug; dlopen + the five-arg create is the right shape. The library
# pair lives on the guest at /Users/sl/osmesa/.
xcrun clang -arch x86_64 -mmacosx-version-min=10.6 \
  -isysroot "$SDK" -bundle "$SRC" \
  -framework IOKit -framework CoreFoundation -o "$OUT" 2>/dev/null
[ -x "$OUT" ] || { echo "DEPLOY ABORT: no executable produced"; exit 1; }

# 2. Positive control: the export table.
EXPORTS=$(nm "$OUT" 2>/dev/null | grep -c " T ")
[ "$EXPORTS" -ge 90 ] || { echo "DEPLOY ABORT: exports=$EXPORTS (<90)"; exit 1; }

# 3. Unchanged-binary refusal.
NEW_MD5=$(md5 -q "$OUT")
if [ -f "$LAST_MD5" ] && [ "$NEW_MD5" = "$(cat "$LAST_MD5")" ]; then
  echo "DEPLOY ABORT: binary unchanged since last deploy ($NEW_MD5)"
  exit 1
fi

# 4. Ship + swap; the guest-side digest must match.
cd /tmp/gld_stage
tar czf /tmp/gld_deploy.tgz VMVirtIOGLEngine.bundle
cat /tmp/gld_deploy.tgz | ssh sl@slqemu.local '
  cat > /tmp/gld_deploy.tgz && cd /tmp && rm -rf gld_stage &&
  mkdir gld_stage && cd gld_stage && tar xzf ../gld_deploy.tgz &&
  sudo -S sh -c "rm -rf /System/Library/Extensions/VMVirtIOGLEngine.bundle &&
    cp -R /tmp/gld_stage/VMVirtIOGLEngine.bundle /System/Library/Extensions/ &&
    chown -R root:wheel /System/Library/Extensions/VMVirtIOGLEngine.bundle &&
    chmod -R 755 /System/Library/Extensions/VMVirtIOGLEngine.bundle" <<< "q"
  md5 -q /System/Library/Extensions/VMVirtIOGLEngine.bundle/Contents/MacOS/VMVirtIOGLEngine
' | tail -1 > /tmp/gld_guest_md5
GUEST_MD5=$(cat /tmp/gld_guest_md5 | tr -d "[:space:]")
[ "$GUEST_MD5" = "$NEW_MD5" ] || {
  echo "DEPLOY ABORT: guest digest $GUEST_MD5 != built $NEW_MD5"
  exit 1
}

echo "$NEW_MD5" > "$LAST_MD5"
echo "DEPLOYED $NEW_MD5 ($EXPORTS exports) — verified on guest"
