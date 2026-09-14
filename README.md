# SYNTH

> Sempre pronto, sempre incompleto.

SYNTH nasce como um sistema aberto, autônomo e observável. Esta realização
fundacional contém somente identidade, CLI, separação XDG, auto-observação,
evidência factual e modelos genéricos de superfícies e relações. Nenhum
ecossistema ou frontend é simulado.

## Construção C++26

Requer um compilador com suporte ao modo C++26, CMake 3.25+,
`nlohmann/json` 3.11+, OpenSSL 3.0+, GNU tar e Linux. O CMake usa `jsoncons`
1.9+ instalado ou adquire a versão 1.9.0 fixada por hash. O runtime executa os
schemas Draft 2020-12 publicados; a suíte usa Python 3 com `jsonschema` como
validador independente.

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
./build/bin/synth ecosystem
./build/bin/synth ecosystem --watch --json
./build/bin/synth resolve synth.cli
./build/bin/synth evidence
```

A saída é legível por pessoas por padrão. Acrescente `--json` para obter a
representação computável. Cada gate informa sua classe epistemológica e a
evidência usada para derivá-lo.

## Realizações externas

A primeira fatia vertical aceita artefatos `tar` publicados por uma fonte local:

```bash
SOURCE=tests/fixtures/local-source

./build/bin/synth search synth-web --source "$SOURCE"
./build/bin/synth info synth-web --source "$SOURCE"
./build/bin/synth install synth-web --source "$SOURCE"
./build/bin/synth installed

./build/bin/synth evidence
./build/bin/synth activate synth-web
./build/bin/synth relations
./build/bin/synth surfaces

./build/bin/synth deactivate synth-web
./build/bin/synth remove synth-web
```

`install` adquire um snapshot estável, verifica SHA-256 e extrai exatamente os
mesmos bytes para o store imutável, sem ativar. `activate` fixa snapshots e
digests das superfícies de arquivo requeridas, inicia uma candidata isolada,
aplica um timeout real à readiness e valida integralmente seu witness antes da
promoção. Relações só aparecem como `OBSERVED` quando a atestação do participante
corresponde ao digest resolvido pelo SYNTH.

Manifestos e witnesses são validados no runtime diretamente contra os JSON
Schemas instalados. O código C++ acrescenta apenas invariantes operacionais que
o schema não expressa, como confinamento de caminhos, unicidade semântica de IDs
e correspondência entre witness, processo, manifesto e evidência resolvida.

As operações mutantes são serializadas por um lock global nesta versão;
consultas permanecem paralelas e sem persistência.

## Projeção do ecossistema

`synth ecosystem --json` deriva uma fotografia versionada dos participantes
registrados, superfícies indexadas e relações observadas. Instalações aparecem
como `INSTALLED/DECLARED`; somente witnesses de realizações ativas produzem
provedores `ACTIVE/OBSERVED`.

`synth resolve <surface> --json` implementa descoberta tardia por capacidade e
retorna todos os provedores ativos observados, sem conhecimento prévio de suas
identidades. A geração da projeção é um SHA-256 da topologia e permanece estável
enquanto os fatos projetados não mudam. A mesma projeção também integra o
documento produzido por `synth evidence`.

`synth ecosystem --watch --json` publica um stream NDJSON somente leitura: a
primeira projeção é imediata e novas linhas aparecem apenas quando `generation`
muda. O núcleo expõe esse stream como `synth.ecosystem.stream.v1`. Participantes
podem solicitar seu descritor versionado por `SYNTH_ECOSYSTEM_STREAM`, sem shell,
daemon, Unix socket ou HTTP no núcleo.

`interface.human.web.v1` é a superfície semântica compartilhada para interfaces
humanas locais. O contrato exige `kind=http` e `media_type=text/html`; múltiplos
provedores ativos podem coexistir e são retornados deterministicamente.

`synth status` descreve a realização local. A aceitação do estado no repositório
também exige o workflow `foundation` verde no `main`.

## Instalação autônoma

```bash
cmake --install build --prefix /caminho/de/instalacao
/caminho/de/instalacao/bin/synth foundation verify
```

Os documentos normativos, a licença e os JSON Schemas Draft 2020-12 são instalados em
`share/synth`. O binário os resolve relativamente à própria localização e não
depende do checkout usado na compilação.

## Estado e configuração

```text
$XDG_CONFIG_HOME/synth/   somente configuração declarada pelo usuário; pode não existir
$XDG_DATA_HOME/synth/     store de artefatos imutáveis
$XDG_STATE_HOME/synth/    evidência observacional persistente
$XDG_RUNTIME_DIR/synth/   estado efêmero do processo
```

`version`, `help`, `status`, `surfaces`, `relations`, `ecosystem`, `resolve` e
`foundation verify` não persistem dados. Somente `evidence` cria o estado
necessário e grava uma nova observação atômica. Na ausência de configuração
humana, a evidência registra `configuration: null`.

## Documentação normativa

- `docs/SYNTH-FOUNDATION-001-v0.4.0.md`
- `docs/SYNTH-PATCH-FOUNDATION-001-v0.1.0.md`
- `docs/SYNTH-FOUNDATION-CORRECTION-001-v0.1.0.md`
- `docs/SYNTH-FOUNDATION-HARDENING-001-v0.1.0.md`
- `docs/SYNTH-REALIZATION-001-v0.1.0.md`
- `docs/SYNTH-ECOSYSTEM-PROJECTION-001-v0.1.0.md`
- `docs/SYNTH-LIVE-ECOSYSTEM-001-v0.1.0.md`

## Licença

GPL-3.0-only. Veja `LICENSE`.
