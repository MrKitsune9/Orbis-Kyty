#!/usr/bin/env bash
# Orbis-Kyty: baixa o KytyPS5, aplica os patches, compila e gera build-kyty/bin/kyty-ps4.
# Rodar no Git Bash (UCRT64), na raiz do repositorio do Eden (a mesma pasta do package-eden.sh).
#   bash build-kyty.sh            # tudo
# Depois:  bash package-kyty.sh   # gera o .pkg
set -e
ROOT="$(cd "$(dirname "$0")" && pwd)"
PKG="$ROOT/emufive-orbis"                     # esta pasta (com patches/, orbis/, CMakeLists.orbis.txt)
[ -f "$PKG/CMakeLists.orbis.txt" ] || PKG="$ROOT"
WORK="$ROOT/kyty-work"
EMUFIVE="${EMUFIVE:-}"
KYTY_SHA=589bd14a9b8934a85e951d1aaa8112a05d62354c

die() { echo "ERRO: $*" >&2; exit 1; }
# Pasta base (tem tools/ e repo/). Padrao: a pasta acima deste repo (ex.: /c/orbisemufive).
BASE="${BASE:-$(cd "$ROOT/.." && pwd)}"
[ -d "$BASE/tools" ] || BASE=/c/orbisemufive
[ -d "$BASE/tools" ] || BASE=/c/nxorbis
TOOLS="$BASE/tools"
EDEN="${EDEN:-$ROOT}"
[ -f "$EDEN/toolchain/eden-ps4.cmake" ] || EDEN="$BASE/repo"
for d in "$TOOLS"/cmake-*/bin "$TOOLS/ninja" "$TOOLS/llvm/bin" "$TOOLS/glslang/bin"; do
  [ -d "$d" ] && PATH="$d:$PATH"
done
export PATH
echo "base=$BASE  eden=$EDEN"
for t in git cmake ninja; do command -v $t >/dev/null || die "nao achei '$t' no PATH"; done
[ -f "$EDEN/toolchain/eden-ps4.cmake" ] || die "nao achei toolchain/eden-ps4.cmake: o Eden nao esta em $EDEN (use EDEN=/caminho/repo-do-eden)"
[ -d "$EDEN/libcxx18" ] || die "nao achei libcxx18/ (o libc++ 18 que o Eden compila)"
[ -f "$EDEN/frontend/emutls/emutls.c" ] || die "nao achei frontend/emutls/emutls.c do Eden"
# Os patches 0001-0041 (EmuFive 1.0.15) vem junto neste pacote (patches/kytyps5/).
PATCHDIR="$PKG/patches/kytyps5"
[ -d "$EMUFIVE/patches/kytyps5" ] && PATCHDIR="$EMUFIVE/patches/kytyps5"
ls "$PATCHDIR"/0001*.patch >/dev/null 2>&1 || die "nao achei os patches do EmuFive em $PATCHDIR"
echo "patches: $PATCHDIR"
mkdir -p "$WORK"; cd "$WORK"

# 1) KytyPS5 no commit certo + submodulos
# instalacao feita com patches antigos (antes do EmuFive 1.0.15): refaz a pasta do zero
if [ -f kytyps5/.kyty-patched ] && [ ! -f kytyps5/.emufive-1015 ]; then
  echo "atualizando para EmuFive 1.0.15: refazendo kytyps5 (demora um pouco)"; rm -rf kytyps5
fi
if [ ! -d kytyps5/.git ]; then git clone --filter=blob:none https://github.com/KytyPS5/KytyPS5 kytyps5; fi
if [ ! -f kytyps5/.kyty-patched ]; then
  git -C kytyps5 checkout -q $KYTY_SHA
  for m in fmt magic_enum spdlog nlohmann_json xxHash Vulkan-Headers VulkanMemoryAllocator SPIRV-Headers LibAtrac9 imgui tracy; do
    git -C kytyps5 submodule update --init "3rdparty/$m"
  done
  for p in "$PATCHDIR"/00*.patch; do
    echo "aplicando $(basename "$p")"; git -C kytyps5 apply --whitespace=nowarn "$p"
  done
  touch kytyps5/.kyty-patched kytyps5/.emufive-1015
fi
# (instalacoes antigas ja tinham o 0100 aplicado)
[ -f kytyps5/.orbis-applied ] || { [ -f kytyps5/.kyty-patched ] && [ -f kytyps5/src/common/orbis/mcontext.h ] && echo 0100-orbis-port.patch > kytyps5/.orbis-applied || true; }
# patches do porte (0100, 0101, ...): cada um e aplicado uma unica vez
for p in "$PKG"/patches/01*.patch; do
  n="$(basename "$p")"
  if ! grep -qx "$n" kytyps5/.orbis-applied 2>/dev/null; then
    echo "aplicando $n"; git -C kytyps5 apply --whitespace=nowarn "$p"; echo "$n" >> kytyps5/.orbis-applied
  fi
done
rm -rf kytyps5/orbis; cp -r "$PKG/orbis" kytyps5/orbis

# 3) dependencias que o KytyPS5 normalmente baixa sozinho
getdep() { # nome url commit
  [ -d "deps/$1/.git" ] || git clone --filter=blob:none --recursive "$2" "deps/$1"
  git -C "deps/$1" checkout -q "$3"; git -C "deps/$1" submodule update --init --recursive -q
}
mkdir -p deps
getdep xbyak  https://github.com/herumi/xbyak 44a72f3
getdep zydis  https://github.com/zyantific/zydis 120e0e7
getdep zstd   https://github.com/facebook/zstd f8745da
getdep zarchive https://github.com/Exzap/ZArchive d2c7177
getdep opus   https://github.com/xiph/opus ddbe483

# 4) toolchain: o do Eden, com os caminhos deste repo
sed -e "s#C:/nxorbis/repo#$(cygpath -m "$EDEN")#g" -e "s#C:/nxorbis/tools#$(cygpath -m "$TOOLS")#g" "$EDEN/toolchain/eden-ps4.cmake" > kyty-ps4.cmake
unset OO_PS4_TOOLCHAIN

mkdir -p cmake-src; cp "$PKG/CMakeLists.orbis.txt" cmake-src/CMakeLists.txt
cmake -S cmake-src -B "$ROOT/build-kyty" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$(cygpath -m "$WORK/kyty-ps4.cmake")" -DCMAKE_BUILD_TYPE=Release \
  -DKYTY_ROOT="$(cygpath -m "$WORK/kytyps5")" -DKYTY_EXTRA_DEPS="$(cygpath -m "$WORK/deps")" \
  -DKYTY_EDEN_FRONTEND="$(cygpath -m "$EDEN/frontend")"
# -k 0: nao para no primeiro erro, para eu ver todos de uma vez
LOG="$ROOT/build-kyty.log"
if ! cmake --build "$ROOT/build-kyty" --target kyty-ps4 -- -k 0 > "$LOG" 2>&1; then
  echo "=== A COMPILACAO FALHOU. Erros (me mande este bloco): ==="
  grep -E "error:|undefined (symbol|reference)|cannot find|No such file" "$LOG" | sed -E 's#C:/orbisemufive/repo/emufive-orbis/kyty-work/##' | sort | uniq -c | sort -rn | head -60
  echo "(log completo: $LOG)"
  exit 1
fi
ls -la "$ROOT/build-kyty/bin/kyty-ps4" && echo "OK. Agora: bash package-kyty.sh"
