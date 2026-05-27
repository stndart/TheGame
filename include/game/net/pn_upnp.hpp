#pragma once

// Opaque stand-in for the game's UPnP client object (sub_D6E180 `this`).
class PNUpnpClient {
public:
  // sub_D6E180 — UPnP WANIPConnection AddPortMapping SOAP + HTTP POST (PN FSM state 1).
  void AddPortMapping();
};
