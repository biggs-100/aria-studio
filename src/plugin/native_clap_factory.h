#ifndef ARIA_PLUGIN_NATIVE_CLAP_FACTORY_H
#define ARIA_PLUGIN_NATIVE_CLAP_FACTORY_H

#include "plugin_factory.h"

namespace aria::plugin {

/// Register the built-in ARIA DSP plugins as CLAP-compatible entries
/// in the PluginFactory registry.
///
/// Registered plugins:
///   - aria.eq          (Effect — 3-band parametric EQ)
///   - aria.compressor  (Effect — dynamics processor)
///   - aria.reverb      (Effect — algorithmic reverb)
///   - aria.delay       (Effect — stereo delay)
///   - aria.synth       (Synth  — virtual-analog subtractive synth)
///
/// Each native plugin wraps DSP logic inside an AudioPlugin subclass
/// that exposes parameters and processes audio identically to an
/// external CLAP plugin — no internal fast path.
void register_native_plugins(PluginFactory& factory);

} // namespace aria::plugin

#endif // ARIA_PLUGIN_NATIVE_CLAP_FACTORY_H
