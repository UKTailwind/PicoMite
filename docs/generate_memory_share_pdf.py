#!/usr/bin/env python3
"""Generate PDF from MEMORY_SHARE_User_Manual.md"""

from fpdf import FPDF
import re

class PDF(FPDF):
    @staticmethod
    def _sanitize(text):
        """Replace non-latin1 characters with safe ASCII equivalents."""
        text = text.replace('\u2013', '-').replace('\u2014', '--')
        text = text.replace('\u2018', "'").replace('\u2019', "'")
        text = text.replace('\u201c', '"').replace('\u201d', '"')
        text = text.replace('\u00b2', '2')
        text = text.replace('\u2126', 'Ohm').replace('\u03a9', 'Ohm')
        text = text.replace('\u2192', '->')
        return text

    def header(self):
        self.set_font('Helvetica', 'B', 16)
        self.cell(0, 10, 'MEMORY SHARE Command User Manual', 0, 1, 'C')
        self.ln(5)

    def footer(self):
        self.set_y(-15)
        self.set_font('Helvetica', 'I', 8)
        self.cell(0, 10, f'Page {self.page_no()}', 0, 0, 'C')

    def section_title(self, title):
        self.set_font('Helvetica', 'B', 14)
        self.set_text_color(0, 51, 102)
        self.cell(0, 10, self._sanitize(title), 0, 1, 'L')
        self.set_text_color(0, 0, 0)
        self.ln(2)

    def subsection_title(self, title):
        self.set_font('Helvetica', 'B', 12)
        self.set_text_color(51, 51, 51)
        self.cell(0, 8, self._sanitize(title), 0, 1, 'L')
        self.set_text_color(0, 0, 0)
        self.ln(1)

    def subsubsection_title(self, title):
        self.set_font('Helvetica', 'B', 11)
        self.set_text_color(80, 80, 80)
        self.cell(0, 7, self._sanitize(title), 0, 1, 'L')
        self.set_text_color(0, 0, 0)
        self.ln(1)

    def body_text(self, text):
        self.set_font('Helvetica', '', 10)
        self.multi_cell(0, 5, self._sanitize(text))
        self.ln(2)

    def code_block(self, code):
        self.set_font('Courier', '', 9)
        self.set_fill_color(245, 245, 245)
        code = self._sanitize(code)
        for line in code.split('\n'):
            if line.strip():
                self.cell(5, 5, '', 0, 0)
                self.cell(0, 5, line, 0, 1, fill=True)
            else:
                self.ln(3)
        self.ln(3)

    def table_row(self, cells, header=False, col_widths=None):
        if header:
            self.set_font('Helvetica', 'B', 9)
            self.set_fill_color(220, 220, 220)
        else:
            self.set_font('Helvetica', '', 9)
            self.set_fill_color(255, 255, 255)

        if col_widths is None:
            col_widths = [50, 140] if len(cells) == 2 else [40] * len(cells)

        cleaned_cells = []
        for cell in cells:
            cell = self._sanitize(cell)
            cell = cell.replace('<br>', ' ')
            cleaned_cells.append(cell)

        line_height = 5
        max_lines = 1
        for i, cell in enumerate(cleaned_cells):
            chars_per_line = int(col_widths[i] / 2.2)
            if chars_per_line > 0:
                lines = (len(cell) // chars_per_line) + 1
                if lines > max_lines:
                    max_lines = lines

        row_height = line_height * max_lines

        x_start = self.get_x()
        y_start = self.get_y()

        for i, cell in enumerate(cleaned_cells):
            self.rect(x_start + sum(col_widths[:i]), y_start, col_widths[i], row_height, 'DF' if header else 'D')
            self.set_xy(x_start + sum(col_widths[:i]) + 1, y_start + 1)
            self.multi_cell(col_widths[i] - 2, line_height, cell, border=0, align='L')

        self.set_xy(x_start, y_start + row_height)

def main():
    pdf = PDF()
    pdf.add_page()

    with open('MEMORY_SHARE_User_Manual.md', 'r', encoding='utf-8') as f:
        content = f.read()

    lines = content.split('\n')
    in_code_block = False
    code_buffer = []
    in_table = False
    table_rows = []

    i = 0
    while i < len(lines):
        line = lines[i]

        # Skip the title (already in header)
        if line.startswith('# MEMORY SHARE'):
            i += 1
            continue

        # Skip LaTeX math blocks (not renderable in simple PDF)
        if line.startswith('$$'):
            i += 1
            # Skip until closing $$
            while i < len(lines) and not lines[i].startswith('$$'):
                i += 1
            i += 1  # skip closing $$
            continue

        # Code block handling
        if line.startswith('```'):
            if in_code_block:
                pdf.code_block('\n'.join(code_buffer))
                code_buffer = []
                in_code_block = False
            else:
                in_code_block = True
            i += 1
            continue

        if in_code_block:
            code_buffer.append(line)
            i += 1
            continue

        # Table handling
        if line.startswith('|'):
            if not in_table:
                in_table = True
                table_rows = []

            if '---' in line:
                i += 1
                continue

            cells = [c.strip() for c in line.split('|')[1:-1]]
            if cells:
                table_rows.append(cells)
            i += 1
            continue
        elif in_table:
            in_table = False
            if table_rows:
                header = table_rows[0] if table_rows else []
                if len(header) == 2:
                    if 'Error' in header[0]:
                        col_widths = [80, 110]
                    elif 'Parameter' in header[0]:
                        col_widths = [45, 145]
                    elif 'Clock Divider' in header[0]:
                        col_widths = [40, 60, 90]
                    else:
                        col_widths = [60, 130]
                elif len(header) == 3:
                    if 'Clock Divider' in header[0]:
                        col_widths = [40, 60, 90]
                    elif 'Host Pin' in header[0] or 'Board A' in header[0]:
                        col_widths = [50, 50, 90]
                    else:
                        col_widths = [63, 63, 64]
                else:
                    col_widths = None

                for j, row in enumerate(table_rows):
                    pdf.table_row(row, header=(j == 0), col_widths=col_widths)
                pdf.ln(3)
            table_rows = []

        # Section headers
        if line.startswith('#### '):
            pdf.subsubsection_title(line[5:])
            i += 1
            continue

        if line.startswith('### '):
            pdf.subsection_title(line[4:])
            i += 1
            continue

        if line.startswith('## '):
            pdf.section_title(line[3:])
            i += 1
            continue

        # Regular text
        if line.strip():
            text = line.replace('**', '')
            text = re.sub(r'`([^`]+)`', r'\1', text)
            if text.startswith('- '):
                text = '  - ' + text[2:]
            elif re.match(r'^\d+\. ', text):
                text = '  ' + text
            pdf.body_text(text)

        i += 1

    # Handle any remaining table
    if in_table and table_rows:
        for j, row in enumerate(table_rows):
            pdf.table_row(row, header=(j == 0))

    pdf.output('MEMORY_SHARE_User_Manual.pdf')
    print('PDF generated: MEMORY_SHARE_User_Manual.pdf')

if __name__ == '__main__':
    main()
