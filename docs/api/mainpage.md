# swarm-adapter-ns3 API

The ns-3 contrib module `swarm-core`: the ns-3 adapter of
[swarm-routing/swarm-core](https://github.com/swarm-routing/swarm-core)'s
ports, which every swarm-routing protocol's ns-3 module links
(`LIBRARIES_TO_LINK ${libswarm-core}`, `#include "ns3/swarm-core-module.h"`).
This reference covers the module's public headers, `model/` and `helper/`;
the simulator-free primitives (`swarm::core`) are documented with
swarm-core.

## Where to start

- **A protocol's routing class** derives from ns3::SwarmRoutingBase, the
  `Ipv4RoutingProtocol` that owns the sockets, the ports, the pending-packet
  queue (ns3::DeferredPacketQueue), MAC-failure handling and the router
  observer's trace sources.
- **The core's ports on ns-3**: ns3::SwarmClock, ns3::SwarmRng and
  ns3::SwarmTimerScheduler; the link monitors ns3::WifiLinkMonitor and
  ns3::IslLinkMonitor; the energy port ns3::EnergyMetricHook.
- **Installing a protocol**: ns3::SwarmRoutingHelper (with its monitors, and
  each node's counters through `CollectStats`, ns3::SwarmRoutingStats) and
  ns3::SwarmLifecycleHelper (node shutdown, crash and restart).

## More

- Install it: [docs/install.md](https://github.com/swarm-routing/swarm-adapter-ns3/blob/main/docs/install.md)
- What counts as the public API, and what breaks it: [docs/api-policy.md](https://github.com/swarm-routing/swarm-adapter-ns3/blob/main/docs/api-policy.md)
- Licence: GPL-2.0-only ([LICENSING.md](https://github.com/swarm-routing/swarm-adapter-ns3/blob/main/LICENSING.md))
