#pragma once
// Factory presets of WBassX (to be generated from the reference's presets by tools/make_presets.py in Phase 4).
#include "WavyPresets.h"

namespace wbassx
{
inline const std::vector<wavy::FactoryPreset>& factoryPresets()
{
    static const std::vector<wavy::FactoryPreset> list {};
    return list;
}
}
