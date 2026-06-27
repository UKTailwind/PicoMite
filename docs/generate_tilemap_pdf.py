"""Convert TILEMAP_User_Manual.md to PDF using fpdf."""
import re
from fpdf import FPDF


class PDF(FPDF):
    def header(self):
        self.set_font('Helvetica', 'B', 15)
        self.cell(0, 10, 'TILEMAP Command -- User Manual', 0, 1, 'C')
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
            # Clean markdown formatting from title
            title = title.replace('`', '')
            pdf.ln(3)
            pdf.set_font('Helvetica', 'B', 11)
            pdf.cell(0, 7, title, 0, 1)
            pdf.ln(1)
            i += 1
            continue

        # Blank line
        if line.strip() == '':
            pdf.ln(2)
            i += 1
            continue

        # Bullet list item
        if line.strip().startswith('- '):
            text = line.strip()[2:]
            text = clean_inline(text)
            pdf.set_font('Helvetica', '', 10)
            x = pdf.get_x()
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
    # Inline code
    text = re.sub(r'`(.+?)`', r'\1', text)
    # Replace em/en dashes and other non-latin1 characters
    text = text.replace('\u2014', '-').replace('\u2013', '-')
    text = text.replace('\u2018', "'").replace('\u2019', "'")
    text = text.replace('\u201c', '"').replace('\u201d', '"')
    return text


def render_table(pdf, table_lines):
    """Render a markdown table."""
    # Parse header
    headers = [c.strip() for c in table_lines[0].split('|') if c.strip()]

    # Skip separator line (row 1)
    data_rows = []
    for row_line in table_lines[2:]:
        cells = [c.strip() for c in row_line.split('|') if c.strip()]
        data_rows.append(cells)

    n_cols = len(headers)
    # Calculate column widths proportionally
    page_w = pdf.w - pdf.l_margin - pdf.r_margin
    col_widths = [page_w / n_cols] * n_cols

    # Header row
    pdf.set_font('Helvetica', 'B', 9)
    for j, h in enumerate(headers):
        pdf.cell(col_widths[j], 6, clean_inline(h), 1, 0, 'C')
    pdf.ln()

    # Data rows
    pdf.set_font('Helvetica', '', 9)
    for row in data_rows:
        max_h = 6
        # Calculate needed height for each cell
        cell_texts = []
        for j in range(n_cols):
            t = clean_inline(row[j]) if j < len(row) else ''
            cell_texts.append(t)
        for j, t in enumerate(cell_texts):
            pdf.cell(col_widths[j], 6, t, 1, 0)
        pdf.ln()


pdf = PDF()
pdf.set_auto_page_break(auto=True, margin=20)
pdf.add_page()

render_markdown(pdf, 'TILEMAP_User_Manual.md')

pdf.output('TILEMAP_User_Manual.pdf')
print('Generated TILEMAP_User_Manual.pdf')
