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
SDK="${MACOSX10_6_SDK:-$HOME/Downloads/MacOSX10.6.sdk}"

rm -rf /tmp/gld_stage
mkdir -p "$STAGE"
# The bundle's Info.plist lives in the repo (source of record —
# /tmp staging dies at boot).
cp "$REPO_ROOT/probe/VMVirtIOGLEngine.Info.plist" "$STAGE/../Info.plist"

# 1. Build — a failure aborts BEFORE anything ships (set -e).
# RUNG 55: the bundle LINKS the build-tree libOSMesa directly —
# the dlopen route crashes in OSMesaCreateContextExt (the glapi
# bridge's dispatch needs link-time binding; the linked ostest
# proves it). The @rpath resolves in the HOST PROCESS (the probe
# carries -rpath /Users/sl/osmesa). The library pair is shipped
# on the guest at /Users/sl/osmesa/.
OSMESA_LIB="$HOME/Mesa-VirGL/build-106/src/gallium/targets/osmesa/libOSMesa.8.dylib"
xcrun clang -arch x86_64 -mmacosx-version-min=10.6 \
  -isysroot "$SDK" -bundle "$SRC" \
  -framework IOKit -framework CoreFoundation \
  "$OSMESA_LIB" -o "$OUT" 2>/dev/null
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
