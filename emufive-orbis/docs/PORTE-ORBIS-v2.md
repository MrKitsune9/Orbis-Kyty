# EmuFive/KytyPS5 → Orbis, usando o Orbis Eden como base (11/11/2026)

## Resposta curta
**Sim, o Orbis Eden serve, e muito.** Ele resolve justamente os bloqueios que eu tinha apontado na v1:
o Vulkan (RADV pro PS4), a saída de vídeo, o áudio, o controle, o tratamento de falhas, o pacote `.pkg`
e a lista de armadilhas do SDK. Não precisa reescrever nada disso.

## O que o Eden já resolve → onde entra no KytyPS5
| Bloqueio (v1) | O que o Eden tem | Uso no KytyPS5 |
|---|---|---|
| Sem Vulkan no Orbis | **orbis-sdk-v1**: Mesa RADV (Vulkan 1.3, "Liverpool" GFX7) + `vkloader`; apresentação por `VK_EXT_headless_surface` (cada present vira um flip na TV) | `graphics/presentation/window/vulkanWindow.cpp` tem só 3 chamadas SDL de Vulkan (`GetInstanceExtensions`, `GetVkGetInstanceProcAddr`, `CreateSurface`). Trocar por: extensões `VK_KHR_surface` + `VK_EXT_headless_surface`, `vkGetInstanceProcAddr` do vkloader e `vkCreateHeadlessSurfaceEXT` (o patch 0001 do Eden faz exatamente isso) |
| SDL3 inexistente | `ps4_sink` (sceAudioOut), `OpenPad/ReadPad` (scePad), `sceVideoOut` | Feito aqui: `orbis/sdl3_shim` cobre o SDL que `libs/audio`, `controller`, `libCes` e `libFont` usam. Falta o `hostInput.cpp` (58 símbolos SDL) trocado por `Ps4::ReadPad()` |
| Sinais/falhas | Faltas chegam por `sceKernelInstallExceptionHandler`, não por `sigaction`; mcontext em +64, `rip` +160, `rsp` +184, endereço da falha +136 (medido no console) | Feito: `common/orbis/mcontext.h` usa esses offsets (com `static_assert`); `hostException.cpp` e `libKernel.cpp` instalam pelo handler do kernel; sinal entre threads via `sceKernelRaiseException` |
| `/proc/self/maps` | `orbis_lazy_memory` (reserva + commit sob demanda de 64 KiB) e `sceKernelReserveVirtualRange` | Ainda por fazer: `memoryAddressSpace.inc` precisa do mesmo esquema. Limites medidos: maior reserva 16 GiB; 8 GiB cabem junto do resto |
| JIT/patcher AVX2 | Código RW → `sceKernelMprotect` RWX (só nessa ordem), via memória direta | Vale pro loader/patcher e pro xbyak do KytyPS5 |
| Memória | Pacote no layout do OpenOrbis (libc.prx + libSceFios2.prx, `gd`) dá ~4,4 GiB de memória direta | Reusar `package-eden.sh` |
| libc/libc++ | `ci/30-libcxx.sh`, `libc_wide_fixes.cpp` (o `wmemchr` do SDK é de 16 bits e corrompe buscas em tipos de 4 bytes), TLS emulado | Incluir `libc_wide_fixes.cpp` e `-femulated-tls` no executável do KytyPS5 |
| Diagnóstico | `boot.log`, crash handler com backtrace, watchdog, perfilador | Reusar `ps4_platform.cpp` inteiro |
| Menu / ícone | `rom_menu`, ícone do Tito | Reusar como seletor de jogo |

## Estado verificado (na minha máquina, sem PS4)
- **90 arquivos** do núcleo (`common`, `kernel`, `loader`, `libs` sem FFmpeg) **compilam para objetos** e viram `libkyty_core.a`, com a mesma triple (`x86_64-pc-freebsd12-elf`), `-D__ORBIS__` e `-U__FreeBSD__` do Eden e libc++ 18.
- A verificação usou os cabeçalhos do musl do OpenOrbis, não os do `orbis-sdk-v1` (os binários do SDK estão em releases do GitHub, que não consigo baixar daqui). No seu PC podem aparecer pequenas diferenças de cabeçalho.
- **Não linkado, não executado.**

## O que falta para linkar o primeiro executável (lista exata, 74 símbolos)
- `Libs::VideoOut` (38) e `Libs::Graphics` (29) + `Libs::Vr` (2): é a camada gráfica. Para o 1º teste, stubs; depois, o `graphics/` real sobre o RADV.
- Módulos deixados de fora por precisarem de FFmpeg: `avPlayer`, `libAudio`, `libVideoDec2`, `libVdecsw`, `ajm`, `libFont`. O FFmpeg do Eden (`ci/40-openssl-ffmpeg.sh`) só tem h264/vp8/vp9 e sem avformat/swresample; para esses módulos precisa ser recompilado com mais coisas. Não é necessário para o Snes9x PS5.

## Riscos reais
1. **GPU.** Os jogos de PS5 usam recursos de RDNA2 (mesh shader, maintenance8, feedback loop...). O RADV do PS4 é GCN: parte dessas extensões não existe e o KytyPS5 precisa tratá-las como opcionais. Jogos 2D/simples são o alvo realista.
2. **Memória.** ~4,4 GiB de memória direta no total, incluindo o driver da GPU (o Eden chegou a ~100 MiB livres numa corrida de MK8D).
3. **Espaço de endereços do guest.** O KytyPS5 quer endereços fixos de PS5; o Orbis só deixa reservar até 16 GiB de uma vez. Ainda não conferi quanto o KytyPS5 pede.
4. **Desempenho.** O KytyPS5 roda o código do jogo nativamente (não emula CPU como o Eden), mas o Jaguar não tem AVX2, então o patcher (patch 0004) vai ser quente. Sem medição.
5. Firmware: o Eden foi medido só no 12.02; o seu console é o 13.52.

## Próximos passos
1. No seu PC: clonar o KytyPS5 (commit 589bd14), aplicar os patches 0001-0005 do EmuFive e `patches/0100-orbis-port.patch`.
2. Configurar com `CMakeLists.orbis.txt` e o `toolchain/eden-ps4.cmake` do Eden (ver `README-ORBIS.md`).
3. Eu escrevo os stubs de `VideoOut`/`Graphics` e o `main` do Orbis (reusando `ps4_platform`), para o primeiro ELF linkar e rodar até o log de boot.
4. Depois: janela Vulkan sobre o RADV, `hostInput` com `scePad`, e o espaço de endereços.
