# SYNTH

> Sempre pronto, sempre incompleto.

SYNTH nasce como um sistema aberto, autônomo e observável. Esta realização
fundacional materializa somente o que existe agora: identidade, CLI, separação
XDG, auto-observação, evidência, superfícies genéricas, relações observadas e
uma interface web humana. Nenhum ecossistema futuro é simulado.

## Estado

```text
FOUNDATION_READY       PASS
SYSTEM_SYNTH_READY     PASS
ECOSYSTEM_SYNTH        NOT_YET_APPLICABLE
```

## Construção C++26

Requer GCC 15+ ou outro compilador com suporte ao modo C++26, CMake 3.30+ e um
ambiente POSIX.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## CLI

```bash
./build/synth version
./build/synth status
./build/synth foundation verify
./build/synth surfaces
./build/synth relations
./build/synth evidence
```

A saída é legível por pessoas por padrão. Acrescente `--json` a qualquer
comando de consulta para obter uma representação computável.

## Estado e configuração

SYNTH respeita a separação XDG:

```text
$XDG_CONFIG_HOME/synth/
$XDG_STATE_HOME/synth/
$XDG_RUNTIME_DIR/synth/
```

Quando uma variável não está definida, são usados os equivalentes XDG
convencionais; para runtime, um diretório por usuário sob `/tmp` é adotado.

## Interface web

Requer Node.js 22.13+.

```bash
npm install
npm run dev
```

A interface é uma projeção do estado real da realização fundacional. Ela
expõe estado, superfícies, evidência e gates sem desenhar relações inexistentes.

## Documentação normativa

- `docs/SYNTH-FOUNDATION-001-v0.4.0.md`
- `docs/SYNTH-PATCH-FOUNDATION-001-v0.1.0.md`

## Licença

GPL-3.0-only. Veja `LICENSE`.
