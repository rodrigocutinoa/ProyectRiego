#!/usr/bin/env bash
# Prepara un Mac (Apple Silicon o Intel) para trabajar con hertzbench en ProyectRiego:
#   1. Instala Deno y los clientes de mosquitto con Homebrew.
#   2. Clona (o actualiza) hertzbench al lado de este repo.
#   3. Compila el binario para este Mac.
#   4. Lo enlaza como `hertzbench` y `hzb` en ~/.local/bin.
#   5. Corre probar-puente.sh para comprobar el puente MQTT en 1883.
#   6. Instala Android Studio con Homebrew, si falta (SIN_ANDROID_STUDIO=1 lo omite).
#
# El MCP no hace falta registrarlo: el .mcp.json del repo lo carga al abrir
# Claude Code en esta carpeta (hay que aprobarlo la primera vez).
#
# Uso: hertzbench/preparar-mac.sh
# Los errores quedan en hertzbench/logs/preparar_<fecha>.log para reportarlos.
set -euo pipefail

DIR="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$DIR/.." && pwd)"
HZB_REPO="${HZB_REPO:-$(dirname "$REPO")/hertzbench}"
BIN_DIR="$HOME/.local/bin"
mkdir -p "$DIR/logs" "$BIN_DIR"
LOG="$DIR/logs/preparar_$(date +%Y-%m-%d_%H%M%S).log"
exec > >(tee -a "$LOG") 2>&1
trap 'echo; echo "ERROR en la línea $LINENO. Log completo: $LOG"' ERR

case "$(uname -m)" in
  arm64) TARGET=macos-arm64 ;;
  x86_64) TARGET=macos-x64 ;;
  *) echo "Arquitectura no soportada: $(uname -m)"; exit 2 ;;
esac
echo "== Mac $(sw_vers -productVersion) $(uname -m) -> $TARGET"

command -v brew >/dev/null || { echo "Falta Homebrew: https://brew.sh"; exit 2; }
echo "== 1. Deno y mosquitto"
command -v deno >/dev/null || brew install deno
command -v mosquitto_sub >/dev/null || brew install mosquitto
deno --version | head -1

echo "== 2. hertzbench en $HZB_REPO"
if [ -d "$HZB_REPO/.git" ]; then
  git -C "$HZB_REPO" pull --ff-only
else
  git clone https://github.com/Rixmerz/hertzbench.git "$HZB_REPO"
fi
echo "commit: $(git -C "$HZB_REPO" rev-parse --short HEAD)"

echo "== 3. Compilar $TARGET"
# Se usa --allow-run completo en vez de `deno task compile`, que falla con el
# Deno de Homebrew (ver hertzbench/reportes/2026-11-13_compile-homebrew.md).
cd "$HZB_REPO/hertzbench"
NO_COLOR=1 deno run --allow-read --allow-write=dist --allow-run --allow-env --allow-net \
  scripts/compile.ts "$TARGET" | grep -v "^\s*[│├└]" || true
BIN="$HZB_REPO/hertzbench/dist/hertzbench-$TARGET"
[ -x "$BIN" ] || { echo "No se generó $BIN"; exit 1; }
file "$BIN"

echo "== 4. Enlazar en $BIN_DIR"
ln -sf "$BIN" "$BIN_DIR/hertzbench"
ln -sf "$BIN" "$BIN_DIR/hzb"
case ":$PATH:" in
  *":$BIN_DIR:"*) ;;
  *) echo "Ojo: $BIN_DIR no está en el PATH. Agrega a ~/.zshrc: export PATH=\"\$HOME/.local/bin:\$PATH\"" ;;
esac
export PATH="$BIN_DIR:$PATH"

echo "== 5. Probar el puente MQTT"
"$BIN" run "$HZB_REPO/hertzbench/scenarios/examples/udp-3-scripted.yaml" \
  --out "$DIR/logs/udp-3-scripted" --quiet
"$DIR/probar-puente.sh"

echo "== 6. Android Studio"
if [ -d "/Applications/Android Studio.app" ]; then
  echo "ya instalado"
elif [ "${SIN_ANDROID_STUDIO:-0}" = 1 ]; then
  echo "omitido (SIN_ANDROID_STUDIO=1)"
else
  brew install --cask android-studio
fi

echo
echo "Listo. Reinicia VS Code / Claude Code y aprueba el MCP 'hertzbench' del .mcp.json."
echo "Luego abre Android Studio una vez (asistente Standard) para descargar el SDK."
echo "Log: $LOG"
