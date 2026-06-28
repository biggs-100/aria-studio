#include "note_property_source.h"

namespace aria::automation {

double NotePropertySource::evaluate(uint64_t /*ppqn*/,
                                    const ModulationContext& ctx) const
{
    switch (property_) {
    case Property::Velocity:
        return ctx.current_note_velocity;
    case Property::Pitch:
        return ctx.current_note_pitch;
    case Property::Pressure:
        // Pressure is not yet in ModulationContext; return 0 for now.
        // TODO: Add pressure to ModulationContext when MPE support is added.
        (void)ctx;
        return 0.0;
    case Property::Timbre:
        // Timbre is not yet in ModulationContext; return 0 for now.
        // TODO: Add MPE timbre to ModulationContext when MPE support is added.
        return 0.0;
    }
    return 0.0;
}

} // namespace aria::automation
