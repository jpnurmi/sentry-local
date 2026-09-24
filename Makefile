# optional machine-specific overrides; kept out of Git
-include Makefile.local
-include .env

.DEFAULT_GOAL := help

# checkout to build into the test apps; environment or make override
SENTRY_NATIVE_DIR ?= ../sentry-native

# required for crash reports
SENTRY_DSN ?=
# sentry-cli reads .sentryclirc or SENTRY_URL, SENTRY_ORG,
# SENTRY_PROJECT, and SENTRY_AUTH_TOKEN from the environment

# Linux/macOS checkouts and generated configuration
SENTRY_DIR ?= ../sentry
RELAY_DIR ?= ../relay
SENTRY_CONF ?= $(CURDIR)/.local/sentry
RELAY_CONF ?= $(CURDIR)/.local/relay
# browser hostname; must resolve to the devserver host
SENTRY_HOST ?= dev.getsentry.net

export SENTRY_CONF RELAY_CONF SENTRY_HOST
export SENTRY_DSN SENTRY_URL SENTRY_ORG SENTRY_PROJECT SENTRY_AUTH_TOKEN

ifneq ($(OS),Windows_NT)
export PATH := $(HOME)/.cargo/bin:$(HOME)/.local/bin:$(HOME)/.local/share/sentry-devenv/bin:$(PATH)
endif

SENTRY_ENV = PATH="$$PWD/.venv/bin:$$PWD/.devenv/bin:$$PWD/node_modules/.bin:$$PATH"

.PHONY: help build debug-files-upload app-hang cpp-exception sync sentry relay ip

define HELP
Sentry minidump precedence

Server:
  make sync                      Update Sentry checkout dependencies
  make sentry                    Set up and start Sentry in one terminal
  make relay                     Build and start Relay in another terminal
  make ip                        Print the server's IPv4 address for client DSN

Client:
  make build                     Build test apps
  make debug-files-upload        Upload debug files
  make app-hang                  Run watchdog crash and app hang cases
  make cpp-exception             Run uncaught C++ exception case

Variables:
  SENTRY_DIR                     Path to getsentry/sentry (default: ../sentry)
  RELAY_DIR                      Path to getsentry/relay (default: ../relay)
  SENTRY_NATIVE_DIR              Path to getsentry/sentry-native (default: ../sentry-native)
  SENTRY_DSN                     DSN for crash reports (required)
  SENTRY_URL                     Sentry API URL for debug files (local: http://dev.getsentry.net:8000/)
  SENTRY_PROJECT                 Project slug for debug files (default: target name for test targets)
  SENTRY_ORG                     Organization slug for debug files
  SENTRY_AUTH_TOKEN              Auth token for debug file uploads (Organization: Read; Release: Admin)
endef

help:
	$(info $(HELP))
	@exit 0

$(SENTRY_NATIVE_DIR)/CMakeLists.txt:
	$(error Missing sentry-native checkout: $(SENTRY_NATIVE_DIR); set SENTRY_NATIVE_DIR in Makefile.local)

build: $(SENTRY_NATIVE_DIR)/CMakeLists.txt
	cmake -S . -B build -DSENTRY_NATIVE_DIR:PATH="$(SENTRY_NATIVE_DIR)" -DCMAKE_BUILD_TYPE=Debug
	cmake --build build --config Debug

debug-files-upload: build
	sentry-cli debug-files upload --wait build/bin/Debug

app-hang: export SENTRY_PROJECT := $(or $(SENTRY_PROJECT),app-hang)
app-hang: $(if $(strip $(SENTRY_DSN)),debug-files-upload)
	$(if $(strip $(SENTRY_DSN)),,$(error Set SENTRY_DSN for the crash report))
	-@cmake -E chdir build/bin/Debug cmake -E env "SENTRY_DSN=$(SENTRY_DSN)" ./app-hang$(if $(filter Windows_NT,$(OS)),.exe) crash
	-@cmake -E chdir build/bin/Debug cmake -E env "SENTRY_DSN=$(SENTRY_DSN)" ./app-hang$(if $(filter Windows_NT,$(OS)),.exe) wait-condition

cpp-exception: export SENTRY_PROJECT := $(or $(SENTRY_PROJECT),cpp-exception)
cpp-exception: $(if $(strip $(SENTRY_DSN)),debug-files-upload)
	$(if $(strip $(SENTRY_DSN)),,$(error Set SENTRY_DSN for the crash report))
	-@cmake -E chdir build/bin/Debug cmake -E env "SENTRY_DSN=$(SENTRY_DSN)" ./cpp-exception$(if $(filter Windows_NT,$(OS)),.exe)

ifeq ($(OS),Windows_NT)
sync sentry relay ip:
	$(error make $@ requires Linux or macOS; run make for the workflow)
else
$(SENTRY_DIR)/.venv/bin/devservices:
	bash scripts/bootstrap.sh sentry "$(SENTRY_DIR)"

sync: $(SENTRY_DIR)/.venv/bin/devservices
	cd "$(SENTRY_DIR)" && $(SENTRY_ENV) .venv/bin/devenv sync

sentry: $(SENTRY_DIR)/.venv/bin/devservices
	cd "$(SENTRY_DIR)" && $(SENTRY_ENV) .venv/bin/sentry init --dev --no-clobber
	"$(SENTRY_DIR)/.venv/bin/python" scripts/setup.py
	@set -e; \
	cd "$(SENTRY_DIR)"; \
	export $(SENTRY_ENV); \
	.venv/bin/devservices down; \
	trap '.venv/bin/devservices down' EXIT; \
	trap 'exit 130' INT; \
	trap 'exit 143' TERM; \
	.venv/bin/devservices toggle relay local; \
	.venv/bin/devservices up --mode ingest; \
	.venv/bin/devservices up --mode symbolicator; \
	.venv/bin/sentry devserver --client-hostname "$(SENTRY_HOST)" 0.0.0.0:8000

relay:
	@test -f "$(RELAY_CONF)/credentials.json" || { echo "Run make sentry first to create the Relay configuration."; exit 1; }
	bash scripts/bootstrap.sh relay "$(RELAY_DIR)"
	# TODO: remove --release when debug relay processes a "sessions" item with two aggregates and no item_count without panic:
	# "New item has 2 items in category 'session', but original (after emitted outcomes) only has 1 left" (relay-server/src/managed/managed.rs:949)
	cd "$(RELAY_DIR)" && rustup run stable cargo run --release --locked --all-features --package relay -- --config "$(RELAY_CONF)" run

ip:
	@case "$$(uname -s)" in \
		Linux) \
			interface=$$(ip -4 route show default | awk '{for (i=1; i<=NF; i++) if ($$i == "dev") {print $$(i+1); exit}}'); \
			address=$$(ip -4 -o address show dev "$$interface" scope global | awk '{split($$4, parts, "/"); print parts[1]; exit}');; \
		Darwin) \
			interface=$$(route -n get default | awk '/interface:/ {print $$2}'); \
			address=$$(ipconfig getifaddr "$$interface");; \
		*) echo "make ip requires Linux or macOS." >&2; exit 1;; \
	esac; \
	test -n "$$address" || { echo "No IPv4 address found on the default network interface." >&2; exit 1; }; \
	printf '%s\n' "$$address"
endif
