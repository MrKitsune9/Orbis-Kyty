# EmuFive 1.0.3 e o port Orbis

Mudanças relevantes em relação à 1.0.0 (mesmo commit do KytyPS5, 589bd14):
- **Dois patches novos**: `0006` (sem `VK_EXT_conditional_rendering` nas GPUs do PS4, device IDs 0x9920-0x9924) e `0007` (instruções curtas que o patcher AVX2 não consegue reescrever pulam por um "relay" alocado perto do código).
- **Jogos reais rodando** no PS4 Pro (via Linux): God of War Sons of Sparta (~40 FPS) e Hollow Knight: Silksong. Isso mostra que o KytyPS5 funciona com o RADV da GPU do PS4, que é o mesmo caminho gráfico do port Orbis.

Verificado aqui:
- `0006` e `0007` aplicam em cima de 0001-0005, e o `0100-orbis-port.patch` aplica em cima dos sete.
- O núcleo (90 arquivos) continua compilando para `libkyty_core.a` com os sete patches + 0100.

O que muda para o port Orbis:
- **`0006` confirma o que o Eden já achou**: a conditional rendering trava a GPU GFX7. O RADV do Orbis precisa da mesma proteção; a checagem por device ID do `0006` pode não bater com o ID que o RADV do orbis-ports reporta (conferir no `mesa.log`/`boot.log`).
- **`0007` mexe em memória executável**: o relay é alocado por `Common::VirtualMemory::AllocFixed(..., ExecuteReadWrite)` numa janela de 16 MB perto do código. No Orbis, memória executável só se obtém mapeando memória direta como RW e depois `sceKernelMprotect` para RWX (nessa ordem), e o endereço é fixo. Ou seja, `sysLinuxVirtual.cpp` precisa de uma versão Orbis de verdade, não só compilar. Entra na mesma tarefa do espaço de endereços do guest.
