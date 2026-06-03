# ProudNet documentation

Transport framing, connection FSM, internal `MessageType` dispatch, and `TheGame.dll` reimpl for the offline path.

| Doc | Role |
| --- | --- |
| [overview.md](overview.md) | Intent — legs, layer stack, components |
| [protocol.md](protocol.md) | Wire — framing, opcodes, offline sequences |
| [message-dispatch.md](message-dispatch.md) | `ProcessMessage_ProudNetLayer` case → handler RVAs |
| [message-type-crossmap.md](message-type-crossmap.md) | GAME ↔ PN18 send-path ordinal crossmap |
| [implementation.md](implementation.md) | Source tree map + verify recipe |
| [stages.md](stages.md) | ctl `game_stage` names ↔ hook RVAs |
| [../plans/proudnet-hook-status.md](../plans/proudnet-hook-status.md) | **Current** hook install matrix |
| [../proudnet-ida-reimpl.md](../proudnet-ida-reimpl.md) | GAME.exe RVA index |
| [../proudnet-sdk-crossmap.md](../proudnet-sdk-crossmap.md) | Historical GAME ↔ PN18 crossmap |

**Redirects (old paths):** [proudnet-offline-protocol.md](proudnet-offline-protocol.md) → protocol; [proudnet-message-dispatch-map.md](proudnet-message-dispatch-map.md) → message-dispatch.

**Game RMI** (application layer): [../rmi/README.md](../rmi/README.md).

*Last updated: 2026-06-03.*
