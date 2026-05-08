# Approval Hooks

Approval gates are usually a CI/CD responsibility: a human reviews a
release, clicks approve, and the pipeline continues. aeb should not
become that UI or scheduler.

But aeb can provide an approval-check hook: a target can ask an
external system whether it is allowed to continue, then fail fast if
the answer is no.

## Shape

The hook should be a closure grammar, consistent with the rest of aeb:

```aether
approval.check(b) {
    name("prod-release")
    subject("apps/api/.dist.ae")
    provider("jira")
    issue("REL-1234")
    require_status("Approved")
    require_field("Risk", "Accepted")
    require_field("CAB", "Approved")
    timeout("30s")
}
```

The check does not pause for a human. It queries a system of record and
returns pass/fail. If approval is missing, the target fails with a
clear message.

## Jira example

A Jira-backed implementation could:

- read credentials from `JIRA_TOKEN` or a CI-provided secret file
- fetch the issue over REST
- verify status, labels, fields, or approvers
- write an approval evidence file under `target/<module>/`
- redact tokens in logs and telemetry

The build remains non-interactive:

```text
approved    -> continue
not approved -> fail
unreachable -> fail or soft-fail, depending on policy
```

## Grammar ideas

```aether
approval.jira(b) {
    issue("REL-1234")
    base_url("https://jira.example.com")
    token_env("JIRA_TOKEN")
    require_status("Approved")
    require_label("release-approved")
    require_field("Change Type", "Standard")
    evidence("target/release-approval.json")
}
```

For generic systems:

```aether
approval.command(b) {
    name("prod-release")
    run("scripts/check-release-approval.sh REL-1234")
    expect_exit(0)
}
```

## Boundary

aeb should support approval checks, not approval workflow ownership.

Good:

- Query Jira/ServiceNow/GitHub/OPA for a go/no-go decision.
- Fail a `.dist.ae` or `.deploy.ae` target if policy is not met.
- Persist machine-readable evidence.
- Let CI own the human approval step.

Bad:

- Pause and wait for humans.
- Store approval state itself.
- Send reminders and escalations.
- Implement a ticketing workflow engine.

## Rule of thumb

Approvals in aeb should be deterministic policy checks over external
state. The source of approval lives elsewhere; aeb only asks whether
the current artifact is allowed to proceed.
