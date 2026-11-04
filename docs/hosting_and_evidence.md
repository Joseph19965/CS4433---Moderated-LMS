# Local Git provenance and remaining hosted evidence

This archive contains a **local Git repository** with two distinct, reproducible versions:

- `v1.0.0`: original offline moderated LMS with individual extensions, tested locally (14 test groups passed). It has no `class_extension` table or class-wide extension feature.
- `v1.1.0`: class-wide deadline CR1 added with deadline precedence; includes lecturer/HoD review queues, expanded diagrams, tests and documentation (15 test groups passed).

Commands from the extracted project folder:

```sh
git log --oneline --decorate
git diff v1.0.0 v1.1.0 -- src include tests config
git show v1.0.0:config/schema.sql
git status
```

To publish, create an empty repository in your own GitHub account, then run:

```sh
git remote add origin https://github.com/YOUR-ACCOUNT/YOUR-REPOSITORY.git
git push -u origin main
git push origin v1.0.0 v1.1.0
```

Use an actual repository URL from GitHub; never submit the placeholder. Under the Actions tab, inspect the configured workflow run and collect the real pass/fail, cppcheck and coverage evidence. The local copy does not constitute hosted CI proof. You can protect main and use pull requests for future changes; the local reconstructed two-commit history is not evidence of an earlier supervisor review or a historical multi-person development process.
