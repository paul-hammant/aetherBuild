# Feature: outbound webhook trigger step

Filed by aeb Claude from Paul's request, 2026-05-16. Design locked
through a Q&A round; this doc is the spec being implemented.

## What this is

A way, **expressed in aeb grammar**, for a pipeline to invoke a URL
when it reaches a given point — so aeb can be the *producer* side of
a webhook-centric automation system. aeb finishes (a target, or the
whole pipeline) → it pings a URL → a general webhook listener on the
far end triggers whatever it triggers (a deploy, an n8n/Zapier flow,
another pipeline, a `repository_dispatch`, …). aeb neither knows nor
cares what the listener does.

Direction is **outbound only**. aeb does not become a server, does
not listen for inbound webhooks, does not poll. (Inbound triggering —
"a webhook starts an aeb run" — is a separate concern; see TODO.md's
`.trigger.ae` roadmap line.)

Core, **language-agnostic** capability — it ships in core aeb
(`lib/webhook/`), not in any `lib/<lang>` SDK. A webhook step works
the same whether the pipeline builds Java, Rust, or nothing at all.

## Motivation

aeb already models a pipeline as a DAG of build/test/dist targets.
The missing piece for CI/automation use is a declared terminal
action: "when this pipeline (or this target) is done, tell something
else." Today that lives in hand-written CI YAML wrapped *around* aeb.
Pulling it into `.ae` grammar keeps the whole pipeline — including its
downstream hand-off — in one greppable, DAG-ordered description.

## Grammar — one builder, no magic filename

The feature is a single SDK builder, `webhook.fire(b) { ... }`, called
inside a `.ae` file's `main()` like any other builder (`brew.formula`,
`bash.test`, …). **There is no `.webhook.ae` suffix and no new target
classification.** aeb has no magic target filenames — behavior comes
from what a file's `main()` calls, never from its name. The webhook
fires because the file calls `webhook.fire(b)`; the filename is
irrelevant and can be anything in any language/script.

Two equally-valid placements, same builder both ways:

### A dedicated file (end-of-pipeline ping)

A `.ae` file whose `main()` only declares dependencies and a webhook.
It is a DAG node like any other; its `build.dep()`s order it last, so
it fires once after everything it follows. Name it whatever you like.

```aether
// java_app/.deploy-hook.ae   (name is free — .通知.ae works the same)
import build
import webhook (url, on, header)

main() {
    b = build.start()
    build.dep(b, "java_app/.tests.ae")
    build.dep(b, "java_app/.dist.ae")
    webhook.fire(b) {
        url("https://hooks.example/deploy?sha={{commit}}&br={{branch}}")
        header("Authorization", "Bearer {{env:DEPLOY_TOKEN}}")
        on("ci")
        on("branch:main")
    }
}
```

### Inside an existing target (per-stage ping)

The `webhook.fire(b)` block sits in a `.build.ae` / `.tests.ae` /
`.dist.ae` next to that target's own builders; it fires when that
target finishes.

```aether
// java_app/.dist.ae
import build
import brew
import webhook (url, on)

main() {
    b = build.start()
    brew.formula(b) { ... }
    webhook.fire(b) {
        url("https://hooks.example/packaged?sha={{commit}}")
        on("ci")
    }
}
```

A webhook-only file shows as `build` in the run summary (the default
classification). That is a cosmetic wart and explicitly *not* worth a
new English filename suffix; if it ever matters, classify by content
("this node calls `webhook.fire(`") — filename-agnostic and
philosophy-consistent. Out of v1 scope.

## The `webhook.fire(b)` SDK

v1 setters (single-arg-per-call, per Aether's fixed arity — `header`
is the one two-arg setter, mirroring how other SDKs pair key/value):

- **`url(STR)`** — the target URL. Supports `{{...}}` context
  interpolation (below). Required.
- **`on(STR)`** — a gate predicate; repeatable. Every `on(...)` must
  pass for the webhook to fire (AND semantics). Optional — no `on()`
  means "always fire when reached".
- **`method(STR)`** — `"POST"` (default) or `"GET"`. Default POST
  with an empty body — a bare ping.
- **`header(KEY, VAL)`** — a request header; repeatable. `VAL`
  supports `{{...}}` interpolation, so `header("Authorization",
  "Bearer {{env:TOKEN}}")` is the auth path. In v1 per Paul's call.

No `body(...)` / payload templating in v1 — see "Not in v1".

## `on()` gate predicates

The gate is evaluated at fire time against the live environment. If
any predicate fails the HTTP call is skipped (and a
`webhook: skipped (on:local)`-style line is printed for visibility).

v1 predicates:

| Predicate              | Fires when                                       |
|------------------------|--------------------------------------------------|
| `on("ci")`             | running under any CI (the `CI` env var is set)   |
| `on("local")`          | NOT under CI                                     |
| `on("branch:main")`    | current branch equals `main`                     |
| `on("branch:rel-*")`   | current branch glob-matches `rel-*`               |
| `on("!branch:wip-*")`  | current branch does NOT glob-match `wip-*`        |
| `on("github")`         | detected CI system is GitHub Actions             |
| `on("!jenkins")`       | detected CI system is NOT Jenkins                |

A leading `!` negates any predicate. Branch globs use
`string.glob_match` (POSIX fnmatch, already in stdlib).

**There is deliberately no `on("success")` / `on("failure")`.** aeb
aborts the run on the first failed target, so a webhook node — be it
a dedicated file or an in-target block — is only ever *reached* when
everything before it succeeded. The webhook is structurally always on
a success path; an outcome gate would have no other value to take. A
failure-notification webhook would need runner-level "run this node
even on abort" support — out of scope (see "Not in v1").

## `url()` / `header()` context interpolation

`{{name}}` tokens are substituted from aeb's runtime context:

| Token              | Value                                            |
|--------------------|--------------------------------------------------|
| `{{branch}}`       | current VCS branch                               |
| `{{commit}}`       | current commit hash (full)                       |
| `{{commit_short}}` | current commit hash (short)                      |
| `{{repo}}`         | repository root directory name                   |
| `{{ci}}`           | detected CI system name, or `local`              |
| `{{target}}`       | the target the webhook follows / is attached to  |
| `{{env:NAME}}`     | the environment variable `NAME` (the escape hatch — `{{env:GITHUB_SHA}}`, secrets, …) |

Values interpolated **into a URL** are URL-encoded (a branch like
`feature/foo` is safe in a query string); values interpolated into a
**header** are inserted verbatim.

**Why `{{...}}` and not `${...}`:** Aether's own string interpolation
grabs `${...}` at *parse* time (LLM.md § "Idioms that keep biting").
A literal `url("...?b=${branch}")` in a `.ae` file would have
`${branch}` consumed by the Aether compiler — resolving to a
nonexistent Aether variable, not aeb's context. `{{...}}` is untouched
by Aether interpolation and survives to the webhook SDK, which does
its own substitution at fire time. Decided syntax.

## Runtime model

Both placements fit the existing orchestrator with no runner surgery:

- **In-target block**: `webhook.fire(b)` is just another builder in
  the target's `main()`; it runs in-process after the preceding
  builders, in topo order.
- **Dedicated file**: a normal DAG node; `build.dep()`s order it; its
  `main()` calls `webhook.fire(b)`.

Because aeb aborts on failure, reaching either = upstream success.

HTTP is done with the native `std.http` stdlib — no `curl`
dependency, consistent with aeb using native stdlib for runtime
capabilities and shelling out only for build *tools*.

A webhook listener being unreachable logs a warning; it does **not**
fail the build (a down listener should not redden a green build).

## CI detection (this feature closes a roadmap gap)

`on("ci")` / `on("github")` / `{{ci}}` need CI-system detection,
which aeb does not have today — TODO.md and LLM.md's scope table both
list "is this CI / which CI" as a roadmap line item. This feature
delivers it: a `_detect_ci()` helper **in `lib/build/`** (Paul's call
— core, reusable by telemetry and the future `.trigger.ae`, not
buried in `lib/webhook`). It probes the well-known per-system env
vars:

| System         | Probe env var                        |
|----------------|--------------------------------------|
| GitHub Actions | `GITHUB_ACTIONS`                     |
| GitLab CI      | `GITLAB_CI`                          |
| Jenkins        | `JENKINS_URL` / `BUILD_NUMBER`       |
| CircleCI       | `CIRCLECI`                           |
| Travis         | `TRAVIS`                             |
| Buildkite      | `BUILDKITE`                          |
| (generic)      | `CI` (set by all of the above)       |

`_detect_ci()` returns the system name, or `local` when none match.
The TODO.md / LLM.md roadmap entries get updated to "done (via the
webhook feature)".

## Not in v1

- **`body(...)` / payload templating** — bare ping only; the signal
  rides in the `url()` query string. A JSON build-summary payload is
  a clean v2.
- **Failure / always webhooks** — would need runner-level "run this
  node even on abort" support. Out of scope (see runtime model).
- **Retries / backoff** on a failed HTTP call — v1 logs the failure
  and does not fail the build; no retry.
- **Inbound webhooks** — aeb-as-listener is explicitly not this.
- **A `.webhook.ae` classification suffix** — rejected; no magic
  filename, no new English suffix (see Grammar).

## Acceptance criteria

- A dedicated webhook file and an in-target `webhook.fire(b)` block
  both issue a real HTTP request when their `on()` gates pass.
- `on("local")` skips under CI; `on("ci")` skips locally;
  `on("branch:X")` and `on("!jenkins")` gate correctly; multiple
  `on()` calls AND together.
- `{{branch}}` / `{{commit}}` / `{{ci}}` / `{{env:X}}` interpolate;
  URL-context values are URL-encoded, header values verbatim.
- The URL/header interpolation and the `on()` predicate evaluator are
  **pure** helpers, unit-tested in `tests/test_webhook_*.ae` (env
  passed in as data, not read).
- A skipped webhook prints why; a fired one is visible in the run
  output.
- Smoke: a local `nc` / one-line python listener receives the ping.
- A webhook listener being unreachable logs a warning, does not fail
  the build.

## Decisions (resolved with Paul)

1. **`header()` is in v1** — Bearer-token auth is common enough for a
   general listener that it earns its place; it is a small isolated
   two-arg setter.
2. **`{{...}}` token syntax** — over `${...}`, for the Aether
   parse-time-interpolation reason above.
3. **`_detect_ci()` lives in `lib/build/`** — core and reusable, not
   `lib/webhook/`.
4. **No `.webhook.ae` suffix / no new classification** — aeb has no
   magic filenames; the webhook fires from the `webhook.fire(b)` call,
   so a dedicated webhook file is named freely.
