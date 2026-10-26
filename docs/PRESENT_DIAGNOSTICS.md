# Opt-in present diagnostics, v1

This addition measures the existing implementation. It does not change the
refresh clock, interpolation, rendering choices, input, guest execution, or
save format. Leave `THIRD_AGE_DIAGNOSTIC_TRACE` unset for normal behavior.
The existing `THIRD_AGE_PRESENT_TRACE` CSV is unchanged.

## Capture

Set `THIRD_AGE_PRESENT_HZ=165` and
`THIRD_AGE_DIAGNOSTIC_TRACE=/absolute/path/diagnostic.csv` for the supported
Third Age windowed high-refresh path. Use `GBARECOMP_FRAME_PHASE` with an
explicit CSV path for exclusive host-service accounting. Existing cadence and
guest-state profilers can run alongside it. Exit normally to write the ring.
No per-frame file operations, flushes, image readbacks, or GPU queries are added.

The diagnostic retains the last 65,536 presents, about 397 seconds at 165 Hz.
The frame-phase ring retains 16,384 guest frames, about 274 seconds at GBA rate.
Capture for under four minutes to retain complete correlated captures. Shutdown
logs give retained/total/dropped present rows and report write failures.
Abnormal termination can lose the in-memory trace. All measurement adds some
CPU overhead; compare diagnostic-on/off runs under the same conditions.

## Clocks, identity, and boundaries

All `*_ns` timestamps use the process steady-clock epoch, shared with the guest
profile `wall_ns`. Do not compare them to civil time or assume SDL performance
counter ticks have the same units. All durations/ages are nanoseconds. IDs and
reason masks are integers; positions/velocities use native expanded-world pixels
(and pixels/second). No column measures physical scanout, VRR, GPU completion,
or input-to-photon latency.

- `sequence`: 1-based present sequence, independent of guest frame and ring wrap.
- `guest_frame`: newest received complete-image frame. `base_source_frame` is
  the last successfully uploaded base image. `camera_source_frame` is the last
  available completed-world image and is valid only with `camera_available=1`.
- `source_sequence`: increments for each new preparation, including repeated
  paused frames. Deduplicate preparation metrics by this value rather than
  assuming each repeated present repeats work.
- `origin`: 0 new/repeated-image submission, 1 guest execution service,
  2 pacer-wait service.
- `deadline_ns`: deadline before this present; zero means no prior deadline.
  `period_ns` is the existing integer-divided period (6,060,606 ns at 165 Hz).
  `lateness_ns` is max(0, start minus deadline), zero for the first anchor.
  `missed_periods` is lateness divided by period, not a count of frames dropped
  by the GPU. `clock_action`: 0 disabled, 1 first anchor, 2 normal advancement,
  3 late resync (the unchanged clock's strict lateness > period condition).
  `resync_count` is cumulative within the capture, excludes the first anchor.
- `refresh_start_ns`: after source preparation, before draw work.
  `present_entry_ns`/`present_exit_ns` surround SDL_RenderPresent.
  `refresh_end_ns`: immediately after present/cadence instrumentation, before
  diagnostic ring recording, legacy trace IO and FPS-title bookkeeping.
- `source_completed_ns`: native latched-copy/render finished before any early
  pacer wait. It is retained separately for the currently received image while
  a future completed image waits. `source_complete_age_ns` is age at redraw
  start; this measures CPU image availability, not the original raster's exact
  last-pixel time. Zero timestamp/age means unavailable.
- `source_received_ns`: HostWindow preparation began. `source_age_ns` is age
  since that receipt at redraw start, distinct from completed-image age.
- `base_uploaded_ns` and `camera_uploaded_ns`: successful upload completion
  timestamps, with corresponding `*_upload_age_ns`. A failed upload preserves
  the previous successful timestamp; gate camera use with camera_available and
  camera_drawn, and inspect base_upload_error/base_source_frame. Zero timestamps
  produce zero age, never a huge inferred age.

Preparation boundaries, repeated per source_sequence:

1. source_received_ns → portrait_observe_end_ns: portrait identification/setup
2. portrait_observe_end_ns → base_upload_entry_ns: frame blending, portrait
   preparation and base color grading
3. base_upload_entry_ns → base_upload_exit_ns: base SDL_UpdateTexture
4. camera_upload_entry_ns → camera_upload_exit_ns: camera upload method,
   including its CPU copies, grading, layer preparation and SDL texture calls
5. source_received_ns → preparation_end_ns: aggregate preparation

The tiny base-exit to camera-entry gap contains only measurement bookkeeping.
Camera upload can return immediately when there is no pending camera source.
No claim is made that the whole camera-upload interval is driver blocking.

## Motion validity

`camera_sim_x/y` are authoritative camera inputs for the most recent completed
image, not an extra mid-guest memory read. `camera_target_x/y` are the camera
image tween target. `camera_raw_dx/dy` are unsnapped offsets actually used by
that draw; `camera_snapped_dx/dy` are its rounded destination displacement.
`camera_displayed_x/y = camera_target - camera_snapped_offset`.

`unit_target_x/y` are the completed cutout's world position. `unit_raw_dx/dy`
are its clamped tween offset. `unit_snapped_dx/dy` are the separate rounded
unit displacement from the actual sprite/world destination rectangles.
`unit_displayed_x/y = unit_target + unit_snapped_offset` are world coordinates.
`unit_total_dx/dy` include both camera and unit displacement on screen.

All drawn camera positions/offsets/alpha, scroll metrics and unit positions are
zero when camera_drawn=0; zero is then missing, not a measured origin. Unit
metrics additionally require unit_layer=1. Do not attribute stale tween values
or a missing layer to motion. Layout width/height and renderer scale explain
native-pixel to SDL-output mapping. `*_alpha` is in [0,1], `*_duration_ns` is the
current tween duration. `scroll_vx/vy`, effective period and queue depth apply
only while continuous_scroll=1. `mouse_owned` and `edge_panning` identify
ownership/input state; motion can still stop at map boundaries or because the
native image has not advanced. A tween stall code does not prove continuous
scroll reset.

## Failure and reset codes

`camera_blocked` is the existing scene-safety bitmask: 1 no ready scene,
2 incompatible backgrounds, 4 combat, 8 dialog, 16 black palette, 32 mosaic,
64 incompatible blend. It is separate from SDL failures.

`camera_upload_errors` bits: 1 base camera texture allocation; 2 weather texture
allocation; 4 weather upload; 8 weather foreground upload; 16 unit texture
allocation; 32 unit upload; 64 unit foreground upload; 128 world upload;
256 HUD upload. Values describe the latest pending upload attempt. Zero means
no observed failure, not that every optional layer was eligible.

`camera_draw_errors` bits: 1 unavailable image, 2 invalid rectangle,
4 clipped out, 8 backing copy, 16 older copy, 32 previous copy, 64 world copy,
128 unit copy, 256 unit foreground copy, 512 weather copy,
1024 weather foreground copy, 2048 HUD copy, 4096 unsupported SDL version.
They describe this attempted draw and are zero when the camera draw is skipped.
Observations do not change which copy errors the original renderer ignores.

`camera_reset_flags` bits for the most recent submission/upload: 1 source absent,
2 same-frame resubmission, 4 changed scene key, 8 frame gap, 16 arrival stall,
32 large camera jump, 64 resource release, 128 weather upload failed,
256 world/HUD upload failed. They are causal observations, not cumulative event
counts; repeated presents can repeat the same flag.

Existing camera/unit tween reasons: 0 reset, 1 first sample/cut, 2 gap,
3 arrival stall, 4 jump, 5 stationary, 6 moving. `unit_motion_reason` is the
unchanged upstream cutout eligibility diagnostic, separate from tween reason.

## Exclusive service accounting

Only when diagnostic tracing is enabled does `GBARECOMP_FRAME_PHASE` append:
`phase_start_ns`, `phase_end_ns`, `guest_interval_valid`, `guest_interval_ns`,
`guest_service_ns`, `guest_exclusive_ns`, `pacer_service_ns`.
Its original eight columns and meanings remain unchanged. With diagnostics
unset, its original header and rows are byte-format compatible.

The guest interval runs from the preceding phase exit to this phase entry.
Guest service includes event servicing and cooperative redraw within that
interval. Exclusive guest = max(0, interval minus service). It still includes
all other work in that interval and is not pure CPU emulation instruction time.
Pacer service is host-service time called from waits since the previous phase
exit; it overlaps the existing pacer duration and must not be added to it.
The first guest interval is invalid/zero because there is no prior exit.
Two extra steady-clock reads per service callback are enabled only for this
opt-in accounting. No service frequency or callback order is changed.

## Validation

Run `python3 tests/test-present-diagnostics.py --baseline /path/to/baseline
--output /path/to/test-artifacts`. It checks unchanged deterministic SDL call
order, uploaded texture bytes and destination geometry against baseline headers,
snap validity, selected failure bits, no early ring write, bounded wrap order,
legacy frame-phase header and service subtraction with first-interval gating.
A full game regression and real SDL trace are additional integration tests;
these unit fixtures do not establish physical 165 Hz presentation.
