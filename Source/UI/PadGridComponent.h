#pragma once

#include "../Profiles/ControllerProfile.h"
#include "Theme.h"
#include <JuceHeader.h>
#include <functional>
#include <unordered_map>

class PadGridComponent : public juce::Component
{
public:
    PadGridComponent();

    void setProfile (const svc::ControllerProfile& profile, bool resetSelection = false);
    void updatePad (int index, const svc::ProfilePad& pad);
    void setSelectedPadIndex (int index);
    void scrollPadIntoView (int index);
    int getDisplayGridColumns() const noexcept { return displayGridColumns; }
    void setDisplayGridColumns (int cols) noexcept { displayGridColumns = juce::jmax (1, cols); updateCanvasSize(); }
    juce::Rectangle<int> padBoundsForIndex (int index) const;
    int getSelectedPadIndex() const noexcept { return selectedPadIndex; }
    int getPadCanvasWidth() const noexcept { return padCanvas.getWidth(); }
    int getViewportClientWidth() const noexcept { return viewport.getWidth(); }
    bool needsHorizontalScroll() const noexcept { return getPadCanvasWidth() > getViewportClientWidth(); }
    int getPadCanvasHeight() const noexcept { return padCanvas.getHeight(); }
    int getViewportClientHeight() const noexcept { return viewport.getHeight(); }
    bool needsVerticalScroll() const noexcept { return getPadCanvasHeight() > getViewportClientHeight(); }
    bool isVerticalScrollbarShown() const noexcept { return viewport.isVerticalScrollBarShown(); }
    juce::Point<int> getViewportViewPosition() const noexcept { return viewport.getViewPosition(); }
    void setViewportViewPosition (int x, int y) { viewport.setViewPosition (x, y); }

    void flashPadHit (int note, int channel, float outputVelocity);
    void decayHitVisuals();
    bool hasActiveHitVisuals() const noexcept;
    void refreshVisualCache();

    void setCanPasteCurve (bool canPaste) noexcept { canPasteCurve = canPaste; }
    bool getCanPasteCurve() const noexcept { return canPasteCurve; }

    void startInlineEditing (int padIndex);
    void commitInlineEditing();
    void cancelInlineEditing();
    juce::Rectangle<int> ghostPadBounds() const;
    std::pair<int, int> cellAt (juce::Point<int> pos) const;
    int padIndexAtCell (int displayRow, int displayCol) const;
    std::pair<int, int> displayToGridCell (int displayRow, int displayCol) const;
    std::pair<int, int> gridToDisplayCell (int gridRow, int gridCol) const;

    std::function<void (int padIndex)> onPadSelected;
    std::function<void()> onAddPadRequested;
    std::function<void()> onDeletePadRequested;
    std::function<void (int fromIndex, int toIndex)> onPadSwapRequested;
    std::function<void (int index, int targetRow, int targetCol)> onPadMoveRequested;
    std::function<void (int sourceIndex, std::optional<std::pair<int, int>> targetCell)> onPadDuplicateRequested;
    std::function<void (int padIndex, const juce::String& newName)> onPadRenamed;
    std::function<void (int padIndex)> onLearnMidiRequested;
    std::function<void (int padIndex)> onCopyCurveRequested;
    std::function<void (int padIndex)> onPasteCurveRequested;
    std::function<void (int padIndex)> onResetCurveRequested;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    enum class DragMode { none, potentialDrag, dragging };
    struct DragState
    {
        DragMode mode = DragMode::none;
        int sourceIndex = -1;
        juce::Point<int> startPos;
        juce::Point<int> currentPos;
        int hoverTargetIndex = -1;
        std::pair<int, int> hoverTargetCell { -1, -1 };
        bool isAltDuplicate = false;
    };

    class PadCanvas : public juce::Component, private juce::Timer
    {
    public:
        explicit PadCanvas (PadGridComponent& owner);
        ~PadCanvas() override;
        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent& event) override;
        void mouseDrag (const juce::MouseEvent& event) override;
        void mouseUp (const juce::MouseEvent& event) override;
        void mouseMove (const juce::MouseEvent& event) override;
        void mouseDoubleClick (const juce::MouseEvent& event) override;
        void mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;
        bool keyPressed (const juce::KeyPress& key) override;

    private:
        void timerCallback() override;
        void updateDragHoverTarget (juce::Point<int> pos);

        PadGridComponent& owner;
        DragState dragState;
        bool isPanningCanvas = false;
        juce::Point<int> panStartScreenPos;
        juce::Point<int> panStartViewPos;
        friend class PadGridComponent;
    };

    svc::ControllerProfile currentProfile;
    int displayGridColumns = 4;
    int selectedPadIndex = 0;
    int hoveredPadIndex = -1;
    bool canPasteCurve = false;

    struct HitVisual
    {
        float intensity = 0.0f;
        double lastUpdateMs = 0.0;
    };

    std::unordered_map<int, HitVisual> hitByPadIndex;

    juce::Label padCountBadge;
    juce::ComboBox columnSelector;
    juce::TextButton quickAddButton { "+" };
    juce::Viewport viewport;
    PadCanvas padCanvas;

    std::unique_ptr<juce::TextEditor> inlineEditor;
    int editingPadIndex = -1;

    int padIndexAt (juce::Point<int> pos) const;
    void updateCanvasSize();
    int cellWidth() const;
    int cellHeight() const;

    friend class PadCanvas;
};
