# Documentation map

**Stateless specs** live under `docs/` and should track the **current** implementation. **Stateful plans** live under `docs/plans/` — progress snapshots and open issues.

## ProudNet (transport + internal envelope)

| Doc | Contents |
| --- | --- |
| [proudnet/README.md](proudnet/README.md) | **Folder index** |
| [proudnet/overview.md](proudnet/overview.md) | Intent — components, legs, layer stack |
| [proudnet/protocol.md](proudnet/protocol.md) | Data — framing, transport opcodes, offline sequences |
| [proudnet/message-dispatch.md](proudnet/message-dispatch.md) | Data — `MessageType` ordinals, per-case handler RVAs |
| [proudnet/message-type-crossmap.md](proudnet/message-type-crossmap.md) | **GAME ↔ PN18** `MessageType` send-path crossmap |
| [proudnet/implementation.md](proudnet/implementation.md) | Implementation — `TheGame.dll` + server file map |
| [proudnet/stages.md](proudnet/stages.md) | ctl `game_stage` names ↔ hook RVAs |
| [proudnet-ida-reimpl.md](proudnet-ida-reimpl.md) | IDA ↔ reimpl — GAME.exe RVAs, connect-path index |
| [proudnet-sdk-crossmap.md](proudnet-sdk-crossmap.md) | GAME ↔ ProudNet v1.8 DLL (IDA **13338**) crossmap |

## RMI (game application layer)

| Doc | Contents |
| --- | --- |
| [rmi/README.md](rmi/README.md) | **Folder index** |
| [rmi/overview.md](rmi/overview.md) | Intent — numbering spaces, end-to-end flow |
| [rmi/client.md](rmi/client.md) | Client — C2S send, S2C receive, leaf convention, registrars |
| [rmi/actions.md](rmi/actions.md) | Verified action → REQ/RES map |
| [rmi/autonav.md](rmi/autonav.md) | **Autonav** — ctl handler pipe, shard nav |
| [rmi/stages.md](rmi/stages.md) | Pointer to proudnet stage table |
| [rmi/fake-server-hooks.md](rmi/fake-server-hooks.md) | **Archival** — removed in-process S2C inject harness |
| [rmi/server.md](rmi/server.md) | Friends server wire builders *(separate package)* |
| [rmi-ida-reimpl.md](rmi-ida-reimpl.md) | IDA ↔ reimpl — game RMI RVAs, handler tables |

## Plans & living docs (`docs/plans/`)

| Doc | Contents |
| --- | --- |
| [plans/proudnet-hook-status.md](plans/proudnet-hook-status.md) | **Current** hook install matrix, SEH rules, verify recipe |
| [plans/controversies-to-resolve.md](plans/controversies-to-resolve.md) | Closed doc vs code resolution log |
