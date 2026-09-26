#!/usr/bin/env python3
"""
Build docs/armcfgen.docx from docs/armcfgen.md.

A small Markdown -> .docx converter (python-docx) covering the subset used by the
manual: # .. #### headings, fenced ``` code blocks, | pipe tables |, - bullets,
numbered items, > notes, and inline **bold** / `code`. Unicode passes through
untouched, and Word wraps table cells itself, so there is none of the latin-1 /
manual-wrapping fuss the PDF generator needs.

Convert to PDF afterwards with Word or LibreOffice (File > Export as PDF), or:
    soffice --headless --convert-to pdf docs/armcfgen.docx
"""

import os
import re
from docx import Document
from docx.shared import Pt, RGBColor
from docx.enum.text import WD_ALIGN_PARAGRAPH, WD_LINE_SPACING
from docx.oxml.ns import qn
from docx.oxml import OxmlElement

HERE = os.path.dirname(os.path.abspath(__file__))
# armcfgen.md moved from Bas/ to docs/, and the docx lives beside it
SRC = os.path.join(HERE, 'armcfgen.md')
OUT = os.path.join(HERE, 'armcfgen.docx')

CODE_FONT = 'Consolas'
CODE_SIZE = 8.5
CODE_FILL = 'F2F2F2'
CODE_COLOR = RGBColor(0x33, 0x33, 0x33)


def shade(paragraph, fill):
    pPr = paragraph._p.get_or_add_pPr()
    shd = OxmlElement('w:shd')
    shd.set(qn('w:val'), 'clear')
    shd.set(qn('w:fill'), fill)
    pPr.append(shd)


def add_inline(paragraph, text, base_bold=False):
    """Render a run sequence handling **bold**, *italic* and `code`; a code span
    may sit inside bold (**... `Option` ...**), and * inside a code span is
    literal, as is a lone * with no closing one."""
    state = {'bold': False, 'italic': False}

    def flush(s, code=False):
        if not s:
            return
        r = paragraph.add_run(s)
        if code:
            r.font.name = CODE_FONT
            r.font.size = Pt(9)
        if state['bold'] or base_bold:
            r.bold = True
        if state['italic']:
            r.italic = True

    buf, i = '', 0
    while i < len(text):
        if text.startswith('**', i):
            flush(buf)
            buf = ''
            state['bold'] = not state['bold']
            i += 2
        elif text[i] == '*' and (state['italic'] or re.match(r'\*\S[^*]*?\S?\*(?!\*)', text[i:])):
            flush(buf)
            buf = ''
            state['italic'] = not state['italic']
            i += 1
        elif text[i] == '`' and text.find('`', i + 1) > i:
            j = text.find('`', i + 1)
            flush(buf)
            buf = ''
            flush(text[i + 1:j], code=True)
            i = j + 1
        else:
            buf += text[i]
            i += 1
    flush(buf)


def add_code(doc, lines):
    p = doc.add_paragraph()
    p.paragraph_format.space_after = Pt(6)
    p.paragraph_format.space_before = Pt(2)
    p.paragraph_format.line_spacing = 1.0
    shade(p, CODE_FILL)
    first = True
    for ln in lines:
        r = p.add_run(ln if ln else ' ')
        r.font.name = CODE_FONT
        r.font.size = Pt(CODE_SIZE)
        r.font.color.rgb = CODE_COLOR
        if not first:
            pass
        first = False
        # line break between code lines (not after the last)
        if ln is not lines[-1]:
            r.add_break()


def add_table(doc, headers, rows):
    t = doc.add_table(rows=1, cols=len(headers))
    t.style = 'Table Grid'
    t.autofit = True
    for i, h in enumerate(headers):
        cell = t.rows[0].cells[i]
        cell.paragraphs[0].text = ''
        add_inline(cell.paragraphs[0], h, base_bold=True)
    for row in rows:
        cells = t.add_row().cells
        for i in range(len(headers)):
            txt = row[i] if i < len(row) else ''
            cells[i].paragraphs[0].text = ''
            add_inline(cells[i].paragraphs[0], txt)
    doc.add_paragraph().paragraph_format.space_after = Pt(2)


def convert(doc, md):
    lines = md.split('\n')
    i = 0
    while i < len(lines):
        line = lines[i]
        stripped = line.strip()

        # fenced code
        if stripped.startswith('```'):
            buf = []
            i += 1
            while i < len(lines) and not lines[i].strip().startswith('```'):
                buf.append(lines[i])
                i += 1
            i += 1
            if buf:
                add_code(doc, buf)
            continue

        # tables
        if stripped.startswith('|'):
            block = []
            while i < len(lines) and lines[i].strip().startswith('|'):
                block.append(lines[i].strip())
                i += 1
            rows = []
            for b in block:
                if re.match(r'^\|[\s:|-]+\|?$', b):   # separator
                    continue
                rows.append([c.strip() for c in b.strip('|').split('|')])
            if rows:
                add_table(doc, rows[0], rows[1:])
            continue

        # headings (Word styles them; a code span in one just loses its backticks)
        stripped = stripped.replace('`', '') if line.startswith('#') else stripped
        if line.startswith('#### '):
            doc.add_heading(stripped[5:], level=3)
        elif line.startswith('### '):
            doc.add_heading(stripped[4:], level=2)
        elif line.startswith('## '):
            doc.add_heading(stripped[3:], level=1)
        elif line.startswith('# '):
            doc.add_heading(stripped[2:], level=0)
        elif stripped == '---':
            pass  # section breaks are conveyed by headings
        elif stripped.startswith('>'):
            # a note: every following '>' line belongs to it
            text = stripped[1:].strip()
            while i + 1 < len(lines) and lines[i + 1].strip().startswith('>'):
                i += 1
                text += ' ' + lines[i].strip()[1:].strip()
            p = doc.add_paragraph(style='Intense Quote')
            add_inline(p, text)
        elif stripped.startswith('- ') or stripped.startswith('* '):
            i, text = gather(lines, i, stripped[2:])
            p = doc.add_paragraph(style='List Bullet')
            add_inline(p, text)
        elif re.match(r'^\d+\.\s', stripped):
            i, text = gather(lines, i, stripped)
            p = doc.add_paragraph()
            p.paragraph_format.left_indent = Pt(14)
            add_inline(p, text)
        elif stripped:
            i, text = gather(lines, i, stripped)
            p = doc.add_paragraph()
            add_inline(p, text)
        i += 1


def starts_block(s):
    """True when a stripped line begins something other than more text."""
    return (not s or s.startswith(('#', '```', '|', '>', '- ', '* ', '---'))
            or re.match(r'^\d+\.\s', s) is not None)


def gather(lines, i, text):
    """The Markdown is hard-wrapped: join the lines that continue this
    paragraph, bullet or numbered item (up to a blank line or a new block)
    into one, so Word gets whole paragraphs.  Returns the last line used."""
    while i + 1 < len(lines) and not starts_block(lines[i + 1].strip()):
        i += 1
        text += ' ' + lines[i].strip()
    return i, text


def main():
    doc = Document()
    normal = doc.styles['Normal']
    normal.font.name = 'Calibri'
    normal.font.size = Pt(10.5)
    # Force single line spacing - the default template line-spaces paragraphs,
    # which renders as "double spaced". Apply to every style we emit.
    for sname in ('Normal', 'Title', 'Heading 1', 'Heading 2', 'Heading 3',
                  'List Bullet', 'Intense Quote'):
        try:
            pf = doc.styles[sname].paragraph_format
            pf.line_spacing = 1.0
            pf.line_spacing_rule = WD_LINE_SPACING.SINGLE
        except KeyError:
            pass
    normal.paragraph_format.space_before = Pt(0)
    normal.paragraph_format.space_after = Pt(4)
    with open(SRC, 'r', encoding='utf-8') as f:
        convert(doc, f.read())
    doc.save(OUT)
    print('Generated:', os.path.normpath(OUT))


if __name__ == '__main__':
    main()
