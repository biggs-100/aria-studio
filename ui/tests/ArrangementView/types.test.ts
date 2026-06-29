// ArrangementView Types — Unit Tests
import { describe, it, expect } from 'vitest';
import {
  clipId,
  trackId,
  ClipData,
  SessionCaptureClipData,
  isSessionCaptureClip,
  AnyClipData,
  PPQN,
} from '../../src/views/ArrangementView/types.js';

function makeSessionCaptureClip(overrides: Partial<SessionCaptureClipData> = {}): SessionCaptureClipData {
  return {
    id: clipId(1),
    start_ppqn: 0,
    length_ppqn: PPQN * 4,
    name: 'Session Capture',
    color: 0,
    gain: 1.0,
    muted: false,
    fade_in_ppqn: 0,
    fade_out_ppqn: 0,
    fade_in_shape: 'None',
    fade_out_shape: 'None',
    looping: false,
    loop_start_ppqn: 0,
    loop_end_ppqn: 0,
    track_id: trackId(1),
    type: 'SessionCapture',
    capture_source_track_id: trackId(1),
    capture_source_scene_id: { value: 1 },
    ...overrides,
  };
}

describe('SessionCaptureClipData', () => {
  it('isSessionCaptureClip returns true for SessionCapture clips', () => {
    const clip: AnyClipData = makeSessionCaptureClip();
    expect(isSessionCaptureClip(clip)).toBe(true);
  });

  it('isSessionCaptureClip returns false for Audio clips', () => {
    const clip: AnyClipData = {
      id: clipId(1),
      start_ppqn: 0,
      length_ppqn: PPQN,
      name: 'Test',
      color: 0,
      gain: 1.0,
      muted: false,
      fade_in_ppqn: 0,
      fade_out_ppqn: 0,
      fade_in_shape: 'None',
      fade_out_shape: 'None',
      looping: false,
      loop_start_ppqn: 0,
      loop_end_ppqn: 0,
      track_id: trackId(1),
      type: 'Audio',
      file_path: '',
    };
    expect(isSessionCaptureClip(clip)).toBe(false);
  });

  it('isSessionCaptureClip returns false for MIDI clips', () => {
    const clip: AnyClipData = {
      id: clipId(1),
      start_ppqn: 0,
      length_ppqn: PPQN,
      name: 'Midi',
      color: 0,
      gain: 1.0,
      muted: false,
      fade_in_ppqn: 0,
      fade_out_ppqn: 0,
      fade_in_shape: 'None',
      fade_out_shape: 'None',
      looping: false,
      loop_start_ppqn: 0,
      loop_end_ppqn: 0,
      track_id: trackId(1),
      type: 'MIDI',
      note_density: 0.5,
    };
    expect(isSessionCaptureClip(clip)).toBe(false);
  });

  it('isSessionCaptureClip returns false for Automation clips', () => {
    const clip: AnyClipData = {
      id: clipId(1),
      start_ppqn: 0,
      length_ppqn: PPQN,
      name: 'Auto',
      color: 0,
      gain: 1.0,
      muted: false,
      fade_in_ppqn: 0,
      fade_out_ppqn: 0,
      fade_in_shape: 'None',
      fade_out_shape: 'None',
      looping: false,
      loop_start_ppqn: 0,
      loop_end_ppqn: 0,
      track_id: trackId(1),
      type: 'Automation',
      curve_points: [],
    };
    expect(isSessionCaptureClip(clip)).toBe(false);
  });

  it('capture_source_track_id and capture_source_scene_id are accessible', () => {
    const clip = makeSessionCaptureClip({
      capture_source_track_id: trackId(5),
      capture_source_scene_id: { value: 3 },
    });
    expect(clip.capture_source_track_id.value).toBe(5);
    expect(clip.capture_source_scene_id.value).toBe(3);
  });
});
