/*
 * Copyright 2020 Xinyue Lu
 *
 * DualSynth bridge - plugin.
 *
 */

#pragma once

#include "version.hpp"
#include "minideen.hpp"

namespace Plugin {
  const char* Identifier = "in.7086.neo_minideen";
  const char* Namespace = "neo_minideen";
  const char* Description = "Neo Minideen Filter " PLUGIN_VERSION;
}

struct LegacyMiniDeen : MiniDeen {
  const char* AVSName() const override { return "minideen"; }
};

std::vector<register_vsfilter_proc> RegisterVSFilters()
{
  return std::vector<register_vsfilter_proc> { VSInterface::RegisterFilter<MiniDeen> };
}

std::vector<register_avsfilter_proc> RegisterAVSFilters()
{
  return std::vector<register_avsfilter_proc> {
    AVSInterface::RegisterFilter<MiniDeen>,
    AVSInterface::RegisterFilter<LegacyMiniDeen>
  };
}
