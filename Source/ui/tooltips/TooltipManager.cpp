#include "TooltipManager.h"

namespace mdsp_ui
{

TooltipManager::TooltipManager (juce::Component& editor, UiContext& ui)
    : editor_ (editor)
{
    overlay_ = std::make_unique<TooltipOverlayComponent> (ui);
    editor_.addChildComponent (overlay_.get());
    overlay_->setVisible (false);
}

TooltipManager::~TooltipManager()
{
    stopTimer();
    // Clean up listeners
    for (auto& pair : registry_)
    {
        if (pair.first != nullptr)
            pair.first->removeMouseListener (this);
    }
}

void TooltipManager::registerTooltip (juce::Component* component, TooltipSpec spec)
{
    if (component == nullptr) return;
    
    unregisterTooltip (component); // Remove old if exists
    
    registry_[component] = spec;
    component->addMouseListener (this, true); // Recursive listener for complex components? Or just top level?
    // "true" means we might hear children, but we map by "component" pointer.
    // If we want the component ITSELF to trigger, we need correct logic in mouseEnter.
}

void TooltipManager::unregisterTooltip (juce::Component* component)
{
    if (registry_.find (component) != registry_.end())
    {
        component->removeMouseListener (this);
        registry_.erase (component);
        if (currentTarget_ == component)
        {
            hideTooltip();
        }
    }
}

void TooltipManager::mouseEnter (const juce::MouseEvent& e)
{
    juce::Component* target = e.eventComponent;
    
    // Check if valid registry target
    if (registry_.find (target) != registry_.end())
    {
        if (currentTarget_ != target)
        {
            currentTarget_ = target;
            isVisible_ = false;
            overlay_->setVisible (false);
            startTimer (250); // Initial delay
        }
    }
}

void TooltipManager::mouseExit (const juce::MouseEvent& e)
{
    // If leaving target
    if (e.eventComponent == currentTarget_)
    {
        // Check if moved to child (if target is container)?
        // For now, strict exit.
        if (!e.eventComponent->contains (e.getPosition()))
        {
            hideTooltip();
        }
    }
}

void TooltipManager::mouseMove (const juce::MouseEvent&)
{
    // Optional: reset timer if we want movement to delay appearance?
    // Runbook: "Hide after: mouse exit (immediate) OR small grace".
    // If already visible, maybe follow mouse?
    // But fixed position "Anchor near hovered component".
    // So mouseMove doesn't affect much unless we exit.
}

void TooltipManager::mouseDown (const juce::MouseEvent&)
{
    hideTooltip(); // Hide on click
}

void TooltipManager::hideTooltip()
{
    stopTimer();
    currentTarget_ = nullptr;
    isVisible_ = false;
    if (overlay_)
        overlay_->setVisible (false);
}

void TooltipManager::showTooltip()
{
    if (currentTarget_ && registry_.count (currentTarget_))
    {
        const auto& spec = registry_[currentTarget_];
        overlay_->updateSpec (spec);
        
        // Position
        juce::Rectangle<int> anchor;
        if (spec.anchorRectProvider)
            anchor = spec.anchorRectProvider();
        else
            anchor = currentTarget_->getScreenBounds(); // Default to screen match?
            
        // Convert to local if provider returned global?
        // Let's assume provider returns EDITOR-relative coords (as per TooltipData.h comments "shared coordinates (usually Editor-relative)").
        // But getScreenBounds is global.
        
        // If provider missing, use target bounds relative to editor.
        if (!spec.anchorRectProvider)
        {
            anchor = editor_.getLocalArea (currentTarget_, currentTarget_->getLocalBounds());
        }
        
        // Position tooltip centered below anchor
        const int gap = 8;
        int x = anchor.getCentreX() - (overlay_->getWidth() / 2);
        int y = anchor.getBottom() + gap;
        
        // Clamp
        auto bounds = editor_.getLocalBounds();
        x = juce::jlimit (bounds.getX() + 4, bounds.getRight() - overlay_->getWidth() - 4, x);
        
        // If flows off bottom, flip to top
        if (y + overlay_->getHeight() > bounds.getBottom())
        {
            y = anchor.getY() - overlay_->getHeight() - gap;
        }
        
        overlay_->setTopLeftPosition (x, y);
        overlay_->setVisible (true);
        overlay_->toFront (false);
        isVisible_ = true;
        
        startTimer (100); // Switch to 10Hz update
    }
}

void TooltipManager::timerCallback()
{
    if (!isVisible_)
    {
        // Delay finished, show!
        showTooltip();
    }
    else
    {
        // Update value
        if (overlay_)
            overlay_->updateValue();
            
        // Validate target (still valid?)
        // If mouse left without event?
        // Using "Desktop::getMousePosition" check?
        // Simple check: if component deleted? (registry_ safety)
    }
}

} // namespace mdsp_ui
