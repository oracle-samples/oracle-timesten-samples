# Modern TimesTen Demo Container Setup

This directory provides a containerized setup for running the modern Python,
Node.js, and Java samples with TimesTen. It uses an official TimesTen image from
Oracle Container Registry as the base, but does not include or redistribute that
image.

The image built here contains TimesTen, language runtimes, and database drivers.
The sample source is **not** copied into the image. When the container starts,
your repository checkout is bind-mounted read-only at `/workspace`, so local
edits are available immediately without rebuilding the image.

This setup is intended for hosts where Podman or Docker can already pull and
unpack container images correctly. It is not a prebuilt trial image. The scripts
build a local helper image from the official TimesTen image and then run the
modern samples inside that container.

## What this provides

| File | Purpose |
| :--- | :------ |
| [Containerfile](./Containerfile) | Adds Python, Node.js, and their database drivers to a local TimesTen base image. |
| [container.cfg](./container.cfg) | Holds the default engine, image, container, optional volume, and demo-user names. |
| [build](./build) | Builds the TimesTen base image and the derived demo image. |
| [crvolume](./crvolume) | Creates the optional named volume for persistent TimesTen data. |
| [ttstart](./ttstart) | Starts the TimesTen demo container and creates the demo account on first use. |
| [ttstop](./ttstop) | Stops the container without removing it. |
| [ttconnect](./ttconnect) | Opens a shell, or runs a command, with the TimesTen environment configured. |
| [rmcontainer](./rmcontainer) | Removes a stopped container; an optional volume is retained. |
| [rmvolume](./rmvolume) | Removes the volume after its container has been removed. |
| [run](./run) | Runs one modern Python, Node.js, or Java sample inside the container. |

The scripts use Podman by default. Set `CONTAINER_ENGINE=docker` to use Docker.
Docker may be simpler on developer machines where rootless Podman storage is
restricted by the host environment.

## Prerequisites

- Linux AMD64 host with Podman or Docker.
- An Oracle account that has accepted the TimesTen image license agreement in
  [Oracle Container Registry](https://container-registry.oracle.com/).
- An Oracle Container Registry authentication token. In Oracle Container
  Registry, select your profile name, choose **Auth Token**, and generate a
  secret key. This token is shown only once and is required instead of the
  Oracle account password for registry login. For details, see [Generating an
  Oracle Container Registry authentication token](https://docs.oracle.com/en/operating-systems/oracle-linux/podman/registries.html#registry_ocr_token).

The default image is pinned to TimesTen 26.1.1.3.0 with JDK 25 on Oracle Linux
9:

```text
container-registry.oracle.com/timesten/timesten:26.1.1.3.0-java25-oraclelinux9
```

It includes the matching `ttjdbc25.jar`, so the Java samples run with the same
JDK 25 setup used by their standalone instructions.

### Podman host storage note

Rootless Podman stores image layers under the user's container storage location.
If that location is on AFS, NFS, or another distributed file system, image pulls
or builds can fail while unpacking the TimesTen image. A common symptom is an
error similar to:

```text
lsetxattr /afs: operation not supported
```

This is a host container-storage issue rather than a TimesTen sample issue. Use
Docker, or configure Podman to use local storage before building the demo image.
Some rootless Podman setups also require `/etc/subuid` and `/etc/subgid` ranges
for the user.

## Build and start

From this directory, log in to Oracle Container Registry with an authentication
token. The default engine is Podman; export `CONTAINER_ENGINE=docker` first if
you use Docker.

```bash
read -r -p 'Oracle Container Registry username: ' OCR_USERNAME
read -r -s -p 'Oracle Container Registry authentication token: ' OCR_TOKEN
printf '\n'
printf '%s' "$OCR_TOKEN" | "${CONTAINER_ENGINE:-podman}" login \
  --username "$OCR_USERNAME" --password-stdin container-registry.oracle.com
unset OCR_TOKEN
```

Generate the token from the OCR profile **Auth Token** menu. For details, see
[Generating an Oracle Container Registry authentication token](https://docs.oracle.com/en/operating-systems/oracle-linux/podman/registries.html#registry_ocr_token).

Then build the local images:

```bash
./build
```

To use a different TimesTen image, choose one that includes JDK 25 and the
matching `ttjdbc25.jar`, then set `TIMESTEN_IMAGE` before building:

```bash
export TIMESTEN_IMAGE=container-registry.oracle.com/timesten/timesten:26.1.1.3.0-java25-oraclelinux9
./build
```

Before the first start, set a password without placing it in shell history,
then start TimesTen:

```bash
read -r -s -p 'TimesTen demo password: ' TT_PASSWORD
export TT_PASSWORD
printf '\n'
./ttstart
```

`ttstart` creates the `demo` user by default and uses the value of `TT_PASSWORD`
only for that provisioning operation. The password is not written to the image,
repository, or persistent volume. Set `DEMO_USER` before `ttstart` to use a
different unquoted TimesTen username.

By default, the container does not use a named volume. Stopping and restarting
the same container keeps its data; removing the container resets the TimesTen
instance and database. The source checkout is mounted read-only at `/workspace`.

### Optional persistent data

If you want the TimesTen instance and database to survive container removal,
create a named volume and set `PERSIST_DATA=true` before the first start:

```bash
./crvolume
export PERSIST_DATA=true
./ttstart
```

The volume name defaults to `tt-modern-demos-data` and can be changed with
`DEMO_VOLUME`. To change between the default and persistent modes, remove the
existing container first with `./ttstop` and `./rmcontainer`.

## Run a sample

Keep `TT_PASSWORD` exported, then use the `run` wrapper. The samples execute
inside the TimesTen container and use a direct connection to `sampledb`.

```bash
./run python aiResponseCache.py
./run nodejs agentWorkflowState.js
./run java AgentWorkflowState
```

The wrapper defaults to the `demo` user. Set `DEMO_USER` to use the account
created by `ttstart` with a different name, or add a sample `-u` option to
override it for one run.

Supported modern samples are:

| Language | Samples |
| :------- | :------ |
| Python | `aiResponseCache.py`, `chatSessionMemory.py`, `agentWorkflowState.py`, `featureStore.py`, `paymentAuthorizationState.py`, `telecomCallRoutingState.py` |
| Node.js | `aiResponseCache.js`, `chatSessionMemory.js`, `agentWorkflowState.js`, `featureStore.js`, `paymentAuthorizationState.js`, `telecomCallRoutingState.js` |
| Java | `AiResponseCache`, `ChatSessionMemory`, `AgentWorkflowState`, `FeatureStore`, `PaymentAuthorizationState`, `TelecomCallRoutingState` |

Java source is compiled into `/tmp` inside the container for each run. The
read-only checkout is never modified.

## Inspect, stop, and clean up

Open an interactive shell:

```bash
./ttconnect
```

Stop the container without removing it:

```bash
./ttstop
```

To remove the stopped container and reset the default, non-volume-backed data:

```bash
./rmcontainer
```

If you enabled persistent data, remove the container and volume, including the
TimesTen database and provisioning marker, with these commands:

```bash
./ttstop
./rmcontainer
./rmvolume
```

The next `./ttstart` after removing the container or volume requires
`TT_PASSWORD` again so it can create the demo user.

## Docker

The same commands work with Docker when the engine is selected explicitly:

```bash
export CONTAINER_ENGINE=docker
./build
./ttstart
./run python aiResponseCache.py
```
