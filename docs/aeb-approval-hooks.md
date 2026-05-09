# Approval Hooks

Approval gates are usually a CI/CD responsibility: a human reviews a
release, clicks approve, and the pipeline continues. aeb should not
become that UI or scheduler.

But aeb can provide an approval-check hook: a target can ask an
external system whether it is allowed to continue, then fail fast if
the answer is no.

## Shape

The hook is a closure grammar, consistent with the rest of aeb:

```aether
approval.jira(b) {
    base_url("https://jira.example.com")
    issue("REL-1234")
    require_status("Approved")
    require_label("release-approved")
    token_env("JIRA_TOKEN")
}
```

The check does not pause for a human. It queries a system of record and
returns pass/fail. If approval is missing, the target fails with a
clear message.

## Jira example

A Jira-backed check:

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

## Generic approval rows

```aether
approval.command(b) {
    subject_env("CHANGE_ID")
    run("scripts/check-release-approval.sh")
    arg_env("CHANGE_ID")
    approvals_path("approvals")
    approver_id_path("id")
    approved_at_path("approved_at")
    approval_status_path("status")
    approval_status("approved")
    require_approver("person1")
    require_approver("person2")
    require_approver("person3")
}
```

The command prints JSON from Jira, ServiceNow, GitHub, GitLab, Azure
DevOps, or a firm-specific policy service. aeb maps provider JSON into
a common evidence shape and fails if required approvers or statuses are
missing.

## Attestation verification

For systems that can emit a plain-text approval claim, aeb can verify
the claim by hash:

```aether
approval.attestation(b) {
    subject_env("CHANGE_ID")
    attestation_command("scripts/approval-attestation.sh \"$CHANGE_ID\"")
    verify_via("https://verify.example.com/c")
}
```

The script prints the release/change ID, approver IDs, timestamps, and
other audit facts. aeb canonicalizes the text, computes SHA-256 with
`std.cryptography`, checks `verify_via/<hash>` with `curl`, and writes
the canonical claim, hash, and verify URL as evidence.

This is a LiveVerify-style pattern, not a claim that `Live Verify` is a
CI/CD standard. The important part is the issuer-owned verification
endpoint for a canonical claim.

## Boundary

aeb should support approval checks, not approval workflow ownership.

Good:

- Query Jira/ServiceNow/GitHub/OPA for a go/no-go decision.
- Verify canonical approval claims against issuer endpoints.
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
