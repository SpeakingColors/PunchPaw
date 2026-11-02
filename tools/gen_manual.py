#!/usr/bin/env python3
"""Build docs/manual.html and docs/manual/tour.png from the real window screenshot and the mockup
layout data. Run from the repo root after refreshing docs/screenshot.png. CI turns manual.html into
a PDF with headless Chromium. Needs Pillow."""
import json, html
from PIL import Image, ImageDraw, ImageFont

L = json.load(open('docs/mockups/layout.json'))
RIG = 96
ox, oy = L['topAbs'][0], L['topAbs'][1]
shot = Image.open('docs/screenshot.png').convert('RGB')
S = shot.width / 1200.0

def top(name):
    for t in L['top']:
        if t.get('aria') == name or t.get('t') == name: return t
def trect(t): return (t['x'] + ox, t['y'] + oy, t['w'], t['h'])

# (number, title, target rect in window units, range, default, text)
tips = {t['aria']: t['tip'] for t in L['top'] if t['c'] == 'led-hit'}
pb = [t['tip'] for t in L['top'] if t['c'] == 'pbtn']
ladder = [t for t in L['top'] if t['c'].startswith('led') and not t['c'].startswith('led-hit')][:16]
lx0, lx1 = ladder[0]['x'] + ox, ladder[-1]['x'] + ox + 12
R = lambda k: tuple(L[k])
rows = [
 ('Preset bar', R('pName'), 'Arrows step through presets. The name opens the preset browser (search, type chips, load, export, edit, delete). Double-click the name to rename one of your own. Save stores the current settings as your own preset and asks before overwriting.', '', 'Start Here 5555'),
 ('Bypass', R('bBypass'), 'Hear the dry signal to compare with the processed sound (shortcut B). The gain reduction meter keeps moving, like the hardware. Switching is crossfaded, no clicks.', 'on / off', 'off'),
 ('Guide', R('bGuide'), 'Short guide and keyboard shortcuts.', '', ''),
 ('Settings', R('bSettings'), 'Info bubbles, power saving, colour look (A neon navy, B purple soda pop, C charcoal) and the detector tap.', '', 'Look C, bubbles on, tap after Input'),
 ('About & Support', R('bAbout'), 'Version, licence, where your presets live, optional Ko-fi link.', '', ''),
 ('Gain reduction ladder', (lx0, ladder[0]['y'] + oy - 6, lx1 - lx0, 24), 'Sixteen LEDs, 1 to 26 dB of gain reduction. Green up to 6 dB, yellow 7 to 10, orange 12 to 14, pink from 17.', '0 to 26 dB', ''),
 ('Ratio', (L['top'][0]['x'] + ox + 8, 118 + oy - 16, 560, 80), 'Eight ratios: ' + '; '.join(tips['Ratio ' + n] for n in ['1:1', '2:1', '3:1', '4:1', '6:1', '10:1', '20:1', 'Nuke']) + ' Click an LED or its label, or use the button: ' + pb[0], '1:1 to Nuke', '6:1'),
 ('THD level', (655 + ox - 52 + 52, 16 + oy - 4, 110, 100), 'Two indicators (not buttons). 1% THD lights when the added distortion reaches about 1%. Redline lights at about 3% THD and also when the output clips. Both look at the input stage and the distortion stages.', '', ''),
 ('Detector', (775 + ox - 52 + 52 - 4, 16 + oy - 4, 130, 130), 'What the detector listens to. ' + ' '.join([tips['Detector HP'], tips['Detector Band'], tips['Detector Link']]) + ' Button: ' + pb[1], 'HP, Band, Link', 'Link on'),
 ('Audio', (895 + ox - 4, 16 + oy - 4, 130, 130), ' '.join([tips['Audio HP'], tips['Audio Dist 2'], tips['Audio Dist 3']]) + ' Button: ' + pb[2], 'Norm, HP, Dist 2, Dist 3', 'Norm'),
 ('Transfer curve', R('cvCurve'), 'Input level (across) against output level (up). The curve follows Ratio, Input and Output. The dot is the live signal.', '-60 to +6 dB across', ''),
 ('Detector and audio filters', R('cvFilt'), 'Solid line: what the detector listens to (high-pass and band emphasis). Dashed pink: the audio high-pass.', '20 Hz to 20 kHz', ''),
 ('Gain reduction history', R('cvHist'), 'How much the compressor turned the signal down, over the last six seconds.', '0 to 30 dB', ''),
 ('Input', tuple(L['kcells'][0]['knob']), 'Input gain and threshold together. Drive harder for more compression and more colour.', '0 to +30 dB (knob 0 to 10)', 'knob 5 = +15 dB'),
 ('Attack', tuple(L['kcells'][1]['knob']), 'How fast compression grabs. 0 is fastest (50 microseconds), 10 is slowest (30 ms). Slow it down to keep drum punch. At 2:1, 3:1 and 4:1 the attack is a little faster than the knob says (our choice).', '50 us to 30 ms', 'knob 5 = 1.2 ms'),
 ('Release', tuple(L['kcells'][2]['knob']), 'How long compression lets go. 0 is fastest (50 ms), 10 is slowest (3.5 s). In 10:1 Opto it stretches with the depth of compression, up to about 20 s.', '50 ms to 3.5 s', 'knob 5 = 418 ms'),
 ('Output', tuple(L['kcells'][3]['knob']), 'Output level after the compressor. 5 is 0 dB. Level Match overrides it while on.', '-24 to +24 dB', 'knob 5 = 0 dB'),
 ('Headroom (HR)', tuple(L['kcells'][4]['btn']), 'As in the hardware manual: higher dB pushes the circuit into more processing and colour, lower dB keeps it cleaner. Only matters with Dist 2 or Dist 3. Click to step up, shift-click to step down, double-click to reset.', '4 to 28 dB in 4 dB steps', '16 dB'),
 ('Mix', tuple(L['kcells'][5]['knob']), 'Blend of the dry signal and the processed signal for parallel compression. Click DRY or COMP under the knob to jump to either end.', '0 to 100 %', '100 %'),
 ('In and Out meters', (L['cvIn'][0], L['cvIn'][1], 60, L['cvIn'][3]), 'Peak level, -60 to +6 dB. Blue up to -12, purple up to 0, pink above. The strip top marks a clip until you click it.', '-60 to +6 dB', ''),
 ('Auto Trim', tuple(L['mbtn'][0]['r']), 'If the output ever reaches 0 dB, the output is turned down by that overshoot plus 0.1 dB and stays there. It never turns back up by itself. The readout turns pink once it has trimmed. Click twice (off, on) to reset.', 'on / off', 'off'),
 ('Level Match', tuple(L['mbtn'][1]['r']), 'Slowly adjusts the output so the processed sound is as loud as the dry sound, for a fair comparison (louder always sounds better). Overrides the Output knob while on. Turn it off when you are done comparing.', 'on / off', 'off'),
]
# tour image: badge anchors in window units (centre of each badge), chosen not to cover text
def cap(k): c = L[k]; return (c[0] + c[2] - 12, c[1] + 12)
def lab(i): c = L['kcells'][i]['labR']; return (c[0] - 14, c[1] + 9)
def hb(k): r = L[k]; return (r[0] + r[2] / 2, r[1] - 1)
lad_y = ladder[0]['y'] + oy + 6
anchors = [hb('pName'), hb('bBypass'), hb('bGuide'), hb('bSettings'), hb('bAbout'),
           (60, lad_y), (62, 118 + oy), (694, 69), (814, 69), (934, 69),
           cap('cvCurve'), cap('cvFilt'), cap('cvHist'), lab(0), lab(1), lab(2), lab(3), lab(4), lab(5),
           (1140, 61), (L['mbtn'][0]['r'][0] + 2, L['mbtn'][0]['r'][1] + 2), (L['mbtn'][1]['r'][0] + 2, L['mbtn'][1]['r'][1] + 2)]
assert len(anchors) == len(rows)
img = shot.copy(); d = ImageDraw.Draw(img)
font = ImageFont.truetype('assets/fonts/BarlowCondensed-Bold.ttf', int(13 * S))
for i, (cx0, cy0) in enumerate(anchors, 1):
    cx, cy, rad = cx0 * S, cy0 * S, 8 * S
    d.ellipse((cx - rad, cy - rad, cx + rad, cy + rad), fill=(2, 235, 186), outline=(0, 0, 0), width=max(1, int(S)))
    w = d.textlength(str(i), font=font)
    d.text((cx - w / 2, cy - 8 * S), str(i), fill=(11, 11, 13), font=font)
img.save('docs/manual/tour.png')

def crop(name, box):
    shot.crop(tuple(int(v * S) for v in box)).save('docs/manual/%s.png' % name)
crop('plate', (52, 46, 1056, 198)); crop('displays', (52, 198, 1056, 326)); crop('knobs', (52, 326, 1056, 518)); crop('meters', (1056, 46, 1148, 518))

e = html.escape
table = '\n'.join('<tr><td>%d</td><th>%s</th><td>%s</td><td>%s</td><td>%s</td></tr>' % (i, e(n), e(t), e(rg), e(df)) for i, (n, r, t, rg, df) in enumerate(rows, 1))

page = '''<!doctype html>
<html lang="en"><head><meta charset="utf-8"><title>Purrrstressor manual</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
:root{--bg:#0b0b0d;--card:#16171a;--line:#33363d;--ink:#e8e9ee;--dim:#a9adb8;--teal:#02EBBA;--pink:#FF7BB1}
@media print{:root{--bg:#fff;--card:#f4f4f6;--line:#c9cbd3;--ink:#16171a;--dim:#4a4d55;--teal:#007a62;--pink:#b0114f}}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--ink);font:16px/1.5 "Barlow Condensed","Arial Narrow",Arial,sans-serif;padding:24px max(16px,env(safe-area-inset-left,0px))}
main{max-width:980px;margin:0 auto}
h1{font-size:40px;margin:0 0 4px;font-style:italic;letter-spacing:.5px}
h2{font-size:26px;margin:36px 0 8px;color:var(--teal);letter-spacing:1px;text-transform:uppercase;break-after:avoid}
h3{font-size:20px;margin:20px 0 4px}
a{color:var(--pink)}
img{max-width:100%;height:auto;border:1px solid var(--line);border-radius:6px;display:block}
table{border-collapse:collapse;width:100%;font-size:15px;display:block;overflow-x:auto}
td,th{border-bottom:1px solid var(--line);padding:5px 8px;text-align:left;vertical-align:top}
th{white-space:nowrap}
code,kbd{font-family:"JetBrains Mono",Consolas,monospace;font-size:13px;background:var(--card);border:1px solid var(--line);border-radius:3px;padding:0 4px}
.note{background:var(--card);border-left:3px solid var(--teal);padding:8px 12px;margin:12px 0}
.dim{color:var(--dim)}
@media print{body{padding:0}h2{break-before:auto}tr{break-inside:avoid}img{break-inside:avoid}}
</style></head><body><main>
<h1>Purrrstressor</h1>
<p class="dim">Manual for version @VER@. Studio 87. Free and open source (AGPL-3.0-or-later). Collects nothing, never uses the network.</p>
<p>A cat-themed compressor in the style of the Empirical Labs Distressor: our own DSP, written from the public manuals.</p>
<img src="manual/tour.png" alt="The Purrrstressor window with numbered controls">

<h2>Quick start</h2>
<ol>
<li>Put Purrrstressor on a track. The first preset, <b>Start Here 5555</b>, is 6:1 with every knob at 5.</li>
<li>Raise <b>Input</b> until the gain reduction ladder moves. Ratio and Release matter most.</li>
<li>Compare with <b>Bypass</b> (B). Turn on <b>Level Match</b> for a fair comparison, and off again when done.</li>
<li>For parallel compression, lower <b>Mix</b>. For colour, pick Dist 2 or Dist 3 and change Headroom.</li>
</ol>

<h2>Signal flow</h2>
<p>Input amp, Input level, gain stage, Distortion, Audio high-pass, Output level, Output amp. The detector (high-pass and band emphasis) steers the gain stage. By default it listens just after Input, which is stable at every setting. In Settings you can move it after the distortion, as in the hardware diagram; that is a feedback loop, so very fast attacks at 20:1 and Nuke become a little slower.</p>
<p>The input and output amps are slightly non-linear and their amount follows Headroom. The added distortion is anti-aliased (ADAA), so there is no oversampling and no latency.</p>

<h2>Interface tour</h2>
@TOUR@
<table><tr><th>#</th><th>Control</th><th>What it does</th><th>Range</th><th>Default</th></tr>
@TABLE@
</table>

<h2>Presets</h2>
<p>14 factory presets are built in. Your own presets are plain text files you can back up or share:</p>
<ul><li>Linux: <code>~/.local/share/Studio 87/Purrrstressor/presets</code></li><li>Windows: <code>%APPDATA%\\Studio 87\\Purrrstressor\\presets</code></li><li>macOS: <code>~/Library/Application Support/Studio 87/Purrrstressor/presets</code></li></ul>
<p>Export and Import in the preset browser use plain text you can paste anywhere. A preset does not change Bypass. The loaded preset name and the plugin settings (look, bubbles, power saving, detector tap, window size) are saved with the project.</p>

<h2>Keyboard and mouse</h2>
<table><tr><td><kbd>B</kbd></td><td>Bypass on or off</td></tr><tr><td><kbd>R</kbd> / <kbd>Shift+R</kbd></td><td>Next or previous ratio</td></tr><tr><td><kbd>D</kbd></td><td>Cycle detector modes (Norm, HP, Band, HP+Band, Link, Link+HP, Link+Band, all)</td></tr><tr><td><kbd>A</kbd></td><td>Cycle audio modes (Norm, HP, Dist 2, Dist 2+HP, Dist 3, Dist 3+HP)</td></tr><tr><td>Knobs</td><td>Drag up or down, mouse wheel, arrow keys. Hold Shift for fine steps. Double-click to reset.</td></tr><tr><td>Window</td><td>Drag any edge or corner. It keeps its proportions. Double-click the wood to go back to 100%.</td></tr></table>
<p class="dim">Keys work while the plugin window has keyboard focus; some hosts keep the focus for themselves.</p>

<h2>Install and uninstall</h2>
<ul>
<li><b>Linux:</b> unpack the zip, run <code>install.sh</code> (copies the VST3 to <code>~/.vst3</code>). <code>uninstall.sh</code> removes it and keeps your presets unless you answer yes.</li>
<li><b>Windows:</b> run the installer. It puts the VST3 in the common VST3 folder and adds an uninstaller. Close your DAW first. Installing a new version over the old one works.</li>
<li><b>macOS:</b> open the package. The plugin goes to <code>/Library/Audio/Plug-Ins/VST3</code>. The build is not notarised, so macOS may ask you to allow it in System Settings, Privacy and Security.</li>
</ul>
<p>After installing, rescan plugins in your host.</p>

<h2>Under the hood</h2>
<p><b>From the public manuals:</b> eight ratios (1:1, 2:1, 3:1, 4:1, 6:1, 10:1 Opto, 20:1, Nuke); attack 50 us to 30 ms; release 50 ms to 3.5 s (Opto up to about 20 s); detector high-pass 100 Hz at 6 dB per octave; detector band emphasis around 6 kHz; audio high-pass 18 dB per octave Bessel, about -3 dB at 60 Hz; Headroom 4 to 28 dB in 4 dB steps; the THD LEDs and Redline at about 3%; the signal order; Dist 2 mostly 2nd harmonic and Dist 3 mostly 3rd.</p>
<p><b>Our own choices (the manuals give no numbers):</b> the knee width of each ratio, the shape of the Opto release curve, how much each distortion adds, the size of the +9 dB band boost, the amount of non-linearity in the input and output amps, the faster attack at 2:1 to 4:1, the loudness averaging of Level Match and the 0.1 dB step of Auto Trim. These are set by ear and may change in later versions; your saved projects keep loading.</p>
<p><b>Not modelled:</b> the exact Link behaviour of the hardware in mono, and the way the hardware shifts the threshold with each ratio.</p>

<h2>Licence and thanks</h2>
<p>AGPL-3.0-or-later. Built with JUCE (AGPLv3). Fonts: Barlow Condensed and JetBrains Mono, SIL Open Font Licence 1.1 (texts in the source folder <code>assets/fonts</code>). Source: <a href="https://github.com/SpeakingColors/Purrrstressor">github.com/SpeakingColors/Purrrstressor</a>. Optional support: <a href="https://ko-fi.com/studio87">ko-fi.com/studio87</a>.</p>
</main></body></html>
'''
ver = '0.1.0'
tour = '<p class="dim">Numbers in the picture match the table.</p>'
open('docs/manual.html', 'w').write(page.replace('@VER@', ver).replace('@TABLE@', table).replace('@TOUR@', tour))
print('manual ok', len(rows), 'controls')
