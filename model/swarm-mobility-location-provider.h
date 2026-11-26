// Copyright (c) 2026 Daniel Joppi (https://github.com/swarm-routing)
// SPDX-License-Identifier: GPL-2.0-only
//
// MobilityLocationProvider: a node's pose, from its ns3::MobilityModel, as
// swarm-core's ILocationProvider (swarm-routing/swarm-core#7, #232). New here;
// the port is experimental in swarm-core (outside its SemVer promise until a
// second protocol uses it), and so is this adapter.
//
// Read-only: attaching it changes nothing (no trace connected, no event
// scheduled, no random draw), so a run's output with and without it is
// byte-identical. pose() reads the model's GetPosition() and GetVelocity(),
// as ns-3's own tracing does; a model that updates lazily (ns-3's
// WaypointMobilityModel with LazyNotify) brings its state up to date on that
// read, as on any other.
//
// Position in metres and velocity in m/s, ns-3's Cartesian frame, both as the
// model reports them: ns-3 always knows a model's velocity (0 when it stands
// still), so the pose's velocity is never empty. A node without a mobility
// model has no pose (empty), the port's "no evidence".
#ifndef SWARM_MOBILITY_LOCATION_PROVIDER_H
#define SWARM_MOBILITY_LOCATION_PROVIDER_H

#include "ns3/mobility-model.h"
#include "ns3/object.h"
#include "ns3/swarm-location.h"

namespace ns3 {

class Node;

class MobilityLocationProvider : public Object, public swarm::core::ILocationProvider {
public:
    static TypeId GetTypeId();

    /// Reads `model` and aggregates itself to the model's node, when it has one.
    void Install(Ptr<MobilityModel> model);
    /// Install() on the node's mobility model. Returns false, and installs
    /// nothing, when the node has none.
    bool Install(Ptr<Node> node);

    /// The model's position and velocity now; empty without a model.
    std::optional<swarm::core::GeoPose> pose() const override;

protected:
    void DoDispose() override;

private:
    Ptr<MobilityModel> m_model;
};

}  // namespace ns3

#endif  // SWARM_MOBILITY_LOCATION_PROVIDER_H
