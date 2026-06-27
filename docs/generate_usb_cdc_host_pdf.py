#!/usr/bin/env python3
"""Generate PDF from USB_CDC_Host_User_Manual.md"""

from fpdf import FPDF
import re

class PDF(FPDF):
    def header(self):
        self.set_font('Helvetica', 'B', 16)
        self.cell(0, 10, 'USB CDC Host Serial Ports (COM3-COM6) User Manual', 0, 1, 'C')
        self.ln(5)

    def footer(self):
        self.set_y(-15)
        self.set_font('Helvetica', 'I', 8)
        self.cell(0, 10, f'Page {self.page_no()}', 0, 0, 'C')

    def section_title(self, title):
        self.set_font('Helvetica', 'B', 14)
        self.set_text_color(0, 51, 102)
        self.cell(0, 10, title, 0, 1, 'L')
        self.set_text_color(0, 0, 0)
        self.ln(2)

    def subsection_title(self, title):
        self.set_font('Helvetica', 'B', 12)
        self.set_text_color(51, 51, 51)
        self.cell(0, 8, title, 0, 1, 'L')
        self.set_text_color(0, 0, 0)
        self.ln(1)

    def subsubsection_title(self, title):
        self.set_font('Helvetica', 'BI', 11)
        self.set_text_color(80, 80, 80)
        self.cell(0, 7, title, 0, 1, 'L')
        self.set_text_color(0, 0, 0)
        self.ln(1)

    @staticmethod
    def _sanitize(text):
        """Replace non-Latin-1 characters with ASCII equivalents."""
        replacements = {
            '\u2013': '-', '\u2014': '--',
            '\u2018': "'", '\u2019': "'",
            '\u201c': '"', '\u201d': '"',
            '\u2192': '->', '\u2190': '<-',
            '\u2026': '...',
            '\u00b7': '*',
            '\u2022': '-',
            '\u00d7': 'x',
            '\u00e9': 'e', '\u00e8': 'e', '\u00ea': 'e',
            '\u00e0': 'a', '\u00e2': 'a',
            '\u250c': '+', '\u2510': '+', '\u2514': '+', '\u2518': '+',
            '\u251c': '+', '\u2524': '+', '\u252c': '+', '\u2534': '+',
            '\u253c': '+',
            '\u2500': '-', '\u2502': '|',
            '\u2550': '=', '\u2551': '|',
            '\u2554': '+', '\u2557': '+', '\u255a': '+', '\u255d': '+',
            '\u2560': '+', '\u2563': '+', '\u2566': '+', '\u2569': '+',
            '\u256c': '+',
        }
        for src, dst in replacements.items():
            text = text.replace(src, dst)
        return text.encode('latin-1', errors='replace').decode('latin-1')

    def body_text(self, text):
        self.set_font('Helvetica', '', 10)
        text = self._sanitize(text)
        self.multi_cell(0, 5, text)
        self.ln(2)

    def code_block(self, code):
        self.set_font('Courier', '', 9)
        self.set_fill_color(245, 245, 245)
        code = self._sanitize(code)
        for line in code.split('\n'):
            self.cell(5, 5, '', 0, 0)
            self.cell(0, 5, line, 0, 1, fill=True)
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
            cell = re.sub(r'`([^`]+)`', r'\1', cell)
            cell = cell.replace('**', '')
            cleaned_cells.append(cell)

        line_height = 5
        max_lines = 1
        for idx, cell in enumerate(cleaned_cells):
            chars_per_line = int(col_widths[idx] / 2.2)
            if chars_per_line > 0:
                lines = (len(cell) // chars_per_line) + 1
                if lines > max_lines:
                    max_lines = lines

        row_height = line_height * max_lines

        x_start = self.get_x()
        y_start = self.get_y()

        for idx, cell in enumerate(cleaned_cells):
            self.rect(x_start + sum(col_widths[:idx]), y_start, col_widths[idx], row_height,
                      'DF' if header else 'D')
            self.set_xy(x_start + sum(col_widths[:idx]) + 1, y_start + 1)
            self.multi_cell(col_widths[idx] - 2, line_height, cell, border=0, align='L')

        self.set_xy(x_start, y_start + row_height)


def main():
    pdf = PDF()
    pdf.add_page()

    with open('USB_CDC_Host_User_Manual.md', 'r', encoding='utf-8') as f:
        content = f.read()

    # Pre-sanitize entire content
    content = PDF._sanitize(content)

    lines = content.split('\n')
    in_code_block = False
    code_buffer = []
    in_table = False
    table_rows = []

    i = 0
    while i < len(lines):
        line = lines[i]

        # Skip the top-level title (already in header)
        if line.startswith('# USB CDC'):
            i += 1
            continue

        # Code block
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

        # Table
        if line.startswith('|'):
            if not in_table:
                in_table = True
                table_rows = []
            if re.match(r'^\|[\s\-:]+\|', line):
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
                ncols = len(table_rows[0]) if table_rows else 2
                if ncols == 4:
                    col_widths = [30, 25, 25, 110]
                elif ncols == 3:
                    col_widths = [60, 50, 80]
                elif ncols == 2:
                    header_row = table_rows[0] if table_rows else []
                    h0 = header_row[0].lower() if header_row else ''
                    if 'error' in h0:
                        col_widths = [75, 115]
                    elif 'parameter' in h0:
                        col_widths = [40, 150]
                    else:
                        col_widths = [50, 140]
                else:
                    col_widths = None

                for j, row in enumerate(table_rows):
                    pdf.table_row(row, header=(j == 0), col_widths=col_widths)
                pdf.ln(3)
            table_rows = []

        # Section headings
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

        # Non-empty body text
        if line.strip():
            text = line.replace('**', '')
            text = re.sub(r'`([^`]+)`', r'\1', text)
            if text.startswith('- '):
                text = '  - ' + text[2:]
            elif re.match(r'^\d+\. ', text):
                text = '  ' + text
            pdf.body_text(text)

        i += 1

    # Flush any trailing table
    if in_table and table_rows:
        for j, row in enumerate(table_rows):
            pdf.table_row(row, header=(j == 0))

    pdf.output('USB_CDC_Host_User_Manual.pdf')
    print('PDF generated: USB_CDC_Host_User_Manual.pdf')


if __name__ == '__main__':
    main()
