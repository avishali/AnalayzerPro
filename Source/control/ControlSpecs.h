#pragma once

#include "ControlIds.h"
#include <cstdint>
#include <string_view>

namespace AnalyzerPro
{

struct Spec
{
    ControlId id{};
    Stage stage{};
    Type type{};
    std::string_view name{};
    std::string_view stableKey{};
};

const Spec& getSpec(ControlId id) noexcept;
std::string_view toStableKey(ControlId id) noexcept;

} // namespace AnalyzerPro
