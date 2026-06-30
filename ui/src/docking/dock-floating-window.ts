// ── DockFloatingWindow — Overlay Window Container ─────────────────────
//
// Component that renders a same-window overlay (floating) panel with
// a title bar. Supports drag-to-move and attach-back detection.
//
// Design:
//   - Extends Component; tracks drag state for window movement
//   - render() produces a container overlay with title bar + content
//   - createFloatingNode pure function for creating float DockNodes
//   - isInsideWindow pure function for hit-testing (reattach detection)
//
// Usage:
//   DockFloatingWindow is mounted by DockManager from the floating[] array.
//   It renders as a positioned overlay within the same WebGPU surface.

import type { VNode } from '../ffi/bridge.js';
import { Component } from '../framework/component.js';
import type { InputEvent } from '../ffi/types.js';
import type { DockNode } from './types.js';

// ── Exported pure functions ──────────────────────────────────────────

/** Default size and position for new floating windows. */
const DEFAULT_FLOAT_X = 100;
const DEFAULT_FLOAT_Y = 100;
const DEFAULT_FLOAT_W = 400;
const DEFAULT_FLOAT_H = 300;

/**
 * Create a floating window DockNode.
 *
 * @param panelType - The type of panel to display.
 * @param x - Left position (defaults to 100).
 * @param y - Top position (defaults to 100).
 * @param w - Width (defaults to 400).
 * @param h - Height (defaults to 300).
 * @returns A DockNode with type 'float' and the given position/size.
 */
export function createFloatingNode(
  panelType: string,
  x: number = DEFAULT_FLOAT_X,
  y: number = DEFAULT_FLOAT_Y,
  w: number = DEFAULT_FLOAT_W,
  h: number = DEFAULT_FLOAT_H,
): DockNode {
  return {
    id: panelType,
    type: 'float',
    panelType,
    x,
    y,
    w,
    h,
  };
}

/**
 * Check whether a point (px, py) is inside a floating window's rectangle.
 *
 * @param node - The floating window node.
 * @param px - Point X coordinate.
 * @param py - Point Y coordinate.
 * @returns True if the point is inside the window bounds.
 */
export function isInsideWindow(node: DockNode, px: number, py: number): boolean {
  if (node.x === undefined || node.y === undefined ||
      node.w === undefined || node.h === undefined) {
    return false;
  }
  return px >= node.x && px < node.x + node.w &&
         py >= node.y && py < node.y + node.h;
}

// ── DockFloatingWindow Component ─────────────────────────────────────

export interface DockFloatingWindowProps {
  panelType: string;
  x: number;
  y: number;
  w: number;
  h: number;
  onMove?: (x: number, y: number) => void;
  onClose?: () => void;
  onReattach?: () => void;
}

export interface DockFloatingWindowState {
  isDragging: boolean;
  dragStartX: number;
  dragStartY: number;
  dragOrigX: number;
  dragOrigY: number;
}

const TITLE_BAR_HEIGHT = 24;

export class DockFloatingWindow extends Component<DockFloatingWindowProps, DockFloatingWindowState> {
  constructor(props: DockFloatingWindowProps) {
    super(props);
    this.state = {
      isDragging: false,
      dragStartX: 0,
      dragStartY: 0,
      dragOrigX: 0,
      dragOrigY: 0,
    };
  }

  /** Start dragging the floating window (from title bar). */
  onTitleMouseDown(event: InputEvent): void {
    this.setState({
      isDragging: true,
      dragStartX: event.x,
      dragStartY: event.y,
      dragOrigX: this.props.x,
      dragOrigY: this.props.y,
    } as Partial<DockFloatingWindowState>);
  }

  /** Handle mouse move to drag window. */
  onWindowMouseMove(event: InputEvent): void {
    if (!this.state.isDragging) return;

    const dx = event.x - this.state.dragStartX;
    const dy = event.y - this.state.dragStartY;

    if (this.props.onMove) {
      this.props.onMove(this.state.dragOrigX + dx, this.state.dragOrigY + dy);
    }
  }

  /** End dragging. */
  onWindowMouseUp(): void {
    if (!this.state.isDragging) return;
    this.setState({
      isDragging: false,
    } as Partial<DockFloatingWindowState>);
  }

  render(): VNode {
    const { x, y, w, h, panelType } = this.props;

    return {
      type: 'container',
      key: String(this.widgetId),
      props: { x, y, w, h },
      children: [
        // Title bar
        {
          type: 'container',
          key: `title-${this.widgetId}`,
          props: { x: 0, y: 0, w: '100%', h: TITLE_BAR_HEIGHT },
          children: [
            {
              type: 'rect',
              key: `title-bg-${this.widgetId}`,
              props: {
                x: 0, y: 0, w: '100%', h: TITLE_BAR_HEIGHT,
                fillR: 0.25, fillG: 0.25, fillB: 0.3, fillA: 1,
                cornerRadius: 4,
              },
            },
            {
              type: 'text',
              key: `title-text-${this.widgetId}`,
              props: {
                text: panelType,
                x: 8, y: Math.round(TITLE_BAR_HEIGHT / 2) + 4,
                r: 0.9, g: 0.9, b: 0.9, a: 1,
                fontSize: 12,
              },
            },
          ],
          eventHandlers: {
            mousedown: (e: InputEvent) => this.onTitleMouseDown(e),
          },
        },
        // Content area
        {
          type: panelType,
          key: `content-${this.widgetId}`,
          props: { x: 0, y: TITLE_BAR_HEIGHT, w: '100%', h: `calc(100% - ${TITLE_BAR_HEIGHT}px)` },
        },
      ],
      eventHandlers: {
        mousemove: (e: InputEvent) => this.onWindowMouseMove(e),
        mouseup: () => this.onWindowMouseUp(),
        mouseleave: () => this.onWindowMouseUp(),
      },
    };
  }
}
