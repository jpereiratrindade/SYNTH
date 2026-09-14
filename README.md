# SYNTH

> Sempre pronto, sempre incompleto.

SYNTH nasce como um sistema aberto, autônomo e observável. Esta realização
fundacional contém somente identidade, CLI, separação XDG, auto-observação,
evidência factual e modelos genéricos de superfícies e relações. Nenhum
ecossistema ou frontend é simulado.

## Requisitos

O build requer Linux, CMake 3.25+, um compilador com suporte ao modo C++26,
`nlohmann/json` 3.11+ e OpenSSL 3.0+. A configuração é verificada na CI com
GCC 16.

O runtime usa GNU tar para inspecionar e extrair artefatos. Os testes também
requerem Python 3 com `jsonschema`. O CMake usa `jsoncons` 1.9+ instalado ou,
na primeira configuração, baixa a versão 1.9.0 fixada por SHA-256. Portanto,
um build sem `jsoncons` instalado precisa de acesso à rede durante o configure.

Fedora Silverblue 44:

```bash
sudo rpm-ostree install gcc-c++ cmake json-devel openssl-devel python3-jsonschema
systemctl reboot
```

O pacote `json-devel` fornece `nlohmann/json`. O reboot só é necessário quando
o `rpm-ostree` cria um novo deployment em vez de aplicar os pacotes ao vivo.

Debian/Ubuntu, usando um compilador que aceite C++26:

```bash
sudo apt-get update
sudo apt-get install g++ cmake libssl-dev nlohmann-json3-dev python3-jsonschema tar
```

## Construção e testes

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Uso básico

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
representação computável dos comandos que a oferecem. `status` e
`foundation verify` informam a classe epistemológica e a evidência usada para
derivar cada gate.

## Realizações externas

A primeira fatia vertical aceita artefatos `tar` publicados por uma fonte
local. O exemplo abaixo isola todo o estado em um diretório temporário e encerra
a realização quando a sequência termina ou falha:

```bash
(
  set -euo pipefail
  DEMO_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/synth-demo.XXXXXX")" || exit 1
  export XDG_CONFIG_HOME="$DEMO_ROOT/config"
  export XDG_DATA_HOME="$DEMO_ROOT/data"
  export XDG_STATE_HOME="$DEMO_ROOT/state"
  export XDG_RUNTIME_DIR="$DEMO_ROOT/runtime"

  cleanup() {
    ./build/bin/synth deactivate synth-web >/dev/null 2>&1 || true
    chmod -R u+w -- "$DEMO_ROOT" 2>/dev/null || true
    rm -rf -- "${DEMO_ROOT:?}"
  }
  trap cleanup EXIT

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
)
```

`evidence` vem antes de `activate` porque o fixture requer a superfície factual
`synth.evidence`. A ativação inicia um processo local; `deactivate` deve ser
executado para encerrá-lo. `remove` apaga o registro da instalação, mas preserva
o artefato no store imutável para futura coleta de lixo. No exemplo, `cleanup`
remove todo o ambiente temporário.

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

## Referência da CLI

```text
synth help
synth version [--json]
synth status [--json]
synth foundation verify [--json]
synth surfaces [--json]
synth relations [--json]
synth ecosystem [--json]
synth ecosystem --watch --json
synth resolve <surface> [--json]
synth evidence [--json]
synth search <query> --source <source> [--json]
synth info <identity> --source <source> [--json]
synth install <identity> --source <source> [--json]
synth installed [--json]
synth activate <identity> [--json]
synth deactivate <identity> [--json]
synth remove <identity> [--json]
```

`--json` e `--watch` podem aparecer antes ou depois dos argumentos. `--watch`
é exclusivo de `ecosystem --watch --json`; o stream termina com `SIGINT`
(`Ctrl+C`) ou `SIGTERM`. `--source <source>` deve acompanhar os comandos e a
posição mostrados nas assinaturas acima. Uma fonte local é um diretório com
`manifests/*.json` e artefatos apontados por caminhos relativos; `<source>`
aceita um caminho ou um localizador `file://`.

Código de saída `0` indica sucesso, inclusive `NO_OP`. Falhas e gates não
satisfeitos retornam `1`; comando desconhecido retorna `2`. Operações de
realização distinguem `BLOCKED` (pré-condição ausente), `REJECTED` (candidata
inválida) e `NO_OP` (estado desejado já presente).

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

Os documentos normativos, a licença e os JSON Schemas Draft 2020-12 são
instalados em `share/synth`. O binário os resolve relativamente à própria
localização e não depende do checkout usado na compilação.

`SYNTH_DATA_DIR` substitui essa raiz de recursos para desenvolvimento e testes
controlados. O diretório indicado precisa conter os documentos e o subdiretório
`schemas/` esperados pelo binário. Como essa variável também determina os
schemas executados pelo runtime, ela deve apontar somente para conteúdo
confiável.

## Estado e configuração

```text
$XDG_CONFIG_HOME/synth/                         configuração humana; pode não existir
$XDG_DATA_HOME/synth/store/<digest>/            artefatos imutáveis
$XDG_STATE_HOME/synth/evidence/                 observações explícitas
$XDG_STATE_HOME/synth/installations/            registros de instalação
$XDG_STATE_HOME/synth/realizations/             promoções, witnesses e snapshots resolvidos
$XDG_STATE_HOME/synth/transactions/             histórico das operações de gestão
$XDG_RUNTIME_DIR/synth/candidates/              candidatas ainda não promovidas
$XDG_RUNTIME_DIR/synth/active/                  runtimes ativos
$XDG_RUNTIME_DIR/synth/transaction.lock         serialização de mutações
```

`version`, `help`, `status`, `foundation verify`, `surfaces`, `relations`,
`ecosystem`, `resolve`, `search`, `info` e `installed` são consultas e não
persistem dados. `evidence` grava uma nova observação atômica. `install`,
`activate`, `deactivate` e `remove` alteram o estado gerenciado e registram uma
transação. Na ausência de configuração humana, a evidência registra
`configuration: null`.

## Documentação normativa

- [SYNTH-FOUNDATION-001/v0.4.0](docs/SYNTH-FOUNDATION-001-v0.4.0.md)
- [SYNTH-PATCH-FOUNDATION-001/v0.1.0](docs/SYNTH-PATCH-FOUNDATION-001-v0.1.0.md)
- [SYNTH-FOUNDATION-CORRECTION-001/v0.1.0](docs/SYNTH-FOUNDATION-CORRECTION-001-v0.1.0.md)
- [SYNTH-FOUNDATION-HARDENING-001/v0.1.0](docs/SYNTH-FOUNDATION-HARDENING-001-v0.1.0.md)
- [SYNTH-REALIZATION-001/v0.1.0](docs/SYNTH-REALIZATION-001-v0.1.0.md)
- [SYNTH-ECOSYSTEM-PROJECTION-001/v0.1.0](docs/SYNTH-ECOSYSTEM-PROJECTION-001-v0.1.0.md)
- [SYNTH-LIVE-ECOSYSTEM-001/v0.1.0](docs/SYNTH-LIVE-ECOSYSTEM-001-v0.1.0.md)

## Licença

GPL-3.0-only. Veja [LICENSE](LICENSE).
