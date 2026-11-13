# Orbis-Kyty
Is a test-only ps5 emulator for ps4 based ON EMUFIVE by blue0x1
made with the help of Claude
# EmuFive/KytyPS5 → Orbis: estado do port (10/11/2026)

Base: KytyPS5 no commit 589bd14 + patches 0001-0005 do EmuFive (aplicam sem erro).
**Nada foi testado no PS4.** O que existe é: toolchain pronto + núcleo do emulador compilando (só checagem de sintaxe, ainda sem link).

## Resultado da checagem de compilação (clang 18, alvo x86_64-scei-ps4, C++20)
| Pasta | Arquivos | Com erro | Observação |
|---|---|---|---|
| common/ | 21 | 0 | memória virtual, arquivos, exceções, threads |
| kernel/ | 8 | 0 | pthread, memória do guest, semáforos, eventos |
| loader/ | 11 | 0 | SELF/ELF, patcher AVX2/FMA/BMI2 (0004), runtime linker |
| libs/ | 65 | 10 | todos por falta de SDL3 (áudio/controle/fonte) ou FFmpeg (vídeo/áudio) |
| graphics/ | ~78 mil linhas | não tentado | depende 100% de Vulkan |

## O que foi feito (patch `0100-orbis-port-wip.patch`, ~170 linhas)
- `abi.h`: o alvo PS4 não aceita `ms_abi`; o único uso (stub de TLS do guest) usa `no_caller_saved_registers`.
- `syncOnAddress.cpp`: sem futex no Orbis → usa o caminho portátil (o mesmo do macOS).
- `memoryAddressSpace.inc`: sem `memfd_create` → usa `shm_open` (como o macOS).
- `hostException.cpp` / `libKernel.cpp`: `sigaction` e contexto do sinal no formato FreeBSD 9 (`mc_rip`, `mc_err`...).
- `profiler`: Tracy desligado no Orbis.
- `common/orbis/atomicRef.h`: `std::atomic_ref` (a libc++ 18 não tem).
- `memory.cpp`: macro `PAGE_SIZE` da libc do PS4 colidia com constante local.

## Toolchain (`setup-toolchain.sh`)
Os releases do OpenOrbis estavam bloqueados, então o script monta tudo do código-fonte: musl do PS4, stubs das libs do sistema, create-fself, **libc++ 18** (a do OpenOrbis é antiga: sem `std::ranges`/`jthread`) e compiler-rt. Verificado: um programa com `ranges`, `jthread` e `filesystem` compila e **linka** num ELF de PS4 (`teste-cpp20.cpp`).
Flags importantes: `-U__FreeBSD__` (a libc++ confunde o PS4 com FreeBSD puro) e `-fexperimental-library` (jthread).

## O que falta (em ordem)
1. **SDL3 e FFmpeg** não existem para o Orbis. Áudio/controle/janela precisam virar `sceAudioOut`/`scePad`/`sceVideoOut`; FFmpeg precisa de stub (sem vídeo/áudio de jogo no começo).
2. **`/proc/self/maps`**: `memoryAddressSpace.inc` lê isso para achar áreas livres ao reservar o espaço de endereços do guest. O Orbis não tem `/proc`; trocar por `sceKernelVirtualQuery` ou sondagem com `mmap`.
3. **Outros ~70 ramos `__APPLE__`** em `pthread.cpp`, `threads.cpp`, `guestInstructionPatcher.cpp`, `x64InstructionEmulator.cpp`, `hostStats.cpp`: precisam de ramo Orbis (principalmente o contexto de sinal, usado pelo patcher AVX2 e pelo emulador x64).
4. **Memória e endereços fixos**: o guest precisa de endereços fixos de PS5 e o Orbis limita a RAM do homebrew. Só dá pra saber no console.
5. **Gráfico**: sem Vulkan no Orbis. Etapa 1 = stub (sem imagem). Depois, framebuffer simples; GNM ou Mesa/RADV por último.
6. **Link e empacotamento**: CMake para o toolchain, `create-fself`, `.pkg`.
7. O layout do `ucontext` do Orbis (FreeBSD 9) está **assumido, não verificado** no console.

## Como reproduzir
`./setup-toolchain.sh <pasta>` (Linux, precisa de clang 18, lld, cmake, ninja, go, python3). Depois aplicar `0100-orbis-port-wip.patch` sobre o KytyPS5 já com os patches 0001-0005. `check-syntax.sh` tem caminhos fixos do meu ambiente; ajuste as variáveis no topo.
