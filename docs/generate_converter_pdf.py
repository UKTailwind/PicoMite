#!/usr/bin/env python3
"""Generate CONVERTER.pdf from badapple/CONVERTER.md (matches the docs/ house style)."""

import os
import re
from fpdf import FPDF

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.normpath(os.path.join(HERE, '..', 'badapple', 'CONVERTER.md'))
OUT = os.path.normpath(os.path.join(HERE, '..', 'badapple', 'CONVERTER.pdf'))


class PDF(FPDF):
    def header(self):
        self.set_font('Helvetica', 'B', 16)
        self.cell(0, 10, 'vid2rgb121 - Video Converter Guide', 0, 1, 'C')
        self.ln(4)

    def footer(self):
        self.set_y(-15)
        self.set_font('Helvetica', 'I', 8)
        self.cell(0, 10, f'Page {self.page_no()}', 0, 0, 'C')

    def section_title(self, title):
        self.ln(2)
        self.set_font('Helvetica', 'B', 14)
        self.set_text_color(0, 51, 102)
        self.cell(0, 10, self._sanitize(title), 0, 1, 'L')
        self.set_text_color(0, 0, 0)
        self.ln(1)

    def subsection_title(self, title):
        self.set_font('Helvetica', 'B', 12)
        self.set_text_color(51, 51, 51)
        self.cell(0, 8, self._sanitize(title), 0, 1, 'L')
        self.set_text_color(0, 0, 0)
        self.ln(1)

    @staticmethod
    def _sanitize(text):
        replacements = {
            '–': '-', '—': '--',
            '‘': "'", '’': "'",
            '“': '"', '”': '"',
            '→': '->', '←': '<-',
            '…': '...', '×': 'x', '≤': '<=', '≥': '>=',
            '⚠': '(!)', '️': '', '\U0001f4a1': '(tip)',
            '•': '-',
        }
        for src, dst in replacements.items():
            text = text.replace(src, dst)
        return text.encode('latin-1', errors='replace').decode('latin-1')

    def body_text(self, text):
        self.set_font('Helvetica', '', 10)
        self.multi_cell(0, 5, self._sanitize(text))
        self.ln(1)

    def note_text(self, text):
        self.set_font('Helvetica', 'I', 10)
        self.set_text_color(120, 70, 0)
        self.multi_cell(0, 5, self._sanitize(text))
        self.set_text_color(0, 0, 0)
        self.ln(1)

    def code_block(self, code):
        self.set_font('Courier', '', 9)
        self.set_fill_color(245, 245, 245)
        for line in self._sanitize(code).split('\n'):
            self.cell(4, 5, '', 0, 0)
            self.cell(0, 5, line, 0, 1, fill=True)
        self.ln(2)

    def _count_wrapped_lines(self, text, cell_width, font_style, font_size):
        self.set_font('Helvetica', font_style, font_size)
        effective = cell_width - 2
        if not text.strip():
            return 1
        lines, line_w = 1, 0.0
        sp = self.get_string_width(' ')
        for word in text.split(' '):
            ww = self.get_string_width(word)
            if line_w == 0.0:
                line_w = ww
            elif line_w + sp + ww > effective:
                lines += 1
                line_w = ww
            else:
                line_w += sp + ww
        return lines

    def table_row(self, cells, header=False, col_widths=None):
        font_style = 'B' if header else ''
        if col_widths is None:
            col_widths = [60, 130] if len(cells) == 2 else [190 / len(cells)] * len(cells)

        cleaned = []
        for cell in cells:
            cell = self._sanitize(cell)
            cell = re.sub(r'`([^`]+)`', r'\1', cell)
            cell = cell.replace('**', '')
            cleaned.append(cell)

        line_height = 5
        max_lines = 1
        for idx, cell in enumerate(cleaned):
            max_lines = max(max_lines, self._count_wrapped_lines(cell, col_widths[idx], font_style, 9))
        row_height = line_height * max_lines + 2

        if self.get_y() + row_height > self.h - self.b_margin:
            self.add_page()

        if header:
            self.set_font('Helvetica', 'B', 9)
            self.set_fill_color(220, 220, 220)
        else:
            self.set_font('Helvetica', '', 9)
            self.set_fill_color(255, 255, 255)

        x_start, y_start = self.get_x(), self.get_y()
        for idx in range(len(cleaned)):
            self.rect(x_start + sum(col_widths[:idx]), y_start, col_widths[idx], row_height, 'DF')
        for idx, cell in enumerate(cleaned):
            self.set_xy(x_start + sum(col_widths[:idx]) + 1, y_start + 1)
            self.multi_cell(col_widths[idx] - 2, line_height, cell, border=0, align='L')
        self.set_xy(x_start, y_start + row_height)


def widths_for(rows):
    n = len(rows[0]) if rows else 2
    if n == 2:
        return [60, 130]
    if n == 3:
        return [40, 28, 122]
    return None


def emit_table(pdf, rows):
    if not rows:
        return
    cw = widths_for(rows)
    for j, row in enumerate(rows):
        pdf.table_row(row, header=(j == 0), col_widths=cw)
    pdf.ln(3)


def main():
    pdf = PDF()
    pdf.set_auto_page_break(auto=True, margin=15)
    pdf.add_page()

    with open(SRC, 'r', encoding='utf-8') as f:
        content = f.read()

    lines = content.split('\n')
    in_code = False
    code_buf = []
    table_rows = []
    in_table = False

    def flush_table():
        nonlocal in_table, table_rows
        if in_table:
            emit_table(pdf, table_rows)
            table_rows = []
            in_table = False

    for line in lines:
        # code fences
        if line.startswith('```'):
            if in_code:
                pdf.code_block('\n'.join(code_buf))
                code_buf = []
                in_code = False
            else:
                flush_table()
                in_code = True
            continue
        if in_code:
            code_buf.append(line)
            continue

        # tables
        if line.lstrip().startswith('|'):
            if re.match(r'^\s*\|[\s:|-]+\|\s*$', line):
                continue  # separator row
            cells = [c.strip() for c in line.strip().strip('|').split('|')]
            table_rows.append(cells)
            in_table = True
            continue
        elif in_table:
            flush_table()

        # headings
        if line.startswith('# '):
            continue  # title is in the page header
        if line.startswith('## '):
            pdf.section_title(line[3:].strip())
            continue
        if line.startswith('### '):
            pdf.subsection_title(line[4:].strip())
            continue
        if line.strip() == '---':
            continue

        stripped = line.strip()
        if not stripped:
            continue

        # inline cleanups
        text = re.sub(r'`([^`]+)`', r'\1', stripped)
        text = text.replace('**', '')

        if text.startswith('> '):
            pdf.note_text(text[2:])
        elif text.startswith('* ') or text.startswith('- '):
            pdf.body_text('  - ' + text[2:])
        elif re.match(r'^\d+\.\s', text):
            pdf.body_text('  ' + text)
        else:
            pdf.body_text(text)

    flush_table()
    pdf.output(OUT)
    print('PDF generated:', OUT)


if __name__ == '__main__':
    main()
