# SYNTH-LIVE-ECOSYSTEM-001 — Projeção viva e interface humana semântica
## v0.1.0 — implemented

```context-metadata+json
{
  "document": {
    "id": "SYNTH-LIVE-ECOSYSTEM-001",
    "version": "0.1.0",
    "status": "implemented",
    "title": "Stream factual do ecossistema e contrato de interface humana",
    "project_name": "SYNTH",
    "repository": "jpereiratrindade/SYNTH",
    "created_at": "2026-09-13",
    "language": "pt-BR",
    "license": "GPL-3.0-only"
  },
  "basis": {
    "projection": "SYNTH-ECOSYSTEM-PROJECTION-001/0.1.0",
    "realization": "SYNTH-REALIZATION-001/0.1.0",
    "preservation_rule": "Evidência pinada não é mutada para simular atualização viva."
  }
}
```

## 1. Capacidade

O SYNTH oferece uma consulta viva, local e somente leitura da Ecosystem
Projection sem introduzir HTTP, daemon ou socket no núcleo:

```text
synth ecosystem --watch --json
```

A saída é NDJSON. A primeira linha é emitida imediatamente; linhas posteriores
aparecem apenas quando o identificador de conteúdo `generation` muda. Mudanças
mais rápidas que o intervalo de observação de 250 ms podem ser coalescidas, pois
o contrato representa o estado presente, não um log de transações.

O processo encerra em `SIGINT` ou `SIGTERM` e não persiste estado.

## 2. Superfície pública

O núcleo observa a superfície:

```text
id            synth.ecosystem.stream.v1
kind          process-stream
direction     outbound
media_type    application/x-ndjson
locator       stdio://synth/ecosystem?watch=1
```

Uma realização que declara `SYNTH_ECOSYSTEM_STREAM` em
`environment_allowed` recebe um descritor JSON versionado:

```json
{
  "$schema": "urn:synth:capability:ecosystem-stream:0.1.0",
  "surface": "synth.ecosystem.stream.v1",
  "media_type": "application/x-ndjson",
  "argv": ["/absolute/synth", "ecosystem", "--watch", "--json"],
  "environment": {
    "XDG_DATA_HOME": "...",
    "XDG_STATE_HOME": "...",
    "XDG_RUNTIME_DIR": "...",
    "SYNTH_DATA_DIR": "..."
  }
}
```

O consumidor executa `argv` diretamente, sem shell, aplicando somente o
ambiente fornecido. O descritor é uma capacidade explícita de runtime; nesta
versão ele ainda não produz uma relação `OBSERVED`, porque o modelo vigente de
relações consumidas exige snapshot e digest de arquivo.

## 3. Evidência e projeção não se confundem

```text
synth.evidence                 snapshot imutável, pinado e atestado
synth.ecosystem.stream.v1      consulta do estado factual presente
```

Uma realização nunca reescreve seu witness para acompanhar o stream. O witness
permanece a evidência de promoção original; atualizações vivas existem apenas em
memória ou em superfícies próprias do participante.

## 4. Interface humana compartilhada

O identificador reservado `interface.human.web.v1` define uma capacidade
semântica comum. Manifestos e witnesses que o utilizam precisam declarar:

```text
kind          http
media_type    text/html
```

O locator é o endpoint observado da realização. Uma mesma realização pode
publicar também uma superfície nominal por compatibilidade. O índice admite
múltiplos provedores e `synth resolve interface.human.web.v1 --json` retorna
todos os provedores `ACTIVE + OBSERVED`, ordenados por identidade.

O contrato identifica uma entrada web humana; não padroniza navegação interna,
autenticação ou composição visual.

## 5. Ambiguidade explícita

Descoberta plural não equivale a seleção implícita. A resolução usada durante
ativação continua bloqueando requisitos com mais de um provedor compatível.
Políticas de preferência, escolha humana ou binding por identidade ficam para
uma versão posterior.

## 6. Geração

`generation` exclui o `observed_at` de nível superior, mas identifica toda a
representação factual restante. Locators, realization IDs, referências de
evidência e instantes internos de relações participam do hash. Clientes devem
comparar apenas igualdade; monotonicidade e equivalência semântica abstrata não
são prometidas.

## 7. Critérios de aceitação

```text
LIVE_STREAM_FIRST_PROJECTION       PASS
LIVE_STREAM_CHANGE_ONLY            PASS
LIVE_STREAM_READ_ONLY              PASS
VERSIONED_RUNTIME_DESCRIPTOR       PASS
PINNED_EVIDENCE_PRESERVED          PASS
SEMANTIC_HUMAN_CONTRACT            PASS
MULTIPLE_PROVIDER_DISCOVERY        PASS
INSTALLED_ARTIFACT_INTEGRATION     PASS
CTEST                              PASS
CI                                 PASS
```

O status `implemented` foi confirmado no commit funcional `2d6b85f` pelas CIs
[34826406855](https://github.com/jpereiratrindade/SYNTH/actions/runs/34826406855)
e [34826429172](https://github.com/jpereiratrindade/SYNTH/actions/runs/34826429172),
incluindo a integração isolada do artefato SYNTH-WEB `e1bfd5d`.
