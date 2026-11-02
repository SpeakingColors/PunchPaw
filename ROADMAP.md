# PunchPaw roadmap

## Verified (automated)
- Core tests (`sh tests/run.sh`, with address and undefined-behaviour sanitizers): knob ranges, level calibration, gain reduction timing, knob-to-sound time, fixed 32 sample latency, stability on silence, DC, full scale and NaN, bit-exact bypass.
- CI builds the VST3 on Linux and Windows, runs the plugin self tests (state round trip, old-state fixture, latency report, editor resize) and pluginval at strictness 10 on Linux.
- The window is ported natively from the approved mockup using positions and texts read from it.

## Not verified yet
- Listening tests by ear on real kicks and drum buses.
- Real hosts (Bitwig, Reaper, Ardour, Ableton Live, others) on every OS.
- The Windows installer, macOS .pkg and Uninstall app, and the Flatpak extension on real machines. The macOS build is ad-hoc signed only.
- Alias measurements of the Ceiling's oversampled clipper against other plugins; CPU load in a real project.
- HiDPI and resizing in real hosts.

## Next
- Fix whatever the first real-host tests find.
- Sidechain input from another track (external key).
- More presets for bass and parallel drum buses once listened to.
- Signed installers if funding allows.
