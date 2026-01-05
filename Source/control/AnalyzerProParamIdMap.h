#pragma once

#include "ControlBinder.h"
#include "ControlIds.h"

namespace ap
{
namespace control
{

using ParamIdMap = AnalyzerPro::ParamIdMap;
using ControlId = AnalyzerPro::ControlId;

/**
    Factory function that returns a ParamIdMap for AnalyzerPro.
    Maps CONTROL_IDS to APVTS parameter ID strings.
    Returns empty string for unmapped ControlIds.
*/
ParamIdMap makeDefaultParamIdMap();

} // namespace control
} // namespace ap

// Make function accessible from AnalyzerPro::control namespace
namespace AnalyzerPro
{
namespace control
{
using ap::control::makeDefaultParamIdMap;
} // namespace control
} // namespace AnalyzerPro
