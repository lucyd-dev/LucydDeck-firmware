# Issue tracker: GitHub

Firmware issues live as GitHub issues in `lucyd-dev/LucydDeck-firmware`. Host issues live in `lucyd-dev/LucydDeck`. Run `gh` inside the clone and it infers the repo; pass `-R lucyd-dev/LucydDeck` for host issues.

## Conventions

- **Create**: `gh issue create --title "..." --body "..."`.
- **Read**: `gh issue view <number> --json number,title,body,labels,comments`.
- **List open**: `gh issue list --state open --json number,title,labels`.
- **Comment**: `gh issue comment <number> --body "..."`.
- **Label**: `gh issue edit <number> --add-label "..."`.
- **Close**: `gh issue close <number> --comment "..."`.
- **Cross-repo reference**: write `lucyd-dev/LucydDeck-firmware#9` or `lucyd-dev/LucydDeck#6` in full. Numbers do not carry across repositories.

## Triage labels

The five canonical roles: `needs-triage`, `needs-info`, `ready-for-agent`, `ready-for-human`, `wontfix`. Create them on first use if they are missing.

## Blocking

GitHub's native issue dependencies are the canonical form. Where they are not available, put a `Blocked by: #10` line at the top of the issue body. A ticket is unblocked when every blocker is closed.

## Pull requests

External PRs are not a request surface. Host and firmware changes go through branches and PRs by the owner.
