# SYNTH-FOUNDATION-001 — Constituição fundacional do SYNTH
## v0.4.0 — Do sistema aberto ao Ecossistema SYNTH

```context-metadata+json
{
  "document": {
    "id": "SYNTH-FOUNDATION-001",
    "version": "0.4.0",
    "status": "foundation-candidate",
    "title": "Constituição fundacional do SYNTH — do sistema aberto ao Ecossistema SYNTH",
    "project_name": "Ecossistema SYNTH",
    "repository": "jpereiratrindade/SYNTH",
    "created_at": "2026-09-13",
    "language": "pt-BR",
    "license": "GPL-3.0-only"
  },
  "lineage": {
    "experimental_predecessor": "SYNTH-0",
    "supersedes_as_candidate": [
      "SYNTH-FOUNDATION-001/0.1.0",
      "SYNTH-FOUNDATION-001/0.2.0",
      "SYNTH-FOUNDATION-001/0.3.0"
    ],
    "preservation_rule": "Versões e experimentos anteriores permanecem como proveniência do aprendizado e não são reescritos."
  },
  "thesis": {
    "statement": "SYNTH nasce como um sistema aberto, pronto e incompleto. Sua constituição não pressupõe um ecossistema pronto, um número fixo de subsistemas ou uma topologia final. Ao admitir sistemas autônomos e relações entre eles, pode tornar-se o Ecossistema SYNTH. Propriedades sistêmicas podem emergir dessas relações e devem ser observadas, não impostas a priori.",
    "motto": "Sempre pronto, sempre incompleto."
  },
  "contextlab": {
    "integration_intent": "O documento é deliberadamente autocontido, versionado e provido de metadados de proveniência para poder ser ingerido e situado pelo ContextLab sem que SYNTH e ContextLab compartilhem estado privado ou ontologia interna."
  }
}
```

> **Natureza deste documento**  
> Este documento é a base teórica e normativa da família SYNTH.  
> Ele define o que deve permanecer verdadeiro enquanto implementações, membros, relações e tecnologias mudam.  
> Ele não descreve um ecossistema já pronto. Ele estabelece as condições pelas quais um sistema SYNTH pode evoluir, relacionar-se e eventualmente constituir o **Ecossistema SYNTH**.

---

# 1. Origem e aprendizado

SYNTH nasce de uma tentativa anterior, SYNTH-0.

O SYNTH-0 demonstrou que mecanismos de declaração, observação, reconciliação, evidência e idempotência são tecnicamente realizáveis. Também revelou um erro metodológico importante: um plano pode ser internamente consistente e ainda violar os princípios que pretendia materializar.

O aprendizado central foi:

> **não se começa desenhando o ecossistema que se deseja ver; começa-se construindo condições simples e estáveis para que relações entre sistemas possam existir e ser observadas.**

SYNTH-0 permanece como experimento histórico.

SYNTH não corrige SYNTH-0 em lugar.

SYNTH recomeça a partir do aprendizado produzido por ele.

---

# 2. A pergunta fundadora

A pergunta não é:

> Como construir vários serviços e ligá-los?

Nem:

> Como definir antecipadamente os subsistemas de um ecossistema?

A pergunta é:

> **Como construir um sistema aberto, coerente e resiliente, capaz de coexistir e relacionar-se com sistemas autônomos ainda desconhecidos e, por essa evolução relacional, poder constituir um ecossistema sem perder prontidão, autonomia e inteligibilidade?**

---

# 3. Sistema primeiro; ecossistema como possibilidade

SYNTH nasce como **Sistema SYNTH**.

Ele não nasce afirmando possuir subsistemas.

Ele não nasce com uma topologia desenhada para preencher.

Ele não nasce com um site que representa um ecossistema que ainda não existe.

Ele nasce com fundamentos que permitem:

- existir autonomamente;
- expor superfícies de interação;
- reconhecer superfícies externas;
- estabelecer relações;
- observar relações;
- preservar proveniência;
- distinguir intenção de fato;
- adaptar sua realização;
- continuar evoluindo sem supor uma forma final.

Se, durante sua evolução, sistemas autônomos passam a coexistir e relações persistentes passam a constituir um todo observável, podemos falar em:

\[
\boxed{\text{Ecossistema SYNTH}}
\]

Assim:

\[
\text{Sistema SYNTH}
\;\xrightarrow{\text{relações e composição}}\;
\text{Ecossistema SYNTH}
\]

Essa transição é possível pela fundação.

Não é obrigada por ela.

---

# 4. O ecossistema não é um produto previamente desenhado

O Ecossistema SYNTH não é definido por:

- um número fixo de membros;
- uma lista antecipada de subsistemas;
- um diagrama estático;
- um monorepo;
- uma interface web;
- um banco central;
- uma aplicação controladora;
- um catálogo obrigatório de capacidades.

Cada estado presente deve ser explícito.

O espaço de estados futuros permanece aberto.

A formulação central é:

\[
\boxed{
\text{o presente é explícito; o futuro é aberto}
}
\]

---

# 5. Inspiração Unix: mecanismo antes de integração

A inspiração Unix é estrutural, não estética.

UNIX mostrou a força de um sistema que oferece mecanismos gerais e relativamente pequenos — processos, arquivos, descritores, streams, permissões e primitivas de comunicação — sem precisar conhecer previamente todas as aplicações que existirão sobre ele.

O princípio relevante para SYNTH é:

> **um ambiente não precisa prescrever todas as relações futuras; precisa oferecer superfícies suficientemente gerais para que sistemas independentes possam compor-se.**

Portanto, SYNTH deve preferir:

- mecanismos gerais a integrações especiais;
- interfaces simples a dependências privadas;
- composição a centralização;
- observabilidade a suposições;
- pequenos elementos combináveis a grandes componentes oniscientes.

---

# 6. Unix na constituição; Nix na realização

Duas inspirações devem permanecer separadas.

## Unix inspira a constituição

Pergunta:

> Como sistemas independentes coexistem e tornam-se combináveis?

Contribuições conceituais:

- autonomia;
- composição;
- superfícies comuns;
- processos independentes;
- interfaces gerais;
- substituibilidade;
- pouca necessidade de conhecimento mútuo.

## Nix/NixOS inspira a realização

Pergunta:

> Como uma composição específica pode ser resolvida, realizada, verificada, ativada e revertida de maneira reproduzível?

Contribuições conceituais:

- descrição declarativa;
- imutabilidade de realizações;
- gerações;
- promoção;
- rollback;
- hashing;
- reprodutibilidade.

Consequentemente:

\[
\boxed{
\text{gerações são mecanismo de realização, não ontologia do ecossistema}
}
\]

---

# 7. Substrato, superfícies, relações e propriedades

A base conceitual deixa de ser “contratos entre sistemas”.

A formulação passa a ser:

\[
\boxed{
SYNTH = B + S + I + R + O
}
\]

onde:

- \(B\) = substrato fundacional;
- \(S\) = sistemas autônomos presentes;
- \(I\) = superfícies de interação;
- \(R\) = relações efetivamente constituídas;
- \(O\) = observações e evidências.

Quando existe uma composição relacional suficientemente estável para constituir um todo:

\[
\boxed{
E = (S,R)
}
\]

pode-se reconhecer um Ecossistema SYNTH.

---

# 8. Substrato fundacional

O substrato SYNTH é o menor conjunto de mecanismos necessários para sustentar:

- identidade;
- localização/descoberta;
- superfícies de interação;
- referências;
- observação;
- evidência;
- proveniência;
- condição operacional;
- mudança segura;
- medição de recursos.

O substrato NÃO define o domínio dos sistemas.

Ele não precisa saber se um sistema futuro trata de:

- contexto documental;
- geoprocessamento;
- memória;
- decisão;
- sensoriamento;
- simulação;
- conhecimento;
- visualização;
- qualquer domínio ainda não imaginado.

---

# 9. Superfície de interação

Uma **superfície** é aquilo que um sistema torna disponível para possível interação sem expor seu estado privado.

Ela pode ser realizada por:

- arquivo;
- diretório;
- stream;
- stdin/stdout;
- Unix socket;
- HTTP;
- mensagem;
- objeto versionado;
- outra interface aberta adequada.

A fundação não fixa um transporte universal.

Uma superfície deve ser:

- identificável;
- observável;
- documentável;
- utilizável sem acesso ao estado privado do sistema;
- suficientemente estável para que outro sistema possa referenciá-la.

---

# 10. Contratos: úteis, mas não ontológicos

Contratos continuam importantes.

Eles deixam de ser a condição existencial para todo pertencimento ao SYNTH.

Um contrato é:

> **uma formalização opcional ou necessária de uma superfície ou relação quando precisamos tornar sua semântica e compatibilidade verificáveis.**

Portanto:

\[
\text{relação} \not\Rightarrow \text{contrato obrigatório}
\]

mas:

\[
\text{relação que exige garantia formal}
\Rightarrow
\text{contrato ou especificação verificável}
\]

SYNTH é:

> **relation-first, interface-enabled, evidence-grounded.**

Em português:

> **relações primeiro, possibilitadas por interfaces e sustentadas por evidência.**

---

# 11. Sistema autônomo

Um sistema é autônomo quando sua existência não depende de acesso ao estado privado de outro sistema.

Autonomia deve poder ser demonstrada por capacidades como:

```text
configurar independentemente
construir independentemente
testar independentemente
versionar independentemente
instalar independentemente
iniciar independentemente
parar independentemente
observar independentemente
```

Um sistema pode usar outro sistema.

Isso não elimina autonomia se a dependência for explícita e ocorrer por uma superfície pública.

---

# 12. Relação como objeto de primeira classe

Uma relação não é simplesmente uma conexão de rede.

É uma interação observável entre sistemas ou entre um sistema e uma superfície.

Uma relação pode possuir:

- origem;
- destino;
- superfície utilizada;
- direção;
- temporalidade;
- condição;
- evidência;
- proveniência;
- semântica, quando conhecida;
- formalização contratual, quando necessária.

A relação não precisa existir porque a Fundação a previu.

Ela pode aparecer porque sistemas independentes encontraram uma superfície mutuamente utilizável.

---

# 13. Relações não são impostas

A Fundação não determina quais relações devem existir.

Ela determina apenas que relações relevantes possam ser:

- reconhecidas;
- descritas;
- observadas;
- diferenciadas de simples proximidade;
- atribuídas a evidência;
- removidas sem destruir autoridade alheia.

Isso é fundamental:

> **o Ecossistema SYNTH não programa suas relações futuras; cria condições para que elas possam constituir-se de forma inteligível.**

---

# 14. Emergência como hipótese observacional

Propriedades do Ecossistema SYNTH podem depender das relações entre membros.

Formalmente:

\[
P(E) = \Phi(S,R)
\]

e não necessariamente:

\[
P(E) = \bigcup_i P(S_i)
\]

Uma propriedade sistêmica pode aparecer sem pertencer isoladamente a qualquer membro.

Exemplos abstratos:

- adaptação;
- circulação de contexto;
- capacidade de recuperação;
- transformação coordenada;
- continuidade informacional.

A Fundação NÃO deve declarar previamente quais propriedades emergirão.

Ela deve permitir que propriedades sejam:

- observadas;
- caracterizadas;
- associadas às relações relevantes;
- reproduzidas;
- eventualmente estabilizadas.

Portanto:

\[
\boxed{
\text{SYNTH não programa emergência; SYNTH torna emergência observável}
}
\]

---

# 15. Sempre pronto

“Sempre pronto” não significa disponibilidade absoluta.

Significa:

> **tudo aquilo que constitui o Sistema SYNTH ou uma composição ativa reconhecida deve funcionar dentro do envelope operacional que declarou suportar.**

Para um estado ativo \(A\):

\[
READY(A)
\iff
\forall c \in Required(A):
Observed(c) \in ViableEnvelope(c)
\]

Uma mudança deliberada não deve ser promovida se tornar o estado ativo `NOT_READY`.

Falhas além do modelo declarado podem produzir `NOT_READY`; isso deve ser factual, explícito e recuperável quando possível.

---

# 16. Sempre incompleto

“Incompleto” não é um estado de saúde.

Não significa:

- faltando componente obrigatório;
- parcialmente implementado;
- quebrado;
- aguardando funcionalidade prometida.

Significa:

\[
\boxed{
\nexists A_{\text{final}}
}
\]

Não existe uma forma terminal do SYNTH.

Uma evolução pode:

- acrescentar;
- remover;
- substituir;
- simplificar;
- dividir;
- fundir;
- reconfigurar;
- mudar relações;
- modificar a experiência;
- melhorar eficiência;
- ampliar ou reduzir capacidades.

Logo:

\[
\boxed{
\text{pronto} \neq \text{pleno}
}
\]

e:

\[
\boxed{
\text{incompleto} \neq \text{quebrado}
}
\]

---

# 17. Resiliência

Resiliência não é ausência de distúrbio.

É capacidade de:

1. perceber mudança;
2. manter estados interpretáveis;
3. preservar função dentro do envelope possível;
4. recuperar condição operacional;
5. reconfigurar quando necessário;
6. aprender sobre limites observados.

O modelo de falhas precisa ser explícito para qualquer promessa de prontidão.

---

# 18. Separação epistemológica

SYNTH deve distinguir classes que não podem ser colapsadas:

```text
DECLARED
RESOLVED
REALIZED
OBSERVED
DERIVED
INFERRED
```

- **DECLARED** — intenção;
- **RESOLVED** — escolha de artefatos/versões/superfícies;
- **REALIZED** — concretização;
- **OBSERVED** — testemunha factual;
- **DERIVED** — cálculo sobre fatos;
- **INFERRED** — proposição inferida com incerteza explícita.

Nenhuma classe substitui outra.

---

# 19. Registro: mapa, não autoridade de existência

O “registro” SYNTH não é equivalente ao Windows Registry.

Ele não concede existência a aplicações.

Ele é uma projeção estruturada capaz de responder, quando aplicável:

```text
o que está presente?
como se identifica?
que superfícies apresenta?
quais relações estão observadas?
qual é sua condição?
qual é a proveniência dessa informação?
```

Portanto:

\[
\boxed{
\text{registro SYNTH}
=
\text{mapa declarativo/factual do estado conhecido}
}
\]

e não:

\[
\boxed{
\text{registro SYNTH}
=
\text{dono dos sistemas}
}
\]

---

# 20. Mudança segura e gerações

Gerações são usadas quando uma realização precisa de mudança controlada.

Uma candidata deve poder ser:

- resolvida;
- realizada;
- observada;
- verificada;

sem contaminar a realização ativa.

Conceitualmente:

```text
ACTIVE
  │
  ├──────── continua servindo
  │
  └── CANDIDATE
        │
        ├── realize in isolation
        ├── observe
        ├── verify
        └── promote or reject
```

Promoção é atomicidade de visibilidade da resolução, não simultaneidade física universal.

---

# 21. Experiência humana

Uma experiência humana de alta qualidade é princípio transversal.

Isso NÃO significa que SYNTH precise nascer como um website.

A experiência apropriada depende da superfície.

No início, pode ser:

- CLI clara;
- estado legível;
- mensagens precisas;
- documentos navegáveis;
- evidência acessível.

Uma interface web pode surgir quando houver algo sistêmico que valha a pena projetar visualmente.

Quando existir, ela deve revelar o sistema real — não desenhar antecipadamente um ecossistema imaginário.

JSON é evidência técnica.

Não é a única experiência humana aceitável.

---

# 22. Eficiência

SYNTH deve tratar CPU e memória como recursos finitos.

Desde o início:

- filas devem ser limitadas;
- caches precisam de política;
- recursos precisam ser mensuráveis;
- backpressure deve ser explícito quando necessário;
- otimização deve seguir medição.

Fluxo:

\[
\boxed{
medir \rightarrow hipótese \rightarrow alterar \rightarrow medir
}
\]

---

# 23. Condição para falar em “Ecossistema SYNTH”

O termo **Ecossistema SYNTH** torna-se factual quando houver:

1. pluralidade de sistemas autônomos;
2. relações observáveis entre eles;
3. persistência suficiente dessas relações para constituir uma composição reconhecível;
4. propriedades do todo que possam ser descritas a partir da composição e das relações.

O número de sistemas não é fixado pela Fundação.

A topologia também não.

---

# 24. ContextLab como integração de referência

ContextLab é um candidato especialmente apropriado para demonstrar a abertura do SYNTH porque seu domínio é diferente e sua autonomia deve ser preservada.

A integração NÃO pressupõe:

- compartilhar banco;
- compartilhar código;
- herdar ontologia;
- subordinar ContextLab ao SYNTH;
- tornar SYNTH dependente do ContextLab.

A integração deve ocorrer por superfícies.

## 24.1 Primeira superfície já disponível: documento

Documentos SYNTH usam um bloco de metadados computáveis:

```text
context-metadata+json
```

Esse bloco permite que ContextLab:

- identifique o documento;
- situe versão;
- preserve proveniência;
- reconheça relações documentais;
- ingira o artefato sem depender da implementação interna do SYNTH.

Assim, o próprio documento fundacional é um primeiro objeto possível de integração.

## 24.2 Relação inicial proposta

Conceitualmente:

```text
SYNTH
  ── publica artefato documental + proveniência ──►
ContextLab
```

e, futuramente:

```text
ContextLab
  ── expõe contexto documental computável ──►
SYNTH
```

A segunda direção só deve ser implementada quando existir uma necessidade concreta.

Nenhuma direção transfere autoridade sobre o objeto original.

## 24.3 Fronteira epistemológica

Contexto documental recebido do ContextLab deve manter sua proveniência.

SYNTH não pode apresentá-lo automaticamente como:

- fato do runtime;
- relação causal;
- configuração desejada;
- verdade do ecossistema.

Ele entra como informação contextual com classe epistemológica explícita.

---

# 25. Integração sem conhecimento prévio

ContextLab é referência, não exceção privilegiada.

Se a integração exigir mudar a Fundação para incluir “ContextLab” como caso especial, a integração está errada.

O objetivo é que a mesma base permita, no futuro, relações com sistemas ainda desconhecidos.

---

# 26. Git, repositórios e código-fonte

A organização Git não constitui a ontologia do SYNTH.

Repositórios podem ser usados para:

- autonomia de evolução;
- isolamento de build;
- proveniência;
- distribuição;
- revisão.

Mas:

\[
\text{repositório} \neq \text{sistema}
\]

e:

\[
\text{árvore de fontes} \neq \text{registro operacional}
\]

---

# 27. Licença

O código original do projeto SYNTH será disponibilizado sob:

```text
GPL-3.0-only
```

Dependências de terceiros mantêm suas próprias licenças compatíveis.

---

# 28. Primeiro patch: o patch fundacional

O primeiro patch do novo repositório `SYNTH` não tentará fabricar um ecossistema.

Seu objetivo será fazer nascer um **Sistema SYNTH mínimo**, coerente com esta Fundação.

O patch deve responder somente:

> **qual é o menor sistema funcional que materializa o substrato fundacional sem inventar membros, relações ou propriedades que ainda não existem?**

Escopo conceitual:

- incorporar esta Fundação;
- estabelecer identidade e versão do SYNTH;
- prover uma forma mínima de auto-observação;
- prover evidência factual;
- separar configuração, estado persistente e runtime;
- prover mecanismo inicial de descrição de superfícies;
- prover mecanismo inicial de descrição/observação de relações;
- ser capaz de validar seus próprios artefatos fundacionais;
- expor experiência humana clara, inicialmente pela CLI;
- medir seu próprio uso básico de recursos;
- incluir testes de conformidade fundacional;
- manter base para ingestão documental pelo ContextLab.

Não entram ainda:

- subsistemas sintéticos obrigatórios;
- topologia de ecossistema;
- Living Topology completa;
- Disturbance Lab;
- catálogo fixo de membros;
- contratos de domínio inventados;
- propriedades emergentes simuladas;
- frontend que represente relações inexistentes.

Critério final:

```text
FOUNDATION_READY
SYSTEM_SYNTH_READY
ECOSYSTEM_SYNTH = NOT_YET_APPLICABLE
```

---

# 29. Regra contra antecipação

Antes de acrescentar qualquer mecanismo, perguntar:

1. Ele é necessário para o Sistema SYNTH atual?
2. Ou estamos tentando antecipar o Ecossistema SYNTH futuro?
3. O Unix resolveria isso oferecendo um mecanismo mais geral?
4. Estamos criando uma relação real ou desenhando uma relação imaginada?
5. A informação apresentada é declarada, observada, derivada ou inferida?
6. O mecanismo preserva autonomia dos sistemas externos?
7. O custo de CPU/memória pode ser observado?
8. ContextLab poderia relacionar-se por uma superfície sem herdar nossa implementação?

Se o mecanismo só existe para preencher uma visão futura:

```text
DO_NOT_IMPLEMENT
```

---

# 30. Método de revisão

A revisão de qualquer patch segue:

```text
R1  fidelidade à Fundação
R2  necessidade atual versus antecipação
R3  autonomia
R4  superfícies e relações
R5  epistemologia/evidência
R6  resiliência
R7  experiência humana
R8  eficiência
R9  engenharia
```

Uma solução tecnicamente elegante que falha em R1 ou R2 deve ser rejeitada.

---

# 31. Critério de maturidade desta Fundação

Esta versão permanece `foundation-candidate`.

Ela só deve ser promovida para:

```text
SYNTH-FOUNDATION-001 / 1.0.0
status = accepted
```

quando:

- não depender de número conhecido de sistemas;
- não depender de ContextLab;
- não exigir contratos como condição universal;
- não pressupor frontend;
- não confundir registro com autoridade;
- não confundir geração com ecossistema;
- não transformar “sempre pronto” em promessa absoluta;
- não transformar “sempre incompleto” em estado operacional;
- permitir uma primeira realização pequena e funcional.

---

# 32. Síntese

SYNTH nasce como sistema.

Não nasce como ecossistema pronto.

Sua Fundação torna possível que:

\[
\text{sistemas autônomos}
+
\text{superfícies}
+
\text{relações}
\rightarrow
\text{composição sistêmica}
\]

e que, sobre essa composição, propriedades possam emergir.

A Fundação não determina quais.

Ela determina que possam ser observadas sem destruir autonomia, proveniência e coerência.

Assim:

\[
\boxed{
\textbf{Sistema SYNTH}
\;\text{pode evoluir para}\;
\textbf{Ecossistema SYNTH}
}
\]

porque a abertura está na constituição, não em uma lista prévia de integrantes.

E:

\[
\boxed{
\textbf{sempre pronto, sempre incompleto}
}
\]

significa:

> tudo que existe como estado ativo deve funcionar dentro de seus limites declarados, enquanto nenhuma realização é tratada como forma final.

---

# 33. Referências conceituais

- Ritchie, D. M.; Thompson, K. *The UNIX Time-Sharing System*. Communications of the ACM, 1974.
- Ritchie, D. M. *The Evolution of the Unix Time-sharing System*. Bell Laboratories.
- The Open Group. *Single UNIX Specification*.
- Nix/NixOS — inspiração para realizações declarativas, gerações e rollback; não como ontologia de pertencimento ao ecossistema.
```
