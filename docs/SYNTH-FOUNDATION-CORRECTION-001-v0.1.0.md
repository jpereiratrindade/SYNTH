# SYNTH-FOUNDATION-CORRECTION-001 — Correção da realização fundacional
## v0.1.0

```context-metadata+json
{
  "document": {
    "id": "SYNTH-FOUNDATION-CORRECTION-001",
    "version": "0.1.0",
    "status": "implemented",
    "title": "Correção da realização fundacional do Sistema SYNTH",
    "project_name": "SYNTH",
    "repository": "jpereiratrindade/SYNTH",
    "created_at": "2026-09-13",
    "language": "pt-BR",
    "license": "GPL-3.0-only"
  },
  "basis": {
    "foundation": "SYNTH-FOUNDATION-001/0.4.0",
    "patch": "SYNTH-PATCH-FOUNDATION-001/0.1.0",
    "preservation_rule": "A correção avança sobre a história existente; nenhum bootstrap anterior é reescrito."
  }
}
```

## Escopo

Esta intervenção corrige somente divergências factuais da primeira realização:

- gates passam a ser derivados de observações e validações executáveis;
- `FOUNDATION_VERIFY` torna-se derivação dos demais gates;
- `SECOND_RUN_NO_OP` significa ausência de mudança de configuração ou realização, sem impedir nova evidência;
- a superfície web não observada e o protótipo de frontend deixam a realização fundacional;
- documentos, licença e esquemas acompanham a instalação em `share/synth`;
- o binário resolve seus dados relativamente à própria instalação, sem depender do checkout;
- RSS corrente e RSS máximo tornam-se métricas distintas;
- o bloco `context-metadata+json` é extraído, parseado e validado;
- a CI usa uma imagem explícita com toolchain C++26.

## Critério de aceitação

```text
foundation verify: todos os gates derivados de evidência
ctest: PASS
instalação isolada: PASS
segunda execução sem mudança de realização: PASS
CI main: PASS
```

Somente com todas essas evidências o primeiro patch fundacional pode ser aceito.
