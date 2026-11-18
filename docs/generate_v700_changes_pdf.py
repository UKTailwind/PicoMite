#!/usr/bin/env python3
"""
Generate PDF/PicoMite_V7.00.00_Changes_since_V6.03.01.pdf from
docs/V7.00.00_Changes_since_V6.03.01.md: what has changed for anyone upgrading
from the last full release, to attach to the release announcement.

From generate_mmb2csub_pdf.py (the docs/ house style: Helvetica headings,
Courier code blocks, latin-1 sanitising, re-flowed hard-wrapped prose, the
word-wrapping table renderer), with three additions: the document's '# ' title
is drawn as a title block on the first page, a '> ' quoted block (the upgrade
warning) is drawn shaded, code blocks inside it included, and the running
header names the document.
"""

import os
import re
from fpdf import FPDF

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, 'V7.00.00_Changes_since_V6.03.01.md')
# the manuals all live in PDF/
OUT = os.path.normpath(os.path.join(HERE, '..', 'PDF', 'PicoMite_V7.00.00_Changes_since_V6.03.01.pdf'))

PAGE_WIDTH = 190  # usable width with default 10mm margins


class PDF(FPDF):
    def header(self):
        if self.page_no() == 1:
            return                       # the title block is the first page's heading
        self.set_font('Helvetica', 'I', 9)
        self.cell(0, 8, 'PicoMite MMBasic V7.00.00 - changes since V6.03.01', 0, 1, 'C')
        self.ln(2)

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
    '•': '-',   '‑': '-',  ' ': ' ',  ' ': ' ',
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
    """Strip inline markdown emphasis/code for flowed text. Emphasis is taken
    out only between code spans: inside `...` an asterisk is code, so
    `PAUSE 60*60*1000` keeps its multiplications."""
    codes = []

    def hide(m):
        codes.append(m.group(1))
        return '\x00%d\x00' % (len(codes) - 1)

    text = re.sub(r'`([^`]*)`', hide, text)          # code spans out of the way first,
    text = re.sub(r'\*\*(.+?)\*\*', r'\1', text)    # so bold around code is still found
    text = re.sub(r'\*([^*]+)\*', r'\1', text)
    text = re.sub(r'\x00(\d+)\x00', lambda m: codes[int(m.group(1))], text)
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


def add_bullet(pdf, text):
    """A list item, hanging-indented so wrapped lines clear the dash."""
    pdf.set_font('Helvetica', '', 10)
    saved = pdf.l_margin
    pdf.set_x(saved)
    pdf.cell(6, 5, '  -', 0, 0)
    # multi_cell wraps back to the left MARGIN, not to the current x, so the
    # margin is what has to move for the continuation lines to line up.
    pdf.set_left_margin(saved + 6)
    pdf.set_x(saved + 6)
    pdf.multi_cell(0, 5, inline(text))
    pdf.set_left_margin(saved)
    pdf.set_x(saved)


def add_title(pdf, title):
    pdf.set_font('Helvetica', 'B', 18)
    pdf.ln(4)
    pdf.multi_cell(0, 9, inline(title), 0, 'C')
    pdf.ln(4)


def add_quote(pdf, lines):
    """A '> ' block: its paragraphs shaded, its code blocks as code."""
    pdf.ln(1)
    para, code, in_code = [], [], False

    def flush():
        nonlocal para
        if para:
            pdf.set_font('Helvetica', '', 10)
            pdf.set_fill_color(255, 244, 214)
            pdf.multi_cell(0, 5, inline(' '.join(para)), 0, 'L', fill=True)
            para = []

    for ln in lines:
        s = ln.strip()
        if s.startswith('```'):
            if in_code:
                add_code(pdf, '\n'.join(code))
                code, in_code = [], False
            else:
                flush()
                in_code = True
            continue
        if in_code:
            code.append(ln)
        elif s:
            para.append(s)
        else:
            flush()
            pdf.set_fill_color(255, 244, 214)
            pdf.cell(0, 2, '', 0, 1, fill=True)
    flush()
    pdf.ln(3)


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
    # A headerless markdown table (`| | |`) - drop the empty banner row.
    show_head = any(h.strip() for h in headers)
    ncols = len(headers)
    pdf.set_font('Helvetica', '', 9)
    maxw = []
    for i in range(ncols):
        cells = [headers[i]] + [r[i] if i < len(r) else '' for r in rows]
        maxw.append(max(pdf.get_string_width(inline(c)) for c in cells) or 1)
    total = sum(maxw)
    widths = [max(16, PAGE_WIDTH * w / total) for w in maxw]
    scale = PAGE_WIDTH / sum(widths)
    widths = [w * scale for w in widths]
    line_h = 4.4
    x0 = pdf.l_margin
    bottom = pdf.h - pdf.b_margin

    # set_auto_page_break(False) also sets b_margin to its default of 0, so the
    # bottom margin has to be saved HERE and restored explicitly. Restoring it
    # with pdf.b_margin - which is 0 by then - re-enables page breaks with no
    # bottom margin at all, and every page after the first table runs its text
    # down through the footer.
    saved_auto, saved_bmargin = pdf.auto_page_break, pdf.b_margin
    pdf.set_auto_page_break(False)

    def wrap_all(cells, style):
        pdf.set_font('Helvetica', style, 9)
        return [wrap_cell(pdf, inline(cells[i] if i < len(cells) else ''), widths[i])
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
    if show_head:
        if pdf.get_y() + line_h * 2 > bottom:   # no room for a header here
            pdf.add_page()
            pdf.set_x(x0)
        draw_row(headers, 'B')
    for r in rows:
        rh = line_h * max(len(w) for w in wrap_all(r, ''))
        if pdf.get_y() + rh > bottom:
            pdf.add_page()
            pdf.set_x(x0)
            if show_head:
                draw_row(headers, 'B')
        draw_row(r, '')

    pdf.set_auto_page_break(saved_auto, saved_bmargin)
    pdf.ln(3)


def process_markdown(pdf, content):
    lines = content.split('\n')
    i = 0
    in_code = False
    code_buf = []
    in_table = False
    t_head, t_rows = [], []
    para = []          # prose lines awaiting re-flow
    bullet = None      # (text,) while a list item is being gathered

    def flush_table():
        nonlocal in_table, t_head, t_rows
        if in_table and t_head:
            add_table(pdf, t_head, t_rows)
        in_table, t_head, t_rows = False, [], []

    def flush_para():
        nonlocal para, bullet
        if bullet is not None:
            add_bullet(pdf, ' '.join(bullet).strip())
            bullet = None
        elif para:
            add_text(pdf, ' '.join(para).strip())
        para = []

    while i < len(lines):
        line = lines[i]

        if line.strip().startswith('```'):
            if in_code:
                add_code(pdf, '\n'.join(code_buf))
                code_buf, in_code = [], False
            else:
                flush_para()
                flush_table()
                in_code = True
            i += 1
            continue
        if in_code:
            code_buf.append(line)
            i += 1
            continue

        stripped = line.strip()

        if stripped.startswith('|'):
            flush_para()
            if re.match(r'^\|[\s:|-]+\|?$', stripped):   # separator row
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

        if stripped.startswith('>'):
            flush_para()
            quote = []
            while i < len(lines) and lines[i].strip().startswith('>'):
                q = lines[i].strip()[1:]
                quote.append(q[1:] if q.startswith(' ') else q)
                i += 1
            add_quote(pdf, quote)
            continue

        if stripped == '---':
            flush_para()
            pdf.ln(2)
        elif line.startswith('# '):
            flush_para()
            add_title(pdf, stripped[2:])
        elif line.startswith('## '):
            flush_para()
            add_section(pdf, stripped[3:], 1)
        elif line.startswith('### '):
            flush_para()
            add_section(pdf, stripped[4:], 2)
        elif line.startswith('#### '):
            flush_para()
            add_section(pdf, stripped[5:], 3)
        elif stripped.startswith('* ') or stripped.startswith('- '):
            flush_para()
            bullet = [stripped[2:]]
        elif re.match(r'^\d+\.\s', stripped):
            flush_para()
            bullet = [stripped]
        elif stripped:
            # a continuation of whatever block is open
            (bullet if bullet is not None else para).append(stripped)
        else:
            flush_para()
            pdf.ln(2)

        i += 1

    flush_para()
    flush_table()


def main():
    import sys
    src = sys.argv[1] if len(sys.argv) > 1 else SRC   # (a draft elsewhere, for checking)
    out = sys.argv[2] if len(sys.argv) > 2 else OUT
    pdf = PDF()
    pdf.set_auto_page_break(auto=True, margin=15)
    pdf.add_page()
    with open(src, 'r', encoding='utf-8') as f:
        process_markdown(pdf, f.read())
    pdf.output(out)
    print('Generated:', os.path.normpath(out), '-', pdf.page_no(), 'pages')


if __name__ == '__main__':
    main()
