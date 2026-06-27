"""Convert PicoMiteBT.md to PDF using fpdf."""
import re
from fpdf import FPDF


class PDF(FPDF):
    def header(self):
        self.set_font('Helvetica', 'B', 15)
        self.cell(0, 10, 'PicoMiteBT -- BLE Console for Pico 2 W', 0, 1, 'C')
        self.ln(5)

    def footer(self):
        self.set_y(-15)
        self.set_font('Helvetica', 'I', 8)
        self.cell(0, 10, f'Page {self.page_no()}', 0, 0, 'C')


_UNICODE_FALLBACKS = {
    '—': '--',   # em dash
    '–': '-',    # en dash
    '−': '-',    # minus sign
    '‘': "'", '’': "'",
    '“': '"', '”': '"',
    '•': '-',    # bullet
    '…': '...',  # ellipsis
    '≤': '<=', '≥': '>=',
    '≠': '!=',
    '×': 'x',
    '→': '->', '←': '<-',
}


def _fold_to_latin1(text):
    """Apply known fallbacks, then strip anything else that won't latin-1."""
    for src, dst in _UNICODE_FALLBACKS.items():
        text = text.replace(src, dst)
    # Anything left that's not encodable -> '?', so the PDF still renders
    return text.encode('latin-1', 'replace').decode('latin-1')


def sanitize_latin1(text):
    return _fold_to_latin1(text)


def clean_inline(text):
    """Strip inline markdown formatting and replace non-latin1 chars."""
    text = re.sub(r'\*\*(.+?)\*\*', r'\1', text)
    text = re.sub(r'\*(.+?)\*', r'\1', text)
    text = re.sub(r'`(.+?)`', r'\1', text)
    text = re.sub(r'\[(.+?)\]\(.+?\)', r'\1', text)
    return _fold_to_latin1(text)


def render_table(pdf, table_lines):
    """Render a markdown table."""
    headers = [c.strip() for c in table_lines[0].split('|') if c.strip()]
    data_rows = []
    for row_line in table_lines[2:]:
        cells = [c.strip() for c in row_line.split('|') if c.strip()]
        data_rows.append(cells)

    n_cols = len(headers)
    page_w = pdf.w - pdf.l_margin - pdf.r_margin
    col_widths = [page_w / n_cols] * n_cols

    pdf.set_font('Helvetica', 'B', 9)
    for j, h in enumerate(headers):
        pdf.cell(col_widths[j], 6, clean_inline(h), 1, 0, 'C')
    pdf.ln()

    pdf.set_font('Helvetica', '', 9)
    for row in data_rows:
        # Compute the tallest cell in this row using multi-cell logic
        cell_lines = []
        max_lines = 1
        for j in range(n_cols):
            t = clean_inline(row[j]) if j < len(row) else ''
            # crude line wrap estimate
            words = t.split()
            line = ''
            lines = []
            for w in words:
                trial = (line + ' ' + w).strip()
                if pdf.get_string_width(trial) < col_widths[j] - 2:
                    line = trial
                else:
                    lines.append(line)
                    line = w
            if line:
                lines.append(line)
            if not lines:
                lines = ['']
            cell_lines.append(lines)
            if len(lines) > max_lines:
                max_lines = len(lines)
        row_h = 5 * max_lines
        x0 = pdf.get_x()
        y0 = pdf.get_y()
        for j in range(n_cols):
            x = x0 + sum(col_widths[:j])
            pdf.set_xy(x, y0)
            pdf.multi_cell(col_widths[j], 5, '\n'.join(cell_lines[j]),
                           border=1)
        pdf.set_xy(x0, y0 + row_h)


def render_markdown(pdf, md_path):
    with open(md_path, 'r', encoding='utf-8') as f:
        lines = f.readlines()
    lines = [sanitize_latin1(l) for l in lines]

    i = 0
    while i < len(lines):
        line = lines[i].rstrip('\n')

        # Top-level title goes in header — skip
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
            pdf.cell(0, 8, clean_inline(title), 0, 1)
            pdf.ln(2)
            i += 1
            continue

        # H3
        if line.startswith('### '):
            title = line[4:].strip()
            title = title.replace('`', '')
            pdf.ln(3)
            pdf.set_font('Helvetica', 'B', 11)
            pdf.cell(0, 7, clean_inline(title), 0, 1)
            pdf.ln(1)
            i += 1
            continue

        # H4
        if line.startswith('#### '):
            title = line[5:].strip()
            title = title.replace('`', '')
            pdf.ln(2)
            pdf.set_font('Helvetica', 'B', 10)
            pdf.cell(0, 6, clean_inline(title), 0, 1)
            pdf.ln(1)
            i += 1
            continue

        # Blank line
        if line.strip() == '':
            pdf.ln(2)
            i += 1
            continue

        # Numbered list item
        m = re.match(r'^\s*(\d+)\.\s+(.*)', line)
        if m:
            num, text = m.group(1), clean_inline(m.group(2))
            pdf.set_font('Helvetica', '', 10)
            pdf.cell(8, 5, num + '.', 0, 0)
            pdf.multi_cell(0, 5, text)
            i += 1
            continue

        # Bullet list item
        if line.strip().startswith('- '):
            text = line.strip()[2:]
            text = clean_inline(text)
            pdf.set_font('Helvetica', '', 10)
            pdf.cell(6, 5, chr(149), 0, 0)
            pdf.multi_cell(0, 5, text)
            i += 1
            continue

        # Bold-only paragraph
        if line.strip().startswith('**') and line.strip().endswith('**'):
            text = line.strip().strip('*')
            pdf.set_font('Helvetica', 'B', 10)
            pdf.multi_cell(0, 5, text)
            pdf.ln(1)
            i += 1
            continue

        # Regular paragraph
        para = clean_inline(line)
        i += 1
        while i < len(lines):
            nxt = lines[i].rstrip('\n')
            if (nxt.strip() == '' or nxt.startswith('#') or nxt.startswith('```')
                    or nxt.strip().startswith('|') or nxt.strip().startswith('- ')
                    or re.match(r'^\s*\d+\.\s+', nxt)
                    or nxt.strip() == '---'
                    or (nxt.strip().startswith('**') and nxt.strip().endswith('**'))):
                break
            para += ' ' + clean_inline(nxt)
            i += 1
        pdf.set_font('Helvetica', '', 10)
        pdf.multi_cell(0, 5, para)
        continue

    return pdf


pdf = PDF()
pdf.set_auto_page_break(auto=True, margin=20)
pdf.add_page()

render_markdown(pdf, 'PicoMiteBT.md')

pdf.output('PicoMiteBT.pdf')
print('Generated PicoMiteBT.pdf')
