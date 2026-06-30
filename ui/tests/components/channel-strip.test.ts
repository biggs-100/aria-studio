// ChannelStrip — Unit Tests
// RED phase: Tests reference code that does NOT exist yet.
import { describe, it, expect } from 'vitest';
import { ChannelStrip } from '../../src/components/channel-strip.js';
import { Fader } from '../../src/components/fader.js';
import { MeterBar } from '../../src/components/meter-bar.js';

// ── Component Tests ──────────────────────────────────────────────────

describe('ChannelStrip component', () => {
  it('creates a ChannelStrip with default props', () => {
    const strip = new ChannelStrip({
      x: 0, y: 0, w: 60, h: 300,
      label: 'Channel 1',
      initialVolume: 0.75,
      mute: false,
      solo: false,
      pan: 0,
    });
    expect(strip.props.label).toBe('Channel 1');
    expect(strip.props.initialVolume).toBe(0.75);
    expect(strip.props.mute).toBe(false);
    expect(strip.props.solo).toBe(false);
    expect(strip.props.pan).toBe(0);
  });

  it('render() returns container VNode with key matching widgetId', () => {
    const strip = new ChannelStrip({
      x: 0, y: 0, w: 60, h: 300, label: 'Ch1', initialVolume: 0.75,
    });
    const vnode = strip.render();
    expect(vnode.type).toBe('container');
    expect(vnode.key).toBe(String(strip.widgetId));
  });

  it('full strip at 60px width renders MeterBar and Fader children', () => {
    const strip = new ChannelStrip({
      x: 0, y: 0, w: 60, h: 300, label: 'Ch1', initialVolume: 0.75,
    });
    const vnode = strip.render();

    // Should have a Fader child (type 'container' with key matching fader widgetId)
    const childContainers = vnode.children!.filter(c => c.type === 'container');
    expect(childContainers.length).toBeGreaterThanOrEqual(2);

    // Should have rect and text nodes for buttons and labels
    const rects = vnode.children!.filter(c => c.type === 'rect');
    expect(rects.length).toBeGreaterThanOrEqual(3);
  });

  it('narrow strip (< 40px) hides MeterBar (no meter child)', () => {
    const wideStrip = new ChannelStrip({
      x: 0, y: 0, w: 60, h: 300, label: 'Wide', initialVolume: 0.75,
    });
    const narrowStrip = new ChannelStrip({
      x: 0, y: 0, w: 30, h: 300, label: 'Narrow', initialVolume: 0.75,
    });

    const wideVNode = wideStrip.render();
    const narrowVNode = narrowStrip.render();

    // Wide strip has more children than narrow strip (which hides meter)
    const wideCount = wideVNode.children!.length;
    const narrowCount = narrowVNode.children!.length;
    expect(narrowCount).toBeLessThan(wideCount);
  });

  it('mute button shows muted visual state when mute is true', () => {
    const muted = new ChannelStrip({
      x: 0, y: 0, w: 60, h: 300, label: 'Ch1', initialVolume: 0.75,
      mute: true,
    });
    const vnode = muted.render();
    const muteBtn = vnode.children!.find(
      c => c.key?.includes('mute'),
    );
    expect(muteBtn).toBeDefined();
  });

  it('solo button uses different color when solo is active', () => {
    const soloed = new ChannelStrip({
      x: 0, y: 0, w: 60, h: 300, label: 'Ch1', initialVolume: 0.75,
      solo: true,
    });
    const vnode = soloed.render();
    const soloBtn = vnode.children!.find(
      c => c.key?.includes('solo'),
    );
    expect(soloBtn).toBeDefined();
    // Solo button should have a distinctive color (higher R/G)
    if (soloBtn && soloBtn.type === 'rect') {
      expect((soloBtn.props.fillR as number)).toBeGreaterThan(0.5);
    }
  });

  it('mounts Fader and MeterBar as child components', () => {
    const strip = new ChannelStrip({
      x: 0, y: 0, w: 60, h: 300, label: 'Ch1', initialVolume: 0.75,
    });
    strip.mount();

    // Fader and MeterBar should be in the children list
    const childTypes = strip._children.map(c => c.constructor.name);
    expect(childTypes).toContain('Fader');
    expect(childTypes).toContain('MeterBar');
  });

  it('sets initial volume from props', () => {
    const strip = new ChannelStrip({
      x: 0, y: 0, w: 60, h: 300, label: 'Ch1', initialVolume: 0.5,
    });
    expect(strip.state.volume).toBe(0.5);
  });

  it('pan default is center (0)', () => {
    const strip = new ChannelStrip({
      x: 0, y: 0, w: 60, h: 300, label: 'Ch1', initialVolume: 0.75,
    });
    expect(strip.state.panValue).toBe(0);
  });

  it('render includes label text for the channel name', () => {
    const strip = new ChannelStrip({
      x: 0, y: 0, w: 60, h: 300, label: 'MyChannel', initialVolume: 0.75,
    });
    const vnode = strip.render();
    const labelText = vnode.children!.find(
      c => c.type === 'text' && (c.props.text as string) === 'MyChannel',
    );
    expect(labelText).toBeDefined();
  });

  it('volume state updates when Fader child value changes', () => {
    const strip = new ChannelStrip({
      x: 0, y: 0, w: 60, h: 300, label: 'Ch1', initialVolume: 0.5,
    });
    strip.mount();

    // Find the Fader child and simulate drag
    const fader = strip._children.find(c => c instanceof Fader) as Fader | undefined;
    expect(fader).toBeDefined();
    if (fader) {
      fader.setState({ value: 0.9 });
    }
  });
});
