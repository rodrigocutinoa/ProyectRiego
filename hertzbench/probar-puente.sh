#!/usr/bin/env bash
# Prueba el puente IP de hertzbench (MQTT en 127.0.0.1:1883) desde el host.
# Guarda todo en hertzbench/logs/<fecha>/ para reportar errores al docente.
#
# Uso: hertzbench/probar-puente.sh [escenario.yaml] [segundos]
set -u
DIR="$(cd "$(dirname "$0")" && pwd)"
ESC="${1:-$DIR/riego-mqtt.yaml}"
SEG="${2:-40}"
HZB="${HZB:-hertzbench}"
LOG="$DIR/logs/$(date +%Y-%m-%d_%H%M%S)"
mkdir -p "$LOG/out"

{
  echo "fecha: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "sistema: $(sw_vers -productName 2>/dev/null) $(sw_vers -productVersion 2>/dev/null) $(uname -m)"
  echo "hertzbench: $(readlink -f "$(command -v "$HZB")" 2>/dev/null)"
  echo "hertzbench commit: $(git -C "$(dirname "$(readlink -f "$(command -v "$HZB")")")" rev-parse --short HEAD 2>/dev/null)"
  echo "deno: $(deno --version 2>/dev/null | head -1)"
  echo "mosquitto: $(mosquitto_sub --help 2>/dev/null | head -1)"
  echo "escenario: $ESC"
} > "$LOG/entorno.txt"
cp "$ESC" "$LOG/"

fallas=0
falla() { echo "FALLA: $*" | tee -a "$LOG/resultado.txt"; fallas=$((fallas + 1)); }
ok()    { echo "OK: $*"    | tee -a "$LOG/resultado.txt"; }

"$HZB" validate "$ESC" > "$LOG/validate.txt" 2>&1 || { falla "validate (ver validate.txt)"; exit 2; }
ok "validate"

"$HZB" run "$ESC" --out "$LOG/out" > "$LOG/run.stdout.txt" 2> "$LOG/run.stderr.txt" &
PID=$!

# Esperar a que el puente escuche en 1883.
for _ in $(seq 1 60); do
  nc -z 127.0.0.1 1883 2>/dev/null && break
  kill -0 $PID 2>/dev/null || break
  sleep 0.5
done
if nc -z 127.0.0.1 1883 2>/dev/null; then ok "puente escuchando en 127.0.0.1:1883"; else falla "el puente no abrió 127.0.0.1:1883"; fi

mosquitto_sub -h 127.0.0.1 -p 1883 -t 'riego/#' -v -i host-sub > "$LOG/mqtt-sub.txt" 2> "$LOG/mqtt-sub.err.txt" &
SUB=$!
sleep 3
mosquitto_pub -h 127.0.0.1 -p 1883 -t riego/cmd -m 'CMD;bomba;ON' -i host-pub > "$LOG/mqtt-pub.txt" 2>&1 \
  && ok "publish riego/cmd desde el host" || falla "publish riego/cmd desde el host (ver mqtt-pub.txt)"
sleep $((SEG - 3))

kill -INT $SUB 2>/dev/null
kill -INT $PID 2>/dev/null
wait $PID; echo "exit hertzbench: $?" >> "$LOG/entorno.txt"
wait $SUB 2>/dev/null

grep -q "riego/lectura LECTURA;suelo;" "$LOG/mqtt-sub.txt" && ok "el host recibe riego/lectura" || falla "el host no recibió riego/lectura"
grep -q "^riego/estado/" "$LOG/mqtt-sub.txt" && ok "el host recibe riego/estado" || falla "el host no recibió riego/estado"
grep -q "riego/cmd CMD;bomba;ON" "$LOG/mqtt-sub.txt" && ok "el host ve su propio riego/cmd" || falla "el host no vio riego/cmd"
grep -q "mqtt rx riego/cmd CMD;bomba;ON" "$LOG/out"/serial-nodo-riego.log 2>/dev/null \
  && ok "nodo-riego recibe el comando del host" || falla "nodo-riego no recibió el comando del host"

echo "logs: $LOG"
[ $fallas -eq 0 ] && { echo "RESULTADO: PASS" | tee -a "$LOG/resultado.txt"; exit 0; }
echo "RESULTADO: FAIL ($fallas)" | tee -a "$LOG/resultado.txt"; exit 1
