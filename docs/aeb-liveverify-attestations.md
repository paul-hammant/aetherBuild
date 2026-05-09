# LiveVerify-Style Attestations

`Live Verify` is not a settled CI/CD or supply-chain standard name.
It is used by identity and payment products, and `LiveVerify` also
appears as a browser-extension pattern for verifying selected text by
hashing it and checking an issuer endpoint.

For aeb, the useful idea is narrower and technical:

```text
canonical claim text -> SHA-256 -> issuer verification URL -> evidence
```

The source of truth remains Jira, ServiceNow, GitHub, GitLab, Azure
DevOps, or a firm-specific approval system. aeb does not decide who is
allowed to approve. It records and verifies the approval claim that the
source system or bridge script emits.

## Grammar

```aether
approval.attestation(b) {
    subject_env("CHANGE_ID")
    attestation_command("scripts/approval-attestation.sh \"$CHANGE_ID\"")
    verify_via("https://verify.example.com/c")
    evidence("target/release-approval-attestation.json")
}
```

The command prints plain text:

```text
Release: R-2026-05-09
Change: CHG123456
person1: approved at 2026-05-09T10:00:00Z
person2: approved at 2026-05-09T10:05:00Z
person3: approved at 2026-05-09T10:07:00Z
```

aeb canonicalizes that text, computes SHA-256 with
`std.cryptography.sha256_hex`, checks:

```text
GET https://verify.example.com/c/<sha256>
```

and writes evidence containing:

- `provider`
- `subject`
- `ok`
- `attestation_sha256`
- `verify_url`
- `claim`

## Canonicalization

The current canonical form is intentionally small:

- convert CRLF and CR to LF
- remove trailing spaces and tabs from each line
- remove trailing blank lines
- ensure exactly one final newline

That keeps bridge scripts easy to write in shell, Python, PowerShell,
or a ticket-system automation.

## Why this fits aeb

Approval systems often already have the right controls: approver
identity, group membership, audit trails, and retention. The missing
piece is usually a deterministic build-time check that says:

```text
this exact release claim is approved by the issuer right now
```

aeb can perform that check without becoming Jira, ServiceNow, or a CI
approval UI. It turns the approval into a reproducible build input and
emits the hash and claim as build evidence.

## Boundary

Good:

- Hash canonical approval claims inside aeb.
- Verify the hash against an issuer-owned endpoint.
- Keep approver IDs, timestamps, release IDs, and claim text in logs.
- Use Jenkins, TeamCity, GitHub Actions, or GitLab parameters to pass
  the release/change ID into the aeb invocation.

Bad:

- Treat aeb as the approval authority.
- Store long-lived approval state in the build graph.
- Encode company approval policy in aeb when it already belongs to the
  ticketing or ITSM system.
