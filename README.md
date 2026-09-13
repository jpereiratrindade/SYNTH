# SYNTH

> Sempre pronto, sempre incompleto.

SYNTH nasce como um sistema aberto, autônomo e observável. Esta realização
fundacional contém somente identidade, CLI, separação XDG, auto-observação,
evidência factual e modelos genéricos de superfícies e relações. Nenhum
ecossistema ou frontend é simulado.

## Construção C++26

Requer um compilador com suporte ao modo C++26, CMake 3.25+ e Linux.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

A CI usa explicitamente a imagem `gcc:16`; não depende do compilador padrão do
runner.

## CLI

```bash
./build/bin/synth version
./build/bin/synth status
./build/bin/synth foundation verify
./build/bin/synth surfaces
./build/bin/synth relations
./build/bin/synth evidence
```

A saída é legível por pessoas por padrão. Acrescente `--json` para obter a
representação computável. Cada gate informa sua classe epistemológica e a
evidência usada para derivá-lo.

`synth status` descreve a realização local. A aceitação do estado no repositório
também exige o workflow `foundation` verde no `main`.

## Instalação autônoma

```bash
cmake --install build --prefix /caminho/de/instalacao
/caminho/de/instalacao/bin/synth foundation verify
```

Os documentos normativos, a licença e os esquemas são instalados em
`share/synth`. O binário os resolve relativamente à própria localização e não
depende do checkout usado na compilação.

## Estado e configuração

```text
$XDG_CONFIG_HOME/synth/   configuração e realização
$XDG_STATE_HOME/synth/    evidência observacional persistente
$XDG_RUNTIME_DIR/synth/   estado efêmero do processo
```

Uma segunda execução pode produzir nova evidência — PID, timestamp, CPU e
memória mudam legitimamente — sem alterar configuração ou realização.

## Documentação normativa

- `docs/SYNTH-FOUNDATION-001-v0.4.0.md`
- `docs/SYNTH-PATCH-FOUNDATION-001-v0.1.0.md`
- `docs/SYNTH-FOUNDATION-CORRECTION-001-v0.1.0.md`

## Licença

GPL-3.0-only. Veja `LICENSE`.
