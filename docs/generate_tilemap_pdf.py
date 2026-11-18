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


def _wrap(pdf, text, w):
    """The words of text as lines that fit width w in the current font; a word longer than a line is
    broken."""
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
    return lines or ['']


def render_table(pdf, table_lines):
    """Render a markdown table: columns sized to their text, cells word-wrapped, and a row never split
    across a page (the header is drawn again on the next). One-line cells at equal widths cut long text
    off."""
    def cells(line):
        return [clean_inline(c.strip()) for c in line.strip().strip('|').split('|')]

    headers = cells(table_lines[0])
    rows = [cells(l) for l in table_lines[2:]]
    n = len(headers)
    page_w = pdf.w - pdf.l_margin - pdf.r_margin
    pdf.set_font('Helvetica', '', 9)
    maxw = []
    for j in range(n):
        texts = [headers[j]] + [r[j] for r in rows if j < len(r)]
        maxw.append(max(pdf.get_string_width(t) for t in texts) or 1)
    widths = [max(16, page_w * m / sum(maxw)) for m in maxw]
    widths = [w * page_w / sum(widths) for w in widths]
    line_h = 4.4
    x0 = pdf.l_margin
    bottom = pdf.h - pdf.b_margin
    # set_auto_page_break(False) also zeroes b_margin: keep both to put back
    saved_auto, saved_b = pdf.auto_page_break, pdf.b_margin
    pdf.set_auto_page_break(False)

    def wrapped(row, style):
        pdf.set_font('Helvetica', style, 9)
        return [_wrap(pdf, row[j] if j < len(row) else '', widths[j]) for j in range(n)]

    def height(row, style):
        return line_h * max(len(c) for c in wrapped(row, style)) + 1.2

    def draw(row, style, align):
        w = wrapped(row, style)
        rh = line_h * max(len(c) for c in w) + 1.2
        y0 = pdf.get_y()
        x = x0
        for j in range(n):
            pdf.rect(x, y0, widths[j], rh)
            yy = y0 + 0.6
            for ln in w[j]:
                pdf.set_xy(x + 1, yy)
                pdf.cell(widths[j] - 2, line_h, ln, 0, 0, align)
                yy += line_h
            x += widths[j]
        pdf.set_xy(x0, y0 + rh)

    if pdf.get_y() + height(headers, 'B') + line_h * 2 > bottom:
        pdf.add_page()
    draw(headers, 'B', 'C')
    for r in rows:
        if pdf.get_y() + height(r, '') > bottom:
            pdf.add_page()
            draw(headers, 'B', 'C')
        draw(r, '', 'L')
    pdf.set_auto_page_break(saved_auto, saved_b)
    pdf.set_font('Helvetica', '', 9)
    pdf.ln(2)


pdf = PDF()
pdf.set_auto_page_break(auto=True, margin=20)
pdf.add_page()

render_markdown(pdf, 'TILEMAP_User_Manual.md')

pdf.output('TILEMAP_User_Manual.pdf')
print('Generated TILEMAP_User_Manual.pdf')
