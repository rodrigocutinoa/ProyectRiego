// Cifra un mensaje de RiegoIoT-App igual que la app (Hito 4), para publicarlo desde
// hertzbench (mqtt_publish) o ponerlo en un escenario.
//
//   deno run hertzbench/cifrar.ts <código> <tópico> "<TIPO;clave;valor>"
//   deno run hertzbench/cifrar.ts 123456 riego/cmd "CMD;bomba;ON"
//
// Esquema (RiegoIoT-App, seguridad/Cripto.kt y enlace/MqttEnlace.kt):
//   clave  = PBKDF2-SHA256(código, sal "RiegoIoT-v1", 120 000 iteraciones, 256 bits)
//   carga  = Base64(iv de 12 bytes + AES-GCM("<hora en ms>|TIPO;clave;valor", AAD = tópico))
// El nodo rechaza los comandos (CMD) con más de 30 s: generarlos justo antes de publicarlos.

const [codigo, topico, linea] = Deno.args;
if (!/^\d{6}$/.test(codigo ?? "") || !topico || !linea) {
  console.error('uso: deno run hertzbench/cifrar.ts <código de 6 dígitos> <tópico> "<TIPO;clave;valor>"');
  Deno.exit(2);
}

const enc = new TextEncoder();
const base = await crypto.subtle.importKey("raw", enc.encode(codigo), "PBKDF2", false, ["deriveKey"]);
const clave = await crypto.subtle.deriveKey(
  { name: "PBKDF2", hash: "SHA-256", salt: enc.encode("RiegoIoT-v1"), iterations: 120_000 },
  base,
  { name: "AES-GCM", length: 256 },
  false,
  ["encrypt"],
);
const iv = crypto.getRandomValues(new Uint8Array(12));
const cifrado = new Uint8Array(
  await crypto.subtle.encrypt(
    { name: "AES-GCM", iv, additionalData: enc.encode(topico), tagLength: 128 },
    clave,
    enc.encode(`${Date.now()}|${linea}`),
  ),
);
const todo = new Uint8Array(iv.length + cifrado.length);
todo.set(iv);
todo.set(cifrado, iv.length);
console.log(btoa(String.fromCharCode(...todo)));
