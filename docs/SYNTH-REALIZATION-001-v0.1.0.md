# SYNTH-REALIZATION-001 — Primeira realização externa gerenciada
## v0.1.0 — Design candidate

```context-metadata+json
{
  "document": {
    "id": "SYNTH-REALIZATION-001",
    "version": "0.1.0",
    "status": "design-candidate",
    "title": "Primeira realização externa gerenciada pelo Sistema SYNTH",
    "project_name": "SYNTH",
    "repository": "jpereiratrindade/SYNTH",
    "created_at": "2026-09-13",
    "language": "pt-BR",
    "license": "GPL-3.0-only"
  },
  "basis": {
    "foundation": "SYNTH-FOUNDATION-001/0.4.0",
    "hardening": "SYNTH-FOUNDATION-HARDENING-001/0.1.0",
    "preservation_rule": "A realização adiciona capacidade sem alterar o estado normativo da Fundação candidata."
  }
}
```

## 1. Hipótese experimental

O Sistema SYNTH pode obter, verificar, instalar, ativar, observar, desativar e
remover um participante independente sem incorporar seu código, acessar seu
estado privado, depender de sua disponibilidade ou reduzir a prontidão do
núcleo quando uma candidata falhar.

`synth-web` é somente a primeira candidata usada para provar o mecanismo. O
núcleo não conhece web, HTTP, React, Node, CSS ou qualquer domínio específico
do participante.

## 2. Escopo da fatia vertical

Esta versão oferece uma única fonte local baseada em arquivos e os comandos:

```text
synth search <query> --source <source>
synth info <identity> --source <source>
synth install <identity> --source <source>
synth installed
synth activate <identity>
synth deactivate <identity>
synth remove <identity>
```

Não entram: upgrade, repositório HTTP, discovery público, marketplace, plugins,
daemon, banco de dados, service mesh, ContextLab runtime, assinatura distribuída
ou rollback de dados privados do participante.

## 3. Objetos preservados

```text
ARTIFACT != INSTALLATION != REALIZATION != RELATION
```

- **Artifact:** arquivo adquirido cuja identidade criptográfica SHA-256 foi verificada.
- **Installation:** referência gerenciada a conteúdo imutável no store do SYNTH.
- **Realization:** processo candidato ou ativo iniciado a partir da instalação.
- **Relation:** interação entre superfícies sustentada por witness de runtime válido.

Uma instalação não ativa processos. Um manifesto declara requisitos, mas não
prova relações observadas.

## 4. Fonte e manifesto

A fonte v0.1.0 é um diretório local com `manifests/*.json` e os artefatos por
caminhos relativos. A implementação trata a resolução como backend de fonte,
sem afirmar que o backend local será único no futuro.

O manifesto segue JSON Schema Draft 2020-12 e descreve identidade, versão,
descrição, artefato e digest, proveniência, superfícies fornecidas e requeridas,
entrypoint, argumentos, ambiente permitido, readiness e envelope de recursos.
Não existe conceito obrigatório de plugin ou tipo constitucional web.

## 5. Integridade e instalação

```text
resolve -> acquire -> SHA-256 verify -> stage -> install
```

SHA-256 é calculado por OpenSSL. Arquivos `tar` são inspecionados e extraídos
pelo GNU tar, após rejeição de caminhos absolutos, travessia `..`, links
simbólicos e hard links.
O store é endereçado pelo digest e não é modificado depois da promoção.

Digest divergente ou manifesto inválido produz `REJECTED`; instalação e estado
ativo permanecem inalterados. Reinstalar a mesma identidade, versão e digest é
`NO_OP`. Versão ou digest diferente exige uma futura operação de upgrade e é
recusado nesta versão.

## 6. Fronteiras XDG

```text
$XDG_CONFIG_HOME/synth/                  configuração humana; não fabricada
$XDG_DATA_HOME/synth/store/<digest>/     artifacts imutáveis instalados
$XDG_STATE_HOME/synth/installations/     metadados de instalação
$XDG_STATE_HOME/synth/realizations/      promoção e witnesses observados
$XDG_STATE_HOME/synth/transactions/      evidência da gestão
$XDG_RUNTIME_DIR/synth/candidates/       candidatas ainda não promovidas
$XDG_RUNTIME_DIR/synth/active/           runtime ativo
```

O SYNTH não copia nem gerencia estado privado do participante.

## 7. Ativação transacional

```text
ACTIVE continua intacto
  -> CANDIDATE
       -> resolve superfícies requeridas
       -> iniciar em diretório e ambiente isolados
       -> readiness verificável
       -> validar witness
       -> VERIFIED
       -> promover atomicamente
```

A candidata recebe somente um ambiente mínimo e as variáveis explicitamente
permitidas no manifesto. Nesta versão, readiness é um comando do próprio
artefato, executado pelo SYNTH até sucesso ou timeout. Essa forma é genérica e
não pressupõe transporte HTTP.

Falha de requisito produz `BLOCKED`. Falha de processo, readiness ou witness
produz `REJECTED`. Em ambos os casos a candidata é encerrada, nenhuma promoção
ocorre e `FOUNDATION_READY`/`SYSTEM_SYNTH_READY` continuam derivados apenas do
núcleo saudável.

## 8. Evidência e witness

O witness segue JSON Schema Draft 2020-12 e contém identidade, versão,
`realization_id`, PID quando aplicável, superfícies fornecidas, superfícies
consumidas, readiness e instante observado. Ele pertence à evidência da
realização, não ao manifesto.

O SYNTH valida estrutura, identidade, versão, realização, processo, readiness e
compatibilidade com as superfícies declaradas antes da promoção. Um requisito
`RESOLVED` só aparece como relação `OBSERVED` depois que um witness válido
registra seu consumo factual. `evidence_ref` aponta para o witness preservado.

## 9. Semântica operacional

- `search` e `info` consultam a fonte sem alterar estado.
- `install` verifica e registra, mas não ativa.
- `installed` consulta instalações sem alterar estado.
- `activate` nunca substitui silenciosamente uma realização ativa.
- `deactivate` de identidade inativa é `NO_OP`.
- `remove` recusa instalação ativa e orienta desativação prévia.
- `remove` elimina a instalação gerenciada, mas não toca estado privado do participante.
- comandos de consulta permanecem puros; operações registram transações próprias.

## 10. Candidata independente

`SYNTH-WEB` vive em repositório Git separado, possui build, testes e versão
próprios, usa somente superfícies públicas e funciona sem checkout, headers ou
estado privado do SYNTH. Sua home apresenta fatos recebidos de `synth.evidence`;
JSON bruto existe apenas como detalhe progressivo.

O build normal do SYNTH usa um artefato fixture previamente construído e não
depende do source tree nem do build do participante.

## 11. Limites explícitos da v0.1.0

- uma instalação por identidade;
- sem upgrade e sem seleção de versões;
- uma realização ativa por identidade;
- isolamento por processo, grupo de processo, diretório de candidata e ambiente mínimo;
- sem sandbox de kernel ou isolamento de rede;
- readiness somente por comando executável do artefato;
- store sem coleta de lixo;
- assinatura criptográfica além do digest fica para evolução futura.

## 12. Dependências e licenças

As versões mínimas são as verificadas pelo build ou exigidas pela implementação.
Nenhuma dependência introduz um framework de aplicação no núcleo.

| Dependência | Versão mínima | Função | Licença |
|---|---:|---|---|
| OpenSSL `libcrypto` | 3.0 | SHA-256 do artefato e do witness | Apache-2.0 |
| GNU tar | 1.34 | inspeção e extração do pacote `tar` validado | GPL-3.0-or-later |
| nlohmann/json | 3.11 | JSON no núcleo; já presente na Fundação | MIT |
| Python | 3.11 | execução dos testes de integração | PSF-2.0 |
| jsonschema | 4.10 | validação independente dos JSON Schemas nos testes; já presente na Fundação | MIT |

O participante independente usa Python 3.11+ e somente sua biblioteca padrão
para o runtime HTTP. CMake 3.25+ (BSD-3-Clause) permanece a ferramenta de build
dos dois projetos.

## 13. Critérios de aceitação

```text
FOUNDATION_VERIFY                  PASS
EXISTING_CONFORMANCE               PASS
REALIZATION_MANIFEST_SCHEMA        PASS
ARTIFACT_INTEGRITY                 PASS
LOCAL_SOURCE_RESOLUTION            PASS
IMMUTABLE_INSTALL_STORE            PASS
INSTALL_NO_ACTIVE_MUTATION         PASS
CANDIDATE_ISOLATION                PASS
MISSING_REQUIREMENT_BLOCKS         PASS
READINESS_VERIFICATION             PASS
RUNTIME_WITNESS                    PASS
OBSERVED_RELATION_GROUNDED         PASS
FAILED_CANDIDATE_PRESERVES_SYNTH   PASS
INSTALL_IDEMPOTENCE                PASS
SAFE_DEACTIVATION                  PASS
SAFE_REMOVAL                       PASS
PURE_QUERY_NO_SIDE_EFFECTS         PASS
SYNTH_WEB_INDEPENDENT_BUILD        PASS
SYNTH_WEB_INDEPENDENT_RUNTIME      PASS
CTEST                              PASS
CI                                 PASS
```

Somente após evidência repetível de todos os critérios o status deste documento
pode mudar de `design-candidate` para `implemented`. Essa mudança não promove
`SYNTH-FOUNDATION-001/0.4.0`, que permanece `foundation-candidate`.
