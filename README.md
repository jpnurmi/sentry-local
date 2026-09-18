# Local Sentry playground

Run `make` for prerequisites, configuration, and available commands.

- **Linux/macOS:** `make sentry`, then `make relay` in another terminal.
- **Hang test:** `make hang SENTRY_DSN="<dsn>"`.
- **C++ exception test:** `make cpp SENTRY_DSN="<dsn>"`.

The targets require existing checkouts and print setup instructions as needed.

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
`devenv sync` in its checkout with the same `SENTRY_CONF` used by Make.
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

## Hang test

Exercises the watchdog-as-crasher case in
[sentry#97529](https://github.com/getsentry/sentry/issues/97529): a main-thread
hang triggers a watchdog crash. An ordinary crash at
that same site provides a comparison with a responsive main thread.

On Windows, install Visual Studio's **Desktop development with C++** workload, CMake 3.18+,
Git, GNU Make, and [sentry-cli](https://docs.sentry.io/cli/installation/).
Use Git Bash with the Visual Studio x64 build environment and these tools on `PATH`.

sentry-cli reads `.sentryclirc`: `[defaults]` supports `url`, `org`, and `project`;
`[auth]` supports `token`. The file is ignored by Git. Keep your usual settings;
local uploads can override them per command.

Run both cases using the DSN from **Client Keys (DSN)**:

```bash
make hang SENTRY_DSN="<dsn>"
```

This builds, uploads debug files, and captures two events:

- `crash`: the watchdog crashes while the main thread keeps sending heartbeats.
- `wait-condition`: the main thread hangs in `wait_for_condition`.

Make continues after each deliberate crash. The watchdog calls `sentry_crash()`
from the same location in both runs. The console prints both thread IDs,
and the `test.case` tag identifies the case.

The static build uses the SDK's internal thread, synchronization, and thread-ID
helpers on Windows, Linux, and macOS.

The SDK's native backend captures and uploads **one crash event per run** in
minidump mode, so the server derives stacks from the dump. The dump identifies
the watchdog as the exception thread. Only the hang case installs `on_crash`:
it adds an `AppHang` exception whose `thread_id` references the main thread
and sets both threads' `crashed` flags to false. The callback
uses `level: error` and `handled: true`, matching the SDK's hang reports, and
supplies no stack trace. The ordinary crash keeps the SDK's event unchanged.

On an unmodified server, check whether both events group into **one issue
using the watchdog stack**. With the thread-selection fixes, expect **two
issues**: the ordinary crash uses the watchdog stack, and the hang uses the
main-thread stack. Inspect Event Grouping Information as well as the thread stacks.

Use values from your target Sentry instance. Tokens need **Organization → Read** and
**Release → Admin**. **For local Sentry**, replace `dev.getsentry.net:8000` in the
displayed DSN with the server's LAN address and port **7899**, keeping the key
and project ID. The resulting DSN is `http://<key>@<server-lan-ip>:7899/<project-id>`.
Run `make ip` on the server to print its IPv4 address from the default network interface.
Pass that DSN and override the upload URL, organization, project, and token for
this invocation:

```bash
make hang \
    SENTRY_DSN="http://<key>@<server-lan-ip>:7899/<project-id>" \
    SENTRY_URL="http://<server-lan-ip>:8001/" \
    SENTRY_ORG="<local-org>" \
    SENTRY_PROJECT="<local-project>" \
    SENTRY_AUTH_TOKEN="<local-token>"
```

## C++ exception test

Exercises server-side processing of the native event and minidump produced for
an uncaught C++ exception. The build enables the C++ integration with
`SENTRY_INTEGRATION_CPP`; the app throws a `std::runtime_error` and uses
`SENTRY_CRASH_REPORTING_MODE_NATIVE_WITH_MINIDUMP` so Relay and Sentry can be
developed against both payloads.

Run the case using the DSN from **Client Keys (DSN)**:

```bash
make cpp SENTRY_DSN="<dsn>"
```

The event has the `test.case=cpp-exception` tag. Before running it, set
`SENTRY_NATIVE_DIR` to a sentry-native checkout containing the C++ integration.
Use the local DSN and sentry-cli overrides described for the hang test when
testing local Relay and Sentry checkouts.

The native event contains an exception with:

- `type`: a platform-specific name ending in `runtime_error`
- `value`: `something went wrong`
- `mechanism.type`: `cpp_exception`
- `mechanism.handled`: `false`

Relay and Sentry should preserve this metadata while using the minidump for the
exception stack.

## Configuration

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
