#include "vlab/telemetry.hpp"
#include <cmath>

namespace vlab {

static const char* kindName(LinkKind k) {
    switch (k) {
        case LinkKind::Spring: return "Spring";
        case LinkKind::Damper: return "Damper";
        case LinkKind::Rod: return "Rod";
        default: return "Pin";
    }
}

void Telemetry::clear() {
    t.clear();
    for (auto& c : col) c.clear();
}

void Telemetry::begin(const Model& m) {
    channels.clear();
    col.clear();
    t.clear();
    auto add = [&](const std::string& group, const std::string& name, const char* unit) {
        channels.push_back({group + " " + name, unit, group});
    };
    for (const Body& b : m.bodies) {
        if (!b.isDynamic()) continue;
        add(b.name, "x", "m");
        add(b.name, "y", "m");
        add(b.name, "dx", "m");
        add(b.name, "dy", "m");
        add(b.name, "angle", "deg");
        add(b.name, "vx", "m/s");
        add(b.name, "vy", "m/s");
        add(b.name, "speed", "m/s");
        add(b.name, "omega", "rad/s");
        add(b.name, "KE", "J");
    }
    for (size_t i = 0; i < m.links.size(); ++i) {
        const Link& l = m.links[i];
        std::string g = std::string(kindName(l.kind)) + " " + std::to_string(i + 1) + " (" + m.bodies[l.a].name + " - " + m.bodies[l.b].name + ")";
        if (l.kind == LinkKind::Spring) {
            add(g, "length", "m");
            add(g, "stretch", "m");
            add(g, "force", "N");
        } else if (l.kind == LinkKind::Damper) {
            add(g, "length", "m");
            add(g, "rate", "m/s");
            add(g, "force", "N");
            add(g, "power", "W");
        } else if (l.kind == LinkKind::Rod) {
            add(g, "length", "m");
        }
    }
    add("Energy", "T kinetic", "J");
    add("Energy", "V potential", "J");
    add("Energy", "E total", "J");
    add("Energy", "work in", "J");
    add("Energy", "lost viscous", "J");
    add("Energy", "lost drag", "J");
    add("Energy", "lost friction", "J");
    add("Energy", "lost impact", "J");
    add("Energy", "lost total", "J");
    add("Energy", "numerical error", "J");
    add("Contacts", "count", "");
    col.assign(channels.size(), {});
}

void Telemetry::append(const World& w) {
    const Model& m = w.model();
    size_t c = 0;
    auto put = [&](double v) {
        if (c < col.size()) col[c].push_back(v);
        ++c;
    };
    t.push_back(std::round(w.time() * 1e9) / 1e9);                // tidy times: 60 s, not 60.0000000002 s
    for (size_t i = 0; i < m.bodies.size(); ++i) {
        const Body& b = m.bodies[i];
        if (!b.isDynamic()) continue;
        const BodyState& s = w.state()[i];
        put(s.pos.x);
        put(s.pos.y);
        put(s.pos.x - b.pos.x);
        put(s.pos.y - b.pos.y);
        put(s.angle * 180.0 / kPi);
        put(s.vel.x);
        put(s.vel.y);
        put(length(s.vel));
        put(s.omega);
        put(0.5 * b.massKg * lengthSq(s.vel) + 0.5 * b.inertia * s.omega * s.omega * (b.lockRot ? 0 : 1));
    }
    for (size_t i = 0; i < m.links.size(); ++i) {
        const Link& l = m.links[i];
        LinkReading r = w.link((int)i);
        if (l.kind == LinkKind::Spring) {
            put(r.length);
            put(r.stretch);
            put(r.force);
        } else if (l.kind == LinkKind::Damper) {
            put(r.length);
            put(r.rate);
            put(r.force);
            put(r.power);
        } else if (l.kind == LinkKind::Rod) {
            put(r.length);
        }
    }
    const Ledger& L = w.ledger();
    put(L.T);
    put(L.V);
    put(L.E);
    put(L.workIn);
    put(L.dViscous);
    put(L.dDrag);
    put(L.dFriction);
    put(L.dImpact);
    put(L.dissipated());
    put(L.numerical());
    put((double)w.contacts().size());
}

int Telemetry::find(const std::string& name) const {
    for (size_t i = 0; i < channels.size(); ++i)
        if (channels[i].name == name) return (int)i;
    return -1;
}

int Telemetry::firstOfGroup(const std::string& group) const {
    for (size_t i = 0; i < channels.size(); ++i)
        if (channels[i].group == group) return (int)i;
    return -1;
}

}  // namespace vlab
