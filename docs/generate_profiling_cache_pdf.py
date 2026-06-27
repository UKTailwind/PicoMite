#!/usr/bin/env python3
"""Generate PDF from docs/option-profiling-cache.md"""

import re
import os
from fpdf import FPDF

UNICODE_MAP = {
    '–': '-', '—': '--',
    '‘': "'", '’': "'",
    '“': '"', '”': '"',
    '→': '->', '←': '<-',
    '≥': '>=', '≤': '<=',
    '²': '2', 'µ': 'u',
    '∞': 'inf', '≈': '~=',
    'μ': 'u', 'Ω': 'Ohm',
    'Ω': 'Ohm', '×': 'x',
    '÷': '/', '°': ' deg',
}

def clean(text):
    for ch, rep in UNICODE_MAP.items():
        text = text.replace(ch, rep)
    # Encode to latin-1, replacing any remaining non-latin-1 chars with '?'
    text = text.encode('latin-1', errors='replace').decode('latin-1')
    return text

class PDF(FPDF):
    def header(self):
        self.set_font('Helvetica', 'B', 16)
        self.set_text_color(0, 51, 102)
        self.cell(0, 10, 'OPTION PROFILING and OPTION TRACECACHE', 0, 1, 'C')
        self.set_font('Helvetica', '', 11)
        self.cell(0, 6, 'PicoMite MMBasic - Performance Optimisation Guide', 0, 1, 'C')
        self.set_text_color(0, 0, 0)
        self.ln(4)

    def footer(self):
        self.set_y(-15)
        self.set_font('Helvetica', 'I', 8)
        self.set_text_color(128, 128, 128)
        self.cell(0, 10, f'Page {self.page_no()}', 0, 0, 'C')
        self.set_text_color(0, 0, 0)

    def h1(self, title):
        self.ln(4)
        self.set_font('Helvetica', 'B', 14)
        self.set_text_color(0, 51, 102)
        self.set_fill_color(230, 240, 255)
        self.cell(0, 9, clean(title), 0, 1, 'L', fill=True)
        self.set_text_color(0, 0, 0)
        self.ln(2)

    def h2(self, title):
        self.ln(2)
        self.set_font('Helvetica', 'B', 12)
        self.set_text_color(30, 80, 150)
        self.cell(0, 8, clean(title), 0, 1, 'L')
        self.set_text_color(0, 0, 0)
        self.ln(1)

    def h3(self, title):
        self.ln(1)
        self.set_font('Helvetica', 'B', 10)
        self.set_text_color(60, 60, 60)
        self.cell(0, 7, clean(title), 0, 1, 'L')
        self.set_text_color(0, 0, 0)

    def body(self, text):
        self.set_font('Helvetica', '', 10)
        self.multi_cell(0, 5, clean(text))
        self.ln(1)

    def blockquote(self, text):
        """Render a > blockquote in a light-grey shaded box."""
        self.set_font('Helvetica', 'I', 9)
        self.set_fill_color(245, 245, 200)
        self.set_left_margin(self.l_margin + 5)
        self.multi_cell(0, 5, clean(text), fill=True)
        self.set_left_margin(self.l_margin - 5)
        self.ln(2)

    def code(self, text):
        self.set_font('Courier', '', 8)
        self.set_fill_color(245, 245, 245)
        self.set_draw_color(200, 200, 200)
        x = self.get_x()
        for line in text.split('\n'):
            self.cell(5, 4.5, '', 0, 0)
            self.cell(0, 4.5, clean(line), 0, 1, fill=True)
        self.set_draw_color(0, 0, 0)
        self.ln(2)

    def bullet(self, text):
        self.set_font('Helvetica', '', 10)
        x = self.get_x()
        self.cell(5, 5, '-', 0, 0)
        self.multi_cell(0, 5, clean(text))

    def rule(self):
        self.set_draw_color(180, 180, 180)
        self.line(self.l_margin, self.get_y(), self.w - self.r_margin, self.get_y())
        self.set_draw_color(0, 0, 0)
        self.ln(3)

    def table_rows(self, rows):
        if not rows:
            return
        col_count = len(rows[0])
        page_w = self.w - self.l_margin - self.r_margin
        if col_count == 2:
            col_widths = [int(page_w * 0.35), int(page_w * 0.65)]
        elif col_count == 3:
            col_widths = [int(page_w * 0.30), int(page_w * 0.35), int(page_w * 0.35)]
        elif col_count == 4:
            col_widths = [int(page_w * 0.25)] * 4
        elif col_count == 5:
            col_widths = [int(page_w * 0.15), int(page_w * 0.15), int(page_w * 0.15), int(page_w * 0.15), int(page_w * 0.40)]
        else:
            w = int(page_w / col_count)
            col_widths = [w] * col_count

        for ri, row in enumerate(rows):
            is_header = (ri == 0)
            if is_header:
                self.set_font('Helvetica', 'B', 9)
                self.set_fill_color(200, 215, 235)
            else:
                self.set_font('Helvetica', '', 9)
                self.set_fill_color(255, 255, 255) if ri % 2 == 0 else self.set_fill_color(248, 248, 248)

            x0 = self.get_x()
            y0 = self.get_y()
            lh = 5
            # Measure max height
            max_h = lh
            for ci, cell in enumerate(row):
                cw = col_widths[ci] if ci < len(col_widths) else col_widths[-1]
                approx_lines = max(1, len(clean(cell)) // max(1, int(cw / 2.1)) + 1)
                max_h = max(max_h, approx_lines * lh)

            # Manual page break: rects/multi_cell below would otherwise render
            # off the page bottom and leave the cursor past the margin, which
            # later triggers a fresh page break for every subsequent paragraph.
            if y0 + max_h > self.h - self.b_margin:
                self.add_page()
                x0 = self.get_x()
                y0 = self.get_y()
                # Re-emit the header row at the top of the new page so split
                # tables stay readable.
                if not is_header and len(rows) > 1:
                    saved_font = ('Helvetica', 'B', 9)
                    self.set_font(*saved_font)
                    self.set_fill_color(200, 215, 235)
                    hdr = rows[0]
                    hdr_h = lh
                    for ci, cell in enumerate(hdr):
                        cw = col_widths[ci] if ci < len(col_widths) else col_widths[-1]
                        approx_lines = max(1, len(clean(cell)) // max(1, int(cw / 2.1)) + 1)
                        hdr_h = max(hdr_h, approx_lines * lh)
                    for ci, cell in enumerate(hdr):
                        cw = col_widths[ci] if ci < len(col_widths) else col_widths[-1]
                        self.rect(x0 + sum(col_widths[:ci]), y0, cw, hdr_h, 'DF')
                        self.set_xy(x0 + sum(col_widths[:ci]) + 1, y0 + 1)
                        self.multi_cell(cw - 2, lh, clean(cell), border=0)
                    y0 += hdr_h
                    self.set_xy(x0, y0)
                    # Restore the body-row font/fill for the actual row about to draw
                    self.set_font('Helvetica', '', 9)
                    self.set_fill_color(255, 255, 255) if ri % 2 == 0 else self.set_fill_color(248, 248, 248)

            for ci, cell in enumerate(row):
                cw = col_widths[ci] if ci < len(col_widths) else col_widths[-1]
                self.rect(x0 + sum(col_widths[:ci]), y0, cw, max_h, 'DF')
                self.set_xy(x0 + sum(col_widths[:ci]) + 1, y0 + 1)
                self.multi_cell(cw - 2, lh, clean(cell), border=0)

            self.set_xy(x0, y0 + max_h)

        self.ln(3)


def parse_and_render(md_path, pdf):
    with open(md_path, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    in_code = False
    code_buf = []
    in_table = False
    table_rows_buf = []
    in_blockquote = False
    bq_buf = []

    def flush_table():
        nonlocal in_table, table_rows_buf
        if table_rows_buf:
            pdf.table_rows(table_rows_buf)
        in_table = False
        table_rows_buf = []

    def flush_bq():
        nonlocal in_blockquote, bq_buf
        if bq_buf:
            pdf.blockquote('\n'.join(bq_buf))
        in_blockquote = False
        bq_buf = []

    i = 0
    while i < len(lines):
        raw = lines[i].rstrip('\n')
        line = raw.rstrip()

        # Code block toggle
        if line.startswith('```'):
            if in_table:
                flush_table()
            if in_blockquote:
                flush_bq()
            if in_code:
                pdf.code('\n'.join(code_buf))
                code_buf = []
                in_code = False
            else:
                in_code = True
            i += 1
            continue

        if in_code:
            code_buf.append(raw)
            i += 1
            continue

        # Blockquote
        if line.startswith('> '):
            if in_table:
                flush_table()
            bq_buf.append(line[2:])
            in_blockquote = True
            i += 1
            continue
        elif in_blockquote and not line.startswith('>'):
            flush_bq()

        # Table rows
        if line.startswith('|'):
            if in_blockquote:
                flush_bq()
            if not in_table:
                in_table = True
                table_rows_buf = []
            if re.match(r'^\|[-| :]+\|$', line):
                i += 1
                continue
            cells = [c.strip() for c in line.split('|')[1:-1]]
            # Strip markdown bold/inline-code
            cells = [re.sub(r'\*\*([^*]+)\*\*', r'\1', c) for c in cells]
            cells = [re.sub(r'`([^`]+)`', r'\1', c) for c in cells]
            table_rows_buf.append(cells)
            i += 1
            continue
        elif in_table:
            flush_table()

        # Horizontal rule
        if line == '---' or line == '***':
            pdf.rule()
            i += 1
            continue

        # Headers
        if line.startswith('# '):
            # Top-level title — skip (already in PDF header), unless it's a section
            i += 1
            continue
        if line.startswith('## '):
            pdf.h1(line[3:])
            i += 1
            continue
        if line.startswith('### '):
            pdf.h2(line[4:])
            i += 1
            continue
        if line.startswith('#### '):
            pdf.h3(line[5:])
            i += 1
            continue

        # Bullet list items
        if line.startswith('- ') or line.startswith('* '):
            text = line[2:]
            text = re.sub(r'\*\*([^*]+)\*\*', r'\1', text)
            text = re.sub(r'`([^`]+)`', r'\1', text)
            pdf.bullet(text)
            i += 1
            continue

        # Numbered list
        m = re.match(r'^\d+\. (.*)', line)
        if m:
            text = m.group(1)
            text = re.sub(r'\*\*([^*]+)\*\*', r'\1', text)
            text = re.sub(r'`([^`]+)`', r'\1', text)
            pdf.bullet(text)
            i += 1
            continue

        # Empty line
        if not line.strip():
            if in_blockquote:
                flush_bq()
            pdf.ln(2)
            i += 1
            continue

        # Regular paragraph text
        text = line
        text = re.sub(r'\*\*([^*]+)\*\*', r'\1', text)
        text = re.sub(r'`([^`]+)`', r'\1', text)
        text = re.sub(r'\*([^*]+)\*', r'\1', text)
        pdf.body(text)
        i += 1

    # Flush any open state
    if in_code:
        pdf.code('\n'.join(code_buf))
    if in_table:
        flush_table()
    if in_blockquote:
        flush_bq()


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    md_path = os.path.join(script_dir, 'docs', 'option-profiling-cache.md')
    pdf_path = os.path.join(script_dir, 'docs', 'option-profiling-cache.pdf')

    pdf = PDF()
    pdf.set_margins(15, 20, 15)
    pdf.set_auto_page_break(auto=True, margin=20)
    pdf.add_page()

    parse_and_render(md_path, pdf)

    pdf.output(pdf_path)
    print(f'PDF generated: {pdf_path}')


if __name__ == '__main__':
    main()
