import assert from "node:assert/strict";
import test from "node:test";

async function render() {
  const workerUrl = new URL("../dist/server/index.js", import.meta.url);
  workerUrl.searchParams.set("test", `${process.pid}-${Date.now()}`);
  const { default: worker } = await import(workerUrl.href);
  return worker.fetch(
    new Request("https://synth.test/", { headers: { accept: "text/html", host: "synth.test", "x-forwarded-proto": "https" } }),
    { ASSETS: { fetch: async () => new Response("Not found", { status: 404 }) } },
    { waitUntil() {}, passThroughOnException() {} },
  );
}

test("server-renders the factual SYNTH console", async () => {
  const response = await render();
  assert.equal(response.status, 200);
  assert.match(response.headers.get("content-type") ?? "", /^text\/html\b/i);
  const html = await response.text();
  assert.match(html, /SYNTH — Sempre pronto, sempre incompleto/);
  assert.match(html, /Sempre pronto/);
  assert.match(html, /SYSTEM_SYNTH_READY/);
  assert.match(html, /NOT YET APPLICABLE/);
  assert.match(html, /No relations/);
  assert.match(html, /https:\/\/synth\.test\/og\.png/);
  assert.doesNotMatch(html, /codex-preview|react-loading-skeleton|Your site is taking shape/);
});
