// ── DockTabBar — Tab Headers with Drag Reorder ────────────────────────
//
// Component that renders tab headers with labels, close buttons, drag
// reorder, and detach-to-float detection.
//
// Design:
//   - Extends Component; tracks drag state
//   - render() produces button/text VNodes for each tab
//   - reorderTabs: pure function for index-based reorder
//   - isDetachEvent: pure function for drag-outside-bounds detection
//
// Usage:
//   DockTabBar is mounted by DockManager as a child for 'tab' nodes.
//   Drag reorder works via mousedown → mousemove tracking → drop at index.

import type { VNode } from '../ffi/bridge.js';
import { Component } from '../framework/component.js';
import type { InputEvent } from '../ffi/types.js';

// ── Exported pure functions ──────────────────────────────────────────

/**
 * Reorder tabs by moving a tab from one index to another.
 * Uses splice to remove and reinsert for natural drag-and-drop ordering.
 *
 * @param tabs - Current ordered array of tab identifiers.
 * @param fromIndex - Index of the tab being dragged.
 * @param toIndex - Target index where the tab is dropped.
 * @returns New array with the tab moved to the target position.
 */
export function reorderTabs(
  tabs: string[],
  fromIndex: number,
  toIndex: number,
): string[] {
  if (fromIndex === toIndex) return [...tabs];
  if (tabs.length <= 1) return [...tabs];

  const result = [...tabs];
  const [moved] = result.splice(fromIndex, 1);
  result.splice(toIndex, 0, moved);
  return result;
}

/**
 * Check if a drag event should trigger detach-to-float.
 * A tab is considered detached when dragged outside the tab bar bounds.
 *
 * @param params.offsetX - The drag offset relative to the tab bar.
 * @param params.barWidth - Total width of the tab bar.
 * @returns true if the tab is being dragged outside the bar.
 */
export function isDetachEvent(params: {
  offsetX: number;
  barWidth: number;
}): boolean {
  return params.offsetX < 0 || params.offsetX > params.barWidth;
}

// ── DockTabBar Component ─────────────────────────────────────────────

export interface DockTabBarProps {
  tabs: string[];
  activeTab: number;
  tabWidth: number;
  barWidth: number;
  onTabSelect?: (index: number) => void;
  onTabClose?: (tabId: string) => void;
  onTabReorder?: (tabs: string[]) => void;
  onTabDetach?: (tabId: string) => void;
}

export interface DockTabBarState {
  dragIndex: number;
  dragOffsetX: number;
  isDragging: boolean;
}

const TAB_HEIGHT = 30;
const CLOSE_BTN_SIZE = 16;

export class DockTabBar extends Component<DockTabBarProps, DockTabBarState> {
  constructor(props: DockTabBarProps) {
    super(props);
    this.state = {
      dragIndex: -1,
      dragOffsetX: 0,
      isDragging: false,
    };
  }

  /** Start tab drag. */
  onTabMouseDown(index: number, _event: InputEvent): void {
    this.setState({
      dragIndex: index,
      isDragging: true,
    } as Partial<DockTabBarState>);
  }

  /** Handle mousemove for drag reorder / detach detection. */
  onTabBarMouseMove(event: InputEvent): void {
    if (!this.state.isDragging) return;

    const { tabs, tabWidth, barWidth, onTabDetach, onTabReorder } = this.props;
    const offsetX = event.x;

    this.setState({ dragOffsetX: offsetX } as Partial<DockTabBarState>);

    // Check detach
    if (onTabDetach && isDetachEvent({ offsetX, barWidth })) {
      const draggedTabId = tabs[this.state.dragIndex];
      onTabDetach(draggedTabId);
      this.setState({
        isDragging: false,
        dragIndex: -1,
      } as Partial<DockTabBarState>);
      return;
    }

    // Calculate target index from drag position
    const targetIndex = Math.min(
      Math.max(Math.floor(offsetX / tabWidth), 0),
      tabs.length - 1,
    );

    // Only fire reorder if index actually changes
    if (onTabReorder && targetIndex !== this.state.dragIndex) {
      const reordered = reorderTabs(tabs, this.state.dragIndex, targetIndex);
      onTabReorder(reordered);
      this.setState({ dragIndex: targetIndex } as Partial<DockTabBarState>);
    }
  }

  /** End tab drag. */
  onTabBarMouseUp(): void {
    if (!this.state.isDragging) return;
    this.setState({
      isDragging: false,
      dragIndex: -1,
      dragOffsetX: 0,
    } as Partial<DockTabBarState>);
  }

  /** Handle close button click. */
  onTabClose(tabId: string): void {
    if (this.props.onTabClose) {
      this.props.onTabClose(tabId);
    }
  }

  render(): VNode {
    const { tabs, activeTab, tabWidth, barWidth } = this.props;

    const tabChildren: VNode[] = [];

    for (let i = 0; i < tabs.length; i++) {
      const isActive = i === activeTab;
      const tabX = i * tabWidth;

      // Tab background rect
      tabChildren.push({
        type: 'rect',
        key: `tab-bg-${this.widgetId}-${i}`,
        props: {
          x: tabX, y: 0, w: tabWidth, h: TAB_HEIGHT,
          fillR: isActive ? 0.35 : 0.2,
          fillG: isActive ? 0.35 : 0.2,
          fillB: isActive ? 0.35 : 0.2,
          fillA: 1,
          cornerRadius: 4,
        },
        eventHandlers: {
          mousedown: (e: InputEvent) => this.onTabMouseDown(i, e),
        },
      });

      // Tab label text
      tabChildren.push({
        type: 'text',
        key: `tab-label-${this.widgetId}-${i}`,
        props: {
          text: tabs[i],
          x: tabX + 8,
          y: Math.round(TAB_HEIGHT / 2) + 5,
          r: 0.9, g: 0.9, b: 0.9, a: 1,
          fontSize: 12,
        },
      });

      // Close button
      const closeX = tabX + tabWidth - CLOSE_BTN_SIZE - 4;
      const closeY = (TAB_HEIGHT - CLOSE_BTN_SIZE) / 2;
      tabChildren.push({
        type: 'rect',
        key: `tab-close-${this.widgetId}-${i}`,
        props: {
          x: closeX, y: closeY, w: CLOSE_BTN_SIZE, h: CLOSE_BTN_SIZE,
          fillR: 0.5, fillG: 0.1, fillB: 0.1, fillA: 0.8,
          cornerRadius: 2,
        },
        eventHandlers: {
          click: () => this.onTabClose(tabs[i]),
        },
      });
    }

    return {
      type: 'container',
      key: String(this.widgetId),
      props: { x: 0, y: 0, w: barWidth, h: TAB_HEIGHT },
      children: tabChildren,
      eventHandlers: {
        mousemove: (e: InputEvent) => this.onTabBarMouseMove(e),
        mouseup: () => this.onTabBarMouseUp(),
        mouseleave: () => this.onTabBarMouseUp(),
      },
    };
  }
}
