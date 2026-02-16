#include "AnalyzerViewModel.h"

void AnalyzerViewModel::setSnapshot(const AnalyzerSnapshot& snapshot)
{
    (void)snapshot;
}

void AnalyzerViewModel::setRenderState(const mdsp_ui::AnalyzerRenderState& rs)
{
    renderState_ = rs;
}

void AnalyzerViewModel::tick(double dtSeconds)
{
    (void)dtSeconds;
}

mdsp_ui::AnalyzerRenderState AnalyzerViewModel::getRenderState() const
{
    return renderState_;
}

void AnalyzerViewModel::applyInteraction(const mdsp_ui::InteractionUpdate& upd)
{
    (void)upd;
}

void AnalyzerViewModel::setViewMode(int mode)
{
    viewMode_ = mode;
    renderState_.viewMode = mode;
}

void AnalyzerViewModel::setDbRange(float topDb, float bottomDb)
{
    if (topDb < 1.0f)
        topDb = 6.0f;
    topDb_ = topDb;
    bottomDb_ = bottomDb;
    renderState_.topDb = topDb;
    renderState_.bottomDb = bottomDb;
}
