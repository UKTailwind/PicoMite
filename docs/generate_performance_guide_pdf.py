"""Convert MMBasic_Performance_Guide.md to PDF/MMBasic_Performance_Guide.pdf.

fpdf 1.7.2 lays the document out (headings, paragraphs with bold, italic and
code, lists, code blocks and tables whose cells wrap); pypdf then adds a
bookmark for every section.  Run from anywhere: python docs/generate_performance_guide_pdf.py
"""
import io
import os
import re
from fpdf import FPDF
from pypdf import PdfReader, PdfWriter

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, 'MMBasic_Performance_Guide.md')
OUT = os.path.join(HERE, '..', 'PDF', 'MMBasic_Performance_Guide.pdf')
TITLE = 'MMBasic Performance Guide'

BODY = 10      # body text size (pt)
LH = 5         # body line height (mm)
CELL = 8.5     # table text size (pt)
CELL_LH = 4.2  # table line height (mm)
PAD = 1.4      # table cell padding (mm)


def latin1(text):
    return (text.replace('—', '-').replace('–', '-')
            .replace('‘', "'").replace('’', "'")
            .replace('“', '"').replace('”', '"')
            .replace('≤', '<=').replace('≥', '>='))


def segments(text):
    """Split inline markdown into (text, style) pieces; style is a set of
    'B', 'I' and 'C' (code).  ** and * toggle bold and italic; a `span` is code."""
    out, bold, italic = [], False, False
    # a * with a space on each side is multiplication, not emphasis (as in Markdown)
    text = latin1(text).replace(' * ', ' \x00 ')
    for tok in re.split(r'(`[^`]+`|\*\*|\*)', text):
        tok = tok.replace('\x00', '*')
        if tok == '':
            continue
        if tok == '**':
            bold = not bold
        elif tok == '*':
            italic = not italic
        elif tok.startswith('`') and tok.endswith('`') and len(tok) > 1:
            out.append((tok[1:-1], frozenset({'C'} | ({'B'} if bold else set()))))
        else:
            out.append((tok, frozenset(({'B'} if bold else set()) | ({'I'} if italic else set()))))
    return out


class Doc(FPDF):
    def __init__(self):
        FPDF.__init__(self, 'P', 'mm', 'A4')
        self.set_margins(18, 16, 18)
        self.set_auto_page_break(True, 18)
        self.marks = []          # (level, title, page)

    def header(self):
        if self.page_no() > 1:
            self.set_font('Helvetica', 'I', 8)
            self.set_text_color(110, 110, 110)
            self.cell(0, 5, TITLE + '  -  MMBasic V7.0.00b7', 0, 1, 'R')
            self.set_text_color(0, 0, 0)
            self.ln(2)

    def footer(self):
        self.set_y(-12)
        self.set_font('Helvetica', 'I', 8)
        self.set_text_color(110, 110, 110)
        self.cell(0, 5, 'Page %d' % self.page_no(), 0, 0, 'C')
        self.set_text_color(0, 0, 0)

    def width(self):
        return self.w - self.l_margin - self.r_margin

    def style(self, st, size):
        if 'C' in st:
            self.set_font('Courier', 'B' if 'B' in st else '', size)
        else:
            self.set_font('Helvetica', ('B' if 'B' in st else '') + ('I' if 'I' in st else ''), size)

    # ---- running text
    def rich(self, text, size=BODY, lh=LH):
        for t, st in segments(text):
            self.style(st, size - (0.5 if 'C' in st else 0))
            self.write(lh, t)
        self.ln(lh)

    def item(self, marker, text, indent):
        left = self.l_margin
        self.set_x(left + indent - 4.5)
        self.set_font('Helvetica', '', BODY)
        self.cell(4.5, LH, marker, 0, 0)
        self.set_left_margin(left + indent)
        self.rich(text)
        self.set_left_margin(left)
        self.set_x(left)

    def heading(self, level, text):
        size, gap = {1: (20, 0), 2: (14, 6), 3: (11.5, 4)}[level]
        if self.get_y() > self.page_break_trigger - (60 if level == 2 else 35):
            self.add_page()
        self.ln(gap)
        plain = ''.join(t for t, _ in segments(text))
        if level > 1:
            self.marks.append((level, plain, self.page_no()))
        self.set_font('Helvetica', 'B', size)
        self.multi_cell(0, size * 0.45, plain)
        if level == 2:
            y = self.get_y() + 0.8
            self.set_draw_color(160, 160, 160)
            self.line(self.l_margin, y, self.w - self.r_margin, y)
            self.set_draw_color(0, 0, 0)
            self.ln(2.5)
        else:
            self.ln(1.5)

    def code(self, lines):
        self.set_font('Courier', '', 9)
        self.set_fill_color(242, 242, 242)
        h = 4.3 * len(lines) + 3
        if self.get_y() + h > self.page_break_trigger:
            self.add_page()
        self.ln(1)
        self.multi_cell(0, 4.3, latin1('\n'.join(lines)), 0, 'L', True)
        self.ln(2)

    # ---- tables
    def layout(self, segs, w):
        """Lines of (text, style) words that fit width w."""
        lines, line, lw = [], [], 0.0
        for t, st in segs:
            for word in re.split(r'(\s+)', t):
                if word == '':
                    continue
                self.style(st, CELL - (0.5 if 'C' in st else 0))
                ww = self.get_string_width(word)
                if word.isspace():
                    if line:
                        line.append((' ', st, ww))
                        lw += ww
                    continue
                if line and lw + ww > w:
                    while line and line[-1][0] == ' ':
                        lw -= line.pop()[2]
                    lines.append(line)
                    line, lw = [], 0.0
                line.append((word, st, ww))
                lw += ww
        while line and line[-1][0] == ' ':
            line.pop()
        if line:
            lines.append(line)
        return lines or [[]]

    def natural(self, segs):
        total = 0.0
        for t, st in segs:
            self.style(st, CELL - (0.5 if 'C' in st else 0))
            total += self.get_string_width(t)
        return total

    def draw_row(self, cells, widths, head):
        laid = [self.layout(c, w - 2 * PAD) for c, w in zip(cells, widths)]
        h = max(len(l) for l in laid) * CELL_LH + 2 * PAD
        return laid, h

    def put_row(self, laid, cells, widths, h, head):
        x0, y0 = self.l_margin, self.get_y()
        x = x0
        for lines, c, w in zip(laid, cells, widths):
            if head:
                self.set_fill_color(225, 225, 225)
                self.rect(x, y0, w, h, 'DF')
            else:
                self.rect(x, y0, w, h)
            plain = ''.join(t for t, _ in c).strip()
            right = (not head) and re.fullmatch(r'[-\d.,]+( ms|x)?', plain) is not None
            y = y0 + PAD
            for line in lines:
                lw = sum(ww for _, _, ww in line)
                xx = x + w - PAD - lw if right else x + PAD
                for word, st, ww in line:
                    self.style(st, CELL - (0.5 if 'C' in st else 0))
                    self.set_xy(xx, y)
                    self.cell(ww, CELL_LH, word, 0, 0)
                    xx += ww
                y += CELL_LH
            x += w
        self.set_xy(x0, y0 + h)

    def table(self, rows):
        head = [[(t, st | {'B'}) for t, st in segments(c)] for c in rows[0]]
        body = [[segments(c) for c in r] for r in rows[2:]]
        n = len(head)
        body = [r + [[]] * (n - len(r)) for r in body]
        nat = [max(self.natural(r[j]) for r in [head] + body) + 2 * PAD + 0.5 for j in range(n)]
        W = self.width()
        if sum(nat) <= W:
            widths = nat[:]
            k = max(range(n), key=lambda j: nat[j])      # the widest column takes the slack
            widths[k] += W - sum(nat)
        else:
            short = [j for j in range(n) if nat[j] <= 30]
            rest = W - sum(nat[j] for j in short)
            long_total = sum(nat[j] for j in range(n) if j not in short)
            widths = [nat[j] if j in short else max(22.0, rest * nat[j] / long_total) for j in range(n)]
            scale = W / sum(widths)
            widths = [w * scale for w in widths]
        self.set_line_width(0.15)
        hl, hh = self.draw_row(head, widths, True)
        if self.get_y() + hh + 2 * CELL_LH + 2 * PAD > self.page_break_trigger:
            self.add_page()
        self.put_row(hl, head, widths, hh, True)
        for r in body:
            laid, h = self.draw_row(r, widths, False)
            if self.get_y() + h > self.page_break_trigger:
                self.add_page()
                self.put_row(hl, head, widths, hh, True)
            self.put_row(laid, r, widths, h, False)
        self.ln(3)


def split_row(line):
    cells = line.strip().strip('|').split('|')
    return [c.strip() for c in cells]


def render(pdf, md):
    lines = md.split('\n')
    i = 0
    while i < len(lines):
        line = lines[i]
        s = line.strip()
        if s.startswith('```'):
            block = []
            i += 1
            while i < len(lines) and not lines[i].strip().startswith('```'):
                block.append(lines[i])
                i += 1
            pdf.code(block)
            i += 1
            continue
        if s.startswith('|'):
            rows = []
            while i < len(lines) and lines[i].strip().startswith('|'):
                rows.append(split_row(lines[i]))
                i += 1
            pdf.table(rows)
            continue
        m = re.match(r'^(#{1,3}) (.*)$', line)
        if m:
            pdf.heading(len(m.group(1)), m.group(2))
            i += 1
            continue
        if s == '':
            pdf.ln(2)
            i += 1
            continue
        m = re.match(r'^(\s*)(- |\d+\. )(.*)$', line)
        if m:
            indent = 5 + 5 * (len(m.group(1)) // 2)
            marker = chr(149) if m.group(2) == '- ' else m.group(2).strip()
            text = m.group(3)
            i += 1
            while i < len(lines) and lines[i].startswith(' ' * (len(m.group(1)) + 2)) \
                    and not re.match(r'^\s*(- |\d+\. )', lines[i]):
                text += ' ' + lines[i].strip()
                i += 1
            pdf.item(marker, text, indent)
            continue
        para = s
        i += 1
        while i < len(lines):
            n = lines[i].strip()
            if n == '' or n.startswith('#') or n.startswith('```') or n.startswith('|') \
                    or re.match(r'^(- |\d+\. )', n):
                break
            para += ' ' + n
            i += 1
        pdf.rich(para)
    return pdf


def main():
    pdf = Doc()
    pdf.add_page()
    with open(SRC, encoding='utf-8') as f:
        render(pdf, f.read())
    data = pdf.output(dest='S').encode('latin-1')
    reader = PdfReader(io.BytesIO(data))
    writer = PdfWriter(clone_from=reader)
    parent = None
    for level, title, page in pdf.marks:
        if level == 2:
            parent = writer.add_outline_item(title, page - 1)
        else:
            writer.add_outline_item(title, page - 1, parent=parent)
    writer.add_metadata({'/Title': TITLE, '/Subject': 'Making MMBasic programs run faster (V7.0.00b7)'})
    with open(OUT, 'wb') as f:
        writer.write(f)
    print('Generated %s (%d pages)' % (os.path.normpath(OUT), len(reader.pages)))


if __name__ == '__main__':
    main()
