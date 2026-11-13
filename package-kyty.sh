#!/usr/bin/env bash
# build-kyty/kyty-ps4 (ELF) -> out-kyty/IV0000-KYPS00001_00-KYTYPS4000000000.pkg
#
# Copiado do package-eden.sh (mesmo layout, que é o único testado no seu PS4 Pro com GoldHEN e que
# dá os ~4,4 GiB de memória direta). Rodar no Git Bash, na raiz do repositório do Eden, depois de o
# ELF do KytyPS5 existir. NÃO foi testado: ainda não há ELF para empacotar.
set -e
ROOT="$(cd "$(dirname "$0")" && pwd)"
# EDEN = repo do Eden (tem sdk-dl/ e assets/). Padrao: /c/nxorbis/repo.
EDEN="${EDEN:-/c/nxorbis/repo}"
[ -d "$EDEN/sdk-dl" ] || EDEN="$ROOT"
SDK="$EDEN/sdk-dl/orbis-sdk-v1/sdk"
BIN="$SDK/bin/windows"
OO="$(cygpath -m "$(cd "$ROOT/.." && pwd)")/tools/OpenOrbis/OpenOrbis/PS4Toolchain"
[ -d "$OO" ] || OO="C:/nxorbis/tools/OpenOrbis/OpenOrbis/PS4Toolchain"
export DOTNET_ROLL_FORWARD=LatestMajor
export OO_PS4_TOOLCHAIN="$(cygpath -m "$SDK")"

TITLE="Orbis-Kyty"
TITLE_ID="KYPS00001"                              # 9 caracteres
CID="IV0000-${TITLE_ID}_00-KYTYPS4000000000"       # exatamente 36 caracteres
ELF="${ELF:-$ROOT/build-kyty/bin/kyty-ps4}"
ST="$ROOT/stage-kyty"
OUT="$ROOT/out-kyty"

[ -f "$ELF" ] || { echo "faltou o ELF: $ELF"; exit 1; }
rm -rf "$ST"; mkdir -p "$ST/sce_sys/about" "$ST/sce_module" "$OUT"
cp "$EDEN/assets/icon0.png" "$ST/sce_sys/icon0.png"          # 512x512
"$BIN/create-fself.exe" -in="$(cygpath -m "$ELF")" -out="$(cygpath -m "$ST/x.oelf")" \
    --eboot "$(cygpath -m "$ST/eboot.bin")" --paid 0x3800000000000011
rm -f "$ST/x.oelf"
cp "$OO/samples/piglet/sce_sys/about/right.sprx" "$ST/sce_sys/about/"
cp "$OO/samples/piglet/sce_module/libc.prx" "$OO/samples/piglet/sce_module/libSceFios2.prx" "$ST/sce_module/"
FILES="eboot.bin sce_sys/param.sfo sce_sys/icon0.png sce_sys/about/right.sprx sce_module/libc.prx sce_module/libSceFios2.prx"
# Arquivos de dados do emulador (create-gp4 só aceita algumas pastas na raiz; use assets/misc).
# Os jogos de PS5 NÃO entram no pacote: ficam no USB (/mnt/usb0) ou em /data e são apontados
# pelo menu/KYTY_MOUNTS.
if [ -d "$ROOT/assets/kyty" ]; then
    mkdir -p "$ST/assets/misc"; cp -r "$ROOT/assets/kyty/." "$ST/assets/misc/"
    for f in $(cd "$ST" && find assets/misc -type f); do FILES="$FILES $f"; done
fi
(
    cd "$ST"
    P="$BIN/PkgTool.Core.exe"; SFO=sce_sys/param.sfo
    "$P" sfo_new $SFO >/dev/null
    s() { "$P" sfo_setentry $SFO "$1" --type "$2" --maxsize "$3" --value "$4" >/dev/null; }
    s APP_TYPE Integer 4 1
    s APP_VER Utf8 8 "01.00"
    s ATTRIBUTE Integer 4 0
    s CATEGORY Utf8 4 gd
    s SYSTEM_VER Integer 4 0
    s CONTENT_ID Utf8 48 "$CID"
    s DOWNLOAD_DATA_SIZE Integer 4 0
    s TITLE Utf8 128 "$TITLE"
    s TITLE_ID Utf8 12 "$TITLE_ID"
    s VERSION Utf8 8 "01.00"
    "$BIN/create-gp4.exe" -out pkg.gp4 --content-id="$CID" --files "$FILES" >/dev/null
    "$P" pkg_build pkg.gp4 "$(cygpath -m "$OUT")" >/dev/null
)
mkdir -p "$ROOT/elf"; cp "$ELF" "$ROOT/elf/kyty-ps4-$(date +%Y%m%d-%H%M).elf"
ls -la "$OUT"
