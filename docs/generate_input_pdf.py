"""Convert Game_Input_Devices_Manual.md to PDF using fpdf."""
import re
from fpdf import FPDF


class PDF(FPDF):
    def header(self):
        self.set_font('Helvetica', 'B', 15)
        self.cell(0, 10, 'Game Input Devices -- User Manual', 0, 1, 'C')
        self.ln(5)

    def footer(self):
        self.set_y(-15)
        self.set_font('Helvetica', 'I', 8)
        self.cell(0, 10, f'Page {self.page_no()}', 0, 0, 'C')


def sanitize_latin1(text):
    """Replace characters that can't be encoded in latin-1."""
    text = text.replace('\u2014', '--').replace('\u2013', '-')
    text = text.replace('\u2018', "'").replace('\u2019', "'")
    text = text.replace('\u201c', '"').replace('\u201d', '"')
    text = text.replace('\u2022', '-')
    text = text.replace('\u2264', '<=').replace('\u2265', '>=')
    text = text.replace('\u2260', '!=')
    text = text.replace('\u00d7', 'x')
    text = text.replace('\u03a9', 'Ohm')
    text = text.replace('\u2212', '-')
    return text


def render_markdown(pdf, md_path):
    with open(md_path, 'r', encoding='utf-8') as f:
        lines = f.readlines()
    # Sanitize all lines up front
    lines = [sanitize_latin1(l) for l in lines]

    i = 0
    while i < len(lines):
        line = lines[i].rstrip('\n')

        # Skip the top-level title (already in header)
        if line.startswith('# ') and not line.startswith('## '):
            i += 1
            continue

        # Horizontal rule
        if line.strip() == '---':
            pdf.ln(3)
            y = pdf.get_y()
            pdf.line(pdf.l_margin, y, pdf.w - pdf.r_margin, y)
            pdf.ln(3)
            i += 1
            continue

        # Code block
        if line.strip().startswith('```'):
            code_lines = []
            i += 1
            while i < len(lines) and not lines[i].strip().startswith('```'):
                code_lines.append(lines[i].rstrip('\n'))
                i += 1
            i += 1  # skip closing ```
            pdf.set_font('Courier', '', 9)
            pdf.set_fill_color(240, 240, 240)
            code_text = '\n'.join(code_lines)
            pdf.multi_cell(0, 4.5, code_text, fill=True)
            pdf.ln(3)
            continue

        # Table
        if '|' in line and line.strip().startswith('|'):
            table_lines = []
            while i < len(lines) and lines[i].strip().startswith('|'):
                table_lines.append(lines[i].strip())
                i += 1
            render_table(pdf, table_lines)
            pdf.ln(3)
            continue

        # H2
        if line.startswith('## '):
            title = line[3:].strip()
            pdf.ln(5)
            pdf.set_font('Helvetica', 'B', 13)
            pdf.cell(0, 8, title, 0, 1)
            pdf.ln(2)
            i += 1
            continue

        # H3
        if line.startswith('### '):
            title = line[4:].strip()
            title = title.replace('`', '')
            pdf.ln(3)
            pdf.set_font('Helvetica', 'B', 11)
            pdf.cell(0, 7, title, 0, 1)
            pdf.ln(1)
            i += 1
            continue

        # H4
        if line.startswith('#### '):
            title = line[5:].strip()
            title = title.replace('`', '')
            pdf.ln(2)
            pdf.set_font('Helvetica', 'B', 10)
            pdf.cell(0, 6, title, 0, 1)
            pdf.ln(1)
            i += 1
            continue

        # Blank line
        if line.strip() == '':
            pdf.ln(2)
            i += 1
            continue

        # Numbered list item
        m = re.match(r'^(\d+)\.\s+(.*)', line.strip())
        if m:
            text = clean_inline(m.group(2))
            pdf.set_font('Helvetica', '', 10)
            pdf.cell(6, 5, m.group(1) + '.', 0, 0)
            pdf.multi_cell(0, 5, text)
            i += 1
            continue

        # Bullet list item
        if line.strip().startswith('- '):
            text = line.strip()[2:]
            text = clean_inline(text)
            pdf.set_font('Helvetica', '', 10)
            pdf.cell(6, 5, chr(149), 0, 0)  # bullet
            pdf.multi_cell(0, 5, text)
            i += 1
            continue

        # Bold paragraph (**...**)
        if line.strip().startswith('**') and line.strip().endswith('**'):
            text = line.strip().strip('*')
            pdf.set_font('Helvetica', 'B', 10)
            pdf.multi_cell(0, 5, text)
            pdf.ln(1)
            i += 1
            continue

        # Regular paragraph - collect continuation lines
        para = clean_inline(line)
        i += 1
        while i < len(lines):
            nxt = lines[i].rstrip('\n')
            if (nxt.strip() == '' or nxt.startswith('#') or nxt.startswith('```')
                    or nxt.strip().startswith('|') or nxt.strip().startswith('- ')
                    or nxt.strip() == '---'
                    or re.match(r'^\d+\.\s+', nxt.strip())
                    or (nxt.strip().startswith('**') and nxt.strip().endswith('**'))):
                break
            para += ' ' + clean_inline(nxt)
            i += 1
        pdf.set_font('Helvetica', '', 10)
        pdf.multi_cell(0, 5, para)
        continue

    return pdf


def clean_inline(text):
    """Strip inline markdown formatting and replace non-latin1 chars."""
    # Bold
    text = re.sub(r'\*\*(.+?)\*\*', r'\1', text)
    # Italic
    text = re.sub(r'\*(.+?)\*', r'\1', text)
    # Inline code
    text = re.sub(r'`(.+?)`', r'\1', text)
    # Links [text](url) -> text
    text = re.sub(r'\[(.+?)\]\(.+?\)', r'\1', text)
    # Replace non-latin1 characters
    text = text.replace('\u2014', '--').replace('\u2013', '-')
    text = text.replace('\u2018', "'").replace('\u2019', "'")
    text = text.replace('\u201c', '"').replace('\u201d', '"')
    text = text.replace('\u00d7', 'x')
    text = text.replace('\u03a9', 'Ohm')
    text = text.replace('\u2212', '-')
    return text


def render_table(pdf, table_lines):
    """Render a markdown table with word-wrapped cells."""
    # Parse header
    headers = [c.strip() for c in table_lines[0].split('|') if c.strip()]

    # Skip separator line (row 1)
    data_rows = []
    for row_line in table_lines[2:]:
        cells = [c.strip() for c in row_line.split('|') if c.strip()]
        data_rows.append(cells)

    n_cols = len(headers)
    page_w = pdf.w - pdf.l_margin - pdf.r_margin

    # Estimate column widths based on max content length
    max_lens = [len(clean_inline(h)) for h in headers]
    for row in data_rows:
        for j in range(n_cols):
            t = clean_inline(row[j]) if j < len(row) else ''
            max_lens[j] = max(max_lens[j], len(t))
    total = sum(max_lens) or 1
    col_widths = [max(page_w * (ml / total), 18) for ml in max_lens]
    # Normalize to fit page width
    scale = page_w / sum(col_widths)
    col_widths = [w * scale for w in col_widths]

    line_h = 5

    def get_row_height(cells_text, font_style=''):
        """Calculate the required row height by simulating word wrap."""
        max_lines = 1
        pdf.set_font('Helvetica', font_style, 9)
        for j, t in enumerate(cells_text):
            w = col_widths[j] - 2  # padding
            if w <= 0:
                w = 1
            # Simulate word wrap to count lines accurately
            words = t.split(' ')
            n_lines = 1
            line_w = 0
            for word in words:
                word_w = pdf.get_string_width(word + ' ')
                if line_w + word_w > w and line_w > 0:
                    n_lines += 1
                    line_w = word_w
                else:
                    line_w += word_w
            max_lines = max(max_lines, n_lines)
        return max_lines * line_h

    def draw_row(cells_text, font_style='', align=''):
        """Draw a row with wrapped cells and matching borders."""
        row_h = get_row_height(cells_text, font_style)
        x_start = pdf.get_x()
        y_start = pdf.get_y()

        # Check page break
        if y_start + row_h > pdf.h - pdf.b_margin:
            pdf.add_page()
            y_start = pdf.get_y()

        pdf.set_font('Helvetica', font_style, 9)
        for j, t in enumerate(cells_text):
            x = x_start + sum(col_widths[:j])
            # Draw cell border
            pdf.rect(x, y_start, col_widths[j], row_h)
            # Draw text inside
            pdf.set_xy(x + 1, y_start + 0.5)
            pdf.multi_cell(col_widths[j] - 2, line_h, t, 0, align or 'L')

        pdf.set_xy(x_start, y_start + row_h)

    # Header row
    h_texts = [clean_inline(h) for h in headers]
    draw_row(h_texts, 'B', 'C')

    # Data rows
    for row in data_rows:
        cells = [clean_inline(row[j]) if j < len(row) else '' for j in range(n_cols)]
        draw_row(cells)


pdf = PDF()
pdf.set_auto_page_break(auto=True, margin=20)
pdf.add_page()

render_markdown(pdf, 'Game_Input_Devices_Manual.md')

pdf.output('Game_Input_Devices_Manual.pdf')
print('Generated Game_Input_Devices_Manual.pdf')
