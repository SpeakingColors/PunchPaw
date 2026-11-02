# PunchPaw

A free, open source, cat-themed kick and drum limiter/compressor (VST3, Linux first): threshold, compression and output gain with program-dependent timing, plus optional manual attack and release.

<p align="center"><img src="docs/screenshot.png" width="900" alt="PunchPaw plugin window"></p>
<p align="center"><sub>The real plugin window, rendered offscreen from the compiled code.</sub></p>

## Status
Version 0.1.0 is a test build: it compiles and passes the automated tests on Linux and Windows and pluginval, but it has not yet been checked by ear in real hosts or on real Windows and macOS machines. See [ROADMAP.md](ROADMAP.md) for what is verified and what is not.

## Download
Get the zip from the [latest release](https://github.com/SpeakingColors/PunchPaw/releases). It holds installers for Windows (.exe), macOS (.pkg) and Linux (Flatpak extension and a plain .vst3), the PDF manual with an interface tour, INSTALL.txt and SHA256SUMS. Everything works offline.

The installers are not code-signed or notarised, because certificates cost money and this is a free project. Windows may show "unknown publisher" (More info, then Run anyway) and macOS may ask you to allow it in Privacy and Security. This only means "not signed by a known company". The source is open and SHA256SUMS lets you check your download.

## Controls
Threshold, Ratio, Output (the classic three), plus our additions: Knee (hard or soft), SC High-pass, Mix, Ceiling and a Timing switch (Auto, or Manual with Attack and Release). Presets: 20 factory presets, your own saved presets, export and import. Every control has a hover info bubble. The interactive design mockup is in `docs/mockup/`.

## Privacy
Collects nothing. No network access.

## Support
<p align="center">
  <a href="https://ko-fi.com/studio87"><img src="https://img.shields.io/badge/Support_Studio_87-on_Ko--fi-FF5E5B?style=for-the-badge&logo=kofi&logoColor=white" alt="Support Studio 87 on Ko-fi"></a>
</p>

Free and open source, made by one person. If it helps your music, a coffee on [Ko-fi](https://ko-fi.com/studio87) keeps it coming.

## License
AGPL-3.0-or-later, see [LICENSE](LICENSE).
