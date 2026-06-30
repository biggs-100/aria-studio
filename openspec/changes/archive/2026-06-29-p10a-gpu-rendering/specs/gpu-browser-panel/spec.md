# GPU Browser Panel Specification

## Purpose

Rewrite the provisional ImGui `BrowserPanel` as a native GPU-widget panel with tree views, search, preview, and drag-and-drop — all rendered via Dawn/Skia.

## Requirements

### Requirement: Dawn/Skia Rendering

The browser panel SHALL render entirely via the GPU widget system and SkiaCanvas, not ImGui. The panel SHALL be a `ContainerWidget` with child widgets for the toolbar, tree, results list, and preview area.

#### Scenario: Panel renders with GPU widgets

- GIVEN an initialized GraphicsEngine with Dawn/Skia
- WHEN the browser panel is created and visible
- THEN the panel renders via `ContainerWidget::render(SkiaCanvas*)`
- AND no calls to ImGui draw functions are made

### Requirement: Drag Source Integration

Each file or category entry SHALL act as a drag source. Dragging SHALL set a payload with the file path or category name. The existing `FileDragCallback` SHALL fire when a drag completes on a valid drop target.

#### Scenario: File dragged to arrangement

- GIVEN a search result entry for "kick.wav"
- WHEN the user drags it to the arrangement drop target
- THEN `drag_cb_` is called with the full file path
- AND a `ClipCreated` event is published on the EventBus

### Requirement: Tree, Category, and Search Views

The panel SHALL support three view modes: FolderTree, CategoryTree, and SearchResults. View mode buttons in the toolbar SHALL toggle between them.

#### Scenario: Switch view mode

- GIVEN the panel in FolderTree mode
- WHEN the user clicks "Categories" in the toolbar
- THEN `view_mode()` returns `CategoryTree`
- AND the category tree is built via `BrowserEngine::search()->search_sync()`
- AND child widgets are rebuilt for the new mode

#### Scenario: Search with results

- GIVEN search text "kick" entered in the search bar
- WHEN the user presses Enter or clicks "Go"
- THEN `execute_search()` is called with params.text = "kick"
- AND results are displayed in a scrollable list widget
- AND each result row shows the file name and is selectable

### Requirement: Audio Preview Waveform

The preview panel SHALL render a waveform thumbnail using SkiaCanvas. The waveform SHALL be fetched via `BrowserEngine::waveforms()->get_waveform(path, 256)` and drawn as peak/min lines.

#### Scenario: Waveform renders for selected file

- GIVEN a selected file "kick.wav"
- WHEN the preview panel draws
- THEN a Skia `drawLine()` is called for each of the 256 peak/min pairs
- AND the waveform fills the preview area

#### Scenario: No waveform data

- GIVEN a selected file with no cached waveform data
- WHEN the preview panel draws
- THEN a horizontal center line and "No waveform data" text are rendered
- AND no crash occurs from null data
