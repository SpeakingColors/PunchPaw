#!/usr/bin/env python3
"""Reads every control position, size, label and tooltip from the approved mockup (docs/mockup/PunchPaw.html)
with Playwright and writes docs/mockup/layout.json. tools/gen_layout.py turns that into src/editor/Layout.h,
so the native editor takes its geometry from the mockup and the two cannot drift apart.
Run from the repo root. The bundled fonts are loaded from assets/fonts so text widths match the plugin."""
import asyncio, json, os, sys
from playwright.async_api import async_playwright

ROOT = os.getcwd()
FONTS = f"""<style>
@font-face{{font-family:'Barlow Condensed';font-weight:500;src:url('file://{ROOT}/assets/fonts/BarlowCondensed-Medium.ttf')}}
@font-face{{font-family:'Barlow Condensed';font-weight:600;src:url('file://{ROOT}/assets/fonts/BarlowCondensed-SemiBold.ttf')}}
@font-face{{font-family:'Barlow Condensed';font-weight:700;src:url('file://{ROOT}/assets/fonts/BarlowCondensed-Bold.ttf')}}
@font-face{{font-family:'Barlow Condensed';font-weight:800;font-style:italic;src:url('file://{ROOT}/assets/fonts/BarlowCondensed-ExtraBoldItalic.ttf')}}
@font-face{{font-family:'JetBrains Mono';font-weight:400;src:url('file://{ROOT}/assets/fonts/JetBrainsMono-Regular.ttf')}}
@font-face{{font-family:'JetBrains Mono';font-weight:600;src:url('file://{ROOT}/assets/fonts/JetBrainsMono-Regular.ttf')}}
</style>"""
src = open('docs/mockup/PunchPaw.html').read()
src = src.replace('<link rel="stylesheet" href="https://fonts.googleapis.com/css2', '<link rel="x-skip" href="https://fonts.googleapis.com/css2')
open('/tmp/pp_wrapped.html', 'w').write('<!doctype html><html><head><meta charset="utf-8">' + FONTS + '</head><body>' + src + '</body></html>')

JS = r"""
() => {
  const st=document.querySelector('#stage').getBoundingClientRect();
  const r2=v=>Math.round(v*100)/100;
  const R=e=>{const r=e.getBoundingClientRect();return [r2(r.left-st.left),r2(r.top-st.top),r2(r.width),r2(r.height)]};
  const S=e=>{const cs=getComputedStyle(e);return {fs:cs.fontSize,ff:cs.fontFamily.indexOf('Barlow')>=0?'ui':'mono',fw:cs.fontWeight,fi:cs.fontStyle,ls:cs.letterSpacing,tt:cs.textTransform,tc:cs.color}};
  const T=e=>Object.assign({t:e.textContent,tip:e.dataset.tip||'',aria:e.getAttribute('aria-label')||'',c:e.className&&e.className.baseVal===undefined?e.className:'',r:R(e),color:e.style.color||''},S(e));
  const q=s=>document.querySelector(s);
  const out={};
  for(const [k,s] of Object.entries({cheekL:'#cheekL',cheekR:'#cheekR',main:'.main',hdr:'.hdr',sub:'#sub',body:'.body',left:'.left',top:'#top',disp:'.disp',knobs:'#knobs',rig:'.rig',meters:'.meters',foot:'.foot',logo:'.brand svg',name:'.brand .name',s87:'.brand .s87',pPrev:'#pPrev',pName:'#pName',pNext:'#pNext',pSave:'#pSave',bBypass:'#bBypass',bGuide:'#bGuide',bSettings:'#bSettings',bAbout:'#bAbout',cvCurve:'#cvCurve',cvShape:'#cvShape',cvHist:'#cvHist',cvIn:'#cvIn',cvOut:'#cvOut',cvDial:'#cvDial',pkIn:'#pkIn',pkOut:'#pkOut',winsz:'#winsz',footL:'.foot span'})) out[k]=R(q(s));
  out.tips={sub:q('#sub').dataset.tip,cheek:q('#cheekL').dataset.tip,dial:q('#cvDial').dataset.tip,curve:q('#cvCurve').dataset.tip,shape:q('#cvShape').dataset.tip,hist:q('#cvHist').dataset.tip,cvIn:q('#cvIn').dataset.tip,cvOut:q('#cvOut').dataset.tip,pkIn:q('#pkIn').dataset.tip,pkOut:q('#pkOut').dataset.tip,
    pPrev:q('#pPrev').dataset.tip,pName:q('#pName').dataset.tip,pNext:q('#pNext').dataset.tip,pSave:q('#pSave').dataset.tip,bBypass:q('#bBypass').dataset.tip,bGuide:q('#bGuide').dataset.tip,bSettings:q('#bSettings').dataset.tip,bAbout:q('#bAbout').dataset.tip};
  out.subParts=[...document.querySelectorAll('#sub > *')].map(T);
  out.caps=[...document.querySelectorAll('.dcell .cap')].map(e=>({t:e.textContent,r:R(e),parts:[...e.querySelectorAll('span')].map(T)}));
  out.mcap=[...document.querySelectorAll('.meters .cap span')].map(T);
  out.vdiv=T(q('.vdiv'));
  out.kcells=[...document.querySelectorAll('#knobs .kcell')].map(c=>({lab:T(c.querySelector('.lab')),knob:T(c.querySelector('.knob')),val:R(c.querySelector('.kval'))}));
  const top=q('#top');
  out.topKids=[...top.children].map(e=>{const o=T(e);o.cls=e.className;if(e.classList.contains('tmk')){o.lab=T(e.querySelector('.lab'));o.knob=T(e.querySelector('.knob'));o.val=R(e.querySelector('.kval'));o.ends=[...e.querySelectorAll('.kend')].map(T)}return o});
  // knob labels (tick texts) are read from the mockup's KN table through the page
  return out;
}
"""

async def main():
    async with async_playwright() as p:
        b = await p.chromium.launch()
        pg = await b.new_page(viewport={'width': 1400, 'height': 900})
        await pg.goto('file:///tmp/pp_wrapped.html')
        await pg.wait_for_timeout(1500)
        # show both timing modes: layout of Auto first, then Manual to read the small knobs
        d = await pg.evaluate(JS)
        await pg.evaluate("() => { window.__pp.st.timing = 1; window.__pp.syncUI(); }")
        d['tmk'] = await pg.evaluate("""() => { const st=document.querySelector('#stage').getBoundingClientRect(); const R=e=>{const r=e.getBoundingClientRect();return [Math.round((r.left-st.left)*100)/100,Math.round((r.top-st.top)*100)/100,Math.round(r.width*100)/100,Math.round(r.height*100)/100]};
          return [...document.querySelectorAll('#top .tmk')].map(e=>({cell:R(e),lab:{t:e.querySelector('.lab').textContent,r:R(e.querySelector('.lab'))},knob:{r:R(e.querySelector('.knob')),tip:e.querySelector('.knob').dataset.tip},val:R(e.querySelector('.kval')),ends:[...e.querySelectorAll('.kend')].map(k=>({t:k.textContent,r:R(k)}))})) }""")
        d['knobTexts'] = await pg.evaluate("""() => [...document.querySelectorAll('.knob')].map(k=>[...k.querySelectorAll('text')].map(t=>t.textContent))""")
        await pg.evaluate("() => { window.__pp.st.timing = 0; window.__pp.syncUI(); }")
        os.makedirs('docs/mockup', exist_ok=True)
        json.dump(d, open('docs/mockup/layout.json', 'w'), indent=1)
        await pg.screenshot(path='/tmp/pp_mockup_ref.png', clip={'x': 0, 'y': 0, 'width': 1400, 'height': 760})
        print('top children', len(d['top']), 'kcells', len(d['kcells']), 'rig', d['rig'])
        await b.close()
asyncio.run(main())
