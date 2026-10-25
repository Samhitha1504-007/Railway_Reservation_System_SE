# Contributing workflow

Never push to `main`. Every change goes through a branch and a pull request.

## 1. Start a task
1. On the project board, drag your issue's card to **In Progress**.
2. Branch from a fresh `main`:
   ```
   git checkout main
   git pull origin main
   git checkout -b feature/<module>-<task>
   ```
   Find your issue numbers with `gh issue list --assignee @me`.

## 2. Open a PR that links the issue
```
git push -u origin feature/<module>-<task>
gh pr create --fill --body "Closes #<N>"
```
- `Closes #N`, `Fixes #N` or `Resolves #N` link and auto-close the issue. A bare `#N` only mentions it.
- One issue per PR where possible; for two, write `Closes #12, closes #15`.
- Fill in the PR template: what changed, and tick the checklist (builds locally, unit tests, no cppcheck warnings).

## 3. Review and CI
- Request a reviewer who is not you: `gh pr edit --add-reviewer <username>`.
- CI (build, unit tests, cppcheck) runs on every PR. On a red cross, click **Details**, fix, then commit and push to the **same branch**. The PR updates itself.
- Address review comments the same way and reply on each thread.

## 4. After merge
The issue closes and the card moves to **Done** automatically. Then:
```
git checkout main
git pull origin main
git branch -d feature/<module>-<task>
```

## Rules
- Small, frequent commits. Commit history is assessed per person.
- Unfinished sprint items move to the next sprint on the board.
