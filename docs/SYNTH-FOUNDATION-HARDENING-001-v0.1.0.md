# SYNTH-FOUNDATION-HARDENING-001 — Endurecimento da realização fundacional
## v0.1.0

```context-metadata+json
{
  "document": {
    "id": "SYNTH-FOUNDATION-HARDENING-001",
    "version": "0.1.0",
    "status": "implemented",
    "title": "Endurecimento da realização fundacional do Sistema SYNTH",
    "project_name": "SYNTH",
    "repository": "jpereiratrindade/SYNTH",
    "created_at": "2026-09-13",
    "language": "pt-BR",
    "license": "GPL-3.0-only"
  },
  "basis": {
    "foundation": "SYNTH-FOUNDATION-001/0.4.0",
    "correction": "SYNTH-FOUNDATION-CORRECTION-001/0.1.0",
    "preservation_rule": "O hardening avança sobre a história existente e não promove por si só a Fundação candidata."
  }
}
```

## Escopo

- substituir o parser JSON local por `nlohmann/json`, versão 3.11 ou superior;
- adotar JSON Schema Draft 2020-12 para superfícies e relações;
- reservar `XDG_CONFIG_HOME` exclusivamente para configuração declarada pelo usuário;
- representar configuração ausente como `null` na evidência e `configuration=none` na saída humana dos gates;
- tornar `version`, `help`, `status`, `surfaces`, `relations` e `foundation verify` consultas sem persistência;
- restringir criação de estado e escrita de evidência ao comando `evidence`.

## Critério de aceitação

```text
parser JSON artesanal ausente
schemas Draft 2020-12 válidos e verificáveis
configuração não fabricada
consultas repetidas sem efeitos colaterais
evidence observa e persiste atomicamente
ctest PASS
instalação isolada PASS
CI main PASS
```

Este hardening não altera o estado normativo de `SYNTH-FOUNDATION-001/0.4.0`, que permanece `foundation-candidate` até decisão explícita posterior.
