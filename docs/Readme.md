# Documentation map

**Stateless specs** live under `docs/` and should track the **current** implementation. **Stateful plans** live under `docs/plans/` - they capture progress at a point in time and are kept even when superseded.

## ProudNet (transport + internal envelope)

| Doc | Contents |
| --- | --- |
| [proudnet/overview.md](proudnet/overview.md) | Intent - components, legs, layer stack |
| [proudnet/protocol.md](proudnet/protocol.md) | Data - framing, transport opcodes, offline sequences |
| [proudnet/message-dispatch.md](proudnet/message-dispatch.md) | Data - `MessageType` ordinals, per-case handler RVAs |
| [proudnet/message-type-crossmap.md](proudnet/message-type-crossmap.md) | **GAME ↔ PN18** `MessageType` send-path crossmap (RmiSend-shaped) |
| [proudnet/implementation.md](proudnet/implementation.md) | Implementation - `TheGame.dll` + Python server file map |
| [proudnet-ida-reimpl.md](proudnet-ida-reimpl.md) | IDA ↔ reimpl - GAME.exe RVAs, hook modes, connect-path index |

## RMI (game application layer)

| Doc | Contents |
| --- | --- |
| [rmi/overview.md](rmi/overview.md) | Intent - numbering spaces, end-to-end flow |
| [rmi/client.md](rmi/client.md) | Client - C2S send, S2C receive, leaf convention, registrars |
| [rmi/autonav.md](rmi/autonav.md) | **Autonav** - ctl handler pipe (`nav_goto_lobby`), UI handler pattern |
| [rmi/fake-server-hooks.md](rmi/fake-server-hooks.md) | **Archival** - removed in-process S2C inject harness |
| [rmi/server.md](rmi/server.md) | Server - wire builders, triggers, opaque replay artifacts |
| [rmi-ida-reimpl.md](rmi-ida-reimpl.md) | IDA ↔ reimpl - game RMI RVAs, handler tables, hooks |

## Historical reference

| Doc | Contents |
| --- | --- |
| [proudnet-sdk-crossmap.md](proudnet-sdk-crossmap.md) | GAME.exe ↔ ProudNet v1.8 DLL symbol crossmap (IDA **13338**) |

## Plans & living docs (`docs/plans/`)

| Doc | Contents |
| --- | --- |
| [plans/proudnet-hook-status.md](plans/proudnet-hook-status.md) | **Current** hook install matrix, SEH rules, verify recipe |
| [plans/proudnet-rmi-server-plan.md](plans/proudnet-rmi-server-plan.md) | RMI server-emulation plan, S2C matrix, frontier |
| [plans/proudnet-game-rmi.md](plans/proudnet-game-rmi.md) | Living RMI investigation reference (runs, open questions) |
| [plans/handoff-next-session.md](plans/handoff-next-session.md) | Agent handoff seed |
