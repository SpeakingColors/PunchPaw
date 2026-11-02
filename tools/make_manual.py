#!/usr/bin/env python3
"""Builds the PDF manual: text from docs/manual.txt plus an Interface tour made from docs/mockup/layout.json (positions and
info-bubble texts of the approved design) and the renders of the REAL compiled editor in docs/tour (auto.png, manual.png, made by CI).
Usage: python3 tools/make_manual.py out.pdf        Needs: pip install reportlab Pillow"""
import io, json, os, sys
from PIL import Image, ImageDraw, ImageFont
from reportlab.lib import colors
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.units import mm
from reportlab.lib.utils import ImageReader
from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, ListFlowable, ListItem, Table, TableStyle, Image as RLImage, PageBreak, CondPageBreak

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
out = sys.argv[1]
L = json.load(open(os.path.join(ROOT, 'docs/mockup/layout.json')))
CUT = 70
esc = lambda s: s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
PURPLE, PINK, TEAL, INK = colors.HexColor("#4F009A"), colors.HexColor("#C2185B"), colors.HexColor("#007A66"), colors.HexColor("#222222")
h1 = ParagraphStyle("h1", fontName="Helvetica-Bold", fontSize=22, spaceAfter=4, textColor=PURPLE)
sub = ParagraphStyle("sub", fontName="Helvetica", fontSize=10, textColor=colors.HexColor("#777777"), spaceAfter=14)
h2 = ParagraphStyle("h2", fontName="Helvetica-Bold", fontSize=14, spaceBefore=12, spaceAfter=4, textColor=PURPLE)
body = ParagraphStyle("b", fontName="Helvetica", fontSize=10.5, leading=14.5, spaceAfter=6)
CELL = ParagraphStyle("tc", fontName="Helvetica", fontSize=9, leading=12, textColor=INK)
CELLB = ParagraphStyle("tcb", fontName="Helvetica-Bold", fontSize=9, leading=12, textColor=PINK)
CELLT = ParagraphStyle("tct", parent=CELLB, textColor=TEAL)

story = [Paragraph("PunchPaw", h1), Paragraph("Manual. Free and open source by Studio 87. Support: ko-fi.com/studio87", sub)]
sections, cur = [], None
for line in open(os.path.join(ROOT, 'docs/manual.txt'), encoding="utf-8").read().splitlines():
    if line.startswith("# "): cur = [line[2:].strip(), []]; sections.append(cur)
    elif cur is not None: cur[1].append(line)
for title, lines in sections:
    story.append(Paragraph(esc(title), h2)); buf, items = [], []
    def flush():
        global buf, items
        if buf: story.append(Paragraph(esc(" ".join(buf)), body)); buf = []
        if items: story.append(ListFlowable([ListItem(Paragraph(esc(i), body)) for i in items], bulletType="bullet", leftIndent=14)); items = []
    for l in lines:
        if l.startswith("- "): flush(); items.append(l[2:].strip())
        elif l.strip() == "": flush()
        else: buf.append(l.strip())
    flush()

# ---- Interface tour -------------------------------------------------------------------------------
T = os.path.join(ROOT, 'docs/tour')
S = 2   # the renders are at 2x
tips = L['tips']
def R(k): return L[k]
def cap(i): return L['caps'][i]['t']
hits = [t for t in L['topKids'] if t['cls'] == 'led-hit']
# groups: (title, [(name, rect, tip)], kind) ; kind 'n' numbered controls
header = [('Previous preset', R('pPrev'), tips['pPrev']), ('Preset name', R('pName'), tips['pName']), ('Next preset', R('pNext'), tips['pNext']),
          ('Save', R('pSave'), tips['pSave']), ('Bypass', R('bBypass'), tips['bBypass']), ('Guide', R('bGuide'), tips['bGuide']),
          ('Settings', R('bSettings'), tips['bSettings']), ('About and Support', R('bAbout'), tips['bAbout'])]
plate = [(h['aria'] + (' (meter view)' if i < 3 else ' (timing)' if i < 5 else ' (knee)'), h['r'], h['tip']) for i, h in enumerate(hits)]
knobs = [(c['lab']['t'], c['knob']['r'], c['knob']['tip']) for c in L['kcells']]
smalls = [(c['lab']['t'] + ' (Manual timing only)', c['knob']['r'], c['knob']['tip']) for c in L['tmk']]
meters = [('Input meter', R('cvIn'), tips['cvIn']), ('Output meter', R('cvOut'), tips['cvOut']), ('Input peak read-out', R('pkIn'), tips['pkIn']), ('Output peak read-out', R('pkOut'), tips['pkOut'])]
displays = [('Analogue meter dial', R('cvDial'), tips['dial']), (cap(0), R('cvCurve'), tips['curve']), (cap(1), R('cvShape'), tips['shape']), (cap(2), R('cvHist'), tips['hist'])]
num = [header, plate, knobs, smalls, meters]

def font(sz):
    for p in ("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", os.path.join(ROOT, "assets/fonts/BarlowCondensed-Bold.ttf")):
        if os.path.exists(p): return ImageFont.truetype(p, sz)
    return ImageFont.load_default()
def badge(dr, x, y, text, col):
    r = 13 * S // 2 + 4
    dr.ellipse([x - r, y - r, x + r, y + r], fill=col, outline=(255, 255, 255), width=2)
    f = font(15 * S // 2); w = dr.textlength(text, font=f)
    dr.text((x - w / 2, y - 9 * S // 2 + 1), text, fill=(255, 255, 255), font=f)
def tour_image(name, manual=False):
    path = os.path.join(T, name)
    if not os.path.exists(path): return None
    im = Image.open(path).convert("RGB"); dr = ImageDraw.Draw(im)
    n = 1
    for grp in num:
        for _, r, _ in grp:
            if grp is smalls and not manual: n += 1; continue
            badge(dr, int(r[0] * S + 4), int(r[1] * S + 4), str(n), (194, 24, 91)); n += 1
    for i, (_, r, _) in enumerate(displays):
        badge(dr, int(r[0] * S + 8), int(r[1] * S + 8), "ABCD"[i], (0, 122, 102))
    b = io.BytesIO(); im.save(b, "PNG"); b.seek(0); return b
def rl(b, width):
    w, h = ImageReader(b).getSize(); b.seek(0)
    return RLImage(b, width=width, height=width * h / w)
def table(rows, first_style=CELLB):
    t = Table(rows, colWidths=[10 * mm, 38 * mm, 122 * mm])
    t.setStyle(TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"), ("BACKGROUND", (0, 0), (-1, -1), colors.HexColor("#F3F3F7")),
                           ("LINEBELOW", (0, 0), (-1, -1), 0.5, colors.white), ("TOPPADDING", (0, 0), (-1, -1), 4), ("BOTTOMPADDING", (0, 0), (-1, -1), 4)]))
    return t
img = tour_image("auto.png")
if img:
    subs = ParagraphStyle("tsub", parent=body, fontName="Helvetica-Bold", fontSize=11.5, leading=15, textColor=TEAL, spaceBefore=6, spaceAfter=3)
    story += [PageBreak(), Paragraph("Interface tour", h2),
              Paragraph(esc("Pink numbers mark controls, teal letters mark displays. Every number is explained in the tables on the next pages. The pictures are renders of the real compiled plugin window (made offscreen by the build with a made-up drum loop), not a mockup. Texts are the same info bubbles you see when you hover a control."), body),
              Spacer(1, 4), rl(img, 170 * mm), Spacer(1, 6),
              Paragraph("Signal flow", subs),
              Paragraph(esc("Input, then the sidechain high-pass (the detector only; your sound is not filtered), the RMS detector, the gain computer (Threshold, Ratio, Knee), automatic or manual attack and release, Output gain, Mix with the dry signal, and finally the Ceiling soft clipper."), body)]
    n = 1
    def block(title, grp, extra=None):
        global n
        rows = []
        for nm, r, tip in grp:
            rows.append([Paragraph(str(n), CELLB), Paragraph(esc(nm), CELLB), Paragraph(esc(tip), CELL)]); n += 1
        story.extend([CondPageBreak(70 * mm), Paragraph(esc(title), subs), table(rows)])
    block("Header", header); block("Plate: meter view, timing and knee", plate)
    block("Knobs", knobs)
    mimg = tour_image("manual.png", True)
    story.extend([CondPageBreak(110 * mm), Paragraph("Manual timing", subs), Paragraph(esc("Switch Timing to Manual and two more knobs appear on the plate."), body)])
    if mimg: story.append(rl(mimg, 150 * mm))
    block("Manual timing knobs", smalls); block("Meters", meters)
    rows = []
    for i, (nm, r, tip) in enumerate(displays):
        rows.append([Paragraph("ABCD"[i], CELLT), Paragraph(esc(nm), CELLT), Paragraph(esc(tip), CELL)])
    story.extend([CondPageBreak(70 * mm), Paragraph("Displays", subs), table(rows)])
    story += [PageBreak(), Paragraph("Under the hood", h2)]
    for t in [
        "Latency is a fixed 32 samples at every sample rate and setting, reported to your host, and the dry path is delayed to match so Bypass and Mix stay aligned.",
        "Detector: true RMS over an 8 ms window. Auto timing: the attack follows the amount of gain reduction (about 15 ms at 10 dB, 5 ms at 20 dB, 3 ms at 30 dB) and the release is a steady 125 dB per second. Manual: Attack 0.1 to 100 ms, Release 10 ms to 2 s.",
        "Ranges: Threshold -40 to 0 dBFS, Output -20 to +20 dB, Mix 0 to 100 percent, SC High-pass off or 20 to 300 Hz, Ceiling -12 to 0 dBFS or Off.",
        "Soft knee (ours) is 8 dB wide. The Ceiling is a soft clipper (identity up to 60 percent of the ceiling, then a tanh curve that never passes it), run 4x oversampled; only the correction signal is filtered, so with nothing clipped the output is the input delayed by 32 samples.",
        "Parameters are smoothed over about 5 ms. The audio thread uses no allocation or locks. Silence, DC, full scale and NaN input were tested in the core tests.",
        "Measured in the core tests: the Ceiling never passes its level in the test signals; the 4x oversampled stage has not been alias-measured against other plugins. Not verified: behaviour in real hosts and on real Windows and macOS machines.",
        "PunchPaw collects nothing and never uses the network.",
    ]: story.append(Paragraph("&bull; " + esc(t), body))
SimpleDocTemplate(out, pagesize=A4, leftMargin=20 * mm, rightMargin=20 * mm, topMargin=18 * mm, bottomMargin=18 * mm, title="PunchPaw manual", author="Studio 87").build(story)
print("wrote", out)
