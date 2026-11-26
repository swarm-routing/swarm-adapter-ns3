// Copyright (c) 2026 Daniel Joppi (https://github.com/swarm-routing)
// SPDX-License-Identifier: GPL-2.0-only
#include "swarm-mobility-location-provider.h"

#include "ns3/log.h"
#include "ns3/node.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("MobilityLocationProvider");
NS_OBJECT_ENSURE_REGISTERED(MobilityLocationProvider);

TypeId MobilityLocationProvider::GetTypeId() {
    static TypeId tid = TypeId("ns3::MobilityLocationProvider")
                            .SetParent<Object>()
                            .SetGroupName("SwarmCore")
                            .AddConstructor<MobilityLocationProvider>();
    return tid;
}

void MobilityLocationProvider::DoDispose() {
    m_model = nullptr;
    Object::DoDispose();
}

void MobilityLocationProvider::Install(Ptr<MobilityModel> model) {
    NS_ASSERT_MSG(!m_model, "MobilityLocationProvider::Install called twice");
    NS_ASSERT(model);
    m_model = model;
    if (Ptr<Node> node = model->GetObject<Node>()) {
        node->AggregateObject(this);
    }
}

bool MobilityLocationProvider::Install(Ptr<Node> node) {
    Ptr<MobilityModel> model = node->GetObject<MobilityModel>();
    if (!model) {
        return false;
    }
    Install(model);
    return true;
}

std::optional<swarm::core::GeoPose> MobilityLocationProvider::pose() const {
    if (!m_model) {
        return std::nullopt;
    }
    const Vector p = m_model->GetPosition();
    const Vector v = m_model->GetVelocity();
    return swarm::core::GeoPose{{p.x, p.y, p.z}, swarm::core::Vec3{v.x, v.y, v.z}};
}

}  // namespace ns3
