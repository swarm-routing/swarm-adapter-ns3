// Copyright (c) 2026 Daniel Joppi (https://github.com/swarm-routing)
// SPDX-License-Identifier: GPL-2.0-only
#include "swarm-lifecycle-helper.h"

#include "ns3/abort.h"
#include "ns3/ipv4.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/txop.h"
#include "ns3/wifi-mac-queue.h"
#include "ns3/wifi-mac.h"
#include "ns3/wifi-net-device.h"
#include "ns3/wifi-phy.h"

#include <cmath>
#include <fstream>
#include <sstream>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("SwarmLifecycleHelper");

namespace {

Ptr<SwarmRoutingBase> SwarmProtocolOf(Ptr<Node> node) {
    if (Ptr<SwarmRoutingBase> p = node->GetObject<SwarmRoutingBase>()) {
        return p;  // aggregated, as SwarmRoutingHelper does
    }
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    return ipv4 ? DynamicCast<SwarmRoutingBase>(ipv4->GetRoutingProtocol()) : nullptr;
}

/// Interface 0 is the loopback; every other one is routed.
void SetInterfaces(Ptr<Node> node, bool up) {
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    if (!ipv4) {
        return;
    }
    for (uint32_t i = 1; i < ipv4->GetNInterfaces(); ++i) {
        if (up && !ipv4->IsUp(i)) {
            ipv4->SetUp(i);
        } else if (!up && ipv4->IsUp(i)) {
            ipv4->SetDown(i);
        }
    }
}

/// How long a shutdown's radio stays on for the leaving control (#288).
Time ShutdownRadioGrace() {
    return MilliSeconds(100);
}

/// Every Wi-Fi radio of the node off or back on (#288). Off, a crashed node
/// neither receives nor acknowledges, so its neighbours see a lost link (the
/// retry limit), as a power failure would show; a device that is not a
/// WifiNetDevice keeps its radio. `flush` also empties the MAC's queues: what
/// a crashed node had queued is lost with it.
void SetRadios(Ptr<Node> node, bool on, bool flush) {
    for (uint32_t d = 0; d < node->GetNDevices(); ++d) {
        Ptr<WifiNetDevice> dev = DynamicCast<WifiNetDevice>(node->GetDevice(d));
        if (!dev) {
            continue;
        }
        for (const Ptr<WifiPhy>& phy : dev->GetPhys()) {
            if (on) {
                phy->ResumeFromOff();  // a no-op unless the radio is off
            } else if (!phy->IsStateOff()) {
                phy->SetOffMode();
            }
        }
        Ptr<WifiMac> mac = dev->GetMac();
        if (!on && flush && mac) {
            if (mac->GetQosSupported()) {
                for (AcIndex ac : {AC_BE, AC_BK, AC_VI, AC_VO}) {
                    mac->GetTxopQueue(ac)->Flush();
                }
            } else if (Ptr<Txop> txop = mac->GetTxop()) {
                txop->GetWifiMacQueue()->Flush();
            }
        }
    }
}

/// A shutdown's radio goes off after the grace, unless the node started again
/// meanwhile (its interfaces are up).
void RadiosOffAfterShutdown(Ptr<Node> node) {
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    if (ipv4 && ipv4->GetNInterfaces() > 1 && ipv4->IsUp(1)) {
        return;
    }
    SetRadios(node, false, false);
}

}  // namespace

bool SwarmLifecycleHelper::ParseOp(const std::string& name, SwarmLifecycleOp& op) {
    if (name == "start") {
        op = SwarmLifecycleOp::Start;
    } else if (name == "shutdown") {
        op = SwarmLifecycleOp::Shutdown;
    } else if (name == "crash") {
        op = SwarmLifecycleOp::Crash;
    } else {
        return false;
    }
    return true;
}

void SwarmLifecycleHelper::Apply(Ptr<Node> node, SwarmLifecycleOp op) {
    NS_LOG_FUNCTION(node->GetId() << static_cast<int>(op));
    Ptr<SwarmRoutingBase> protocol = SwarmProtocolOf(node);
    switch (op) {
    case SwarmLifecycleOp::Shutdown:
        if (protocol) {
            protocol->Shutdown();  // the leaving control goes out before the interfaces go
        }
        SetInterfaces(node, false);
        // The leaving control is queued in the MAC: the radio stays on to send it.
        Simulator::Schedule(ShutdownRadioGrace(), &RadiosOffAfterShutdown, node);
        break;
    case SwarmLifecycleOp::Crash:
        if (protocol) {
            protocol->Crash();
        }
        SetInterfaces(node, false);
        SetRadios(node, false, true);
        break;
    case SwarmLifecycleOp::Start:
        SetRadios(node, true, false);
        SetInterfaces(node, true);  // then: the protocol's sockets come back with them
        if (protocol) {
            protocol->Start();
        }
        break;
    }
}

void SwarmLifecycleHelper::Schedule(Time at, Ptr<Node> node, SwarmLifecycleOp op) const {
    NS_ABORT_MSG_IF(at < Simulator::Now(), "a lifecycle operation in the past");
    Simulator::Schedule(at - Simulator::Now(), &SwarmLifecycleHelper::Apply, node, op);
}

void SwarmLifecycleHelper::Schedule(Time at, const NodeContainer& nodes,
                                    SwarmLifecycleOp op) const {
    for (auto it = nodes.Begin(); it != nodes.End(); ++it) {
        Schedule(at, *it, op);
    }
}

uint32_t SwarmLifecycleHelper::ScheduleFile(const std::string& path,
                                            const NodeContainer& nodes) const {
    std::ifstream in(path);
    NS_ABORT_MSG_UNLESS(in, "cannot read the lifecycle schedule " << path);
    std::string line;
    uint32_t lineNo = 0;
    uint32_t scheduled = 0;
    while (std::getline(in, line)) {
        ++lineNo;
        const auto hash = line.find('#');
        if (hash != std::string::npos) {
            line.erase(hash);
        }
        if (line.find_first_not_of(" \t\r") == std::string::npos) {
            continue;  // blank or comment-only
        }
        std::istringstream fields(line);
        double seconds = 0.0;
        uint32_t index = 0;
        std::string name;
        std::string rest;
        SwarmLifecycleOp op = SwarmLifecycleOp::Start;
        NS_ABORT_MSG_UNLESS((fields >> seconds >> index >> name) && !(fields >> rest) &&
                                ParseOp(name, op),
                            path << ":" << lineNo << ": expected `<time s> <node> <op>`");
        NS_ABORT_MSG_UNLESS(std::isfinite(seconds) && seconds >= 0.0,
                            path << ":" << lineNo << ": time must be finite and not negative");
        NS_ABORT_MSG_UNLESS(index < nodes.GetN(),
                            path << ":" << lineNo << ": node " << index << " not in the container");
        NS_ABORT_MSG_UNLESS(scheduled < kMaxEntries,
                            path << ": more than " << kMaxEntries << " operations");
        Schedule(Seconds(seconds), nodes.Get(index), op);
        ++scheduled;
    }
    return scheduled;
}

}  // namespace ns3
