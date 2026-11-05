# SYNTH-REMOTE-PARTICIPANT-001 — Participante remoto observado
## v0.1.0 — experimental

## Hipótese

SYNTH pode registrar e observar sistema remoto independente sem representá-lo
como artefato instalado ou realização local e sem controlar seu lifecycle.

## Separações normativas

```text
REMOTE_PARTICIPANT != LOCAL_REALIZATION != OBSERVER
AVAILABILITY != IDENTITY_ASSURANCE != AUTHORIZATION
```

Estados remotos são `REGISTERED`, `AVAILABLE` e `UNAVAILABLE`. `ACTIVE` fica
reservado a realizações locais cujo processo é gerenciado pelo SYNTH.

Uma observação possui prazo de validade. Observação ausente ou expirada não
sustenta disponibilidade. O observador pode atestar `AVAILABLE` ou
`UNAVAILABLE`; sua atestação não eleva garantia de identidade.

`identity_assurance=UNVERIFIED` significa que houve somente verificação fraca,
como marcador textual em HTTP. `VERIFIED` exige o contrato criptográfico 0.2.0
descrito abaixo. A projeção deve preservar essa distinção.

## Extensão 0.2.0 — identidade remota assinada

O registro pode declarar `VERIFIED/ed25519-signed-manifest-v1` somente quando
fixa uma chave pública Ed25519 e seu fingerprint SHA-256. Cada observação deve
carregar uma declaração assinada contendo exatamente identidade, versão,
endpoint, nonce de desafio, emissão e expiração. O SYNTH verifica a assinatura, o fingerprint,
a correspondência com o registro e a validade antes de persistir a atestação.

Alteração de qualquer campo, chave divergente ou prova expirada é rejeitada.
Disponibilidade continua sendo uma dimensão separada: assinatura válida não
implica que o endpoint esteja disponível, e resposta HTTP sem assinatura não
eleva a garantia para `VERIFIED`.

## CLI experimental

```text
synth remote register <manifest.json>
synth remote attest <observation.json>
synth remote list
synth remote remove <identity>
```

Registro e observação são validados por JSON Schema. Endpoint e garantia da
observação precisam coincidir com registro. Observação expirada deixa de
publicar superfícies observadas.

## Critérios

- participante remoto nunca ganha `realization_id`;
- processo observador não aparece como participante observado;
- endpoint divergente é rejeitado;
- disponibilidade expira;
- queda e retorno não exigem reinstalação;
- sistemas remotos não são iniciados, encerrados ou modificados.
