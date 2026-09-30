#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Komsky
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Writes the HTML for the demo desktop: a code editor showing Lupka's own
# zoom maths, a terminal and presenter notes, at 1920x1080. make-demo.sh
# renders it to an image with headless Chrome.

import html
import pathlib
import sys

from pygments import highlight
from pygments.formatters import HtmlFormatter
from pygments.lexers import CppLexer

root = pathlib.Path(__file__).resolve().parents[2]
source = (root / "src/overlay/zoommath.cpp").read_text().splitlines()
# Skip the licence header; show the part that makes ZoomIt's viewport.
start = next(i for i, line in enumerate(source) if line.startswith("namespace zoommath"))
code = "\n".join(source[start:start + 44])
first_line = start + 1

formatter = HtmlFormatter(style="one-dark", nowrap=True)
highlighted = highlight(code, CppLexer(), formatter)
lines = highlighted.rstrip("\n").split("\n")
numbers = "\n".join(str(first_line + i) for i in range(len(lines)))

tree = [
    ("lupka", 0, "dir"), ("src", 1, "dir"), ("capture", 2, "dir"), ("hotkeys", 2, "dir"),
    ("overlay", 2, "dir"), ("annotations.cpp", 3, ""), ("overlaywindow.cpp", 3, ""),
    ("session.cpp", 3, ""), ("zoommath.cpp", 3, "active"), ("zoommath.h", 3, ""),
    ("app.cpp", 2, ""), ("main.cpp", 2, ""), ("recorder.cpp", 2, ""), ("tests", 1, "dir"),
    ("CMakeLists.txt", 1, ""), ("README.md", 1, ""),
]
tree_html = "\n".join(
    f'<div class="node {kind}" style="padding-left:{14 + depth * 16}px">'
    f'{"&#9662; " if kind == "dir" else ""}{html.escape(name)}</div>'
    for name, depth, kind in tree
)

terminal = [
    ('prompt', '~/lupka$ ', 'cmake --build build'),
    ('out', '[42/42] Linking CXX executable lupka', ''),
    ('prompt', '~/lupka$ ', 'ctest --test-dir build'),
    ('out', '1/7 Test #1: zoommath ........   Passed', ''),
    ('out', '2/7 Test #2: keynames ........   Passed', ''),
    ('out', '3/7 Test #3: annotations .....   Passed', ''),
    ('out', '4/7 Test #4: settings ........   Passed', ''),
    ('out', '5/7 Test #5: capture .........   Passed', ''),
    ('out', '6/7 Test #6: breaktimer ......   Passed', ''),
    ('out', '7/7 Test #7: demoscript ......   Passed', ''),
    ('ok', '100% tests passed, 0 tests failed out of 7', ''),
    ('prompt', '~/lupka$ ', ''),
]
terminal_html = "\n".join(
    f'<div class="{kind}">{html.escape(a)}<span class="cmd">{html.escape(b)}</span></div>'
    for kind, a, b in terminal
)

page = f"""<!doctype html>
<html><head><meta charset="utf-8"><style>
* {{ box-sizing: border-box; margin: 0; padding: 0; }}
html, body {{ width: 1920px; height: 1080px; overflow: hidden; }}
body {{
  font-family: "Ubuntu Sans", "Noto Sans", sans-serif;
  background: radial-gradient(circle at 20% 110%, #3a7bd5 0%, transparent 55%),
              radial-gradient(circle at 95% -10%, #b44fc8 0%, transparent 50%),
              linear-gradient(135deg, #1d2440 0%, #2a1f3d 100%);
}}
.bar {{ height: 34px; background: #0e0f13; color: #e8e8ec; font-size: 15px; font-weight: 600;
        display: flex; align-items: center; justify-content: center; position: relative; }}
.bar .left {{ position: absolute; left: 16px; display: flex; gap: 6px; }}
.bar .left span {{ width: 8px; height: 8px; border-radius: 4px; background: #666; }}
.bar .left span:first-child {{ width: 24px; background: #e8e8ec; }}
.bar .right {{ position: absolute; right: 18px; display: flex; gap: 14px; }}
.bar .right span {{ width: 14px; height: 14px; border-radius: 3px; border: 2px solid #cfcfd6; }}
.win {{ position: absolute; border-radius: 12px; overflow: hidden;
        box-shadow: 0 18px 50px rgba(0,0,0,.55), 0 0 0 1px rgba(255,255,255,.06); }}
.title {{ height: 40px; display: flex; align-items: center; justify-content: center; font-size: 14px;
          font-weight: 600; position: relative; }}
.title .buttons {{ position: absolute; right: 14px; display: flex; gap: 10px; }}
.title .buttons span {{ width: 14px; height: 14px; border-radius: 7px; background: rgba(255,255,255,.14); }}
#editor {{ left: 56px; top: 70px; width: 1300px; height: 960px; background: #21252b; color: #abb2bf; }}
#editor .title {{ background: #1b1e23; color: #c8ccd4; }}
#editor .body {{ display: flex; height: 920px; }}
.side {{ width: 250px; background: #1e2127; padding-top: 12px; font-size: 14px; color: #9da5b4; }}
.node {{ height: 27px; line-height: 27px; white-space: nowrap; }}
.node.dir {{ color: #c8ccd4; }}
.node.active {{ background: #2c313a; color: #ffffff; box-shadow: inset 3px 0 #61afef; }}
.main {{ flex: 1; display: flex; flex-direction: column; background: #282c34; }}
.tabs {{ height: 38px; background: #21252b; display: flex; font-size: 14px; }}
.tab {{ padding: 0 22px; line-height: 38px; color: #7f848e; }}
.tab.on {{ background: #282c34; color: #e6e6e6; box-shadow: inset 0 2px #61afef; }}
.code {{ flex: 1; display: flex; padding-top: 14px;
         font: 15px/24px "DejaVu Sans Mono", "Noto Sans Mono", monospace; }}
.gutter {{ width: 64px; text-align: right; padding-right: 18px; color: #4b5263; white-space: pre; }}
pre {{ font: inherit; white-space: pre; color: #abb2bf; }}
.status {{ height: 28px; background: #21252b; color: #7f848e; font-size: 13px; display: flex;
           align-items: center; gap: 26px; padding: 0 16px; }}
#term {{ left: 1400px; top: 70px; width: 470px; height: 520px; background: #16181d; }}
#term .title {{ background: #202329; color: #c8ccd4; }}
#term .text {{ padding: 14px 16px; font: 14px/23px "DejaVu Sans Mono", monospace; color: #b8bfcc; }}
#term .prompt {{ color: #98c379; }}
#term .cmd {{ color: #e6e6e6; }}
#term .ok {{ color: #98c379; font-weight: bold; }}
#notes {{ left: 1400px; top: 620px; width: 470px; height: 410px; background: #f6f3ea; color: #33302a; }}
#notes .title {{ background: #ebe6d8; color: #4a463d; }}
#notes .text {{ padding: 22px 28px; font-size: 19px; line-height: 34px; }}
#notes h2 {{ font-size: 24px; margin-bottom: 10px; }}
#notes li {{ margin-left: 22px; }}
{formatter.get_style_defs('pre')}
</style></head><body>
<div class="bar"><div class="left"><span></span><span></span><span></span></div>
Wed 30 Sep  10:24<div class="right"><span></span><span></span><span></span></div></div>
<div class="win" id="editor">
  <div class="title">zoommath.cpp - lupka<div class="buttons"><span></span><span></span><span></span></div></div>
  <div class="body"><div class="side">{tree_html}</div>
    <div class="main">
      <div class="tabs"><div class="tab">overlaywindow.cpp</div><div class="tab on">zoommath.cpp</div>
        <div class="tab">zoommath.h</div></div>
      <div class="code"><div class="gutter">{numbers}</div><pre>{highlighted}</pre></div>
      <div class="status"><span>master</span><span>Ln 37, Col 5</span><span>UTF-8</span><span>C++</span></div>
    </div></div>
</div>
<div class="win" id="term"><div class="title">Terminal<div class="buttons"><span></span><span></span><span></span></div></div>
  <div class="text">{terminal_html}</div></div>
<div class="win" id="notes"><div class="title">Talk notes<div class="buttons"><span></span><span></span><span></span></div></div>
  <div class="text"><h2>How the zoom follows you</h2><ol>
    <li>View origin = cursor &minus; cursor / zoom</li>
    <li>Clamp it to the screen edges</li>
    <li>Keep the pointer out of the outer eighth</li>
    <li>Animate in log space, 420 ms</li></ol></div></div>
</body></html>
"""
out = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else "scene.html")
out.write_text(page)
