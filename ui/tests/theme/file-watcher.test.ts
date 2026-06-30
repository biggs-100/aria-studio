// FileWatcher — Unit Tests
import { describe, it, expect, vi } from 'vitest';
import { FileWatcher } from '../../src/theme/file-watcher.js';

describe('FileWatcher', () => {
  it('creates a FileWatcher instance', () => {
    const watcher = new FileWatcher();
    expect(watcher).toBeInstanceOf(FileWatcher);
  });

  it('watch() does not throw for a valid path', () => {
    const watcher = new FileWatcher();
    // In a test environment, this won't actually start watching
    // since we're not in Node.js (jsdom), but it shouldn't throw.
    expect(() => watcher.watch('/tmp/test-theme.json')).not.toThrow();
  });

  it('onChange() registers a callback', () => {
    vi.useFakeTimers();
    const watcher = new FileWatcher();
    const cb = vi.fn();
    watcher.onChange(cb);
    (watcher as any).watching = true;

    (watcher as any).handleChange('/tmp/test-theme.json');
    // handleChange uses setTimeout (200ms debounce) — advance past it
    vi.advanceTimersByTime(250);
    expect(cb).toHaveBeenCalledWith('/tmp/test-theme.json');
    vi.useRealTimers();
  });

  it('debounce prevents multiple rapid callbacks', () => {
    vi.useFakeTimers();

    const watcher = new FileWatcher();
    const cb = vi.fn();
    watcher.onChange(cb);
    (watcher as any).watching = true;

    (watcher as any).handleChange('/tmp/test-theme.json');
    (watcher as any).handleChange('/tmp/test-theme.json');
    (watcher as any).handleChange('/tmp/test-theme.json');

    vi.advanceTimersByTime(300);
    expect(cb).toHaveBeenCalledTimes(1);

    vi.useRealTimers();
  });

  it('stop() cancels watching', () => {
    const watcher = new FileWatcher();
    expect(() => watcher.stop()).not.toThrow();
  });
});
