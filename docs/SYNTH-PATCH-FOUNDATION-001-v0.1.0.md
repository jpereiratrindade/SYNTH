# SYNTH-PATCH-FOUNDATION-001 — Patch fundacional do Sistema SYNTH
## v0.1.0 — Especificação candidata

```context-metadata+json
{
  "document": {
    "id": "SYNTH-PATCH-FOUNDATION-001",
    "version": "0.1.0",
    "status": "patch-spec-candidate",
    "title": "Patch fundacional do Sistema SYNTH",
    "project_name": "Ecossistema SYNTH",
    "repository": "jpereiratrindade/SYNTH",
    "created_at": "2026-09-13",
    "language": "pt-BR",
    "license": "GPL-3.0-only"
  },
  "basis": {
    "foundation": "SYNTH-FOUNDATION-001/0.4.0",
    "goal": "Criar a menor realização funcional do Sistema SYNTH sem antecipar um ecossistema que ainda não existe."
  }
}
```

# 1. Princípio do patch

O patch deve terminar com um **Sistema SYNTH funcional**.

Ele não deve terminar com uma simulação de ecossistema.

Resultado pretendido:

```text
FOUNDATION_READY       PASS
SYSTEM_SYNTH_READY     PASS
ECOSYSTEM_SYNTH        NOT_YET_APPLICABLE
```

# 2. O que nasce neste patch

O patch deve introduzir somente:

- documentação fundacional versionada;
- licença GPL-3.0-only;
- identidade do projeto;
- build C++26 mínimo;
- executável `synth`;
- configuração/estado/runtime seguindo XDG;
- auto-observação;
- evidência factual;
- descrição genérica de superfícies;
- descrição genérica de relações;
- CLI humana;
- métricas básicas do próprio processo;
- testes de conformidade fundacional;
- CI mínimo.

# 3. CLI mínima

```text
synth version
synth status
synth foundation verify
synth surfaces
synth relations
synth evidence
```

No estado inicial:

```text
synth surfaces
```

pode mostrar apenas as superfícies realmente existentes do próprio SYNTH.

```text
synth relations
```

pode legitimamente responder:

```text
No relations observed.
```

Isso é mais correto do que inventar relações.

# 4. Separação XDG

```text
$XDG_CONFIG_HOME/synth/
$XDG_STATE_HOME/synth/
$XDG_RUNTIME_DIR/synth/
```

Cada classe de informação deve possuir finalidade inequívoca.

# 5. Evidência inicial

O próprio SYNTH deve ser capaz de observar e registrar, no mínimo:

```text
identity
version
process
uptime
RSS
CPU
configuration root
state root
runtime root
observed surfaces
observed relations
timestamp
epistemic_class = OBSERVED
```

# 6. Superfícies

O patch deve definir um descritor genérico de superfície.

Ele não deve definir superfícies de Pulso, Trama, Lente ou qualquer membro imaginado.

Campos mínimos candidatos:

```text
id
owner
kind
locator
direction
media_type
observability
metadata
```

`kind` e `locator` devem permitir evolução sem enumeração rígida de todos os transportes futuros.

# 7. Relações

Uma relação só pode existir se houver observação ou declaração correspondente.

Campos mínimos candidatos:

```text
id
source
target
surface
status
epistemic_class
evidence_ref
observed_at
```

Não existe relação hardcoded para fins visuais.

# 8. ContextLab

Integração fundacional inicial:

- `docs/SYNTH-FOUNDATION-001-v0.4.0.md` mantém `context-metadata+json`;
- documentos normativos seguintes preservam metadados equivalentes;
- o repositório não incorpora ContextLab;
- não há dependência de build/runtime;
- ContextLab pode ingerir documentos SYNTH como artefatos documentais externos;
- integração runtime bidirecional fica explicitamente fora deste patch.

# 9. Experiência humana

A experiência inicial é CLI.

Requisitos:

- texto legível;
- sem JSON cru por padrão;
- `--json` pode expor representação computável;
- status deve distinguir `DECLARED`, `OBSERVED` e `DERIVED` quando necessário;
- mensagens de erro devem apontar causa e evidência.

# 10. Não entra

```text
frontend web
membros externos obrigatórios
subsistemas sintéticos
topologia desenhada
service mesh
plugin framework
membership registry
contratos de domínio
generation promotion completa
rollback de ecossistema
Disturbance Lab
```

# 11. Gates

```text
FOUNDATION_FILE_PRESENT           PASS
GPL_3_ONLY                        PASS
CPP26_BUILD                       PASS
FOUNDATION_VERIFY                 PASS
XDG_SEPARATION                    PASS
SELF_OBSERVATION                  PASS
RESOURCE_OBSERVATION              PASS
SURFACE_MODEL_GENERIC             PASS
RELATION_MODEL_GENERIC            PASS
NO_FAKE_RELATIONS                 PASS
CONTEXTLAB_DOCUMENT_COMPAT        PASS
CLI_HUMAN_READABLE                PASS
SECOND_RUN_NO_OP                  PASS
```

# 12. Critério de parada

Quando todos os gates passarem:

```text
SYSTEM_SYNTH_READY
```

O patch termina.

Não se acrescenta “só mais uma coisa”.

O próximo patch deverá nascer de uma necessidade factual observada no sistema pronto.
