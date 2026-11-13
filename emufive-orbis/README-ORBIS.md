# Como usar este pacote (no seu PC, dentro do ambiente do Orbis Eden)

1. KytyPS5: `git clone https://github.com/KytyPS5/KytyPS5` e `git checkout 589bd14a9b8934a85e951d1aaa8112a05d62354c`; submódulos: fmt, magic_enum, spdlog, nlohmann_json, xxHash, Vulkan-Headers, VulkanMemoryAllocator, SPIRV-Headers, LibAtrac9, imgui, tracy.
2. Aplicar os patches do EmuFive (`EmuFive-1.0.0/patches/kytyps5/0001..0005`) e depois `patches/0100-orbis-port.patch` (`git apply`).
3. Dependências que o KytyPS5 baixa pelo CMake (xbyak 44a72f3, zydis 120e0e7 com submódulos, zstd f8745da, ZArchive d2c7177, opus ddbe483): clonar numa pasta e passar em `-DKYTY_EXTRA_DEPS=`.
4. Uma pasta com `CMakeLists.orbis.txt` copiado como `CMakeLists.txt`; configurar:
   `cmake -S <pasta> -B build-kyty -DKYTY_ROOT=<KytyPS5> -DCMAKE_TOOLCHAIN_FILE=<repo-eden>/toolchain/eden-ps4.cmake -DKYTY_EXTRA_DEPS=<pasta-deps>`
   e `cmake --build build-kyty --target kyty_core`.
   `ps4-local.cmake.example` é o toolchain que usei para testar no Linux (serve de referência).
5. `check-syntax.sh` é a checagem rápida de sintaxe por pasta; os caminhos no topo são do meu ambiente.
