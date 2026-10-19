#!/usr/bin/env python3
"""
Generate PDF from PLAY_SAMPLE_User_Manual.md
"""

from fpdf import FPDF
import re


class PDF(FPDF):
    def header(self):
        self.set_font('Helvetica', 'B', 16)
        self.cell(0, 10, 'PLAY SAMPLE Command User Manual', 0, 1, 'C')
        self.ln(5)

    def footer(self):
        self.set_y(-15)
        self.set_font('Helvetica', 'I', 8)
        self.cell(0, 10, f'Page {self.page_no()}', 0, 0, 'C')


def parse_markdown(filename):
    with open(filename, 'r', encoding='utf-8') as f:
        return f.read()


def sanitize(text):
    replacements = {
        '→': '->',
        '−': '-',
        '—': '-',
        '–': '-',
        '“': '"',
        '”': '"',
        '’': "'",
        '\\x1e': '<RS>',
    }
    for old, new in replacements.items():
        text = text.replace(old, new)
    return text


def plain(text):
    """Drop inline **bold** and `code` markers; a cell cannot switch font mid-text."""
    return text.replace('**', '').replace('`', '')


def add_section(pdf, title, level=1):
    if level == 1:
        pdf.set_font('Helvetica', 'B', 14)
        pdf.ln(5)
    elif level == 2:
        pdf.set_font('Helvetica', 'B', 12)
        pdf.ln(3)
    else:
        pdf.set_font('Helvetica', 'B', 11)
        pdf.ln(2)
    pdf.multi_cell(0, 6, sanitize(title))
    pdf.ln(2)


def add_text(pdf, text):
    pdf.set_font('Helvetica', '', 10)
    text = sanitize(plain(text.replace('`', '"')))
    pdf.multi_cell(0, 5, text)


def add_code(pdf, code):
    pdf.set_font('Courier', '', 9)
    pdf.set_fill_color(245, 245, 245)
    code = sanitize(code)
    for line in code.split('\n'):
        if line.strip():
            pdf.cell(0, 4, '  ' + line, 0, 1, fill=True)
        else:
            pdf.cell(0, 4, '', 0, 1, fill=True)
    pdf.ln(2)


def wrap_cell(pdf, text, w):
    """Wrap one cell's text to width w (current font), breaking over-long words."""
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
    return lines


def add_table(pdf, headers, rows):
    pdf.set_font('Helvetica', 'B', 9)
    num_cols = len(headers)
    page_width = 190

    if num_cols == 2:
        max_first_col = max(len(headers[0]), max((len(row[0]) for row in rows), default=0))
        max_second_col = max(len(headers[1]), max((len(row[1]) for row in rows), default=0))
        total_content = max_first_col + max_second_col
        if total_content > 0:
            first_ratio = max_first_col / total_content
            first_width = max(40, min(120, int(page_width * first_ratio)))
            second_width = page_width - first_width
            col_widths = [first_width, second_width]
        else:
            col_widths = [95, 95]
    elif num_cols == 3:
        col_widths = [50, 50, 90]
    elif num_cols == 4:
        col_widths = [45, 45, 45, 55]
    elif num_cols == 5:
        col_widths = [20, 55, 40, 25, 50]
    else:
        col_widths = [page_width // num_cols] * num_cols

    widths = [col_widths[i] if i < len(col_widths) else 40 for i in range(num_cols)]
    line_h = 4.5
    pad = 0.5                                   # text offset from the top of the row
    x0 = pdf.l_margin

    # Rows are word-wrapped, never cut short, and paginated here: auto page
    # break is off while a table is drawn, or a row near the bottom margin
    # spills its cells over several pages. set_auto_page_break(False) also
    # zeroes b_margin, so both are saved first and restored after.
    saved_auto, saved_bmargin = pdf.auto_page_break, pdf.b_margin
    bottom = pdf.h - saved_bmargin
    pdf.set_auto_page_break(False)

    def wrap_row(cells, style):
        pdf.set_font('Helvetica', style, 9)
        return [wrap_cell(pdf, sanitize(cells[i] if i < len(cells) else ''), widths[i])
                for i in range(num_cols)]

    def row_height(wrapped):
        return line_h * max(len(w) for w in wrapped) + 2 * pad

    def draw_row(cells, style, align):
        wrapped = wrap_row(cells, style)
        rh = row_height(wrapped)
        y0 = pdf.get_y()
        x = x0
        for i in range(num_cols):
            pdf.rect(x, y0, widths[i], rh)
            yy = y0 + pad
            for ln in wrapped[i]:
                pdf.set_xy(x + 1, yy)
                pdf.cell(widths[i] - 2, line_h, ln, 0, 0, align)
                yy += line_h
            x += widths[i]
        pdf.set_xy(x0, y0 + rh)

    head_h = row_height(wrap_row(headers, 'B'))
    first_h = row_height(wrap_row(rows[0], '')) if rows else 0
    if pdf.get_y() + head_h + first_h > bottom:     # header + first row must fit
        pdf.add_page()
    pdf.set_x(x0)
    draw_row(headers, 'B', 'C')
    for row in rows:
        if pdf.get_y() + row_height(wrap_row(row, '')) > bottom:
            pdf.add_page()                          # whole row on the next page
            pdf.set_x(x0)
            draw_row(headers, 'B', 'C')             # repeat the header there
        draw_row(row, '', 'L')

    pdf.set_auto_page_break(saved_auto, saved_bmargin)
    pdf.ln(3)


def process_markdown(pdf, content):
    lines = content.split('\n')
    i = 0
    in_code_block = False
    code_buffer = []
    in_table = False
    table_headers = []
    table_rows = []

    while i < len(lines):
        line = lines[i]

        if line.strip().startswith('```'):
            if in_code_block:
                add_code(pdf, '\n'.join(code_buffer))
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

        if '|' in line and not line.strip().startswith('|--'):
            cells = [plain(c.strip()) for c in line.split('|')[1:-1]]
            if cells:
                if not in_table:
                    table_headers = cells
                    in_table = True
                elif re.match(r'^\|[\s:|-]+\|?$', line.strip()):  # | :--- | separator row
                    pass
                else:
                    table_rows.append(cells)
            i += 1
            continue
        elif in_table and '|' not in line:
            if table_headers and table_rows:
                add_table(pdf, table_headers, table_rows)
            table_headers = []
            table_rows = []
            in_table = False

        if line.strip().startswith('|--') or line.strip().startswith('|-'):
            i += 1
            continue

        if line.startswith('# '):
            i += 1
            continue
        elif line.startswith('## '):
            add_section(pdf, line[3:].strip(), 1)
        elif line.startswith('### '):
            add_section(pdf, line[4:].strip(), 2)
        elif line.startswith('#### '):
            add_section(pdf, line[5:].strip(), 3)
        elif line.strip().startswith('**') and line.strip().endswith('**'):
            pdf.set_font('Helvetica', 'B', 10)
            pdf.ln(2)
            pdf.multi_cell(0, 5, sanitize(line.strip().replace('**', '')))
        elif line.strip().startswith('- '):
            pdf.set_font('Helvetica', '', 10)
            text = sanitize(plain(line.strip()[2:].replace('`', '"')))
            pdf.multi_cell(0, 5, '  - ' + text)
        elif re.match(r'^\d+\.', line.strip()):
            pdf.set_font('Helvetica', '', 10)
            text = sanitize(plain(line.strip().replace('`', '"')))
            pdf.multi_cell(0, 5, '  ' + text)
        elif line.strip():
            add_text(pdf, line.strip())

        i += 1

    if in_table and table_headers and table_rows:
        add_table(pdf, table_headers, table_rows)


def main():
    pdf = PDF()
    pdf.set_auto_page_break(auto=True, margin=15)
    pdf.add_page()

    content = parse_markdown('PLAY_SAMPLE_User_Manual.md')
    process_markdown(pdf, content)

    output_file = 'PLAY_SAMPLE_User_Manual.pdf'
    pdf.output(output_file)
    print(f'Generated: {output_file}')


if __name__ == '__main__':
    main()
