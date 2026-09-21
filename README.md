# Local Sentry playground

Run `make` for prerequisites, configuration, and available commands.

- **Linux/macOS:** `make sentry`, then `make relay` in another terminal.

The targets require existing checkouts and print setup instructions as needed.
Test behavior and expected results are documented with the implementations in
[`src/`](src/).

`make build` builds test apps available on the current platform;
`make debug-files-upload` builds and uploads their debug files without capturing
an event. Output is in `build/bin/Debug`.

## Local server — Linux/macOS

**Debian/Ubuntu:** install [Docker Engine](https://docs.docker.com/engine/install/)
with access for your regular user, then:

```sh
sudo apt-get update
sudo apt-get install build-essential cmake curl git pkg-config libssl-dev python3-dev watchman
```

On other Linux distributions, install equivalent packages.

**macOS:** install [Homebrew](https://brew.sh/), then:

```sh
xcode-select --install
brew install cmake pkg-config
```

From this repository:

```sh
make sentry
```

On first use, this installs missing uv and [Sentry devenv](https://github.com/getsentry/devenv),
installs dependencies, and initializes the database in your Sentry checkout.
On macOS, setup also installs Chrome if missing and starts Colima when Docker is
unavailable. It generates Relay credentials and configures Symbolicator. Later
runs restart the services and reuse existing data.

In a second terminal:

```sh
make relay
```

This installs rustup if missing and updates stable Rust.
It then builds and starts Relay with that toolchain.
Rerun it after changing Relay code. After changing Sentry dependencies, run
`make sync` from this repository.
Make adds the uv, Rust, and devenv installation directories to `PATH`; add them
to your shell profile if you also run those tools directly:

```sh
export PATH="$HOME/.cargo/bin:$HOME/.local/bin:$HOME/.local/share/sentry-devenv/bin:$PATH"
```

Open `dev.getsentry.net:8000` on the server. Sign in with `admin@sentry.io` /
`admin`, change the password, and create a Native project. Copy its DSN from
**Client Keys (DSN)** and create an auth token with **Organization → Read** and **Release → Admin**.

Relay listens on `0.0.0.0:7899`. Other machines must use the server's **LAN address**;
Relay's `127.0.0.1` addresses connect to services on that same server. Allow ports
7899 (ingest), 8001 (API), and 8000 (UI) on your development network. For browser
access from another machine, map `dev.getsentry.net` to the server in that machine's hosts file.

Configuration is in the ignored `.local/` directory. Docker services and databases
are shared with other Sentry checkouts. Ctrl+C in `make sentry` stops its workers
and dependencies too. Stop `make relay` with Ctrl+C in its terminal.

## Client

On Windows, install Visual Studio's **Desktop development with C++** workload, CMake 3.18+,
Git, GNU Make, and [sentry-cli](https://docs.sentry.io/cli/installation/).
Use Git Bash with the Visual Studio x64 build environment and these tools on `PATH`.

sentry-cli reads `.sentryclirc`: `[defaults]` supports `url`, `org`, and `project`;
`[auth]` supports `token`. The file is ignored by Git. Keep your usual settings;
local uploads can override them per command.

Run a test target shown by `make` using the DSN from **Client Keys (DSN)**.
The target builds and uploads its debug files before capturing events:

```bash
make <target> SENTRY_DSN="<dsn>"
```

Use values from your target Sentry instance. Tokens need **Organization → Read** and
**Release → Admin**. When everything runs on the same machine, use the displayed
DSN unchanged and set `SENTRY_URL` to `http://dev.getsentry.net:8000/`.

When the client runs on another machine, replace `dev.getsentry.net` in the DSN
with the server's LAN address and change the port to Relay's port **7899**. Use
port **8001** for `SENTRY_URL`. Run `make ip` on the server to print its IPv4
address from the default network interface. A cross-machine invocation is:

```bash
make <target> \
    SENTRY_DSN="http://<key>@<server-lan-ip>:7899/<project-id>" \
    SENTRY_URL="http://<server-lan-ip>:8001/" \
    SENTRY_ORG="<local-org>" \
    SENTRY_PROJECT="<local-project>" \
    SENTRY_AUTH_TOKEN="<local-token>"
```

## Configuration

Copy `.env.example` to `.env` to keep Sentry credentials and other client
settings in a local file:

```sh
cp .env.example .env
```

The Makefile loads `.env` automatically and exports its supported variables to
child processes. `.env` is ignored by Git; `.env.example` contains placeholders
only and remains tracked. Use unquoted `KEY=value` entries. Values passed on the
`make` command line take precedence over `.env`. `app-hang` and `cpp-exception`
default `SENTRY_PROJECT` to their respective target names when it is unset.

Set `SENTRY_DIR`, `RELAY_DIR`, and `SENTRY_NATIVE_DIR` to existing checkouts in
`Makefile.local`; use forward slashes in Windows paths. Defaults are `../sentry`,
`../relay`, and `../sentry-native`. Make uses these checkouts as they are, so select
the branches you want to test there.

`SENTRY_CONF` and `RELAY_CONF` default to `.local/sentry` and `.local/relay` in
this repository. `SENTRY_HOST` sets the browser hostname and defaults to
`dev.getsentry.net`.

Set `SENTRY_DSN` in the environment or pass it to the commands above.
For sentry-cli, use `.sentryclirc` or the `SENTRY_URL`, `SENTRY_ORG`,
`SENTRY_PROJECT`, and `SENTRY_AUTH_TOKEN` environment variables.
