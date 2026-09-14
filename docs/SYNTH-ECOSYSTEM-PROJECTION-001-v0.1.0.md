# SYNTH-ECOSYSTEM-PROJECTION-001 — Namespace factual do ecossistema
## v0.1.0 — implemented

```context-metadata+json
{
  "document": {
    "id": "SYNTH-ECOSYSTEM-PROJECTION-001",
    "version": "0.1.0",
    "status": "implemented",
    "title": "Projeção factual e descoberta tardia do ecossistema SYNTH",
    "project_name": "SYNTH",
    "repository": "jpereiratrindade/SYNTH",
    "created_at": "2026-09-13",
    "language": "pt-BR",
    "license": "GPL-3.0-only"
  },
  "basis": {
    "foundation": "SYNTH-FOUNDATION-001/0.4.0",
    "realization": "SYNTH-REALIZATION-001/0.1.0",
    "preservation_rule": "A descoberta projeta fatos existentes sem introduzir identidades ou transportes constitucionais."
  }
}
```

## 1. Capacidade

O SYNTH oferece uma representação versionada e factual dos participantes,
superfícies e relações que conhece no estado local presente. Clientes descobrem
capacidades por semântica de superfície, sem conhecer antecipadamente a
identidade dos participantes que as fornecem.

```text
participante -> declara superfície -> instalação conhecida
             -> witness válido     -> superfície observada
             -> relação verificada -> topologia observada
```

A projeção inspira-se na separação Unix entre namespace estável e objetos
descobertos tardiamente. Assim como sysfs exporta objetos e atributos presentes,
e systemd distingue unidades disponíveis de unidades carregadas, o modelo
preserva explicitamente:

```text
INSTALLED / DECLARED != ACTIVE / OBSERVED
```

Referências informativas: [Linux sysfs](https://docs.kernel.org/filesystems/sysfs.html),
[systemd D-Bus](https://www.freedesktop.org/wiki/Software/systemd/dbus/) e
[DNS-SD RFC 6763](https://www.rfc-editor.org/rfc/rfc6763.html).

## 2. Camadas

```text
REGISTRO
  instalações e superfícies declaradas
       ↓
TOPOLOGIA OBSERVADA
  realizações ativas, superfícies de witness e relações verificadas
       ↓
PROJEÇÕES
  CLI humana, CLI JSON e evidência persistida
```

O modelo não depende de HTTP. Unix socket, HTTP e DNS-SD poderão adaptar a mesma
projeção em versões futuras, sem se tornarem sua fonte da verdade.

## 3. Documento público

O schema `urn:synth:schema:ecosystem-projection:0.1.0` define:

- `participants`: núcleo e instalações observadas localmente;
- `surfaces`: índice por capacidade, com seus provedores;
- `relations`: relações ativas sustentadas por witness;
- `generation`: SHA-256 da topologia canônica;
- `observed_at`: instante em que a consulta foi produzida.

Cada participante possui `state`, evidência de registro, realização quando
aplicável, superfícies declaradas e superfícies observadas. Um provedor no índice
de superfícies é `DECLARED` quando apenas seu manifesto é conhecido, e
`OBSERVED` somente quando uma realização ativa publica a superfície por witness
válido.

## 4. Geração factual

`generation` não é um contador persistido. É o SHA-256 determinístico de
`participants + surfaces + relations`, excluindo apenas o `observed_at` de nível
superior da projeção. Instantes presentes em relações, locators, realization IDs
e referências de evidência continuam fatos e participam do conteúdo identificado.

```text
mesma topologia -> mesma geração
topologia diferente -> geração diferente
```

Essa escolha mantém consultas puras, evita coordenação adicional e não permite
que uma queda entre a mutação e a atualização de um contador publique gerações
incoerentes. Clientes precisam apenas comparar igualdade. Monotonicidade não é
prometida nesta versão.

## 5. Interfaces iniciais

```text
synth ecosystem
synth ecosystem --json
synth ecosystem --watch --json
synth resolve <surface>
synth resolve <surface> --json
```

`ecosystem` retorna a fotografia completa. Com `--watch --json`, emite NDJSON
imediatamente e novamente somente quando `generation` muda. `resolve` filtra o índice e retorna
somente provedores `ACTIVE + OBSERVED`; uma superfície meramente declarada não
é resolvida como capacidade disponível.

`synth evidence` incorpora a mesma projeção ao documento de evidência. Assim,
participantes que já consomem `synth.evidence` podem descobrir a topologia sem
uma dependência em servidor web ou IPC específico.

## 6. Pureza e ordenação

As projeções são derivadas exclusivamente do estado observado e não persistem
arquivos. Participantes e índices são ordenados deterministicamente. As
operações mutantes continuam serializadas pelo lock da realização; consultas de
ecossistema continuam paralelas.

## 7. Limites da v0.1.0

- namespace representado em JSON, sem FUSE ou `/sys/synth`;
- escopo somente local;
- sem assinatura distribuída ou autoridade de pertencimento remoto;
- subscription local limitada ao stream NDJSON de `ecosystem --watch`;
- sem Unix socket;
- sem HTTP no núcleo;
- `resolve` retorna todos os provedores ativos compatíveis, sem política de escolha;
- geração identificadora de conteúdo, não contador monotônico.

DNS-SD permanece apenas possibilidade futura para descoberta de anúncios de
rede. Ele não decidirá pertencimento legítimo ao ecossistema, e qualquer adoção
deverá considerar os requisitos de privacidade do
[RFC 8882](https://www.rfc-editor.org/rfc/rfc8882.html).

## 8. Critérios de aceitação

```text
ECOSYSTEM_SCHEMA_VALID             PASS
ECOSYSTEM_PROJECTION_PURE          PASS
CONTENT_DERIVED_GENERATION         PASS
INSTALLED_NOT_ACTIVE_PROJECTED     PASS
LATE_BOUND_SURFACE_DISCOVERY       PASS
EVIDENCE_EMBEDS_ECOSYSTEM          PASS
DEACTIVATION_UNRESOLVES_SURFACE    PASS
CLI_HUMAN_AND_JSON                 PASS
CTEST                              PASS
CI                                 PASS
```

O status `implemented` foi confirmado no commit funcional `22ee0ed` pela
[CI foundation 34795526429](https://github.com/jpereiratrindade/SYNTH/actions/runs/34795526429),
com todos os critérios executados no toolchain controlado.
