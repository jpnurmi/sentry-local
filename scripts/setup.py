import json
import os
import sys
from pathlib import Path

import yaml
from sentry_relay.auth import generate_key_pair, generate_relay_id


os.umask(0o077)
sentry = Path(os.environ["SENTRY_CONF"]).expanduser().resolve()
relay = Path(os.environ["RELAY_CONF"]).expanduser().resolve()
relay.mkdir(parents=True, exist_ok=True)

config = relay / "config.yml"
if not config.exists():
    config.write_text(
        """relay:
  upstream: http://127.0.0.1:8001/
  host: 0.0.0.0
  port: 7899
processing:
  enabled: true
  kafka_config:
    - name: bootstrap.servers
      value: 127.0.0.1:9092
    - name: message.max.bytes
      value: "50000000"
  redis: redis://127.0.0.1:6379
  objectstore:
    objectstore_url: http://127.0.0.1:8888/
"""
    )

credentials = relay / "credentials.json"
if not credentials.exists():
    secret_key, public_key = generate_key_pair()
    credentials.write_text(
        json.dumps(
            {
                "secret_key": str(secret_key),
                "public_key": str(public_key),
                "id": str(generate_relay_id()),
            }
        )
    )
auth = json.loads(credentials.read_text())

settings = sentry / "sentry.conf.py"
extra = (
    "\nSENTRY_USE_RELAY = True\n"
    f"SENTRY_RELAY_STATIC_AUTH[{auth['id']!r}] = "
    f"{{'public_key': {auth['public_key']!r}, 'internal': True}}\n"
)
if extra not in settings.read_text():
    with settings.open("a") as file:
        file.write(extra)

config = sentry / "config.yml"
options = yaml.safe_load(config.read_text())
host = "host.lima.internal" if sys.platform == "darwin" else "host.docker.internal"
options.update(
    {
        "symbolicator.enabled": True,
        "symbolicator.options": {"url": "http://127.0.0.1:3021"},
        "system.internal-url-prefix": f"http://{host}:8001",
        "filestore.backend": "filesystem",
        "filestore.options": {"location": str(sentry / "files")},
    }
)
config.write_text(yaml.safe_dump(options, sort_keys=False))

print(rf"""
Local Sentry: http://{os.environ['SENTRY_HOST']}:8000
Login: admin@sentry.io / admin

1. Once the UI loads, sign in and create a Native project.
   Copy its Client Keys (DSN). Create an auth token with
   Organization: Read and Release: Admin for debug-file uploads.

2. Run make relay in another terminal. Wait for Relay to be ready.

3. Choose a test with make on the client machine.
   Example: the app hang test:

   make app-hang \
       SENTRY_DSN="http://<key>@<server-lan-ip>:7899/<project-id>" \
       SENTRY_URL="http://<server-lan-ip>:8001/" \
       SENTRY_ORG="<local-org>" \
       SENTRY_PROJECT="<local-project>" \
       SENTRY_AUTH_TOKEN="<local-token>"

   Replace `app-hang` with `cpp-exception` for the C++ exception test.

Run make ip on this server for <server-lan-ip>.
Keep the key and project ID from the DSN.
The org/project slugs and token must come from this local Sentry instance.
The client needs access to ports 7899 (captures) and 8001 (debug-file uploads).
For browser access, also allow port 8000 and map {os.environ['SENTRY_HOST']}
to the server's LAN address in the client's hosts file.

Ctrl+C in make sentry stops its workers and dependencies too.
Stop make relay with Ctrl+C in its terminal.
""")
