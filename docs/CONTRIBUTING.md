# Contributing to PDFMaestro

Thank you for your interest in contributing. Here is how to get involved.

---

## Getting started

```bash
git clone https://github.com/RincolTech-Solutions-ltd/PDFMaestro.git
cd PDFMaestro
pip install --user -e ".[dev]"
python3 -m pdfmaestro
```

---

## Branch naming

| Type | Branch name |
|---|---|
| New feature | `feature/<short-description>` |
| Bug fix | `fix/<short-description>` |
| Docs only | `docs/<short-description>` |
| Refactor | `refactor/<short-description>` |

Never commit directly to `main`.

---

## Commit messages

Follow Conventional Commits:

```
feat: add freehand ink annotation tool
fix: signature SMask fails on rotated pages
refactor: extract renderer into viewer.py
docs: add crop section to USAGE.md
```

---

## Code style

Run the linter before opening a PR:

```bash
ruff check pdfmaestro/
```

Line length is 100 characters. Type hints are expected for all public functions.

---

## Running tests

```bash
pytest tests/
```

---

## Opening a pull request

1. Fork the repo or create a branch from `main`.
2. Make your changes, write/update tests if applicable.
3. Open a PR against `main` with a clear description of what changed and why.

---

## Reporting bugs

Open an issue on GitHub. Include:

- OS and Python version
- Steps to reproduce
- Expected vs. actual behaviour
- The PDF file (if it is not confidential and the bug is file-specific)

---

## License

By contributing, you agree that your changes will be licensed under the **GPL-3.0-or-later** license that covers this project.
