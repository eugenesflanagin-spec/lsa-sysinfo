#!/usr/bin/env python3
"""Build one PDF + one self-contained HTML page from the LSA docs
(USER-GUIDE + RESULTS + CLI-REFERENCE + GLOSSARY).

Usage: python3 -I build_lsa_pdf.py <docs-dir> <out.pdf> --html <out.html>
Each Markdown file is converted with pandoc (GitHub-style anchors); its ids are prefixed with the
file's name so links between files keep working inside the single document. The HTML has the
images inlined (data: URIs), so it works on its own (Windows installer / zip, file:// links);
its element ids are the PDF's named destinations, used by lsa-gui's Guide links and `lsa guide`.
WeasyPrint renders the PDF from the same HTML.
"""
import base64, html, mimetypes, os, re, subprocess, sys

docs = os.path.abspath(sys.argv[1])
out = sys.argv[2]
html_out = sys.argv[sys.argv.index("--html") + 1] if "--html" in sys.argv else out + ".html"
FILES = ["USER-GUIDE.md", "RESULTS.md", "CLI-REFERENCE.md", "GLOSSARY.md"]

def prefix(rel):
    return re.sub(r"[^a-z0-9]+", "-", rel.lower().removesuffix(".md")).strip("-")

def data_uri(path):
    mt = mimetypes.guess_type(path)[0] or "application/octet-stream"
    return "data:%s;base64,%s" % (mt, base64.b64encode(open(path, "rb").read()).decode())

def convert(rel):
    p = prefix(rel)
    src_dir = os.path.dirname(os.path.join(docs, rel))
    body = subprocess.run(["pandoc", "-f", "gfm", "-t", "html5", os.path.join(docs, rel)],
                          capture_output=True, text=True, check=True).stdout
    body = re.sub(r'\bid="([^"]+)"', lambda m: f'id="{p}--{m.group(1)}"', body)

    def href(m):
        target = html.unescape(m.group(1))
        if re.match(r"[a-z]+:", target):
            return m.group(0)
        path, _, frag = target.partition("#")
        if not path:
            return f'href="#{p}--{frag}"'
        if path.endswith(".md"):
            tp = prefix(os.path.relpath(os.path.normpath(os.path.join(src_dir, path)), docs))
            return f'href="#{tp}--{frag}"' if frag else f'href="#{tp}"'
        return m.group(0)
    body = re.sub(r'href="([^"]+)"', href, body)
    body = re.sub(r'src="(?![a-z]+:)([^"]+)"',
                  lambda m: f'src="{data_uri(os.path.normpath(os.path.join(src_dir, m.group(1))))}"', body)
    return f'<section class="doc" id="{p}">\n{body}\n</section>'

CSS = """
@page { size: A4; margin: 18mm 16mm 20mm 16mm;
  @bottom-center { content: "LSA User Guide  ·  page " counter(page); font: 9pt 'Noto Sans', sans-serif; color: #666; } }
body { font-family: 'Noto Sans', 'DejaVu Sans', sans-serif; font-size: 10pt; line-height: 1.5; color: #1a1a1a; }
@media screen { body { max-width: 52em; margin: 0 auto; padding: 0 16px; } }
h1 { font-size: 24pt; color: #0b4f8a; border-bottom: 2px solid #0b4f8a; padding-bottom: 4px; }
h2 { font-size: 18pt; color: #0b4f8a; margin-top: 1.4em; }
h3 { font-size: 14pt; color: #134; margin-top: 1.2em; }
h4 { font-size: 12pt; }
h1, h2, h3, h4 { break-after: avoid; }
section.doc { break-before: page; }
a { color: #0b5cad; text-decoration: none; }
code { font-family: 'DejaVu Sans Mono', monospace; font-size: 9.5pt; background: #f2f4f7; padding: 0 2px; }
pre { background: #f2f4f7; padding: 8px; font-size: 9pt; white-space: pre-wrap; word-break: break-all; border-left: 3px solid #0b4f8a; }
pre code { background: none; }
table { border-collapse: collapse; width: 100%; font-size: 9.5pt; margin: 0.6em 0; }
th, td { border: 1px solid #c8ced6; padding: 3px 5px; vertical-align: top; word-break: break-word; }
th { background: #e6edf5; }
img { max-width: 100%; border: 1px solid #ccc; }
blockquote { border-left: 4px solid #e0a800; background: #fff8e1; margin: 0.6em 0; padding: 4px 10px; }
.cover { text-align: center; padding-top: 70mm; }
.cover h1 { border: none; font-size: 24pt; }
.toc li { margin: 3px 0; }
@media print { .toc a::after { content: leader('.') target-counter(attr(href), page); } }
"""

files = [f for f in FILES if os.path.exists(os.path.join(docs, f))]
toc = "".join(
    f'<li><a href="#{prefix(f)}">{html.escape(re.sub(r"^#+ *", "", next((l for l in open(os.path.join(docs, f)) if l.startswith("# ")), f)).strip())}</a></li>'
    for f in files)
doc = f"""<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>LSA User Guide</title><style>{CSS}</style></head><body>
<div class="cover"><h1>Linux System Audit: Cache &amp; Memory Benchmark</h1>
<p style="font-size:14pt">User guide, result reference, command reference and glossary</p>
<p style="font-size:11pt; margin: 30mm 20mm 0 20mm">LSA is a clean-room Linux implementation of AIDA64 Business's
Cache &amp; Memory Benchmark, built to produce results 1:1 with the native Windows app.
AIDA64 is made by FinalWire Ltd.; LSA is an independent project, not affiliated with or endorsed by FinalWire,
and contains none of its code or data.</p></div>
<section class="doc"><h1>Contents</h1><ol class="toc">{toc}</ol></section>
{''.join(convert(f) for f in files)}
</body></html>"""
os.makedirs(os.path.dirname(os.path.abspath(html_out)), exist_ok=True)
open(html_out, "w").write(doc)
subprocess.run(["weasyprint", "-q", html_out, out], check=True)
print("wrote", out, "and", html_out, "from", len(files), "files")
