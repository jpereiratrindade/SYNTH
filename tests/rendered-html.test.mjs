import assert from "node:assert/strict";
import { access, readFile, readdir } from "node:fs/promises";
import test from "node:test";

test("builds a portable factual SYNTH interface", async () => {
  const [html, source, assets] = await Promise.all([
    readFile(new URL("../dist/index.html", import.meta.url), "utf8"),
    readFile(new URL("../web/App.tsx", import.meta.url), "utf8"),
    readdir(new URL("../dist/assets/", import.meta.url)),
  ]);
  assert.match(html, /SYNTH — Sempre pronto, sempre incompleto/);
  assert.match(html, /<script type="module" crossorigin src="\/assets\//);
  assert.match(source, /SYSTEM_SYNTH_READY/);
  assert.match(source, /NOT YET APPLICABLE/);
  assert.match(source, /No relations/);
  assert.ok(assets.some((file) => file.endsWith(".js")));
  assert.ok(assets.some((file) => file.endsWith(".css")));
  await access(new URL("../dist/og.png", import.meta.url));
});
