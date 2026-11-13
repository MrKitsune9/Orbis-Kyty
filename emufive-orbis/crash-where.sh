#!/usr/bin/env bash
# Traduz as posicoes de /data/kyty/crash.log para nomes de funcao.
# Uso:  bash emufive-orbis/crash-where.sh crash.log [caminho-do-kyty-ps4]
LOG="${1:?uso: crash-where.sh crash.log [elf]}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ELF="${2:-$ROOT/emufive-orbis/build-kyty/bin/kyty-ps4}"
TOOLS="$(cd "$ROOT/.." && pwd)/tools/llvm/bin"; export PATH="$TOOLS:$PATH"
main_rt=$(grep -o 'main=[0-9a-f]*' "$LOG" | head -1 | cut -d= -f2)
main_sym=$(llvm-nm "$ELF" | awk '$3=="main"{print $1}')
[ -n "$main_rt" ] && [ -n "$main_sym" ] || { echo "nao achei main no log/ELF"; exit 1; }
base=$(( 0x$main_rt - 0x$main_sym ))
echo "base do programa: $(printf '%x' $base)"
for a in $(grep -oE '(rip|ret)=[0-9a-f]+' "$LOG" | cut -d= -f2); do
  off=$(printf '%x' $(( 0x$a - base )))
  echo "$a -> $(llvm-symbolizer --obj="$ELF" --demangle --no-inlines 0x$off 2>/dev/null | head -2 | tr '\n' ' ')"
done
