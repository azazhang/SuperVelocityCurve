#include "PadGridComponent.h"
#include "MidiNoteNames.h"
#include "ScrollHelpers.h"

PadGridComponent::PadCanvas::PadCanvas (PadGridComponent& o) : owner (o)
{
    setWantsKeyboardFocus (true);
    setViewportIgnoreDragFlag (true);
}

PadGridComponent::PadCanvas::~PadCanvas()
{
    stopTimer();
}

void PadGridComponent::PadCanvas::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (svc::ui::Theme::panel()));

    const auto& pads = owner.currentProfile.getPads();
    const bool isDragging = (dragState.mode == DragMode::dragging && dragState.sourceIndex >= 0);

    // 1. Draw existing pads
    for (int i = 0; i < static_cast<int> (pads.size()); ++i)
    {
        auto bounds = owner.padBoundsForIndex (i);
        const auto& pad = pads[static_cast<size_t> (i)];

        const bool isDragSource = isDragging && (dragState.sourceIndex == i);
        const bool isSwapTarget = isDragging && (dragState.hoverTargetIndex == i);

        juce::uint32 baseColour = svc::ui::Theme::padIdle();
        if (! pad.enabled)
            baseColour = svc::ui::Theme::padDisabled();
        else if (i == owner.selectedPadIndex)
            baseColour = svc::ui::Theme::padSelected();
        else if (i == owner.hoveredPadIndex && ! isDragging)
            baseColour = svc::ui::Theme::padHover();

        const auto hitIt = owner.hitByPadIndex.find (i);
        if (hitIt != owner.hitByPadIndex.end())
        {
            const auto blend = juce::jlimit (0.0f, 1.0f, hitIt->second.intensity);
            baseColour = juce::Colour (baseColour).interpolatedWith (juce::Colour (svc::ui::Theme::padHit()), blend).getARGB();
        }

        const auto rect = bounds.toFloat();

        if (isDragSource)
        {
            // Dimmed source slot
            g.setColour (juce::Colour (svc::ui::Theme::panelRaised()).withAlpha (0.35f));
            g.fillRoundedRectangle (rect, 7.0f);
            g.setColour (juce::Colour (svc::ui::Theme::border()).withAlpha (0.5f));
            const float dashPattern[] = { 4.0f, 3.0f };
            g.drawDashedLine (juce::Line<float> (rect.getX(), rect.getY(), rect.getRight(), rect.getY()), dashPattern, 2, 1.0f);
            g.drawRoundedRectangle (rect, 7.0f, 1.0f);
            continue;
        }

        g.setColour (juce::Colour (baseColour));
        g.fillRoundedRectangle (rect, 7.0f);

        if (hitIt != owner.hitByPadIndex.end())
        {
            const auto blend = juce::jlimit (0.0f, 1.0f, hitIt->second.intensity);
            if (blend > 0.05f)
            {
                juce::ColourGradient hitGlow (juce::Colour (svc::ui::Theme::padHit()).withAlpha (0.4f * blend),
                                              rect.getCentreX(), rect.getCentreY(),
                                              juce::Colour (svc::ui::Theme::padHit()).withAlpha (0.0f),
                                              rect.getCentreX(), rect.getY(), true);
                g.setGradientFill (hitGlow);
                g.fillRoundedRectangle (rect, 7.0f);
            }
        }

        if (svc::ui::Theme::getMode() == svc::ui::ThemeMode::dark)
        {
            g.setColour (juce::Colours::white.withAlpha (0.07f));
            g.drawRoundedRectangle (rect.reduced (0.5f), 7.0f, 1.0f);
        }

        if (isSwapTarget)
        {
            // Accent highlight for swap target
            g.setColour (juce::Colour (svc::ui::Theme::accentWarm()));
            g.drawRoundedRectangle (rect.expanded (1.0f), 7.5f, 2.0f);

            // "SWAP" badge
            auto badgeArea = juce::Rectangle<float> (rect).removeFromTop (14.0f).removeFromRight (34.0f).translated (-4.0f, 4.0f);
            g.setColour (juce::Colour (svc::ui::Theme::accentWarm()));
            g.fillRoundedRectangle (badgeArea, 3.0f);
            g.setColour (juce::Colour (svc::ui::Theme::background()));
            g.setFont (svc::ui::Theme::smallFont().boldened());
            g.drawText ("SWAP", badgeArea, juce::Justification::centred, false);
        }
        else if (i == owner.selectedPadIndex)
        {
            g.setColour (juce::Colour (svc::ui::Theme::accent()).withAlpha (0.45f));
            g.drawRoundedRectangle (rect.expanded (0.5f), 7.5f, 1.4f);
            g.setColour (juce::Colour (svc::ui::Theme::accent()));
            g.drawRoundedRectangle (rect, 7.0f, 1.0f);
        }
        else
        {
            g.setColour (juce::Colour (svc::ui::Theme::border()).withAlpha (0.85f));
            g.drawRoundedRectangle (rect, 7.0f, 1.0f);
        }

        // Pad label and MIDI info (hidden if currently editing inline)
        if (owner.editingPadIndex != i)
        {
            auto textArea = bounds.reduced (6);
            const auto labelColour = pad.enabled ? svc::ui::Theme::textOnBackground (baseColour)
                                                 : juce::Colour (svc::ui::Theme::textSecondary());
            g.setColour (labelColour);
            g.setFont (svc::ui::Theme::bodyFont().boldened());
            g.drawFittedText (pad.label, textArea.removeFromTop (textArea.getHeight() / 2), juce::Justification::centred, 2);

            g.setFont (svc::ui::Theme::smallFont());
            g.setColour (labelColour.withAlpha (pad.enabled ? 0.88f : 0.75f));
            if (owner.displayGridColumns >= 8)
            {
                g.drawFittedText (svc::ui::formatMidiNoteShort (pad.midiNote),
                                textArea.removeFromTop (textArea.getHeight() / 2),
                                juce::Justification::centred,
                                1);
                g.drawFittedText ("Ch " + juce::String (pad.midiChannel),
                                textArea,
                                juce::Justification::centred,
                                1);
            }
            else
            {
                g.drawText (svc::ui::formatMidiNote (pad.midiNote) + "  Ch" + juce::String (pad.midiChannel),
                            textArea,
                            juce::Justification::centred);
            }
        }

        if (pad.retriggerGuardMs > 0.0)
        {
            g.setColour (juce::Colour (svc::ui::Theme::accentWarm()).withAlpha (0.85f));
            g.fillEllipse (static_cast<float> (bounds.getRight()) - 12.0f,
                           static_cast<float> (bounds.getY()) + 4.0f,
                           8.0f,
                           8.0f);
        }

        // Mini dynamics indicator bar at bottom of pad card
        const auto padF = bounds.toFloat();
        const auto barRect = juce::Rectangle<float> (padF.getX() + 6.0f, padF.getBottom() - 5.0f,
                                                     padF.getWidth() - 12.0f, 2.5f);
        g.setColour (juce::Colour (svc::ui::Theme::border()).withAlpha (0.4f));
        g.fillRoundedRectangle (barRect, 1.2f);

        const float floorVal = juce::jlimit (0.0f, 1.0f, pad.curve.getFloor());
        const float ceilVal = juce::jlimit (0.0f, 1.0f, pad.curve.getCeiling());
        const float minVal = std::min (floorVal, ceilVal);
        const float maxVal = std::max (floorVal, ceilVal);
        const float activeX = barRect.getX() + minVal * barRect.getWidth();
        const float activeW = juce::jmax (2.0f, (maxVal - minVal) * barRect.getWidth());
        g.setColour (juce::Colour (svc::ui::Theme::accent()).withAlpha (pad.enabled ? 0.75f : 0.35f));
        g.fillRoundedRectangle (activeX, barRect.getY(), activeW, barRect.getHeight(), 1.0f);
    }

    // 2. Draw empty cell move target highlight
    if (isDragging && dragState.hoverTargetCell.first >= 0)
    {
        const auto targetRect = juce::Rectangle<int> (8 + dragState.hoverTargetCell.second * owner.cellWidth(),
                                                     8 + dragState.hoverTargetCell.first * owner.cellHeight(),
                                                     owner.cellWidth() - 6,
                                                     owner.cellHeight() - 6).toFloat();
        g.setColour (juce::Colour (svc::ui::Theme::accent()).withAlpha (0.15f));
        g.fillRoundedRectangle (targetRect, 7.0f);
        g.setColour (juce::Colour (svc::ui::Theme::accent()).withAlpha (0.85f));
        const float dashPattern[] = { 4.0f, 3.0f };
        g.drawDashedLine (juce::Line<float> (targetRect.getX(), targetRect.getY(), targetRect.getRight(), targetRect.getY()), dashPattern, 2, 1.5f);
        g.drawRoundedRectangle (targetRect, 7.0f, 1.5f);

        g.setFont (svc::ui::Theme::smallFont().boldened());
        g.setColour (juce::Colour (svc::ui::Theme::accent()));
        g.drawText ("MOVE HERE", targetRect, juce::Justification::centred, false);
    }

    // 3. Draw Ghost (+) Pad Slot
    const auto ghostBounds = owner.ghostPadBounds();
    if (! ghostBounds.isEmpty())
    {
        auto gRect = ghostBounds.toFloat();
        g.setColour (juce::Colour (svc::ui::Theme::panelRaised()).withAlpha (0.45f));
        g.fillRoundedRectangle (gRect, 7.0f);

        g.setColour (juce::Colour (svc::ui::Theme::border()).withAlpha (0.6f));
        const float dashPattern[] = { 4.0f, 3.0f };
        g.drawDashedLine (juce::Line<float> (gRect.getX(), gRect.getY(), gRect.getRight(), gRect.getY()), dashPattern, 2, 1.0f);
        g.drawRoundedRectangle (gRect, 7.0f, 1.0f);

        auto plusArea = gRect.removeFromTop (gRect.getHeight() * 0.55f);
        g.setFont (svc::ui::Theme::sectionFont().boldened());
        g.setColour (juce::Colour (svc::ui::Theme::accent()).withAlpha (0.8f));
        g.drawText ("+", plusArea, juce::Justification::centred, false);

        g.setFont (svc::ui::Theme::smallFont());
        g.setColour (juce::Colour (svc::ui::Theme::textSecondary()).withAlpha (0.75f));
        g.drawText ("Add pad", gRect, juce::Justification::centredTop, false);
    }

    // 4. Draw Dragged Ghost Card following cursor
    if (isDragging)
    {
        const int w = owner.cellWidth() - 6;
        const int h = owner.cellHeight() - 6;
        auto ghostRect = juce::Rectangle<float> (static_cast<float> (dragState.currentPos.x - w / 2),
                                                static_cast<float> (dragState.currentPos.y - h / 2),
                                                static_cast<float> (w),
                                                static_cast<float> (h));

        // Soft drop shadow
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.fillRoundedRectangle (ghostRect.translated (0.0f, 3.0f), 8.0f);

        // Card body
        g.setColour (juce::Colour (svc::ui::Theme::panelRaised()).withAlpha (0.95f));
        g.fillRoundedRectangle (ghostRect, 7.0f);
        g.setColour (juce::Colour (dragState.isAltDuplicate ? svc::ui::Theme::accentWarm() : svc::ui::Theme::accent()));
        g.drawRoundedRectangle (ghostRect, 7.0f, 1.5f);

        if (dragState.isAltDuplicate)
        {
            auto dupBadge = ghostRect.removeFromTop (13.0f).removeFromRight (14.0f).translated (-2.0f, -2.0f);
            g.setColour (juce::Colour (svc::ui::Theme::accentWarm()));
            g.fillEllipse (dupBadge);
            g.setColour (juce::Colour (svc::ui::Theme::background()));
            g.setFont (svc::ui::Theme::smallFont().boldened());
            g.drawText ("+", dupBadge, juce::Justification::centred, false);
        }

        // Content
        const auto& pad = pads[static_cast<size_t> (dragState.sourceIndex)];
        auto labelArea = ghostRect.removeFromTop (ghostRect.getHeight() * 0.55f).reduced (4.0f);
        g.setColour (juce::Colour (svc::ui::Theme::textPrimary()));
        g.setFont (svc::ui::Theme::bodyFont().boldened());
        g.drawText (pad.label, labelArea, juce::Justification::centred, true);

        g.setFont (svc::ui::Theme::smallFont());
        g.setColour (juce::Colour (svc::ui::Theme::textSecondary()));
        g.drawText (svc::ui::formatMidiNote (pad.midiNote), ghostRect.reduced (4.0f), juce::Justification::centredTop, false);
    }
}

void PadGridComponent::PadCanvas::mouseDown (const juce::MouseEvent& event)
{
    if (owner.inlineEditor != nullptr && ! owner.inlineEditor->getBounds().contains (event.getPosition()))
        owner.commitInlineEditing();

    const auto pos = event.getPosition();

    // Right-click context menu
    if (event.mods.isPopupMenu())
    {
        const auto index = owner.padIndexAt (pos);
        if (index >= 0)
        {
            owner.setSelectedPadIndex (index);
            if (owner.onPadSelected)
                owner.onPadSelected (index);

            juce::PopupMenu menu;
            menu.addItem (1, "Rename");
            menu.addItem (2, "Duplicate Pad");
            menu.addItem (3, "MIDI Learn Note");
            menu.addSeparator();
            menu.addItem (4, "Copy Velocity Curve");
            menu.addItem (5, "Paste Velocity Curve", owner.canPasteCurve);
            menu.addItem (6, "Reset to Linear");
            menu.addSeparator();
            menu.addItem (7, "Delete Pad", owner.currentProfile.getPads().size() > 1);

            juce::Component::SafePointer<PadCanvas> safeThis (this);
            menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this)
                                                         .withTargetScreenArea (juce::Rectangle<int> (event.getScreenPosition().x, event.getScreenPosition().y, 1, 1)),
                                [safeThis, index] (int result)
                                {
                                    if (safeThis == nullptr)
                                        return;
                                    auto& grid = safeThis->owner;
                                    if (result == 1)
                                        grid.startInlineEditing (index);
                                    else if (result == 2 && grid.onPadDuplicateRequested)
                                        grid.onPadDuplicateRequested (index, std::nullopt);
                                    else if (result == 3 && grid.onLearnMidiRequested)
                                        grid.onLearnMidiRequested (index);
                                    else if (result == 4 && grid.onCopyCurveRequested)
                                        grid.onCopyCurveRequested (index);
                                    else if (result == 5 && grid.onPasteCurveRequested)
                                        grid.onPasteCurveRequested (index);
                                    else if (result == 6 && grid.onResetCurveRequested)
                                        grid.onResetCurveRequested (index);
                                    else if (result == 7 && grid.onDeletePadRequested)
                                        grid.onDeletePadRequested();
                                });
        }
        return;
    }

    if (isShowing() || isOnDesktop())
        grabKeyboardFocus();

    // Check Ghost Add Pad Slot click
    if (owner.ghostPadBounds().contains (pos))
    {
        if (owner.onAddPadRequested)
            owner.onAddPadRequested();
        return;
    }

    // Left click on existing pad
    const auto index = owner.padIndexAt (pos);
    if (index >= 0)
    {
        owner.setSelectedPadIndex (index);
        if (owner.onPadSelected)
            owner.onPadSelected (index);

        dragState.mode = DragMode::potentialDrag;
        dragState.sourceIndex = index;
        dragState.startPos = pos;
        dragState.currentPos = pos;
        dragState.isAltDuplicate = event.mods.isAltDown();
        isPanningCanvas = false;
    }
    else
    {
        // Click on empty canvas background -> pan canvas
        dragState = {};
        isPanningCanvas = true;
        panStartScreenPos = event.getScreenPosition();
        panStartViewPos = owner.viewport.getViewPosition();
    }
}

void PadGridComponent::PadCanvas::updateDragHoverTarget (juce::Point<int> pos)
{
    const auto targetPad = owner.padIndexAt (pos);
    if (targetPad >= 0 && targetPad != dragState.sourceIndex)
    {
        dragState.hoverTargetIndex = targetPad;
        dragState.hoverTargetCell = { -1, -1 };
    }
    else if (targetPad == -1)
    {
        const auto cell = owner.cellAt (pos);
        if (cell.first >= 0)
        {
            const auto occIndex = owner.padIndexAtCell (cell.first, cell.second);
            if (occIndex >= 0)
            {
                if (occIndex != dragState.sourceIndex)
                {
                    dragState.hoverTargetIndex = occIndex;
                    dragState.hoverTargetCell = { -1, -1 };
                }
                else
                {
                    dragState.hoverTargetIndex = -1;
                    dragState.hoverTargetCell = { -1, -1 };
                }
            }
            else
            {
                dragState.hoverTargetIndex = -1;
                dragState.hoverTargetCell = cell;
            }
        }
        else
        {
            dragState.hoverTargetIndex = -1;
            dragState.hoverTargetCell = { -1, -1 };
        }
    }
    else
    {
        dragState.hoverTargetIndex = -1;
        dragState.hoverTargetCell = { -1, -1 };
    }
}

void PadGridComponent::PadCanvas::mouseDrag (const juce::MouseEvent& event)
{
    if (isPanningCanvas)
    {
        const auto delta = event.getScreenPosition() - panStartScreenPos;
        owner.viewport.setViewPosition (panStartViewPos - delta);
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        return;
    }

    const auto pos = event.getPosition();

    if (dragState.mode == DragMode::potentialDrag)
    {
        if (pos.getDistanceFrom (dragState.startPos) > 4)
        {
            dragState.mode = DragMode::dragging;
            startTimerHz (30);
        }
    }

    if (dragState.mode == DragMode::dragging)
    {
        dragState.currentPos = pos;
        dragState.isAltDuplicate = event.mods.isAltDown();

        // Edge auto-scroll when dragging near viewport boundary
        const auto vpPos = owner.viewport.getLocalPoint (this, pos);
        if (owner.viewport.autoScroll (vpPos.x, vpPos.y, 28, 14))
        {
            dragState.currentPos = getLocalPoint (&owner.viewport, vpPos);
        }

        updateDragHoverTarget (dragState.currentPos);

        if (dragState.isAltDuplicate)
            setMouseCursor (juce::MouseCursor::CopyingCursor);
        else
            setMouseCursor (juce::MouseCursor::DraggingHandCursor);

        repaint();
    }
}

void PadGridComponent::PadCanvas::timerCallback()
{
    if (dragState.mode != DragMode::dragging)
    {
        stopTimer();
        return;
    }

    // Edge auto-scroll when user holds mouse near viewport boundary
    const auto vpPos = owner.viewport.getLocalPoint (this, dragState.currentPos);
    if (owner.viewport.autoScroll (vpPos.x, vpPos.y, 28, 14))
    {
        dragState.currentPos = getLocalPoint (&owner.viewport, vpPos);
        updateDragHoverTarget (dragState.currentPos);
        repaint();
    }
}

void PadGridComponent::PadCanvas::mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    owner.viewport.mouseWheelMove (event, wheel);

    if (dragState.mode == DragMode::dragging)
    {
        const auto vpPos = owner.viewport.getLocalPoint (this, event.getPosition());
        dragState.currentPos = getLocalPoint (&owner.viewport, vpPos);
        updateDragHoverTarget (dragState.currentPos);
        repaint();
    }
}

void PadGridComponent::PadCanvas::mouseUp (const juce::MouseEvent& event)
{
    juce::ignoreUnused (event);
    stopTimer();
    setMouseCursor (juce::MouseCursor::NormalCursor);

    if (isPanningCanvas)
    {
        isPanningCanvas = false;
        return;
    }

    if (dragState.mode == DragMode::dragging)
    {
        if (dragState.hoverTargetIndex >= 0 && dragState.hoverTargetIndex != dragState.sourceIndex)
        {
            if (dragState.isAltDuplicate)
            {
                if (owner.onPadDuplicateRequested)
                    owner.onPadDuplicateRequested (dragState.sourceIndex, std::nullopt);
            }
            else
            {
                if (owner.onPadSwapRequested)
                    owner.onPadSwapRequested (dragState.sourceIndex, dragState.hoverTargetIndex);
            }
        }
        else if (dragState.hoverTargetCell.first >= 0)
        {
            const auto targetCell = owner.displayToGridCell (dragState.hoverTargetCell.first, dragState.hoverTargetCell.second);
            if (dragState.isAltDuplicate)
            {
                if (owner.onPadDuplicateRequested)
                    owner.onPadDuplicateRequested (dragState.sourceIndex, targetCell);
            }
            else
            {
                if (owner.onPadMoveRequested)
                    owner.onPadMoveRequested (dragState.sourceIndex, targetCell.first, targetCell.second);
            }
        }
    }

    dragState = {};
    repaint();
}

void PadGridComponent::PadCanvas::mouseDoubleClick (const juce::MouseEvent& event)
{
    const auto index = owner.padIndexAt (event.getPosition());
    if (index >= 0)
        owner.startInlineEditing (index);
}

bool PadGridComponent::PadCanvas::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::backspaceKey || key == juce::KeyPress::deleteKey)
    {
        if (owner.onDeletePadRequested && owner.currentProfile.getPads().size() > 1)
        {
            owner.onDeletePadRequested();
            return true;
        }
    }
    else if (key == juce::KeyPress ('d', juce::ModifierKeys::commandModifier, 0)
             || key == juce::KeyPress ('d', juce::ModifierKeys::ctrlModifier, 0))
    {
        if (owner.onPadDuplicateRequested && owner.selectedPadIndex >= 0)
        {
            owner.onPadDuplicateRequested (owner.selectedPadIndex, std::nullopt);
            return true;
        }
    }
    else if (key == juce::KeyPress::returnKey)
    {
        if (owner.selectedPadIndex >= 0)
        {
            owner.startInlineEditing (owner.selectedPadIndex);
            return true;
        }
    }
    else if (key == juce::KeyPress ('c', juce::ModifierKeys::commandModifier, 0)
             || key == juce::KeyPress ('c', juce::ModifierKeys::ctrlModifier, 0))
    {
        if (owner.onCopyCurveRequested && owner.selectedPadIndex >= 0)
        {
            owner.onCopyCurveRequested (owner.selectedPadIndex);
            return true;
        }
    }
    else if (key == juce::KeyPress ('v', juce::ModifierKeys::commandModifier, 0)
             || key == juce::KeyPress ('v', juce::ModifierKeys::ctrlModifier, 0))
    {
        if (owner.onPasteCurveRequested && owner.selectedPadIndex >= 0)
        {
            owner.onPasteCurveRequested (owner.selectedPadIndex);
            return true;
        }
    }
    else if (key.isKeyCode (juce::KeyPress::leftKey) || key.isKeyCode (juce::KeyPress::rightKey)
             || key.isKeyCode (juce::KeyPress::upKey) || key.isKeyCode (juce::KeyPress::downKey))
    {
        const auto& pads = owner.currentProfile.getPads();
        if (pads.empty())
            return false;

        int newIdx = owner.selectedPadIndex;
        if (key.isKeyCode (juce::KeyPress::leftKey))
            newIdx = juce::jmax (0, newIdx - 1);
        else if (key.isKeyCode (juce::KeyPress::rightKey))
            newIdx = juce::jmin (static_cast<int> (pads.size()) - 1, newIdx + 1);
        else if (key.isKeyCode (juce::KeyPress::upKey))
            newIdx = juce::jmax (0, newIdx - owner.displayGridColumns);
        else if (key.isKeyCode (juce::KeyPress::downKey))
            newIdx = juce::jmin (static_cast<int> (pads.size()) - 1, newIdx + owner.displayGridColumns);

        if (newIdx != owner.selectedPadIndex)
        {
            owner.setSelectedPadIndex (newIdx);
            if (owner.onPadSelected)
                owner.onPadSelected (newIdx);
            owner.scrollPadIntoView (newIdx);
            return true;
        }
    }

    return false;
}

void PadGridComponent::PadCanvas::mouseMove (const juce::MouseEvent& event)
{
    const auto index = owner.padIndexAt (event.getPosition());
    if (index != owner.hoveredPadIndex)
    {
        const auto prev = owner.hoveredPadIndex;
        owner.hoveredPadIndex = index;

        if (prev >= 0)
            repaint (owner.padBoundsForIndex (prev));

        if (index >= 0)
            repaint (owner.padBoundsForIndex (index));
    }
}

PadGridComponent::PadGridComponent()
    : padCanvas (*this)
{
    padCanvas.setOpaque (true);
    setOpaque (true);

    padCountBadge.setFont (svc::ui::Theme::smallFont().boldened());
    padCountBadge.setColour (juce::Label::textColourId, juce::Colour (svc::ui::Theme::textSecondary()));
    padCountBadge.setColour (juce::Label::backgroundColourId, juce::Colour (svc::ui::Theme::panelRaised()).withAlpha (0.6f));
    padCountBadge.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (padCountBadge);

    columnSelector.addItem ("4 Cols", 4);
    columnSelector.addItem ("8 Cols", 8);
    columnSelector.setSelectedId (displayGridColumns, juce::dontSendNotification);
    columnSelector.onChange = [this]
    {
        setDisplayGridColumns (columnSelector.getSelectedId());
    };
    addAndMakeVisible (columnSelector);

    quickAddButton.setTooltip ("Add pad");
    quickAddButton.onClick = [this]
    {
        if (onAddPadRequested)
            onAddPadRequested();
    };
    addAndMakeVisible (quickAddButton);

    addAndMakeVisible (viewport);
    viewport.setViewedComponent (&padCanvas, false);
    svc::ui::configurePadGridViewport (viewport);
}

void PadGridComponent::setProfile (const svc::ControllerProfile& profile, bool resetSelection)
{
    currentProfile = profile;
    displayGridColumns = profile.getDisplayGridColumns();

    if (resetSelection)
        selectedPadIndex = profile.getPads().empty() ? -1 : 0;
    else
        selectedPadIndex = juce::jlimit (-1, juce::jmax (0, static_cast<int> (profile.getPads().size()) - 1),
                                       selectedPadIndex);

    padCountBadge.setText (juce::String (profile.getPads().size()) + " Pads", juce::dontSendNotification);
    columnSelector.setSelectedId (displayGridColumns, juce::dontSendNotification);

    hitByPadIndex.clear();
    updateCanvasSize();
    svc::ui::updatePadGridScrollbars (viewport, padCanvas);
    padCanvas.repaint();
    repaint();
}

void PadGridComponent::scrollPadIntoView (int index)
{
    const auto bounds = padBoundsForIndex (index);
    if (bounds.isEmpty())
        return;

    const auto viewArea = viewport.getViewArea();
    int newX = viewArea.getX();
    int newY = viewArea.getY();

    if (bounds.getX() < viewArea.getX() + 8)
        newX = std::max (0, bounds.getX() - 8);
    else if (bounds.getRight() > viewArea.getRight() - 8)
        newX = bounds.getRight() + 8 - viewArea.getWidth();

    if (bounds.getY() < viewArea.getY() + 8)
        newY = std::max (0, bounds.getY() - 8);
    else if (bounds.getBottom() > viewArea.getBottom() - 8)
        newY = bounds.getBottom() + 8 - viewArea.getHeight();

    if (newX != viewArea.getX() || newY != viewArea.getY())
        viewport.setViewPosition (newX, newY);
}

void PadGridComponent::updatePad (int index, const svc::ProfilePad& pad)
{
    if (index < 0 || index >= static_cast<int> (currentProfile.getPads().size()))
        return;

    currentProfile.getPads()[static_cast<size_t> (index)] = pad;
    padCanvas.repaint (padBoundsForIndex (index));
}

void PadGridComponent::setSelectedPadIndex (int index)
{
    selectedPadIndex = juce::jlimit (0, juce::jmax (0, static_cast<int> (currentProfile.getPads().size()) - 1), index);
    padCanvas.repaint();
}

void PadGridComponent::flashPadHit (int note, int channel, float outputVelocity)
{
    const auto& pads = currentProfile.getPads();
    for (int i = 0; i < static_cast<int> (pads.size()); ++i)
    {
        const auto& pad = pads[static_cast<size_t> (i)];
        if (pad.midiNote == note && pad.midiChannel == channel)
        {
            hitByPadIndex[i] = { outputVelocity, juce::Time::getMillisecondCounterHiRes() };
            padCanvas.repaint (padBoundsForIndex (i));
            return;
        }
    }
}

void PadGridComponent::refreshVisualCache()
{
    padCountBadge.setColour (juce::Label::textColourId, juce::Colour (svc::ui::Theme::textSecondary()));
    padCountBadge.setColour (juce::Label::backgroundColourId, juce::Colour (svc::ui::Theme::panelRaised()).withAlpha (0.6f));
    padCanvas.repaint();
    repaint();
}

bool PadGridComponent::hasActiveHitVisuals() const noexcept
{
    for (const auto& entry : hitByPadIndex)
        if (entry.second.intensity > 0.02f)
            return true;

    return false;
}

void PadGridComponent::decayHitVisuals()
{
    const auto now = juce::Time::getMillisecondCounterHiRes();
    juce::RectangleList<int> dirty;

    for (auto it = hitByPadIndex.begin(); it != hitByPadIndex.end();)
    {
        const auto padIndex = it->first;
        const auto age = now - it->second.lastUpdateMs;
        if (age > 500.0)
        {
            dirty.add (padBoundsForIndex (padIndex));
            it = hitByPadIndex.erase (it);
            continue;
        }

        const auto prev = it->second.intensity;
        const auto next = juce::jmax (0.0f, prev - 0.08f);

        if (next <= 0.001f)
        {
            dirty.add (padBoundsForIndex (padIndex));
            it = hitByPadIndex.erase (it);
        }
        else if (std::abs (next - prev) > 0.0001f)
        {
            it->second.intensity = next;
            dirty.add (padBoundsForIndex (padIndex));
            ++it;
        }
        else
        {
            ++it;
        }
    }

    if (! dirty.isEmpty())
    {
        for (const auto& rect : dirty)
            padCanvas.repaint (rect);
    }
}

int PadGridComponent::cellWidth() const
{
    return displayGridColumns >= 8 ? 72 : 96;
}

int PadGridComponent::cellHeight() const
{
    return 68;
}

std::pair<int, int> PadGridComponent::cellAt (juce::Point<int> pos) const
{
    if (pos.x < 8 || pos.y < 8)
        return { -1, -1 };

    const int col = (pos.x - 8) / cellWidth();
    const int row = (pos.y - 8) / cellHeight();

    if (col < 0 || col >= displayGridColumns || row < 0)
        return { -1, -1 };

    int maxDisplayRow = 0;
    for (int i = 0; i < static_cast<int> (currentProfile.getPads().size()); ++i)
    {
        const auto bounds = padBoundsForIndex (i);
        maxDisplayRow = std::max (maxDisplayRow, (bounds.getY() - 8) / cellHeight());
    }
    const auto [ghostRow, ghostCol] = currentProfile.suggestNextGridCell (displayGridColumns);
    maxDisplayRow = std::max (maxDisplayRow, ghostRow);

    if (row > maxDisplayRow + 1)
        return { -1, -1 };

    return { row, col };
}

int PadGridComponent::padIndexAtCell (int displayRow, int displayCol) const
{
    const auto& pads = currentProfile.getPads();
    for (int i = 0; i < static_cast<int> (pads.size()); ++i)
    {
        const auto bounds = padBoundsForIndex (i);
        const int cellCol = (bounds.getX() - 8) / cellWidth();
        const int cellRow = (bounds.getY() - 8) / cellHeight();
        if (cellRow == displayRow && cellCol == displayCol)
            return i;
    }
    return -1;
}

std::pair<int, int> PadGridComponent::displayToGridCell (int displayRow, int displayCol) const
{
    int maxCol = 0;
    for (const auto& p : currentProfile.getPads())
        maxCol = std::max (maxCol, p.gridCol);

    const int sourceCols = std::max (displayGridColumns, maxCol + 1);
    const int colMultiplier = (sourceCols + displayGridColumns - 1) / displayGridColumns;

    if (colMultiplier <= 1)
        return { displayRow, displayCol };

    const int gridRow = displayRow / colMultiplier;
    const int gridCol = (displayRow % colMultiplier) * displayGridColumns + displayCol;
    return { gridRow, gridCol };
}

std::pair<int, int> PadGridComponent::gridToDisplayCell (int gridRow, int gridCol) const
{
    int maxCol = 0;
    for (const auto& p : currentProfile.getPads())
        maxCol = std::max (maxCol, p.gridCol);

    const int sourceCols = std::max (displayGridColumns, maxCol + 1);
    const int colMultiplier = (sourceCols + displayGridColumns - 1) / displayGridColumns;

    const int displayCol = gridCol % displayGridColumns;
    const int displayRow = gridRow * colMultiplier + (gridCol / displayGridColumns);
    return { displayRow, displayCol };
}

juce::Rectangle<int> PadGridComponent::ghostPadBounds() const
{
    if (currentProfile.getPads().size() >= static_cast<size_t> (svc::kMaxProfilePads))
        return {};

    const auto [ghostRow, ghostCol] = currentProfile.suggestNextGridCell (displayGridColumns);
    return { 8 + ghostCol * cellWidth(),
             8 + ghostRow * cellHeight(),
             cellWidth() - 6,
             cellHeight() - 6 };
}

void PadGridComponent::updateCanvasSize()
{
    const auto& pads = currentProfile.getPads();
    if (pads.empty())
    {
        padCanvas.setSize (viewport.getWidth(), viewport.getHeight());
        return;
    }

    int maxCol = 0;
    for (const auto& pad : pads)
        maxCol = std::max (maxCol, pad.gridCol);

    const int sourceCols = std::max (displayGridColumns, maxCol + 1);
    const int colMultiplier = (sourceCols + displayGridColumns - 1) / displayGridColumns;

    int maxDisplayRow = 0;
    for (const auto& pad : pads)
        maxDisplayRow = juce::jmax (maxDisplayRow, pad.gridRow * colMultiplier + (pad.gridCol / displayGridColumns));

    const auto [ghostRow, ghostCol] = currentProfile.suggestNextGridCell (displayGridColumns);
    maxDisplayRow = juce::jmax (maxDisplayRow, ghostRow);

    padCanvas.setSize (juce::jmax (displayGridColumns * cellWidth() + 16, viewport.getWidth()),
                       (maxDisplayRow + 1) * cellHeight() + 16);
}

juce::Rectangle<int> PadGridComponent::padBoundsForIndex (int index) const
{
    const auto& pads = currentProfile.getPads();
    if (index < 0 || index >= static_cast<int> (pads.size()))
        return {};

    const auto [displayRow, displayCol] = gridToDisplayCell (pads[static_cast<size_t> (index)].gridRow,
                                                            pads[static_cast<size_t> (index)].gridCol);

    return { 8 + displayCol * cellWidth(),
             8 + displayRow * cellHeight(),
             cellWidth() - 6,
             cellHeight() - 6 };
}

int PadGridComponent::padIndexAt (juce::Point<int> pos) const
{
    const auto& pads = currentProfile.getPads();

    for (int i = static_cast<int> (pads.size()) - 1; i >= 0; --i)
    {
        if (padBoundsForIndex (i).contains (pos))
            return i;
    }

    return -1;
}

void PadGridComponent::startInlineEditing (int padIndex)
{
    const auto& pads = currentProfile.getPads();
    if (padIndex < 0 || padIndex >= static_cast<int> (pads.size()))
        return;

    commitInlineEditing();

    editingPadIndex = padIndex;
    inlineEditor = std::make_unique<juce::TextEditor>();
    inlineEditor->setFont (svc::ui::Theme::bodyFont().boldened());
    inlineEditor->setColour (juce::TextEditor::textColourId, juce::Colour (svc::ui::Theme::textPrimary()));
    inlineEditor->setColour (juce::TextEditor::backgroundColourId, juce::Colour (svc::ui::Theme::panelRaised()));
    inlineEditor->setColour (juce::TextEditor::outlineColourId, juce::Colour (svc::ui::Theme::accent()));
    inlineEditor->setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour (svc::ui::Theme::accent()));
    inlineEditor->setJustification (juce::Justification::centred);
    inlineEditor->setText (pads[static_cast<size_t> (padIndex)].label, false);
    inlineEditor->selectAll();
    inlineEditor->setSelectAllWhenFocused (true);

    auto bounds = padBoundsForIndex (padIndex).reduced (6);
    inlineEditor->setBounds (bounds.removeFromTop (bounds.getHeight() / 2));
    padCanvas.addAndMakeVisible (*inlineEditor);
    inlineEditor->toFront (true);
    if (inlineEditor->isShowing() || inlineEditor->isOnDesktop())
        inlineEditor->grabKeyboardFocus();

    inlineEditor->onReturnKey = [this] { commitInlineEditing(); };
    inlineEditor->onEscapeKey = [this] { cancelInlineEditing(); };
    inlineEditor->onFocusLost = [this] { commitInlineEditing(); };
}

void PadGridComponent::commitInlineEditing()
{
    if (inlineEditor == nullptr || editingPadIndex < 0)
        return;

    const auto editor = std::move (inlineEditor);
    const auto padIdx = editingPadIndex;
    editingPadIndex = -1;

    editor->onFocusLost = nullptr;
    editor->onReturnKey = nullptr;
    editor->onEscapeKey = nullptr;

    const auto newText = editor->getText().trim();

    const auto& pads = currentProfile.getPads();
    if (padIdx >= 0 && padIdx < static_cast<int> (pads.size()))
    {
        if (newText.isNotEmpty() && newText != pads[static_cast<size_t> (padIdx)].label)
        {
            if (onPadRenamed)
                onPadRenamed (padIdx, newText);
        }
    }

    padCanvas.repaint();
}

void PadGridComponent::cancelInlineEditing()
{
    if (inlineEditor == nullptr)
        return;

    const auto editor = std::move (inlineEditor);
    editingPadIndex = -1;

    editor->onFocusLost = nullptr;
    editor->onReturnKey = nullptr;
    editor->onEscapeKey = nullptr;

    padCanvas.repaint();
}

void PadGridComponent::paint (juce::Graphics& g)
{
    svc::ui::Theme::fillPanel (g, getLocalBounds().toFloat(), 10.0f);
    g.setColour (juce::Colour (svc::ui::Theme::textPrimary()));
    g.setFont (svc::ui::Theme::sectionFont());
    g.drawText ("Pad Layout", getLocalBounds().removeFromTop (28).reduced (12, 0), juce::Justification::centredLeft);
}

void PadGridComponent::resized()
{
    auto area = getLocalBounds().reduced (8);
    auto header = area.removeFromTop (28);

    // Left: space for title "Pad Layout" (~85px) then padCountBadge
    header.removeFromLeft (90);
    padCountBadge.setBounds (header.removeFromLeft (65).reduced (0, 3));

    // Right: quickAddButton (24px) + columnSelector (72px)
    quickAddButton.setBounds (header.removeFromRight (24).reduced (0, 2));
    header.removeFromRight (6);
    columnSelector.setBounds (header.removeFromRight (72).reduced (0, 2));

    area.removeFromTop (4);
    viewport.setBounds (area);
    updateCanvasSize();
    svc::ui::updatePadGridScrollbars (viewport, padCanvas);
}
