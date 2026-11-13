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
if [ -z "$EMUFIVE" ]; then
  for d in "$ROOT/EmuFive-1.0.3" "$ROOT/../EmuFive-1.0.3" "/c/orbisemufive/EmuFive-1.0.3" "$HOME/Downloads/EmuFive-1.0.3"; do
    [ -d "$d/patches/kytyps5" ] && EMUFIVE="$d" && break
  done
fi
[ -d "$EMUFIVE/patches/kytyps5" ] || die "diga onde esta o EmuFive:  EMUFIVE=/caminho/EmuFive-1.0.3 bash build-kyty.sh"
echo "EmuFive: $EMUFIVE"
mkdir -p "$WORK"; cd "$WORK"

# 1) KytyPS5 no commit certo + submodulos
if [ ! -d kytyps5/.git ]; then git clone --filter=blob:none https://github.com/KytyPS5/KytyPS5 kytyps5; fi
if [ ! -f kytyps5/.kyty-patched ]; then
  git -C kytyps5 checkout -q $KYTY_SHA
  for m in fmt magic_enum spdlog nlohmann_json xxHash Vulkan-Headers VulkanMemoryAllocator SPIRV-Headers LibAtrac9 imgui tracy; do
    git -C kytyps5 submodule update --init --depth 1 "3rdparty/$m" || git -C kytyps5 submodule update --init "3rdparty/$m"
  done
  # 2) patches do EmuFive (0001..0007) e depois o do porte para Orbis
  for p in "$EMUFIVE"/patches/kytyps5/000*.patch "$PKG/patches/0100-orbis-port.patch"; do
    echo "aplicando $(basename "$p")"; git -C kytyps5 apply --whitespace=nowarn "$p"
  done
  touch kytyps5/.kyty-patched
fi
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
cmake --build "$ROOT/build-kyty" --target kyty-ps4
ls -la "$ROOT/build-kyty/bin/kyty-ps4" && echo "OK. Agora: bash package-kyty.sh"
