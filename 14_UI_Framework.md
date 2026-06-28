# ARIA DAW — UI Framework Specification

> **Version**: 0.1
> **Status**: Draft
> **Last Updated**: 2026-06-27
> **Language**: TypeScript (components) + C++ (graphics engine)
> **Rendering**: WebGPU + Skia

---

## Table of Contents

1. [Overview](#1-overview)
2. [Architecture](#2-architecture)
3. [Component Model](#3-component-model)
4. [Layout Engine](#4-layout-engine)
5. [Docking System](#5-docking-system)
6. [Event System](#6-event-system)
7. [Animation Engine](#7-animation-engine)
8. [Input System](#8-input-system)
9. [Focus & Keyboard Navigation](#9-focus--keyboard-navigation)
10. [Component Library](#10-component-library)
11. [API Reference](#11-api-reference)
12. [RFC Index](#12-rfc-index)

---

## 1. Overview

### 1.1. Purpose

The UI Framework is the complete user interface system for ARIA DAW. It is built on a custom GPU-accelerated rendering engine (WebGPU + Skia) with a declarative component model written in TypeScript. Every pixel—from buttons to waveforms to piano roll notes—is rendered on the GPU.

### 1.2. Design Goals

| Goal | Target |
|---|---|
| Frame rate | 144 FPS on mid-range GPU |
| Component model | Declarative, reactive (similar to React/Figma) |
| Layout | Flexbox-based, GPU-computed |
| Docking | Fully customizable, multi-window |
| Animations | Easing curves on all interactions |
| Accessibility | Full keyboard navigation, screen reader support |

### 1.3. Architecture

```
┌──────────────────────────────────────────────────────────────┐
│  TypeScript Component Layer                                  │
│  ┌────────────────────────────────────────────────────────┐ │
│  │  Virtual Component Tree (declarative)                   │ │
│  │  Render → Diff → Patch cycle                           │ │
│  └────────────────────────┬───────────────────────────────┘ │
│                           │                                   │
│  ┌────────────────────────▼───────────────────────────────┐ │
│  │  Layout Engine (Flexbox)                                │ │
│  │  Compute position/size for every component              │ │
│  └────────────────────────┬───────────────────────────────┘ │
│                           │                                   │
│  ┌────────────────────────▼───────────────────────────────┐ │
│  │  Paint Tree (C++ FFI Bridge)                            │ │
│  │  Convert component state → Skia draw commands           │ │
│  └────────────────────────┬───────────────────────────────┘ │
│                           │                                   │
│  ┌────────────────────────▼───────────────────────────────┐ │
│  │  Skia Canvas (C++)                                     │ │
│  │  GPU-accelerated 2D drawing                            │ │
│  └────────────────────────┬───────────────────────────────┘ │
│                           │                                   │
│  ┌────────────────────────▼───────────────────────────────┐ │
│  │  WebGPU (Vulkan / Metal / DirectX 12)                   │ │
│  │  Hardware-accelerated rendering                         │ │
│  └────────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────┘
```

---

## 2. Architecture

### 2.1. Render Pipeline

```typescript
// Each frame:
// 1. Process input events (mouse, keyboard, touch)
// 2. Update component state (reactive)
// 3. Recompute layout (if dirty)
// 4. Generate paint commands
// 5. Submit to GPU

interface FrameLoop {
  inputPhase(): void;       // Collect and dispatch input
  updatePhase(): void;      // State changes, side effects
  layoutPhase(): void;      // Flexbox layout computation
  paintPhase(): void;       // Generate display list
  renderPhase(): void;      // Submit to GPU
}

// Frame timing (target 144 FPS = 6.94ms per frame)
// Input:     0.5ms
// Update:    1.0ms
// Layout:    1.0ms
// Paint:     2.0ms
// Render:    2.0ms
// Buffer:    0.44ms
```

### 2.2. Component Tree

```typescript
// Components form a tree structure:
// Root
// ├── DockingContainer
// │   ├── Panel (Browser)
// │   │   ├── SearchBar
// │   │   ├── FileTree
// │   │   └── PreviewPanel
// │   ├── Panel (Arrangement)
// │   │   ├── TransportBar
// │   │   ├── Timeline
// │   │   ├── TrackList
// │   │   └── TrackContent
// │   └── Panel (Mixer)
// │       ├── ChannelStrip (×16)
// │       └── MasterStrip
// └── StatusBar
```

---

## 3. Component Model

### 3.1. Base Component

```typescript
interface Component<P = {}> {
  // Identification
  readonly id: string;
  readonly type: string;
  
  // Properties (immutable from parent)
  props: P;
  
  // State (mutable internally)
  state: Record<string, any>;
  
  // Lifecycle
  onMount(): void;
  onUnmount(): void;
  onPropsChange(oldProps: P): void;
  
  // Rendering
  render(): VNode | VNode[];
  
  // Layout
  layout(): LayoutStyle;
  
  // Events
  onEvent(event: UIEvent): void;
  
  // Children
  children(): Component[];
}
```

### 3.2. Virtual DOM

```typescript
// Components return VNodes (virtual nodes) from render().
// The framework diffs VNodes and applies minimal changes.

interface VNode {
  type: string | ComponentClass;
  props: Record<string, any>;
  children: VNode[];
  key?: string;         // For efficient reordering
  ref?: string;          // Reference for imperative access
}

// Diff algorithm:
// 1. Same type + same key = update in place
// 2. Same type + different key = replace
// 3. Different type = replace + unmount old
// 4. New vnode = mount new

// Patch operations:
// - CREATE: Add new component
// - UPDATE: Update props/sate of existing
// - MOVE: Reorder children
// - REMOVE: Unmount and destroy
// - REPLACE: Full replacement
```

### 3.3. Example: Button Component

```typescript
class Button implements Component<ButtonProps> {
  id: string;
  type = 'Button';
  props: ButtonProps;
  state = { hovered: false, pressed: false };
  
  render(): VNode {
    return {
      type: 'container',
      props: {
        style: this.getStyle(),
        onMouseEnter: () => this.setState({ hovered: true }),
        onMouseLeave: () => this.setState({ hovered: false }),
        onMouseDown: () => this.setState({ pressed: true }),
        onMouseUp: () => this.handleClick(),
      },
      children: [
        {
          type: 'text',
          props: { text: this.props.label },
          children: [],
        }
      ],
    };
  }
  
  private getStyle(): Style {
    const base = {
      padding: [8, 16],
      borderRadius: 6,
      transition: 'all 100ms ease-out',
    };
    
    if (this.state.pressed) {
      return { ...base, backgroundColor: '#333', transform: 'scale(0.97)' };
    }
    if (this.state.hovered) {
      return { ...base, backgroundColor: '#444' };
    }
    return { ...base, backgroundColor: '#333' };
  }
}
```

### 3.4. Component Lifecycle

```
Mount
  constructor()
  ↓
  onMount()          ← Component is attached to DOM
  ↓
  [Props Change]
    onPropsChange()
  ↓
  [State Change]
    render()         ← Re-render triggered
  ↓
  [User Interaction]
    onEvent()
    setState()
    render()
  ↓
Unmount
  onUnmount()        ← Component is removed
```

---

## 4. Layout Engine

### 4.1. Flexbox Layout

```typescript
// The layout engine implements a GPU-computed subset of CSS Flexbox:

interface LayoutStyle {
  // Display
  display: 'flex' | 'none';
  
  // Flex direction
  flexDirection: 'row' | 'column';
  flexWrap: 'wrap' | 'nowrap';
  
  // Alignment
  justifyContent: 'flex-start' | 'flex-end' | 'center' |
                  'space-between' | 'space-around' | 'space-evenly';
  alignItems: 'flex-start' | 'flex-end' | 'center' | 'stretch';
  alignContent: 'flex-start' | 'flex-end' | 'center' | 'stretch';
  
  // Flex item
  flex: number;          // Grow factor
  flexShrink: number;    // Shrink factor
  flexBasis: number;     // Initial size
  
  // Size
  width: number | string;   // px or %
  height: number | string;
  minWidth: number;
  maxWidth: number;
  
  // Margin/Padding
  margin: [number, number, number, number];  // top, right, bottom, left
  padding: [number, number, number, number];
  
  // Position
  position: 'relative' | 'absolute';
  top: number;
  left: number;
  
  // Overflow
  overflow: 'visible' | 'hidden' | 'scroll';
}
```

### 4.2. Layout Computation

```typescript
class LayoutEngine {
  // Compute layout for the entire tree
  computeLayout(root: VNode, viewport: Size): void;
  
  // Compute layout for a subtree (partial update)
  computeSubtree(node: VNode, constraints: Size): void;
  
  // Mark subtree as dirty (on size/pos change)
  markDirty(node: VNode): void;
  
  // GPU-accelerated layout
  // Layout is computed on the GPU when possible (parallel per-node)
  // Fallback to CPU for complex cases
  
  // Spacer / divider
  // Used by docking system for resizable panels
}
```

### 4.3. Responsive Design

```typescript
// Components can adapt to available space:
// - Narrow (< 200px): Icon only, compact
// - Normal (200-400px): Icon + label
// - Wide (> 400px): Full detail

enum SizeClass {
  Compact,
  Regular,
  Expanded,
  Full
}

function getSizeClass(width: number): SizeClass {
  if (width < 200) return SizeClass.Compact;
  if (width < 400) return SizeClass.Regular;
  if (width < 800) return SizeClass.Expanded;
  return SizeClass.Full;
}
```

---

## 5. Docking System

### 5.1. Dock Architecture

```typescript
// The docking system allows fully customizable panel layouts:

class DockManager {
  // Root container
  root: DockContainer;
  
  // Register a panel type
  registerPanel(type: string, factory: () => Component): void;
  
  // Open a panel in a dock location
  openPanel(type: string, location: DockLocation): void;
  
  // Close a panel
  closePanel(panelId: string): void;
  
  // Move panel to new location
  movePanel(panelId: string, location: DockLocation): void;
  
  // Save/restore layout
  saveLayout(): DockLayout;
  loadLayout(layout: DockLayout): void;
  
  // Reset to default layout
  resetToDefault(): void;
  
  // Layout presets
  enum Preset {
    Production,       // Arrangement + Mixer + Browser
    Mixing,           // Full screen mixer
    Editing,          // Piano roll focused
    Performance,      // Session view focused
    Minimal           // Single view
  }
  void applyPreset(Preset preset);
}

type DockLocation =
  | { type: 'left' }
  | { type: 'right' }
  | { type: 'top' }
  | { type: 'bottom' }
  | { type: 'center' }
  | { type: 'tab', groupId: string }
  | { type: 'split', direction: 'horizontal' | 'vertical', ratio: number }
  | { type: 'float', x: number, y: number, width: number, height: number }
  | { type: 'window' };  // Separate OS window
```

### 5.2. Default Layout

```
┌───────────────────────────────────────────────────────────────┐
│  Menu Bar                                                     │
├──────────┬────────────────────────────────────────────────────┤
│          │                                       │            │
│ Browser  │   Arrangement View                    │  Mixer     │
│          │                                       │            │
│ Samples  │  ┌──────────────────────────────────┐  │  Kick     │
│ Plugins  │  │ Timeline                         │  │  Snare    │
│ Projects │  ├──────────────────────────────────┤  │  HiHat    │
│          │  │ Track 1 │ ░░░░░░░░░░░░░░░░░░░░ │  │  Bass     │
│          │  │ Track 2 │ ░░░░░░░░░░░░░░░░░░░░ │  │  Synth    │
│          │  │ Track 3 │ ░░░░░░░░░░░░░░░░░░░░ │  │  Pad      │
│          │  └──────────────────────────────────┘  │          │
│          │                                       │  Master   │
├──────────┴───────────────────────────────────────┴──────────┤
│  Status Bar  |  140 BPM  |  4/4  |  2.1.0  |  -14.2 LUFS   │
└──────────────────────────────────────────────────────────────┘
```

### 5.3. Multi-Window Support

```typescript
class WindowManager {
  // Create a new OS window for a panel
  void createWindow(panelId: string, options: WindowOptions);
  
  // Close a window
  void closeWindow(windowId: string);
  
  // Move panel between windows
  void moveToWindow(panelId: string, windowId: string);
  
  // Window options
  interface WindowOptions {
    title: string;
    x: number;
    y: number;
    width: number;
    height: number;
    minimizable: boolean;
    resizable: boolean;
  }
  
  // Monitor info
  struct Monitor {
    id: string;
    name: string;
    bounds: Rect;
    scaleFactor: number;
    isPrimary: boolean;
  }
  Monitor[] monitors();
}
```

---

## 6. Event System

### 6.1. Event Types

```typescript
interface UIEvent {
  type: string;
  target: string;          // Component ID
  timestamp: number;
  propagationStopped: boolean;
  
  stopPropagation(): void;
}

interface MouseEvent extends UIEvent {
  x: number;
  y: number;
  button: 0 | 1 | 2;       // Left, Middle, Right
  buttons: number;           // Bitmask
  modifiers: Modifiers;
  clicks: number;            // Click count (1=single, 2=double, etc.)
  deltaX: number;
  deltaY: number;
  deltaZ: number;
}

interface KeyEvent extends UIEvent {
  key: string;               // 'a', 'Enter', 'Escape', etc.
  code: string;              // 'KeyA', 'Enter', etc.
  repeat: boolean;
  modifiers: Modifiers;
}

interface WheelEvent extends UIEvent {
  deltaX: number;
  deltaY: number;
  deltaZ: number;
  deltaMode: 'pixel' | 'line' | 'page';
}

interface TouchEvent extends UIEvent {
  touches: TouchPoint[];
  changedTouches: TouchPoint[];
  
  // Gesture recognition
  isTap: boolean;
  isDoubleTap: boolean;
  isLongPress: boolean;
  isPinch: boolean;
  isPan: boolean;
  isSwipe: boolean;
}

interface Modifiers {
  shift: boolean;
  ctrl: boolean;
  alt: boolean;
  meta: boolean;
}
```

### 6.2. Event Propagation

```typescript
// Events flow from root to target (capture) and back (bubble):
//
// Root (capture) → Container → Panel → Button (target)
//                                                    ↓
// Root (bubble)  ← Container ← Panel ← Button

class EventDispatcher {
  dispatch(event: UIEvent): void;
  
  // Add event listener
  addListener(componentId: string,
              eventType: string,
              handler: (event: UIEvent) => void,
              phase: 'capture' | 'bubble'): void;
  
  // Remove event listener
  removeListener(componentId: string, eventType: string): void;
  
  // Hit testing: find component at position
  hitTest(x: number, y: number): Component | null;
}
```

---

## 7. Animation Engine

### 7.1. Animation System

```typescript
class AnimationEngine {
  // Create an animation
  animate(options: AnimationOptions): AnimationHandle;
  
  // Easing functions
  static easings: Record<string, (t: number) => number>;
  
  // Built-in animations
  fadeIn(component: Component, duration?: number): AnimationHandle;
  fadeOut(component: Component, duration?: number): AnimationHandle;
  slideIn(component: Component, direction: Direction): AnimationHandle;
  scaleIn(component: Component, from?: number): AnimationHandle;
}

interface AnimationOptions {
  target: string;           // Component ID
  property: string;         // 'opacity', 'x', 'y', 'scale', 'color', etc.
  from: number | Color;
  to: number | Color;
  duration: number;         // Milliseconds
  easing: string;            // 'ease-out', 'ease-in-out', 'bounce', etc.
  delay?: number;
  loop?: boolean;
  yoyo?: boolean;           // Reverse on completion
  onComplete?: () => void;
}
```

### 7.2. Animation Timing

```typescript
// Standard animation durations and easings:

const Animations = {
  // Hover effects: fast and subtle
  hover: {
    duration: 100,       // ms
    easing: 'ease-out',
  },
  
  // Click/press feedback
  press: {
    duration: 80,
    easing: 'ease-in',
  },
  
  // Panel open/close
  panel: {
    duration: 200,
    easing: 'ease-out',
  },
  
  // Dialog/modal
  modal: {
    duration: 300,
    easing: 'ease-out-back',  // Overshoot slightly
  },
  
  // Value change (meter, fader)
  value: {
    duration: 150,
    easing: 'ease-out',
  },
  
  // Scroll momentum
  scroll: {
    duration: 500,       // Decay time
    easing: 'ease-out-quart',
  },
  
  // Drag and drop
  drag: {
    duration: 200,
    easing: 'ease-out',
  },
};
```

### 7.3. GPU-Accelerated Animations

```typescript
// All animations run on the GPU:
// - Transform (position, scale, rotation) → GPU vertex shader
// - Opacity → GPU fragment shader (alpha blend)
// - Color transitions → GPU fragment shader (lerp)
// - Blur/glow → GPU compute shader (separated blur)
//
// CPU is never involved in animation frame updates
// after the animation is started.
//
// Animations are declared declaratively:

<Button
  style={{
    transition: {
      property: 'background-color',
      duration: 100,
      easing: 'ease-out',
    },
    hover: {
      backgroundColor: '#444',   // Animates from #333
    },
  }}
>
```

---

## 8. Input System

### 8.1. Input Manager

```typescript
class InputManager {
  // Mouse
  onMouseDown(event: RawMouseEvent): void;
  onMouseMove(event: RawMouseEvent): void;
  onMouseUp(event: RawMouseEvent): void;
  onWheel(event: RawWheelEvent): void;
  
  // Keyboard
  onKeyDown(event: RawKeyEvent): void;
  onKeyUp(event: RawKeyEvent): void;
  
  // Touch
  onTouchStart(event: RawTouchEvent): void;
  onTouchMove(event: RawTouchEvent): void;
  onTouchEnd(event: RawTouchEvent): void;
  
  // Pen/Tablet
  onPenDown(event: RawPenEvent): void;
  onPenMove(event: RawPenEvent): void;
  onPenUp(event: RawPenEvent): void;
  
  // Cursor management
  setCursor(cursor: CursorType): void;
  CursorType = 'default' | 'pointer' | 'text' | 'grab' | 'grabbing'
              | 'crosshair' | 'move' | 'not-allowed'
              | 'ew-resize' | 'ns-resize' | 'nesw-resize' | 'nwse-resize';
  
  // Gesture recognition
  readonly gestures: GestureRecognizer;
}
```

### 8.2. Keyboard Shortcut System

```typescript
class ShortcutManager {
  // Register a shortcut
  register(shortcut: Shortcut, handler: () => void): void;
  
  // Unregister
  unregister(shortcut: Shortcut): void;
  
  // Check if shortcut is pressed (for display)
  isPressed(shortcut: Shortcut): boolean;
  
  // Get shortcut for action (for help/display)
  getShortcut(action: string): Shortcut | null;
  
  // Conflict resolution
  // - Context-aware (different shortcuts in different panels)
  // - Most specific context wins
  // - User can override any shortcut
  
  // Context stack
  pushContext(context: string): void;  // 'arrangement', 'piano-roll', 'mixer'
  popContext(): void;
}

interface Shortcut {
  key: string;
  ctrl?: boolean;
  shift?: boolean;
  alt?: boolean;
  meta?: boolean;
  context?: string;       // Restrict to context
}
```

---

## 9. Focus & Keyboard Navigation

### 9.1. Focus Manager

```typescript
class FocusManager {
  // Current focused component
  current: Component | null;
  
  // Focus a component
  focus(component: Component): void;
  
  // Blur
  blur(): void;
  
  // Tab navigation
  focusNext(): void;
  focusPrevious(): void;
  
  // Focusable components register with tabIndex
  // Tab order is DOM order (customizable via tabIndex prop)
  
  // Focus ring (visible indicator)
  showFocusRing: boolean;
  
  // Keyboard navigation within a component
  // Arrow keys, Enter, Escape handled per-component
}

// All interactive components support:
// - Tab to focus
// - Enter/Space to activate
// - Escape to cancel
// - Arrow keys for navigation within groups
```

---

## 10. Component Library

### 10.1. Core Components

```typescript
// All built-in components follow the same pattern:

// Layout
Container, FlexRow, FlexColumn, Spacer, Divider, ScrollView, ZStack

// Input
Button, ToggleButton, IconButton, TextInput, Dropdown, Slider,
Knob, RotaryEncoder, Switch, Checkbox, RadioButton, ColorPicker,
NumberInput, SearchField

// Display
Text, Label, Icon, Image, Tooltip, Badge, ProgressBar, Spinner,
MeterBar, WaveformDisplay, SpectrogramDisplay

// Feedback
Toast, Notification, Dialog, ConfirmDialog, Modal, Popover,
ContextMenu, MenuBar, DropdownMenu

// Navigation
Tabs, TabPanel, Accordion, Breadcrumb, Pagination

// DAW-specific
ChannelStrip, Fader, PanKnob, MeterBar, Timeline, PianoKey,
NoteBlock, AutomationPoint, ClipBlock, Marker, Playhead
```

### 10.2. Theming Support

```typescript
// Every component reads from the current theme:

interface ComponentStyle {
  // Colors (from theme)
  backgroundColor: Color;
  textColor: Color;
  accentColor: Color;
  borderColor: Color;
  
  // Typography (from theme)
  fontFamily: string;
  fontSize: number;
  fontWeight: number;
  lineHeight: number;
  
  // Spacing (from theme)
  padding: Spacing;
  margin: Spacing;
  gap: number;
  
  // Effects
  borderRadius: number;
  boxShadow: Shadow;
  opacity: number;
  
  // Animation
  transition: Transition;
}
```

---

## 11. API Reference

### 11.1. Public API

```typescript
class UIEngine {
  // Initialize rendering
  init(canvas: HTMLCanvasElement | GPUSwapChain): void;
  
  // Root component
  render(root: Component): void;
  
  // Frame
  requestAnimationFrame(): void;
  
  // Layout
  layout: LayoutEngine;
  
  // Docking
  docking: DockManager;
  
  // Events
  events: EventDispatcher;
  
  // Animations
  animations: AnimationEngine;
  
  // Input
  input: InputManager;
  
  // Shortcuts
  shortcuts: ShortcutManager;
  
  // Focus
  focus: FocusManager;
  
  // Windows
  windows: WindowManager;
  
  // Theme
  setTheme(theme: Theme): void;
  theme: Theme;
}
```

---

## 12. RFC Index

| RFC | Component | Status |
|---|---|---|
| RFC-UI-001 | Render Pipeline (Input→Update→Layout→Paint→Render) | 🔲 Pending |
| RFC-UI-002 | Component Model & Virtual DOM | 🔲 Pending |
| RFC-UI-003 | Flexbox Layout Engine | 🔲 Pending |
| RFC-UI-004 | GPU-Accelerated Layout Computation | 🔲 Pending |
| RFC-UI-005 | Docking System (Panels, Split, Tab, Float) | 🔲 Pending |
| RFC-UI-006 | Multi-Window Support | 🔲 Pending |
| RFC-UI-007 | Event System (Capture/Bubble) | 🔲 Pending |
| RFC-UI-008 | Animation Engine (Easing, GPU) | 🔲 Pending |
| RFC-UI-009 | Input Manager (Mouse/Key/Touch/Pen) | 🔲 Pending |
| RFC-UI-010 | Keyboard Shortcut System | 🔲 Pending |
| RFC-UI-011 | Focus & Tab Navigation | 🔲 Pending |
| RFC-UI-012 | Core Component Library | 🔲 Pending |
| RFC-UI-013 | ScrollView & Virtual Scrolling | 🔲 Pending |
| RFC-UI-014 | Drag & Drop System | 🔲 Pending |
| RFC-UI-015 | Context Menu System | 🔲 Pending |
| RFC-UI-016 | Dialog & Modal System | 🔲 Pending |
| RFC-UI-017 | Accessibility & Screen Readers | 🔲 Pending |
| RFC-UI-018 | TypeScript→C++ FFI Bridge | 🔲 Pending |
| RFC-UI-019 | Component DevTools & Inspector | 🔲 Pending |
| RFC-UI-020 | Reactivity & State Management | 🔲 Pending |

---

## Appendix A: Performance Budget

| Phase | 144 FPS (6.94ms) | 60 FPS (16.67ms) |
|---|---|---|
| Input processing | 0.5ms | 1ms |
| State updates | 1ms | 3ms |
| Layout computation | 1ms | 3ms |
| Paint tree generation | 2ms | 5ms |
| GPU submission | 2ms | 4ms |
| **Total** | **6.5ms** | **16ms** |

## Appendix B: Component Count Budget

| Scenario | Components | FPS |
|---|---|---|
| Empty project | ~200 | 144 |
| Production project (32 tracks) | ~1500 | 144 |
| Large project (100 tracks) | ~4000 | 60+ |
| Max complexity | ~10000 | 30+ |
