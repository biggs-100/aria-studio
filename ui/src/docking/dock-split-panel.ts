// ── DockSplitPanel — Divider Drag Resize ──────────────────────────────
//
// Component that renders a horizontal or vertical split with a draggable
// divider. Drag-to-resize is implemented via calculateResize.
//
// Design:
//   - Extends Component; tracks drag state
//   - render() produces container + divider VNodes
//   - calculateResize is a pure function (exported for testing)
//
// Usage:
//   DockSplitPanel is mounted by DockManager as a child for 'split' nodes.
//   The divider is a thin rect with mousedown/mousemove/mouseup handlers.

import type { VNode } from '../ffi/bridge.js';
import { Component } from '../framework/component.js';
import type { InputEvent } from '../ffi/types.js';
import { DEFAULT_MIN_SIZE } from './types.js';

// ── Exported pure function ───────────────────────────────────────────

/**
 * Calculate new panel sizes after dragging a divider.
 *
 * @param totalSize - Total available size (width or height) in pixels.
 * @param currentSizes - Current sizes of all panels.
 * @param dividerIndex - Index of the divider (between panel[i] and panel[i+1]).
 * @param delta - Signed pixel delta (negative = left/up, positive = right/down).
 * @param minSize - Minimum size per panel (defaults to 100).
 * @returns New array of sizes after applying the drag and enforcing min-size.
 */
export function calculateResize(
  totalSize: number,
  currentSizes: number[],
  dividerIndex: number,
  delta: number,
  minSize: number = DEFAULT_MIN_SIZE,
): number[] {
  if (currentSizes.length < 2) {
    return [...currentSizes];
  }

  const sizes = [...currentSizes];
  const leftIdx = dividerIndex;
  const rightIdx = dividerIndex + 1;

  if (rightIdx >= sizes.length) {
    return sizes;
  }

  // Calculate how much we can actually move.
  // Negative delta (drag left/up) shrinks the left panel, grows the right.
  // Positive delta (drag right/down) shrinks the right panel, grows the left.
  let actualDelta = delta;

  // Left panel should not go below minSize
  const maxLeftShrink = sizes[leftIdx] - minSize;
  if (actualDelta < -maxLeftShrink) {
    actualDelta = -maxLeftShrink;
  }

  // Right panel should not go below minSize
  const maxRightShrink = sizes[rightIdx] - minSize;
  if (actualDelta > maxRightShrink) {
    actualDelta = maxRightShrink;
  }

  // Apply the delta
  sizes[leftIdx] += actualDelta;
  sizes[rightIdx] -= actualDelta;

  return sizes;
}

// ── DockSplitPanel Component ─────────────────────────────────────────

export interface DockSplitPanelProps {
  direction: 'horizontal' | 'vertical';
  sizes: number[];
  minSize?: number;
  totalSize: number;
  onResize?: (sizes: number[]) => void;
}

export interface DockSplitPanelState {
  isDragging: boolean;
  dragIndex: number;
  dragStartSizes: number[];
  dragStartPos: number;
}

const DIVIDER_SIZE = 4;

export class DockSplitPanel extends Component<DockSplitPanelProps, DockSplitPanelState> {
  constructor(props: DockSplitPanelProps) {
    super(props);
    this.state = {
      isDragging: false,
      dragIndex: -1,
      dragStartSizes: [],
      dragStartPos: 0,
    };
  }

  /** Handle divider mousedown — start drag. */
  onDividerMouseDown(index: number, event: InputEvent): void {
    const primarySize = this.props.direction === 'horizontal' ? event.x : event.y;
    this.setState({
      isDragging: true,
      dragIndex: index,
      dragStartSizes: [...this.props.sizes],
      dragStartPos: primarySize,
    } as Partial<DockSplitPanelState>);
  }

  /** Handle mousemove on the panel root — update resize during drag. */
  onPanelMouseMove(event: InputEvent): void {
    if (!this.state.isDragging) return;

    const primarySize = this.props.direction === 'horizontal' ? event.x : event.y;
    const delta = primarySize - this.state.dragStartPos;

    const newSizes = calculateResize(
      this.props.totalSize,
      this.state.dragStartSizes,
      this.state.dragIndex,
      delta,
      this.props.minSize || DEFAULT_MIN_SIZE,
    );

    if (this.props.onResize) {
      this.props.onResize(newSizes);
    }
  }

  /** Handle mouseup — end drag. */
  onPanelMouseUp(): void {
    if (!this.state.isDragging) return;

    this.setState({
      isDragging: false,
      dragIndex: -1,
      dragStartSizes: [],
      dragStartPos: 0,
    } as Partial<DockSplitPanelState>);
  }

  /** Handle mouseleave on the panel root — end drag if dragging. */
  onPanelMouseLeave(): void {
    if (this.state.isDragging) {
      this.onPanelMouseUp();
    }
  }

  render(): VNode {
    const { direction, sizes, totalSize } = this.props;
    const childCount = sizes.length;
    const isHorizontal = direction === 'horizontal';

    // Calculate pixel sizes from ratios, accounting for dividers
    const dividerTotalWidth = (childCount - 1) * DIVIDER_SIZE;
    const available = totalSize - dividerTotalWidth;
    const sizeSum = sizes.reduce((a, b) => a + b, 0);
    const pixelSizes = sizes.map(s => Math.round((s / sizeSum) * available));

    const children: VNode[] = [];
    let offset = 0;

    for (let i = 0; i < childCount; i++) {
      // Panel content area
      children.push({
        type: 'container',
        key: `panel-${this.widgetId}-${i}`,
        props: {
          x: isHorizontal ? offset : 0,
          y: isHorizontal ? 0 : offset,
          w: isHorizontal ? pixelSizes[i] : '100%',
          h: isHorizontal ? '100%' : pixelSizes[i],
        },
      });

      offset += pixelSizes[i];

      // Divider (between children)
      if (i < childCount - 1) {
        children.push({
          type: 'rect',
          key: `divider-${this.widgetId}-${i}`,
          props: {
            x: isHorizontal ? offset : 0,
            y: isHorizontal ? 0 : offset,
            w: isHorizontal ? DIVIDER_SIZE : '100%',
            h: isHorizontal ? '100%' : DIVIDER_SIZE,
            fillR: 0.3, fillG: 0.3, fillB: 0.3, fillA: 1,
            strokeWidth: 0,
          },
          eventHandlers: {
            mousedown: (e: InputEvent) => this.onDividerMouseDown(i, e),
          },
        });

        offset += DIVIDER_SIZE;
      }
    }

    return {
      type: 'container',
      key: String(this.widgetId),
      props: { x: 0, y: 0, w: '100%', h: '100%' },
      children,
      eventHandlers: {
        mousemove: (e: InputEvent) => this.onPanelMouseMove(e),
        mouseup: () => this.onPanelMouseUp(),
        mouseleave: () => this.onPanelMouseLeave(),
      },
    };
  }
}
