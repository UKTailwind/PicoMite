#!/usr/bin/env python3
"""
Generate Bas/armcfgen.pdf from Bas/armcfgen.md

Follows the docs/ house style (Helvetica headings, Courier code blocks, latin-1
sanitising, page header/footer) but uses a word-wrapping table renderer so the
content-heavy reference/troubleshooting tables are not truncated.
"""

import os
import re
from fpdf import FPDF

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, '..', 'Bas', 'armcfgen.md')
OUT = os.path.join(HERE, '..', 'Bas', 'armcfgen.pdf')

PAGE_WIDTH = 190  # usable width with default 10mm margins


class PDF(FPDF):
    def header(self):
        self.set_font('Helvetica', 'B', 15)
        self.cell(0, 10, 'armcfgen.py - User Manual', 0, 1, 'C')
        self.ln(3)

    def footer(self):
        self.set_y(-15)
        self.set_font('Helvetica', 'I', 8)
        self.cell(0, 10, f'Page {self.page_no()}', 0, 0, 'C')


UNICODE_REPLACEMENTS = {
    '—': '--',  '–': '-',   '…': '...',
    '‘': "'",   '’': "'",   '“': '"',  '”': '"',
    '→': '->',  '←': '<-',  '×': 'x',  '÷': '/',
    '≈': '~',   '≠': '!=',  '≤': '<=', '≥': '>=',
    '−': '-',   'π': 'pi',  '·': '*',  '°': 'deg',
    '•': '-',   '‑': '-',  ' ': ' ',  ' ': ' ',
}


def sanitize(text):
    for u, r in UNICODE_REPLACEMENTS.items():
        text = text.replace(u, r)
    out = []
    for ch in text:
        try:
            ch.encode('latin-1')
            out.append(ch)
        except UnicodeEncodeError:
            out.append('?')
    return ''.join(out)


def inline(text):
    """Strip inline markdown emphasis/code for flowed text."""
    text = re.sub(r'\*\*(.+?)\*\*', r'\1', text)
    text = re.sub(r'`([^`]*)`', r'\1', text)
    return sanitize(text)


def add_section(pdf, title, level=1):
    title = inline(title)
    if level == 1:
        pdf.set_font('Helvetica', 'B', 13)
        pdf.ln(4)
    elif level == 2:
        pdf.set_font('Helvetica', 'B', 11)
        pdf.ln(2)
    else:
        pdf.set_font('Helvetica', 'B', 10)
        pdf.ln(1)
    pdf.multi_cell(0, 6, title)
    pdf.ln(1)


def add_text(pdf, text, indent=''):
    pdf.set_font('Helvetica', '', 10)
    pdf.multi_cell(0, 5, indent + inline(text))


def add_note(pdf, text):
    pdf.set_font('Helvetica', 'I', 9)
    pdf.set_text_color(80, 80, 80)
    pdf.multi_cell(0, 5, '  ' + inline(text))
    pdf.set_text_color(0, 0, 0)


def add_code(pdf, code):
    pdf.set_font('Courier', '', 8)
    pdf.set_fill_color(245, 245, 245)
    for line in code.split('\n'):
        pdf.cell(0, 4, '  ' + sanitize(line), 0, 1, fill=True)
    pdf.ln(2)


def wrap_cell(pdf, text, w):
    """Wrap one cell's text to width w (current font), breaking long tokens."""
    avail = w - 2
    lines, cur = [], ''
    for word in text.split(' '):
        while pdf.get_string_width(word) > avail and len(word) > 1:
            # hard-break an over-long token (e.g. a long identifier)
            cut = len(word)
            while cut > 1 and pdf.get_string_width(word[:cut]) > avail:
                cut -= 1
            if cur:
                lines.append(cur)
                cur = ''
            lines.append(word[:cut])
            word = word[cut:]
        trial = (cur + ' ' + word).strip()
        if pdf.get_string_width(trial) <= avail:
            cur = trial
        else:
            if cur:
                lines.append(cur)
            cur = word
    lines.append(cur)
    return lines or ['']


def add_table(pdf, headers, rows):
    ncols = len(headers)
    pdf.set_font('Helvetica', '', 9)
    # proportional widths from max measured content, clamped
    maxw = []
    for i in range(ncols):
        cells = [headers[i]] + [r[i] if i < len(r) else '' for r in rows]
        maxw.append(max(pdf.get_string_width(sanitize(c)) for c in cells) or 1)
    total = sum(maxw)
    widths = [max(16, PAGE_WIDTH * w / total) for w in maxw]
    scale = PAGE_WIDTH / sum(widths)
    widths = [w * scale for w in widths]
    line_h = 4.4
    x0 = pdf.l_margin
    bottom = pdf.h - pdf.b_margin

    # Take full control of pagination so fpdf never auto-breaks mid-row.
    saved_auto = pdf.auto_page_break
    pdf.set_auto_page_break(False)

    def wrap_all(cells, style):
        pdf.set_font('Helvetica', style, 9)
        return [wrap_cell(pdf, sanitize(cells[i] if i < len(cells) else ''), widths[i])
                for i in range(ncols)]

    def draw_row(cells, style):
        wrapped = wrap_all(cells, style)
        rh = line_h * max(len(w) for w in wrapped)
        y0 = pdf.get_y()
        x = x0
        for i in range(ncols):
            pdf.rect(x, y0, widths[i], rh)
            yy = y0 + 0.6
            for ln in wrapped[i]:
                pdf.set_xy(x + 1, yy)
                pdf.cell(widths[i] - 2, line_h, ln, 0, 0, 'L')
                yy += line_h
            x += widths[i]
        pdf.set_xy(x0, y0 + rh)

    pdf.set_x(x0)
    draw_row(headers, 'B')
    for r in rows:
        rh = line_h * max(len(w) for w in wrap_all(r, ''))
        if pdf.get_y() + rh > bottom:          # would overflow -> new page first
            pdf.add_page()
            pdf.set_x(x0)
            draw_row(headers, 'B')             # repeat the header on the new page
        draw_row(r, '')

    pdf.set_auto_page_break(saved_auto, pdf.b_margin)
    pdf.ln(3)


def process_markdown(pdf, content):
    lines = content.split('\n')
    i = 0
    in_code = False
    code_buf = []
    in_table = False
    t_head, t_rows = [], []

    def flush_table():
        nonlocal in_table, t_head, t_rows
        if in_table and t_head and t_rows:
            add_table(pdf, t_head, t_rows)
        in_table, t_head, t_rows = False, [], []

    while i < len(lines):
        line = lines[i]

        if line.strip().startswith('```'):
            if in_code:
                add_code(pdf, '\n'.join(code_buf))
                code_buf, in_code = [], False
            else:
                flush_table()
                in_code = True
            i += 1
            continue
        if in_code:
            code_buf.append(line)
            i += 1
            continue

        stripped = line.strip()

        # Tables
        if stripped.startswith('|'):
            if re.match(r'^\|[\s:|-]+\|?$', stripped):  # separator row
                i += 1
                continue
            cells = [c.strip() for c in stripped.strip('|').split('|')]
            if not in_table:
                t_head, in_table = cells, True
            else:
                t_rows.append(cells)
            i += 1
            continue
        elif in_table:
            flush_table()

        if stripped == '---':
            pdf.ln(2)
        elif line.startswith('# '):
            pass  # title is in the page header
        elif line.startswith('## '):
            add_section(pdf, stripped[3:], 1)
        elif line.startswith('### '):
            add_section(pdf, stripped[4:], 2)
        elif line.startswith('#### '):
            add_section(pdf, stripped[5:], 3)
        elif stripped.startswith('> '):
            add_note(pdf, stripped[2:])
        elif stripped.startswith('* ') or stripped.startswith('- '):
            add_text(pdf, stripped[2:], indent='  - ')
        elif re.match(r'^\d+\.\s', stripped):
            add_text(pdf, stripped, indent='  ')
        elif stripped:
            add_text(pdf, stripped)
        else:
            pdf.ln(2)

        i += 1

    flush_table()


def main():
    pdf = PDF()
    pdf.set_auto_page_break(auto=True, margin=15)
    pdf.add_page()
    with open(SRC, 'r', encoding='utf-8') as f:
        process_markdown(pdf, f.read())
    pdf.output(OUT)
    print('Generated:', os.path.normpath(OUT))


if __name__ == '__main__':
    main()
