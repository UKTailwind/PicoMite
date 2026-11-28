"""Convert PicoMite_App_User_Guide.md to PDF/PicoMite_App_User_Guide.pdf using fpdf 1.7.2.

The renderer of generate_sql_pdf.py, with what a guide of several chapters needs: a title page, a page with the
front matter (About This Guide, Support, Copyright) and the contents with their page numbers (the document is laid
out twice: the first time to find the pages), each chapter (## heading) on a new page, column widths for a table
from a <!-- widths: a,b,c --> line before it (percentages), bold, italic and `code` (Courier) kept in paragraphs and
list items, and PDF bookmarks for the chapters and their sections (pypdf, after fpdf has written the file).

Run: python docs/generate_app_guide_pdf.py"""
import os
import re

from fpdf import FPDF

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, 'PicoMite_App_User_Guide.md')
OUT = os.path.join(HERE, '..', 'PDF', 'PicoMite_App_User_Guide.pdf')

NAME = 'PicoMite App User Guide'
SUBTITLE = 'The programs that come with the PicoMitePC3 and PicoMiteHDMIUSB firmware'
FIRMWARE = 'For firmware V8.00.00b5 and later'
DATE = '28 November 2026'
AUTHOR = 'Peter Mather'
URL = 'https://github.com/UKTailwind/PicoMite'

FRONT = [
    ('About This Guide',
     'This guide is for anyone using the programs that come with the PicoMitePC3 and PicoMiteHDMIUSB firmware: '
     'MicroPython, Doom, the NES and BBC Micro emulators, ScummVM, curl, and the SQLite engine of MMBasic\'s SQL '
     'command. '
     'It covers installing each one, starting it, its keys and leaving it. It assumes you can use the MMBasic '
     'command prompt and copy files to the SD card. The firmware, the programs and this guide are at ' + URL + '.'),
    ('Support',
     'Questions should be raised on the Back Shed forum ( http://www.thebackshed.com/forum/Microcontrollers ), '
     'and problems with the programs reported as issues of the repository above.'),
    ('Copyright',
     'This guide is copyright 2026 Peter Mather and is distributed under a Creative Commons '
     'Attribution-NonCommercial-ShareAlike 3.0 Australia license (CC BY-NC-SA 3.0), as the PicoMite User Manual is. '
     'The programs have their own licences (see Appendix A).'),
]

LATIN1 = {'—': '--', '–': '-', '‘': "'", '’': "'", '“': '"', '”': '"',
          '•': '-', '≤': '<=', '≥': '>=', '≠': '!=', '×': 'x', '→': '->'}


def latin1(text):
    for k, v in LATIN1.items():
        text = text.replace(k, v)
    return text.encode('latin-1', 'replace').decode('latin-1')


class PDF(FPDF):
    def header(self):
        if self.page_no() == 1:
            return
        self.set_font('Helvetica', 'I', 8)
        self.set_text_color(110, 110, 110)
        self.cell(0, 6, NAME, 0, 0, 'R')
        self.set_text_color(0, 0, 0)
        self.ln(10)

    def footer(self):
        if self.page_no() == 1:
            return
        self.set_y(-15)
        self.set_font('Helvetica', 'I', 8)
        self.cell(0, 10, 'Page %d' % self.page_no(), 0, 0, 'C')


# ---- inline text: **bold**, *italic*, `code` and [text](url), written as runs with pdf.write()
def runs(text):
    """The text as (style, string) runs: style '' 'B' 'I' or 'C' (Courier)."""
    out = []
    pos = 0
    pat = re.compile(r'`([^`]+)`|\*\*(.+?)\*\*|\*(?=\S)(.+?)(?<=\S)\*|\[(.+?)\]\(.+?\)')
    for m in pat.finditer(text):
        if m.start() > pos:
            out.append(('', text[pos:m.start()]))
        if m.group(1) is not None:
            out.append(('C', m.group(1)))
        elif m.group(2) is not None:
            out.extend(('B' if s == '' else s, t) for s, t in runs(m.group(2)))
        elif m.group(3) is not None:
            out.append(('I', m.group(3)))
        else:
            out.append(('', m.group(4)))
        pos = m.end()
    if pos < len(text):
        out.append(('', text[pos:]))
    return out


def plain(text):
    return ''.join(t for _, t in runs(text))


def write_rich(pdf, text, size=10, h=5):
    for style, t in runs(text):
        if style == 'C':
            pdf.set_font('Courier', '', size - 0.5)
        else:
            pdf.set_font('Helvetica', style, size)
        pdf.write(h, t)
    pdf.ln(h)


# ---- tables
def table_row(pdf, col_widths, cells, align, bold=False):
    """One row, each cell word-wrapped inside its border; the page turned first if the whole row does not fit
    (auto page break off while its cells are drawn, so a row never spills one cell per page)."""
    line_h = 5
    pdf.set_font('Helvetica', 'B' if bold else '', 9)
    lines = max(len(pdf.multi_cell(w - 2, line_h, t, split_only=True)) for w, t in zip(col_widths, cells))
    row_h = max(6, lines * line_h + 1)
    if pdf.get_y() + row_h > pdf.page_break_trigger:
        pdf.add_page()
    x, y = pdf.l_margin, pdf.get_y()
    auto = pdf.auto_page_break
    pdf.set_auto_page_break(False, pdf.b_margin)
    for w, t in zip(col_widths, cells):
        if bold:
            pdf.set_fill_color(230, 230, 230)
            pdf.rect(x, y, w, row_h, 'DF')
        else:
            pdf.rect(x, y, w, row_h)
        pdf.set_xy(x + 1, y + 0.5)
        pdf.multi_cell(w - 2, line_h, t, 0, align)
        x += w
    pdf.set_auto_page_break(auto, pdf.b_margin)
    pdf.set_xy(pdf.l_margin, y + row_h)


def split_row(line):
    s = line.strip()
    if s.startswith('|'):
        s = s[1:]
    if s.endswith('|'):
        s = s[:-1]
    return [c.strip() for c in s.split('|')]


def render_table(pdf, table_lines, widths):
    headers = split_row(table_lines[0])
    rows = [split_row(l) for l in table_lines[2:]]
    n = len(headers)
    page_w = pdf.w - pdf.l_margin - pdf.r_margin
    if widths and len(widths) == n:
        tot = float(sum(widths))
        col_widths = [page_w * w / tot for w in widths]
    else:
        col_widths = [page_w / n] * n
    # a table that fits on a page is kept on one: the page is turned first if it would be split
    total = 0
    for cells, bold in [(headers, True)] + [(r, False) for r in rows]:
        pdf.set_font('Helvetica', 'B' if bold else '', 9)
        lines = max(len(pdf.multi_cell(w - 2, 5, plain(t), split_only=True)) for w, t in zip(col_widths, cells))
        total += max(6, lines * 5 + 1)
    if pdf.get_y() + total > pdf.page_break_trigger and total < pdf.page_break_trigger - pdf.t_margin - 10:
        pdf.add_page()
    table_row(pdf, col_widths, [plain(h) for h in headers], 'L', bold=True)
    for r in rows:
        if len(r) != n:
            raise SystemExit('table row has %d cells, header %d: %s' % (len(r), n, r))
        table_row(pdf, col_widths, [plain(c) for c in r], 'L')


# ---- the document
def list_continuation(lines, i, text):
    while i < len(lines):
        nxt = lines[i].rstrip('\n')
        s = nxt.strip()
        if not nxt[:1].isspace() or s == '' or s.startswith('- ') or s.startswith('```') or s.startswith('|') \
                or re.match(r'^\d+\.\s', s):
            break
        text += ' ' + s
        i += 1
    return text, i


def title_page(pdf):
    pdf.add_page()
    pdf.set_y(70)
    pdf.set_font('Helvetica', 'B', 30)
    pdf.cell(0, 14, 'PicoMite', 0, 1, 'C')
    pdf.set_font('Helvetica', 'B', 24)
    pdf.cell(0, 12, 'App User Guide', 0, 1, 'C')
    pdf.ln(8)
    pdf.set_font('Helvetica', '', 13)
    pdf.multi_cell(0, 7, SUBTITLE, 0, 'C')
    pdf.ln(4)
    pdf.set_font('Helvetica', '', 11)
    pdf.cell(0, 7, FIRMWARE, 0, 1, 'C')
    pdf.cell(0, 7, DATE, 0, 1, 'C')
    pdf.set_y(220)
    pdf.set_font('Helvetica', '', 12)
    pdf.cell(0, 7, AUTHOR, 0, 1, 'C')
    pdf.ln(6)
    pdf.set_font('Helvetica', '', 10)
    pdf.cell(0, 6, 'The firmware, the programs and this guide are at', 0, 1, 'C')
    pdf.cell(0, 6, URL, 0, 1, 'C')


def front_page(pdf, chapters):
    pdf.add_page()
    for head, text in FRONT:
        pdf.set_font('Helvetica', 'B', 13)
        pdf.cell(0, 8, head, 0, 1)
        pdf.set_font('Helvetica', '', 10)
        pdf.multi_cell(0, 5, latin1(text), 0, 'L')
        pdf.ln(4)
    pdf.ln(2)
    pdf.set_font('Helvetica', 'B', 13)
    pdf.cell(0, 8, 'Contents', 0, 1)
    pdf.ln(1)
    for title, page in chapters:
        pdf.set_font('Helvetica', '', 11)
        w = pdf.w - pdf.l_margin - pdf.r_margin
        num = str(page) if page else ''
        tw = pdf.get_string_width(title)
        nw = pdf.get_string_width(num)
        dots = ''
        dw = pdf.get_string_width('.')
        if page:
            dots = '.' * max(2, int((w - tw - nw - 4) / dw))
        pdf.cell(w - nw - 2, 7, title + ' ' + dots, 0, 0)
        pdf.cell(nw + 2, 7, num, 0, 1, 'R')


def render(chapters_pages):
    """Lay the guide out; returns the PDF and the headings found: (level, title, page)."""
    with open(SRC, encoding='utf-8') as f:
        lines = [latin1(l) for l in f.readlines()]
    pdf = PDF()
    pdf.set_auto_page_break(auto=True, margin=20)
    pdf.set_margins(20, 15, 20)
    title_page(pdf)
    chapters = [l[3:].strip() for l in lines if l.startswith('## ')]
    front_page(pdf, [(plain(c), chapters_pages.get(plain(c))) for c in chapters])
    heads = []
    widths = None
    i = 0
    while i < len(lines):
        line = lines[i].rstrip('\n')
        s = line.strip()
        if line.startswith('# '):                                       # the title: on the title page
            i += 1
            continue
        m = re.match(r'<!--\s*widths:\s*([\d,\s]+)-->', s)
        if m:
            widths = [float(x) for x in m.group(1).split(',') if x.strip()]
            i += 1
            continue
        if s.startswith('```'):
            code = []
            i += 1
            while i < len(lines) and not lines[i].strip().startswith('```'):
                code.append(lines[i].rstrip('\n'))
                i += 1
            i += 1
            pdf.set_font('Courier', '', 9)
            pdf.set_fill_color(240, 240, 240)
            pdf.multi_cell(0, 4.5, '\n'.join(code), fill=True)
            pdf.ln(3)
            continue
        if s.startswith('|'):
            t = []
            while i < len(lines) and lines[i].strip().startswith('|'):
                t.append(lines[i].strip())
                i += 1
            render_table(pdf, t, widths)
            widths = None
            pdf.ln(3)
            continue
        if line.startswith('## '):
            title = plain(line[3:].strip())
            pdf.add_page()
            heads.append((1, title, pdf.page_no()))
            pdf.set_font('Helvetica', 'B', 18)
            pdf.cell(0, 10, title, 0, 1)
            pdf.ln(3)
            i += 1
            continue
        if line.startswith('### '):
            title = plain(line[4:].strip())
            if pdf.get_y() > pdf.page_break_trigger - 50:               # (a heading starts with some of its text)
                pdf.add_page()
            pdf.ln(3)
            heads.append((2, title, pdf.page_no()))
            pdf.set_font('Helvetica', 'B', 12)
            pdf.cell(0, 7, title, 0, 1)
            pdf.ln(1)
            i += 1
            continue
        if s == '':
            pdf.ln(2)
            i += 1
            continue
        m = re.match(r'^(\d+)\.\s', s)
        if m or s.startswith('- '):
            text = s[m.end():] if m else s[2:]
            text, i = list_continuation(lines, i + 1, text)
            pdf.set_font('Helvetica', '', 10)
            lm = pdf.l_margin
            pdf.set_x(lm)
            pdf.cell(7, 5, (m.group(1) + '.') if m else chr(149), 0, 0)
            pdf.set_left_margin(lm + 7)                                 # (the item's lines wrap to its indent)
            write_rich(pdf, text)
            pdf.set_left_margin(lm)
            pdf.set_x(lm)
            continue
        para = line
        i += 1
        while i < len(lines):
            nxt = lines[i].rstrip('\n')
            n = nxt.strip()
            if n == '' or nxt.startswith('#') or n.startswith('```') or n.startswith('|') or n.startswith('- ') \
                    or n.startswith('<!--') or re.match(r'^\d+\.\s', n):
                break
            para += ' ' + nxt
            i += 1
        write_rich(pdf, para.strip())
    return pdf, heads


def bookmarks(path, heads):
    from pypdf import PdfReader, PdfWriter
    r = PdfReader(path)
    w = PdfWriter()
    w.append(r)
    parent = None
    for level, title, page in heads:
        if level == 1:
            parent = w.add_outline_item(title, page - 1)
        else:
            w.add_outline_item(title, page - 1, parent=parent)
    w.add_metadata({'/Title': NAME, '/Author': AUTHOR, '/Subject': SUBTITLE})
    with open(path, 'wb') as f:
        w.write(f)


def main():
    _, heads = render({})                                               # pass 1: where the chapters fall
    pages = {t: p for lvl, t, p in heads if lvl == 1}
    pdf, heads2 = render(pages)                                         # pass 2: the contents with their pages
    if [(l, t, p) for l, t, p in heads2] != heads:
        raise SystemExit('the pages moved between the passes')
    out = os.path.abspath(OUT)
    pdf.output(out)
    bookmarks(out, heads2)
    print('Generated %s: %d pages, %d chapters' % (out, pdf.page_no(), len(pages)))


if __name__ == '__main__':
    main()
