#pragma once
// Factory presets of WDelay. The reference's 88 presets are carried over by their values with Wavy names in the release
// preparation (Phase 5); until then the list is empty.
#include "WavyPresets.h"

namespace wdelay
{
inline const std::vector<wavy::FactoryPreset>& factoryPresets()
{
    static const std::vector<wavy::FactoryPreset> list;
    return list;
}
}
