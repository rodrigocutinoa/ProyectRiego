# Bug: `deno task compile` fails with Homebrew's Deno (NotCapable on `Deno.execPath()`)

Issue para hertzbench. Estado: **enviado** el 2026-11-16 como [#109](https://github.com/Rixmerz/hertzbench/issues/109) (reproducido también en `7f5bd4ae`).

**English**

`deno task compile` stops before compiling anything when Deno comes from Homebrew on macOS.

**Environment**

- macOS 27.0.1, Apple Silicon (arm64), MacBook Air M2
- Deno 2.9.8 from `brew install deno` (`aarch64-apple-darwin`)
- hertzbench `main` at `19847355`

**Steps**

```bash
brew install deno
git clone https://github.com/Rixmerz/hertzbench.git
cd hertzbench/hertzbench
deno task compile macos-arm64
```

**Result**

```
compile macos-arm64 -> .../hertzbench/hertzbench/dist/hertzbench-macos-arm64
error: Uncaught (in promise) NotCapable: Requires run access to "/opt/homebrew/Cellar/deno/2.9.8/bin/deno", run again with the --allow-run flag
  }).output();
     ^
    at file:///.../hertzbench/hertzbench/scripts/compile.ts:80:6
```

**Cause**

`/opt/homebrew/bin/deno` is a symlink to `../Cellar/deno/2.9.8/bin/deno`. `scripts/compile.ts` spawns
`Deno.execPath()`, which returns the resolved Cellar path, while the task grants only
`--allow-run=deno`, which resolves to `/opt/homebrew/bin/deno`. The two paths differ, so the spawn
is denied. The curl installer (`~/.deno/bin/deno`, not a symlink) does not hit this.

**Workaround (works)**

```bash
deno run --allow-read --allow-write=dist --allow-run --allow-env --allow-net scripts/compile.ts macos-arm64
```

The binary builds (Mach-O arm64, 71 MB) and `run scenarios/examples/udp-3-scripted.yaml` passes 5/5.

**Possible fix**

Spawn `"deno"` instead of `Deno.execPath()` in `scripts/compile.ts`, so it matches the permission,
or keep `Deno.execPath()` and widen the task's permission.

---

**Español**

`deno task compile` se detiene antes de compilar cuando Deno viene de Homebrew en macOS.
`/opt/homebrew/bin/deno` es un enlace simbólico a la carpeta `Cellar`. El script lanza
`Deno.execPath()`, que devuelve la ruta real, y el permiso `--allow-run=deno` no la cubre. Se
resuelve corriendo `scripts/compile.ts` con `--allow-run`, y el binario funciona. Arreglo posible:
lanzar `"deno"` en lugar de `Deno.execPath()`.
