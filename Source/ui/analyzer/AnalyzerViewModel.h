#pragma once

#include <mdsp_ui/analyzer/AnalyzerRenderState.h>
#include <mdsp_ui/analyzer/AnalyzerController.h>
#include "../../dsp_adapters/AnalyzerSnapshotAdapter.h"

class AnalyzerViewModel
{
public:
    void setSnapshot(const AnalyzerSnapshot& snapshot);
    void setRenderState(const mdsp_ui::AnalyzerRenderState& rs);
    void tick(double dtSeconds);
    mdsp_ui::AnalyzerRenderState getRenderState() const;
    void applyInteraction(const mdsp_ui::InteractionUpdate& upd);

    void setViewMode(int mode);
    int getViewMode() const noexcept { return viewMode_; }
    void setDbRange(float topDb, float bottomDb);
    float getTopDb() const noexcept { return topDb_; }
    float getBottomDb() const noexcept { return bottomDb_; }

private:
    int viewMode_ = 0;
    float topDb_ = 6.0f;
    float bottomDb_ = -120.0f;
    mdsp_ui::AnalyzerRenderState renderState_;
};
