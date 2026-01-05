#include "AnalyzerProControlContext.h"

namespace AnalyzerPro
{
namespace control
{

AnalyzerProControlContext::AnalyzerProControlContext(juce::AudioProcessorValueTreeState* apvts)
    : binder(apvts, makeDefaultParamIdMap())
{
}

} // namespace control
} // namespace AnalyzerPro
