#!/bin/bash
# usage: syntax.sh <dir-under-src> ; prints per-file error counts
OO=/tmp/claude-0/-home-claude/31221dcf-bef1-52cb-85d8-fcf5a736daa9/scratchpad/oo
K=/tmp/claude-0/-home-claude/31221dcf-bef1-52cb-85d8-fcf5a736daa9/scratchpad/src/kytyps5
D=/tmp/claude-0/-home-claude/31221dcf-bef1-52cb-85d8-fcf5a736daa9/scratchpad/src/deps
T=3rdparty
FLAGS="--target=x86_64-pc-freebsd12-elf -funwind-tables -D__PS4__ -D__ORBIS__ -D_BSD_SOURCE=1 -march=btver2 -U__FreeBSD__ -U__FreeBSD_cc_version -DPS4 -D_GNU_SOURCE -fPIC -std=c++20 -fexperimental-library -fexceptions -frtti -nostdinc++ -nostdlibinc -isystem $OO/cxxinst/include/c++/v1 -isystem $OO/musl/ps4/include -isystem $OO/shim -include $K/src/common/orbis/atomicRef.h -ferror-limit=0 -w"
INC="-I$K/$T/tracy/public -I$K/$T/magic_enum/include/magic_enum -I$K/$T/magic_enum/include -I$K/$T/VulkanMemoryAllocator/include -I$K/$T/SPIRV-Headers/include -I$K/$T/imgui -I/tmp/claude-0/-home-claude/31221dcf-bef1-52cb-85d8-fcf5a736daa9/scratchpad/gen -I$K/src -I$K/$T/fmt/include -I$K/$T/magic_enum/include -I$K/$T/spdlog/include -I$K/$T/nlohmann_json/include -I$K/$T/xxHash -I$K/$T/Vulkan-Headers/include -I$K/$T/cpuinfo/include -I$K/$T/stb -I$K/$T/renderdoc -I$D/xbyak -I$D/zydis/include -I$D/zydis/dependencies/zycore/include -I$D/zarchive/include -I$D/zstd/lib -I$D/opus/include -I$K/$T/LibAtrac9/C/src"
for f in $(find $K/src/$1 -name '*.cpp' | sort); do
  n=$(clang++ $FLAGS $INC -fsyntax-only $f 2>&1 | grep -c 'error:')
  echo "$n ${f#$K/src/}"
done
