#ifndef ARIA_AUTOMATION_NOTE_PROPERTY_SOURCE_H
#define ARIA_AUTOMATION_NOTE_PROPERTY_SOURCE_H

#include "modulation_types.h"
#include "modulation_source.h"

#include <cstdint>

namespace aria::automation {

/// Reads current note properties (velocity, pitch, pressure, timbre)
/// from `ModulationContext` and outputs them as modulation values.
///
/// This source is stateless — it always reads from the context provided
/// at evaluate time. The engine is responsible for populating
/// `ModulationContext` with the latest note data.
class NotePropertySource : public ModulationSource {
public:
    enum class Property {
        Velocity,    ///< Note-on velocity (0.0–1.0)
        Pitch,       ///< Note pitch normalised (0.0–1.0)
        Pressure,    ///< Channel / poly pressure (0.0–1.0)
        Timbre       ///< MPE timbre (0.0–1.0)
    };

    void set_property(Property prop) { property_ = prop; }
    Property property() const { return property_; }

    // ─── Overrides ───────────────────────────────────────────
    double evaluate(uint64_t ppqn,
                    const ModulationContext& ctx) const override;

private:
    Property property_ = Property::Velocity;
};

} // namespace aria::automation

#endif // ARIA_AUTOMATION_NOTE_PROPERTY_SOURCE_H
