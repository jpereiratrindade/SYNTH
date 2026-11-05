# SYNTH-ECOSYSTEM-PROJECTION-002 — Projeção com participantes remotos
## v0.2.0 — experimental

Esta versão estende projeção 0.1.0 sem reclassificar realizações locais.

## Estados

- `INSTALLED`, `ACTIVE`: artefatos e realizações locais.
- `REGISTERED`, `AVAILABLE`, `UNAVAILABLE`: participantes remotos.

Participante remoto inclui `participant_kind=REMOTE`, `identity_assurance`,
instantes da observação e expiração. Ele nunca recebe `realization_id`.

Provedor remoto `AVAILABLE/OBSERVED` significa observação válida de
disponibilidade. Não significa identidade verificada nem autorização. Campo
`identity_assurance` acompanha participante e provedor para impedir colapso
dessas propriedades.

Projeção usa `urn:synth:schema:ecosystem-projection:0.2.0`. Consumidores de
0.1.0 precisam declarar suporte explícito antes de aceitar stream 0.2.0.
