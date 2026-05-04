# Cores docs

Reference docs for the Cores SDK and how it feeds Studio and the public website.

These are the docs that don't fit in a header comment but aren't user-facing tutorial content either — they're the rules of the road for anyone (human or AI) writing drivers, modules, or annotations against this repo.

## Contents

- [studio-annotations.md](studio-annotations.md) — `@studio` directive reference. Every annotation the manifest generator understands, with examples. Read this before adding a new `core_*.h` module or Kiln tile driver to Studio.
- [sdk-api-tiers.md](sdk-api-tiers.md) — Tier 1 (handle-based) vs Tier 2 (default-instance) APIs. Explains why generated Studio C looks different from SDK examples and lists which functions live on which tier.

## Where else docs live

- [`AI.md`](../AI.md) — driver authoring playbook for AI agents. Covers tile driver scaffolding, register-map conventions, integer-only conversions, the integration checklist, and lessons learned from past drivers.
- [`Kiln/drivers/tile_*.h`](../kiln/drivers/) — Doxygen comments inside each tile driver header are the authoritative API doc for that tile. The website tile pages render directly from these headers.
- [`sdk/core/core_*.h`](../sdk/core/) — Doxygen + `@studio` annotations inside each core module header are the authoritative API doc for that category. The website SDK pages render from `manifests/sdk-docs/<category>.json`, which is generated from the headers.
- `manifests/` — generated artifacts (do not edit by hand). Run `tools/gen_studio_manifest.py` to regenerate.

## Authoritative source

When the docs and the headers disagree, the headers win. The manifests are derived; the docs in this directory describe conventions and tooling. If you find drift between any of these, fix the headers first, regenerate manifests, and update the prose docs in the same change.
