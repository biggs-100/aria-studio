// ── DockManager — Layout Tree Management ─────────────────────────────
//
// Component that owns the docking layout tree, panel registry, and
// all docking operations (open, close, float, reattach, save, load).
//
// Design:
//   - Extends Component; layout tree lives in state
//   - Tree mutations call setState → dirty propagation → re-render
//   - render() traverses the DockNode tree and produces VNodes
//   - Panel registry maps type strings to factory functions
//   - saveLayout() produces plain DockLayout JSON
//   - loadLayout() merges and validates incoming layout
//   - Missing/unregistered panel types get replaced with 'placeholder'

import type { VNode } from '../ffi/bridge.js';
import { Component } from '../framework/component.js';
import type { DockLayout, DockLocation, DockNode, PanelFactory } from './types.js';
import { DEFAULT_SPLIT_RATIO } from './types.js';

// ── State interface ─────────────────────────────────────────────────

export interface DockState {
  layout: DockLayout;
}

// ── DockManager ─────────────────────────────────────────────────────

export class DockManager extends Component<{}, DockState> {
  /** Panel registry: maps panel type key → factory function. */
  private registry: Map<string, PanelFactory> = new Map();
  /** Counter for generating stable node IDs. */
  private _nextId = 1;

  constructor() {
    super({});
    this.state = {
      layout: {
        root: { id: 'root', type: 'leaf' },
        floating: [],
      },
    };
  }

  // ── Panel Registry ────────────────────────────────────────────

  /**
   * Register a panel type with a factory function.
   * The factory receives the panelId and returns a type key
   * that the Bridge serialiser can handle.
   */
  registerPanel(type: string, factory: PanelFactory): void {
    this.registry.set(type, factory);
  }

  /**
   * Look up whether a panel type is registered.
   * Returns true if the type exists in the registry.
   */
  hasPanel(type: string): boolean {
    return this.registry.has(type);
  }

  // ── Tree Generation ───────────────────────────────────────────

  /** Generate a unique node ID. */
  private _genId(prefix: string = 'n'): string {
    return `${prefix}-${this._nextId++}`;
  }

  /** Deep-clone a layout tree (avoids mutation of previous state). */
  private _cloneTree(node: DockNode): DockNode {
    return JSON.parse(JSON.stringify(node));
  }

  /** Deep-clone an entire layout. */
  private _cloneLayout(layout: DockLayout): DockLayout {
    return {
      root: this._cloneTree(layout.root),
      floating: layout.floating.map(f => this._cloneTree(f)),
    };
  }

  // ── openPanel ─────────────────────────────────────────────────

  /**
   * Open a panel at the specified location.
   *
   * Behaviour by location:
   * - 'center': places the panel in the focused area.
   *   Empty leaf → fill. Occupied leaf → convert to tab group.
   *   Tab → append. Split → add to the last child.
   * - 'left' | 'right' | 'top' | 'bottom': wraps current root in a
   *   new split, placing the new panel at the specified side.
   */
  openPanel(panelId: string, location: DockLocation): void {
    const layout = this._cloneLayout(this.state.layout);

    if (location === 'center') {
      this._openCenter(layout, panelId);
    } else {
      this._openSide(layout, panelId, location);
    }

    this.setState({ layout } as Partial<DockState>);
  }

  /** Handle center placement. */
  private _openCenter(layout: DockLayout, panelId: string): void {
    const root = layout.root;

    if (root.type === 'leaf' && !root.panelType) {
      // Empty leaf — place panel directly
      root.panelType = panelId;
    } else if (root.type === 'leaf' && root.panelType) {
      // Occupied leaf — convert to tab group
      const existingType = root.panelType;
      root.type = 'tab';
      root.tabs = [existingType, panelId];
      root.activeTab = root.tabs.length - 1;
      delete root.panelType;
    } else if (root.type === 'tab') {
      // Tab group — append
      root.tabs!.push(panelId);
      root.activeTab = root.tabs!.length - 1;
    } else if (root.type === 'split') {
      // Split — add to the last child (or the focused one)
      const lastIdx = root.children!.length - 1;
      const last = root.children![lastIdx];
      this._addToSplitChild(root, last, panelId, lastIdx);
    }
  }

  /** Add a panel to a child within a split container. */
  private _addToSplitChild(
    _split: DockNode,
    child: DockNode,
    panelId: string,
    _index: number,
  ): void {
    if (child.type === 'leaf') {
      if (!child.panelType) {
        child.panelType = panelId;
      } else {
        // Convert to tab
        const existingType = child.panelType;
        child.type = 'tab';
        child.tabs = [existingType, panelId];
        child.activeTab = child.tabs.length - 1;
        delete child.panelType;
      }
    } else if (child.type === 'tab') {
      child.tabs!.push(panelId);
      child.activeTab = child.tabs!.length - 1;
    } else if (child.type === 'split') {
      // Recurse into deepest split child
      const lastIdx = child.children!.length - 1;
      this._addToSplitChild(child, child.children![lastIdx], panelId, lastIdx);
    }
  }

  /** Handle side placement (left/right/top/bottom). */
  private _openSide(layout: DockLayout, panelId: string, location: DockLocation): void {
    const direction: 'horizontal' | 'vertical' =
      (location === 'left' || location === 'right') ? 'horizontal' : 'vertical';

    const newLeaf: DockNode = {
      id: this._genId('leaf'),
      type: 'leaf',
      panelType: panelId,
    };

    const currentRoot = this._cloneTree(layout.root);

    // Ensure sizes are initialised
    const sizes = [DEFAULT_SPLIT_RATIO, DEFAULT_SPLIT_RATIO];

    if (location === 'left' || location === 'top') {
      layout.root = {
        id: this._genId('split'),
        type: 'split',
        direction,
        children: [newLeaf, currentRoot],
        sizes,
      };
    } else {
      layout.root = {
        id: this._genId('split'),
        type: 'split',
        direction,
        children: [currentRoot, newLeaf],
        sizes,
      };
    }
  }

  // ── closePanel ────────────────────────────────────────────────

  /**
   * Close a panel by removing it from the layout tree.
   * Cleans up: empty tabs become empty leafs; single-child splits collapse.
   */
  closePanel(panelId: string): void {
    const layout = this._cloneLayout(this.state.layout);

    this._removeFromTree(layout.root, panelId);

    // Clean up floating windows too
    layout.floating = layout.floating.filter(fw => fw.panelType !== panelId);

    this.setState({ layout } as Partial<DockState>);
  }

  /**
   * Recursively find and remove a panel from the tree.
   * Returns true if the panel was found and removed.
   */
  private _removeFromTree(node: DockNode, panelId: string): boolean {
    if (node.type === 'leaf') {
      if (node.panelType === panelId) {
        node.panelType = undefined;
        return true;
      }
      return false;
    }

    if (node.type === 'tab' && node.tabs) {
      const idx = node.tabs.indexOf(panelId);
      if (idx !== -1) {
        node.tabs.splice(idx, 1);
        // Clean up tab group after removal
        this._cleanupTab(node);
        return true;
      }
      return false;
    }

    if (node.type === 'split' && node.children) {
      for (let i = 0; i < node.children.length; i++) {
        if (this._removeFromTree(node.children[i], panelId)) {
          // Clean up split after child removal — filter empty leafs
          this._cleanupSplit(node);
          return true;
        }
      }
    }

    return false;
  }

  /**
   * Clean up a tab node after panel removal:
   * - 0 panels → convert to empty leaf
   * - 1 panel → convert to leaf
   * - 2+ panels → adjust activeTab if needed
   */
  private _cleanupTab(node: DockNode): void {
    if (!node.tabs || node.tabs.length === 0) {
      node.type = 'leaf';
      delete node.tabs;
      delete node.activeTab;
      node.panelType = undefined;
    } else if (node.tabs.length === 1) {
      const remaining = node.tabs[0];
      node.type = 'leaf';
      node.panelType = remaining;
      delete node.tabs;
      delete node.activeTab;
    } else if (node.activeTab !== undefined && node.activeTab >= node.tabs.length) {
      node.activeTab = node.tabs.length - 1;
    }
  }

  /**
   * Clean up a split node after child removal:
   * - Filter out empty leafs
   * - If 1 child remains → collapse split into that child's type
   * - If 0 children → convert to empty leaf
   */
  private _cleanupSplit(node: DockNode): void {
    if (!node.children) return;

    // Remove empty leaf nodes
    node.children = node.children.filter(c => !(c.type === 'leaf' && !c.panelType));

    if (node.children.length === 0) {
      // No children left — become empty leaf
      node.type = 'leaf';
      node.panelType = undefined;
      delete node.children;
      delete node.direction;
      delete node.sizes;
    } else if (node.children.length === 1) {
      // Single child — collapse the split into the child's configuration
      const child = node.children[0];
      node.type = child.type;
      node.tabs = child.tabs;
      node.activeTab = child.activeTab;
      node.children = child.children;
      node.direction = child.direction;
      node.sizes = child.sizes;

      // Only leaf nodes carry panelType
      if (child.type === 'leaf') {
        node.panelType = child.panelType;
      } else {
        delete (node as { panelType?: string }).panelType;
      }
    }
  }

  // ── Float / Reattach ──────────────────────────────────────────

  /**
   * Float a panel by moving it from the main tree to the floating array.
   */
  floatPanel(panelId: string, position: { x: number; y: number; w: number; h: number }): void {
    const layout = this._cloneLayout(this.state.layout);

    // Remove from main tree
    this._removeFromTree(layout.root, panelId);

    // Add to floating windows
    layout.floating.push({
      id: panelId,
      type: 'float',
      panelType: panelId,
      x: position.x,
      y: position.y,
      w: position.w,
      h: position.h,
    });

    this.setState({ layout } as Partial<DockState>);
  }

  /**
   * Reattach a floating window back into the main layout.
   */
  reattachPanel(panelId: string, location: DockLocation): void {
    const layout = this._cloneLayout(this.state.layout);

    // Find and remove from floating
    const fwIdx = layout.floating.findIndex(fw => fw.panelType === panelId);
    if (fwIdx === -1) return;

    layout.floating.splice(fwIdx, 1);

    // Open back in the main tree
    if (location === 'center') {
      this._openCenter(layout, panelId);
    } else {
      this._openSide(layout, panelId, location);
    }

    this.setState({ layout } as Partial<DockState>);
  }

  // ── Serialization ─────────────────────────────────────────────

  /**
   * Save the current layout as a plain DockLayout object.
   * Safe for JSON.stringify() output.
   */
  saveLayout(): DockLayout {
    return this._cloneLayout(this.state.layout);
  }

  /**
   * Load a layout from a DockLayout object.
   * Validates and fixes: missing panel types get replaced with 'placeholder'.
   */
  loadLayout(layout: DockLayout): void {
    // Clone to avoid external mutation
    const loaded = this._cloneLayout(layout);

    // Validate panels — replace missing/unregistered types with placeholder
    this._validatePanels(loaded.root);
    for (const fw of loaded.floating) {
      if (fw.panelType && !this.registry.has(fw.panelType)) {
        fw.panelType = 'placeholder';
      }
    }

    this.setState({ layout: loaded } as Partial<DockState>);
  }

  /** Recursively validate panel types in the tree. */
  private _validatePanels(node: DockNode): void {
    if (node.type === 'leaf') {
      if (node.panelType && !this.registry.has(node.panelType)) {
        node.panelType = 'placeholder';
      }
    } else if (node.type === 'tab' && node.tabs) {
      // Tab panels don't have panelType — they're referenced by tabs array
      // Validation at the leaf level already covers panel types
    } else if (node.type === 'split' && node.children) {
      for (const child of node.children) {
        this._validatePanels(child);
      }
    }
    // Float nodes are validated at the caller level
  }

  // ── Rendering ─────────────────────────────────────────────────

  /**
   * Render the layout tree as a VNode tree for bridge serialisation.
   * Recursively walks the DockNode tree producing container VNodes.
   */
  render(): VNode {
    const layout = this.state.layout;

    // Render main tree
    const mainChildren = this._renderNode(layout.root);

    // Render floating windows as an overlay
    const floatChildren: VNode[] = [];
    for (const fw of layout.floating) {
      floatChildren.push(this._renderFloatNode(fw));
    }

    const children: VNode[] = [...mainChildren, ...floatChildren];

    return {
      type: 'container',
      key: String(this.widgetId),
      props: { x: 0, y: 0, w: '100%', h: '100%' },
      children,
    };
  }

  /** Render a single DockNode (leaf, tab, or split) into VNode children. */
  private _renderNode(node: DockNode): VNode[] {
    if (node.type === 'leaf') {
      const type = node.panelType || 'empty-panel';
      return [{
        type,
        key: String(this._genId('leaf')),
        props: {
          panelType: node.panelType,
          x: 0, y: 0, w: '100%', h: '100%',
        },
      }];
    }

    if (node.type === 'tab' && node.tabs) {
      // Render tab headers + active panel
      const children: VNode[] = [];

      // Tab header bar
      children.push({
        type: 'container',
        key: String(this._genId('tab-header')),
        props: { x: 0, y: 0, w: '100%', h: 30 },
        children: node.tabs.map((tabId, i) => ({
          type: 'button',
          key: String(this._genId('tab')),
          props: {
            label: tabId,
            x: i * 100, y: 0, w: 100, h: 30,
            active: i === node.activeTab,
          },
        })),
      });

      // Active panel content
      if (node.activeTab !== undefined && node.activeTab < node.tabs.length) {
        const activeId = node.tabs[node.activeTab];
        children.push({
          type: activeId,
          key: String(this._genId('panel')),
          props: { x: 0, y: 30, w: '100%', h: 'calc(100% - 30px)' },
        });
      }

      return [{
        type: 'container',
        key: String(this._genId('tab-group')),
        props: { x: 0, y: 0, w: '100%', h: '100%' },
        children,
      }];
    }

    if (node.type === 'split' && node.children) {
      // Render split with children side by side or stacked
      return node.children.map((child, _i) => ({
        type: 'container',
        key: String(this._genId('split-child')),
        props: {
          x: node.direction === 'horizontal'
            ? `${(_i / node.children!.length) * 100}%` : 0,
          y: node.direction === 'vertical'
            ? `${(_i / node.children!.length) * 100}%` : 0,
          w: node.direction === 'horizontal'
            ? `${100 / node.children!.length}%` : '100%',
          h: node.direction === 'vertical'
            ? `${100 / node.children!.length}%` : '100%',
        },
        children: this._renderNode(child),
      }));
    }

    return [];
  }

  /** Render a floating window node as an overlay. */
  private _renderFloatNode(node: DockNode): VNode {
    return {
      type: 'container',
      key: String(this._genId('float')),
      props: {
        x: node.x || 100,
        y: node.y || 100,
        w: node.w || 400,
        h: node.h || 300,
      },
      children: [
        {
          type: 'container',
          key: String(this._genId('float-title')),
          props: { x: 0, y: 0, w: '100%', h: 24 },
          children: [{
            type: 'text',
            key: String(this._genId('title-text')),
            props: {
              text: node.panelType || 'Floating Panel',
              x: 8, y: 16,
              r: 1, g: 1, b: 1, a: 1,
              fontSize: 13,
            },
          }],
        },
        {
          type: node.panelType || 'empty-panel',
          key: String(this._genId('float-content')),
          props: { x: 0, y: 24, w: '100%', h: 'calc(100% - 24px)' },
        },
      ],
    };
  }
}
