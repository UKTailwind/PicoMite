#!/usr/bin/env python3
"""Generate CONVERTER.pdf from docs/CONVERTER.md (matches the docs/ house style)."""

import os
import re
from fpdf import FPDF

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, 'CONVERTER.md')
OUT = os.path.join(HERE, 'CONVERTER.pdf')


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
            '•': '-', '\u2011': '-', '\u2260': '!=',
        }
        for src, dst in replacements.items():
            text = text.replace(src, dst)
        return text.encode('latin-1', errors='replace').decode('latin-1')

    @staticmethod
    def _plain(text):
        """Render inline markdown plain: *italic*, `code` and **bold** keep only their text."""
        text = re.sub(r'(?<![\w*`])\*([^*`\s][^*`]*?)\*(?![\w*`])', r'\1', text)
        text = re.sub(r'`([^`]+)`', r'\1', text)
        return text.replace('**', '')

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
        # Ask multi_cell itself, at the width table_row gives it: it keeps its own
        # margin inside that width and splits words too long for the column
        return max(1, len(self.multi_cell(cell_width - 2, 5, text, split_only=True)))

    def table_row(self, cells, header=False, col_widths=None):
        font_style = 'B' if header else ''
        if col_widths is None:
            col_widths = [60, 130] if len(cells) == 2 else [190 / len(cells)] * len(cells)

        cleaned = []
        for cell in cells:
            cleaned.append(self._plain(self._sanitize(cell)))

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
        # No automatic page break while the cells are drawn, so the row stays together
        auto_break = self.auto_page_break
        self.set_auto_page_break(False, self.b_margin)
        for idx, cell in enumerate(cleaned):
            self.set_xy(x_start + sum(col_widths[:idx]) + 1, y_start + 1)
            self.multi_cell(col_widths[idx] - 2, line_height, cell, border=0, align='L')
        self.set_auto_page_break(auto_break, self.b_margin)
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
        # code fences (a fence inside a list item is indented)
        if line.lstrip().startswith('```'):
            if in_code:
                pdf.code_block('\n'.join(code_buf))
                code_buf = []
                in_code = False
            else:
                flush_table()
                in_code = True
                fence_indent = len(line) - len(line.lstrip())
            continue
        if in_code:
            code_buf.append(line[fence_indent:] if line[:fence_indent].isspace() else line)
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
        text = PDF._plain(stripped)

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
