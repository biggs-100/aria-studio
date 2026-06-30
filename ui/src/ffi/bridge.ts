// ── TypeScript ↔ C++ FFI Bridge ─────────────────────────────────────
//
// Bridges TypeScript UI component declarations to C++ draw commands
// and routes input events from C++ back to TypeScript handlers.
//
// Design:
//   - serialize(component): converts a virtual component tree to a
//     flat array of DrawCommands.
//   - execute(commands): sends the command array to C++ via the FFI
//     JSON buffer.
//   - onEvent(callback): registers a handler for C++ input events.

import type { DrawCommand, EventHandler, InputEvent } from './types.js';

// ── VNode (virtual component node) ─────────────────────────────────

/** Minimal virtual component node for serialisation. */
export interface VNode {
  type: string;
  /** Maps to C++ WidgetID for dirty-subtree tracking and event routing. */
  key: string;
  props: Record<string, unknown>;
  children?: VNode[];
  /** Event handlers keyed by event type (e.g. "mousedown", "click"). */
  eventHandlers?: Record<string, (e: InputEvent) => void>;
}

// ── Bridge ─────────────────────────────────────────────────────────

/**
 * TypeScript FFI bridge.
 *
 * Manages the command pipeline from TS components to C++ SkiaCanvas
 * and input event routing from C++ back to TS handlers.
 */
export class Bridge {
  private pendingCommands: DrawCommand[] = [];
  private eventHandler: EventHandler | null = null;
  private eventQueue: ReturnType<typeof setTimeout> | null = null;

  // ── Widget registry (P10b) ────────────────────────────────────
  /** Maps C++ WidgetID → Component for event routing. */
  readonly widgetMap: Map<number /* WidgetID */, unknown> = new Map();

  // ── Serialization ─────────────────────────────────────────────

  /**
   * Serialize a virtual component tree into a flat array of draw commands.
   * This is a simplified recursive walk. In P10b this becomes a full
   * vdom diff/patch system.
   */
  serialize(component: VNode): DrawCommand[] {
    const commands: DrawCommand[] = [];
    this.serializeNode(component, commands);
    return commands;
  }

  private serializeNode(node: VNode, out: DrawCommand[]): void {
    switch (node.type) {
      case 'container':
        this.serializeContainer(node, out);
        break;
      case 'rect':
        this.serializeRect(node, out);
        break;
      case 'text':
        this.serializeText(node, out);
        break;
      case 'button':
        this.serializeButton(node, out);
        break;
      default:
        // Unknown node type — skip, but still recurse children
        break;
    }

    // Recurse children
    if (node.children) {
      for (const child of node.children) {
        this.serializeNode(child, out);
      }
    }
  }

  private serializeContainer(node: VNode, out: DrawCommand[]): void {
    const { x = 0, y = 0, w = 100, h = 100, clip = true } = node.props;

    // Save state, apply clip rect, draw background
    if (clip) {
      out.push({ type: 'save' });
      out.push({ type: 'clip_rect', x: x as number, y: y as number, w: w as number, h: h as number });
    }
  }

  private serializeRect(node: VNode, out: DrawCommand[]): void {
    const {
      x = 0, y = 0, w = 50, h = 50,
      fillR = 1, fillG = 1, fillB = 1, fillA = 1,
      strokeR = 0, strokeG = 0, strokeB = 0, strokeA = 0,
      strokeWidth = 0, cornerRadius = 0,
    } = node.props;

    out.push({
      type: 'draw_rect',
      x: x as number, y: y as number, w: w as number, h: h as number,
      r: fillR as number, g: fillG as number, b: fillB as number, a: fillA as number,
      sr: strokeR as number, sg: strokeG as number, sb: strokeB as number, sa: strokeA as number,
      stroke_width: strokeWidth as number,
      corner_radius: cornerRadius as number,
    });
  }

  private serializeText(node: VNode, out: DrawCommand[]): void {
    const {
      text = '',
      x = 0, y = 0,
      r = 1, g = 1, b = 1, a = 1,
      fontFamily = 'sans-serif',
      fontSize = 14,
    } = node.props;

    if (!text) return;

    out.push({
      type: 'draw_text',
      text: text as string,
      x: x as number, y: y as number,
      r: r as number, g: g as number, b: b as number, a: a as number,
      font_family: fontFamily as string,
      font_size: fontSize as number,
    });
  }

  private serializeButton(node: VNode, out: DrawCommand[]): void {
    const {
      x = 0, y = 0, w = 80, h = 30,
      label = 'Button',
      bgR = 0.24, bgG = 0.24, bgB = 0.24, bgA = 1,
      textR = 1, textG = 1, textB = 1, textA = 1,
      fontSize = 14,
      cornerRadius = 4,
    } = node.props;

    // Background rect
    out.push({
      type: 'draw_rect',
      x: x as number, y: y as number, w: w as number, h: h as number,
      r: bgR as number, g: bgG as number, b: bgB as number, a: bgA as number,
      corner_radius: cornerRadius as number,
    });

    // Label text (centered approximately)
    out.push({
      type: 'draw_text',
      text: label as string,
      x: (x as number) + 8,
      y: (y as number) + (h as number) / 2 + (fontSize as number) / 3,
      r: textR as number, g: textG as number, b: textB as number, a: textA as number,
      font_size: 14,
    });
  }

  // ── Execution ─────────────────────────────────────────────────

  /**
   * Queue commands for the next C++ execution batch.
   * Multiple calls within the same microtask are batched into one
   * JSON buffer to reduce FFI overhead.
   */
  execute(commands: DrawCommand[]): void {
    if (commands.length === 0) return;

    this.pendingCommands.push(...commands);

    // Schedule execution on next microtask
    if (!this.eventQueue) {
      this.eventQueue = setTimeout(() => {
        this.flushPending();
      }, 0);
    }
  }

  /**
   * Immediately serialise and execute a component tree.
   * Convenience wrapper around serialize + execute.
   */
  render(component: VNode): void {
    const commands = this.serialize(component);
    this.execute(commands);
  }

  /**
   * Flush pending commands to C++ via the JSON FFI buffer.
   * In the browser, this calls the native C++ Bridge::execute.
   * In Node/headless, it logs the JSON.
   */
  private flushPending(): void {
    if (this.pendingCommands.length === 0) return;

    const json = JSON.stringify(this.pendingCommands);

    // Dispatch to C++ FFI
    // In the browser environment, this calls the global __aria_ffi_execute
    // function installed by the C++ runtime.
    if (typeof (globalThis as Record<string, unknown>).__aria_ffi_execute === 'function') {
      ((globalThis as Record<string, unknown>).__aria_ffi_execute as (json: string) => void)(json);
    } else {
      // Fallback for headless/dev: log the buffer
      console.debug('[ARIA FFI] Execute:', json.substring(0, 200) + (json.length > 200 ? '...' : ''));
    }

    this.pendingCommands = [];
    this.eventQueue = null;
  }

  // ── Dirty-tree serialization (P10b) ───────────────────────────

  /**
   * Serialize only dirty subtrees from a VNode tree.
   * Walks the VNode tree, calling serialize() on each dirty subtree root.
   * Clean branches produce zero commands — their state remains on the GPU surface.
   */
  serializeDirty(root: VNode, dirtyIds: Set<number>): DrawCommand[] {
    const commands: DrawCommand[] = [];
    this._collectDirtyVNodes(root, dirtyIds, commands);
    return commands;
  }

  /**
   * Walk VNode tree collecting dirty subtree roots.
   * When a node's key is in dirtyIds, serialize its full subtree and stop descending.
   */
  private _collectDirtyVNodes(node: VNode, dirtyIds: Set<number>, out: DrawCommand[]): void {
    const id = parseInt(node.key, 10);
    if (dirtyIds.has(id)) {
      // Serialize this full dirty subtree using existing recursive serialization
      const subtreeCommands = this.serialize(node);
      for (let i = 0; i < subtreeCommands.length; i++) {
        out.push(subtreeCommands[i]);
      }
      return;
    }

    // Not dirty — recurse children looking for dirty subtrees
    if (node.children) {
      for (let i = 0; i < node.children.length; i++) {
        this._collectDirtyVNodes(node.children[i], dirtyIds, out);
      }
    }
  }

  // ── Event routing (P10b) ──────────────────────────────────────

  /**
   * Route an input event to the component associated with the widget ID.
   * Called by the C++ EventDispatcher when it produces a hit-test result.
   */
  routeEvent(event: InputEvent): void {
    // Route to widget-specific component if registered
    const component = this.widgetMap.get(event.widget_id);

    // Always forward to global event handler
    if (this.eventHandler) {
      this.eventHandler(event);
    }
  }

  // ── Event handling ────────────────────────────────────────────

  /** Register an input event handler called by C++ EventDispatcher. */
  onEvent(handler: EventHandler): void {
    this.eventHandler = handler;

    // Install the global callback for C++ to call
    if (typeof globalThis !== 'undefined') {
      (globalThis as Record<string, unknown>).__aria_ffi_on_event =
        (json: string) => {
          if (this.eventHandler) {
            const event = JSON.parse(json);
            this.eventHandler(event);
          }
        };
    }
  }

  /** Remove the registered event handler. */
  offEvent(): void {
    this.eventHandler = null;
    if (typeof globalThis !== 'undefined') {
      delete (globalThis as Record<string, unknown>).__aria_ffi_on_event;
    }
  }
}
