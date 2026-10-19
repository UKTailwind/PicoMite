"""Convert Game_Development_Guide.md to PDF using fpdf."""
import re
from fpdf import FPDF


class PDF(FPDF):
    def header(self):
        self.set_font('Helvetica', 'B', 15)
        self.cell(0, 10, 'Writing Games in PicoMite MMBasic', 0, 1, 'C')
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
            title = clean_inline(line[3:].strip())
            pdf.ln(5)
            pdf.set_font('Helvetica', 'B', 13)
            pdf.cell(0, 8, title, 0, 1)
            pdf.ln(2)
            i += 1
            continue

        # H3
        if line.startswith('### '):
            title = clean_inline(line[4:].strip())
            pdf.ln(3)
            pdf.set_font('Helvetica', 'B', 11)
            pdf.cell(0, 7, title, 0, 1)
            pdf.ln(1)
            i += 1
            continue

        # H4
        if line.startswith('#### '):
            title = clean_inline(line[5:].strip())
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
        if re.match(r'^\d+\.\s', line.strip()):
            text = re.sub(r'^\d+\.\s', '', line.strip())
            text, i = list_continuation(lines, i + 1, text)
            text = clean_inline(text)
            pdf.set_font('Helvetica', '', 10)
            num = re.match(r'^(\d+)\.\s', line.strip()).group(1)
            pdf.cell(8, 5, num + '.', 0, 0)
            pdf.multi_cell(0, 5, text)
            continue

        # Bullet list item
        if line.strip().startswith('- '):
            text = line.strip()[2:]
            text, i = list_continuation(lines, i + 1, text)
            text = clean_inline(text)
            pdf.set_font('Helvetica', '', 10)
            pdf.cell(6, 5, chr(149), 0, 0)  # bullet
            pdf.multi_cell(0, 5, text)
            continue

        # Bold paragraph (**...**)
        if line.strip().startswith('**') and line.strip().endswith('**'):
            text = clean_inline(line.strip())
            pdf.set_font('Helvetica', 'B', 10)
            pdf.multi_cell(0, 5, text)
            pdf.ln(1)
            i += 1
            continue

        # Blockquote
        if line.strip().startswith('> '):
            text = clean_inline(line.strip()[2:])
            pdf.set_font('Helvetica', 'I', 10)
            pdf.set_x(pdf.l_margin + 10)
            pdf.multi_cell(0, 5, text)
            pdf.ln(1)
            i += 1
            continue

        # Regular paragraph - collect continuation lines, then clean the
        # whole paragraph so a `code` or **bold** span may cross a line break
        para = line
        i += 1
        while i < len(lines):
            nxt = lines[i].rstrip('\n')
            if (nxt.strip() == '' or nxt.startswith('#') or nxt.startswith('```')
                    or nxt.strip().startswith('|') or nxt.strip().startswith('- ')
                    or nxt.strip().startswith('> ')
                    or re.match(r'^\d+\.\s', nxt.strip())
                    or nxt.strip() == '---'
                    or (nxt.strip().startswith('**') and nxt.strip().endswith('**'))):
                break
            para += ' ' + nxt
            i += 1
        para = clean_inline(para)
        pdf.set_font('Helvetica', '', 10)
        pdf.multi_cell(0, 5, para)
        continue

    return pdf


def list_continuation(lines, i, text):
    """Append a list item's indented continuation lines; return (text, i)."""
    while i < len(lines):
        nxt = lines[i].rstrip('\n')
        s = nxt.strip()
        if (not nxt[:1].isspace() or s == '' or s.startswith('- ')
                or s.startswith('```') or s.startswith('|')
                or re.match(r'^\d+\.\s', s)):
            break
        text += ' ' + s
        i += 1
    return text, i


def clean_inline(text):
    """Strip inline markdown formatting and replace non-latin1 chars."""
    # Inline code: set aside first, so a * inside `...` is not emphasis
    codes = []

    def keep_code(m):
        codes.append(m.group(1))
        return '\x00%d\x00' % (len(codes) - 1)
    text = re.sub(r'`([^`]+)`', keep_code, text)
    # Bold
    text = re.sub(r'\*\*(.+?)\*\*', r'\1', text)
    # Italic
    text = re.sub(r'\*(?=\S)(.+?)(?<=\S)\*', r'\1', text)
    # Links [text](url) -> text
    text = re.sub(r'\[(.+?)\]\(.+?\)', r'\1', text)
    # Put the inline code back, without its backticks
    text = re.sub(r'\x00(\d+)\x00', lambda m: codes[int(m.group(1))], text)
    # Replace non-latin1 characters
    text = text.replace('\u2014', '--').replace('\u2013', '-')
    text = text.replace('\u2018', "'").replace('\u2019', "'")
    text = text.replace('\u201c', '"').replace('\u201d', '"')
    text = text.replace('\u00d7', 'x')
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
    table_row(pdf, col_widths, [clean_inline(h) for h in headers], 'C')

    # Data rows - a long cell word-wraps inside its border
    pdf.set_font('Helvetica', '', 9)
    for row in data_rows:
        cells = [clean_inline(row[j]) if j < len(row) else ''
                 for j in range(n_cols)]
        table_row(pdf, col_widths, cells, 'L')


def table_row(pdf, col_widths, cells, align):
    """Draw one table row, each cell word-wrapped inside its border.

    A one-line row is 6 high with its text centred, as before; a wrapped
    row grows by 5 per line. The row is measured first and the page turned
    if the whole row (text offset included) does not fit; auto page break
    is off while its cells are drawn, so a row near the bottom margin can
    never spill one cell per page."""
    line_h = 5
    lines = max(len(pdf.multi_cell(w, line_h, t, split_only=True))
                for w, t in zip(col_widths, cells))
    row_h = max(6, lines * line_h)
    offset = (row_h - lines * line_h) / 2
    if pdf.get_y() + row_h > pdf.page_break_trigger:
        pdf.add_page()
    x, y = pdf.l_margin, pdf.get_y()
    auto = pdf.auto_page_break
    pdf.set_auto_page_break(False, pdf.b_margin)
    for w, t in zip(col_widths, cells):
        pdf.rect(x, y, w, row_h)
        pdf.set_xy(x, y + offset)
        pdf.multi_cell(w, line_h, t, 0, align)
        x += w
    pdf.set_auto_page_break(auto, pdf.b_margin)
    pdf.set_xy(pdf.l_margin, y + row_h)


pdf = PDF()
pdf.set_auto_page_break(auto=True, margin=20)
pdf.add_page()

render_markdown(pdf, 'Game_Development_Guide.md')

pdf.output('Game_Development_Guide.pdf')
print('Generated Game_Development_Guide.pdf')
