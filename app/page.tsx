"use client";

import { useMemo, useState } from "react";

type View = "estado" | "superficies" | "evidencia" | "fundacao";

const gates = [
  "FOUNDATION_FILE_PRESENT", "GPL_3_ONLY", "CPP26_BUILD", "FOUNDATION_VERIFY",
  "XDG_SEPARATION", "SELF_OBSERVATION", "RESOURCE_OBSERVATION",
  "SURFACE_MODEL_GENERIC", "RELATION_MODEL_GENERIC", "NO_FAKE_RELATIONS",
  "CONTEXTLAB_DOCUMENT_COMPAT", "CLI_HUMAN_READABLE", "SECOND_RUN_NO_OP",
];

const surfaces = [
  { id: "synth.cli", kind: "process-stream", locator: "stdio://synth", direction: "bidirectional", media: "text/plain", state: "SELF-OBSERVED" },
  { id: "synth.evidence", kind: "file", locator: "$XDG_STATE_HOME/synth/evidence/latest.json", direction: "outbound", media: "application/json", state: "SELF-OBSERVED" },
  { id: "synth.web", kind: "web-document", locator: "web://synth-console", direction: "outbound", media: "text/html", state: "BUILD-OBSERVED" },
];

const commands = ["synth status", "synth foundation verify", "synth evidence --json"];

export default function Home() {
  const [view, setView] = useState<View>("estado");
  const [jsonMode, setJsonMode] = useState(false);
  const [copied, setCopied] = useState("");
  const [observation, setObservation] = useState("SNAPSHOT DE LANÇAMENTO");
  const [menuOpen, setMenuOpen] = useState(false);

  const evidenceJson = useMemo(() => JSON.stringify({
    identity: "SYNTH",
    version: "0.1.0",
    roots: {
      configuration: "$XDG_CONFIG_HOME/synth",
      state: "$XDG_STATE_HOME/synth",
      runtime: "$XDG_RUNTIME_DIR/synth",
    },
    observed_surfaces: surfaces.map(({ id, kind, locator }) => ({ id, kind, locator })),
    observed_relations: [],
    epistemic_class: "OBSERVED",
  }, null, 2), []);

  function select(next: View) {
    setView(next);
    setMenuOpen(false);
    document.getElementById("console")?.scrollIntoView({ behavior: "smooth", block: "start" });
  }

  async function copy(command: string) {
    await navigator.clipboard?.writeText(command);
    setCopied(command);
    window.setTimeout(() => setCopied(""), 1600);
  }

  function observeNow() {
    setObservation(new Intl.DateTimeFormat("pt-BR", { hour: "2-digit", minute: "2-digit", second: "2-digit" }).format(new Date()));
  }

  return (
    <main>
      <nav className="topbar" aria-label="Navegação principal">
        <a className="brand" href="#top" aria-label="SYNTH — início">
          <span className="brand-mark">S</span><span>SYNTH</span><span className="version">v0.1.0</span>
        </a>
        <button className="menu-toggle" onClick={() => setMenuOpen(!menuOpen)} aria-expanded={menuOpen} aria-label="Abrir menu">{menuOpen ? "×" : "≡"}</button>
        <div className={menuOpen ? "nav-links open" : "nav-links"}>
          <button onClick={() => select("estado")}>Estado</button>
          <button onClick={() => select("superficies")}>Superfícies</button>
          <button onClick={() => select("evidencia")}>Evidência</button>
          <button onClick={() => select("fundacao")}>Fundação</button>
        </div>
        <div className="live"><span /> SISTEMA OPERACIONAL</div>
      </nav>

      <section className="hero" id="top">
        <div className="hero-copy">
          <div className="eyebrow"><span>01</span> ESTADO PRESENTE <i /></div>
          <h1>Sempre pronto.<br /><em>Sempre incompleto.</em></h1>
          <p className="lede">Um sistema aberto que observa o que existe — sem inventar o que ainda não existe.</p>
          <div className="hero-actions">
            <button className="primary" onClick={() => select("evidencia")}>Explorar evidência <span>↘</span></button>
            <button className="text-action" onClick={() => select("fundacao")}>Ver os 13 gates <span>→</span></button>
          </div>
        </div>
        <div className="manifesto" aria-label="Manifesto do sistema">
          <span>SYSTEM / 001</span>
          <p>O presente é explícito.<br />O futuro permanece aberto.</p>
          <div className="manifesto-axis"><i /><i /><i /><i /></div>
        </div>
      </section>

      <section className="readiness" aria-label="Estado de prontidão">
        {[["FOUNDATION_READY", "PASS"], ["SYSTEM_SYNTH_READY", "PASS"], ["ECOSYSTEM_SYNTH", "NOT YET APPLICABLE"]].map(([label, value], index) => (
          <article className={index === 2 ? "gate pending" : "gate"} key={label}>
            <span className="gate-index">0{index + 1}</span><p>{label}</p><strong><i />{value}</strong>
          </article>
        ))}
      </section>

      <section className="principle">
        <div className="section-label"><span>PRINCÍPIO FUNDACIONAL</span><b>RELATION-FIRST · EVIDENCE-GROUNDED</b></div>
        <blockquote>“Não se começa desenhando o ecossistema que se deseja ver.”</blockquote>
        <p>Começa-se construindo condições simples e estáveis para que relações reais possam existir — e ser observadas.</p>
      </section>

      <section className="console" id="console">
        <div className="console-heading">
          <div>
            <div className="eyebrow"><span>02</span> CONSOLE DO SISTEMA <i /></div>
            <h2>O sistema revela<br />o que sabe.</h2>
          </div>
          <p>Uma projeção humana da realização C++26. Sem abstrações que escondem a origem dos fatos.</p>
        </div>

        <div className="console-shell">
          <div className="console-tabs" role="tablist" aria-label="Visões do sistema">
            {(["estado", "superficies", "evidencia", "fundacao"] as View[]).map((item, index) => (
              <button key={item} role="tab" aria-selected={view === item} className={view === item ? "active" : ""} onClick={() => setView(item)}>
                <span>0{index + 1}</span>{item === "superficies" ? "SUPERFÍCIES" : item.toUpperCase()}
              </button>
            ))}
          </div>

          {view === "estado" && <div className="panel state-panel" role="tabpanel">
            <div className="panel-title"><div><span>IDENTIDADE</span><h3>Sistema SYNTH</h3></div><b className="class-tag">OBSERVED</b></div>
            <div className="state-grid">
              <div className="big-status"><span className="orb" /><strong>READY</strong><p>Dentro do envelope operacional declarado.</p></div>
              <dl>
                <div><dt>Versão</dt><dd>0.1.0</dd></div><div><dt>Linguagem</dt><dd>C++26</dd></div>
                <div><dt>Superfícies</dt><dd>03 presentes</dd></div><div><dt>Relações</dt><dd>00 observadas</dd></div>
                <div><dt>Epistemologia</dt><dd>OBSERVED</dd></div>
              </dl>
            </div>
            <div className="notice"><span>!</span><p><strong>Nenhum ecossistema é reivindicado.</strong> Um sistema pronto não precisa fingir uma composição que ainda não existe.</p></div>
          </div>}

          {view === "superficies" && <div className="panel" role="tabpanel">
            <div className="panel-title"><div><span>SUPERFÍCIES PRESENTES</span><h3>Interfaces observáveis</h3></div><b className="class-tag">03 OBJECTS</b></div>
            <div className="surface-list">
              {surfaces.map((surface, index) => <article key={surface.id}>
                <div className="surface-number">0{index + 1}</div>
                <div><span>{surface.kind}</span><h4>{surface.id}</h4><code>{surface.locator}</code></div>
                <dl><div><dt>direção</dt><dd>{surface.direction}</dd></div><div><dt>mídia</dt><dd>{surface.media}</dd></div></dl>
                <b>{surface.state}</b>
              </article>)}
            </div>
          </div>}

          {view === "evidencia" && <div className="panel" role="tabpanel">
            <div className="panel-title evidence-title"><div><span>EVIDÊNCIA FACTUAL</span><h3>Estado computável</h3></div>
              <div className="mode-switch"><button className={!jsonMode ? "on" : ""} onClick={() => setJsonMode(false)}>HUMANO</button><button className={jsonMode ? "on" : ""} onClick={() => setJsonMode(true)}>JSON</button></div>
            </div>
            {jsonMode ? <pre className="json-view">{evidenceJson}</pre> : <div className="evidence-grid">
              <article><span>IDENTITY</span><strong>SYNTH</strong><small>OBSERVED</small></article>
              <article><span>VERSION</span><strong>0.1.0</strong><small>OBSERVED</small></article>
              <article><span>UPTIME</span><strong>AMOSTRADO</strong><small>NO PROCESSO</small></article>
              <article><span>RSS / CPU</span><strong>MEDIDO</strong><small>GETRUSAGE</small></article>
              <article className="wide"><span>XDG ROOTS</span><strong>CONFIG · STATE · RUNTIME</strong><small>SEPARADOS</small></article>
              <article><span>RELATIONS</span><strong>0</strong><small>OBSERVED</small></article>
            </div>}
            <div className="observation-bar"><span>OBSERVAÇÃO DA INTERFACE · {observation}</span><button onClick={observeNow}>Atualizar observação ↻</button></div>
          </div>}

          {view === "fundacao" && <div className="panel" role="tabpanel">
            <div className="panel-title"><div><span>CONFORMIDADE</span><h3>13 gates fundacionais</h3></div><b className="class-tag">13 / 13 PASS</b></div>
            <div className="gate-list">{gates.map((gate, index) => <div key={gate}><span>{String(index + 1).padStart(2, "0")}</span><code>{gate}</code><b><i />PASS</b></div>)}</div>
          </div>}
        </div>
      </section>

      <section className="command-section">
        <div className="command-copy"><div className="eyebrow"><span>03</span> EXPERIÊNCIA HUMANA <i /></div><h2>Claro primeiro.<br /><em>Computável quando preciso.</em></h2><p>A CLI fala com pessoas por padrão. JSON é uma escolha explícita — nunca uma barreira.</p></div>
        <div className="terminal" aria-label="Exemplos da interface de linha de comando">
          <div className="terminal-bar"><span>SYNTH / TERMINAL</span><i /><i /><i /></div>
          {commands.map(command => <button key={command} onClick={() => copy(command)}><code><span>$</span> {command}</code><b>{copied === command ? "COPIADO" : "COPIAR"}</b></button>)}
          <div className="terminal-output"><span>FOUNDATION_READY</span><b>PASS</b><span>SYSTEM_SYNTH_READY</span><b>PASS</b><span>ECOSYSTEM_SYNTH</span><em>NOT_YET_APPLICABLE</em></div>
        </div>
      </section>

      <section className="no-relations">
        <div className="zero">0</div><div><span>RELAÇÕES OBSERVADAS</span><h2>No relations<br />observed.</h2><p>Isso não é ausência de visão. É precisão epistemológica.</p></div>
      </section>

      <footer>
        <div className="footer-brand"><span className="brand-mark inverse">S</span><strong>SYNTH</strong></div>
        <p>Sistema aberto · C++26 · GPL-3.0-only</p><p className="footer-motto">SEMPRE PRONTO, SEMPRE INCOMPLETO.</p>
      </footer>
    </main>
  );
}
