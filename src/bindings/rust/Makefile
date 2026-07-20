# Convenience targets for the GTCaca Rust bindings (gtcaca-sys + gtcaca).
# Run from this directory: `make <target>` (e.g. `make crate-publish`).
#
# ── FIRST-TIME SETUP (once per machine) ────────────────────────────────────
#   Publishing to crates.io needs an account + an auth token:
#     1. Sign in at https://crates.io  ("Log in with GitHub").
#     2. Verify your email  (Account Settings → Profile) — publishing is
#        BLOCKED until an email is confirmed.
#     3. Create a token     (Account Settings → API Tokens → New Token).
#     4. Store it locally:  `cargo login`   (paste the token when prompted).
#   After that, `make crate-publish` just works.
#
# Publishes are PERMANENT: a version can be yanked but never re-uploaded or
# deleted. Use `make crate-publish-dry` first, and bump the version (in
# Cargo.toml [workspace.package]) if you need to re-release.

SHELL := /bin/bash
SYNC  := scripts/sync-sources.sh

.DEFAULT_GOAL := help

.PHONY: help build test smoke fmt lint vendor crate-package crate-publish-dry \
        crate-publish crate-check-names clean

help: ## Show this help
	@grep -hE '^[a-zA-Z_-]+:.*?## ' $(MAKEFILE_LIST) \
	  | awk 'BEGIN{FS=":.*?## "}{printf "  \033[36m%-20s\033[0m %s\n",$$1,$$2}'

build: ## Build both crates (against the in-repo C sources)
	cargo build

test: ## Run the test suite
	cargo test

smoke: ## Non-interactive FFI round-trip check (runs under a pty)
	cargo build --example smoke
	TERM=xterm script -q /dev/null ./target/debug/examples/smoke | tr -d '\r' | grep -a SMOKE_OK

fmt: ## Format
	cargo fmt

lint: ## Clippy (warnings as-is)
	cargo clippy --workspace --examples

vendor: ## Copy the C sources into gtcaca-sys/vendor (needed for publishing)
	bash $(SYNC)

crate-package: vendor ## Build the -sys tarball in isolation (no upload)
	cargo package -p gtcaca-sys --allow-dirty
	@$(MAKE) --no-print-directory _unvendor

crate-publish-dry: ## Dry-run publish of both crates (no upload)
	@$(MAKE) --no-print-directory vendor
	cargo publish -p gtcaca-sys --dry-run --allow-dirty
	cargo publish -p gtcaca     --dry-run --allow-dirty
	@$(MAKE) --no-print-directory _unvendor

crate-publish: ## Publish BOTH crates to crates.io (needs `cargo login` first — see top of file)
	@command -v cargo >/dev/null || { echo "cargo not found"; exit 1; }
	@echo ">> vendoring C sources into gtcaca-sys/vendor ..."
	@$(MAKE) --no-print-directory vendor
	@echo ">> publishing gtcaca-sys (the -sys crate must go first) ..."
	cargo publish -p gtcaca-sys
	@echo ">> publishing gtcaca (cargo waits for -sys to appear in the index) ..."
	cargo publish -p gtcaca
	@$(MAKE) --no-print-directory _unvendor
	@echo ">> done. Published gtcaca-sys and gtcaca."

crate-check-names: ## Check whether the crate names are already taken on crates.io
	@for c in gtcaca-sys gtcaca; do \
	  code=$$(curl -s -o /dev/null -w '%{http_code}' \
	    -A 'gtcaca-make/1.0 (name availability check)' \
	    "https://crates.io/api/v1/crates/$$c"); \
	  if [ "$$code" = "200" ]; then echo "  $$c: TAKEN"; \
	  elif [ "$$code" = "404" ]; then echo "  $$c: available"; \
	  else echo "  $$c: unknown (HTTP $$code)"; fi; \
	done

clean: ## Remove build artifacts and the vendored sources
	cargo clean
	@$(MAKE) --no-print-directory _unvendor

# Internal: drop the vendored copy so local dev builds track the live src/ tree.
.PHONY: _unvendor
_unvendor:
	@rm -rf gtcaca-sys/vendor
