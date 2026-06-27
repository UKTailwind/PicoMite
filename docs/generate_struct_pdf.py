#!/usr/bin/env python3
"""
Generate PDF from MMBasic_Structures_Manual.md
"""

from fpdf import FPDF
import re

class PDF(FPDF):
    def header(self):
        self.set_font('Helvetica', 'B', 16)
        self.cell(0, 10, 'MMBasic Structures User Manual', 0, 1, 'C')
        self.ln(5)

    def footer(self):
        self.set_y(-15)
        self.set_font('Helvetica', 'I', 8)
        self.cell(0, 10, f'Page {self.page_no()}', 0, 0, 'C')

def parse_markdown(filename):
    """Parse markdown file and return structured content."""
    with open(filename, 'r', encoding='utf-8') as f:
        content = f.read()
    return content

def add_section(pdf, title, level=1):
    """Add a section title."""
    if level == 1:
        pdf.set_font('Helvetica', 'B', 14)
        pdf.ln(5)
    elif level == 2:
        pdf.set_font('Helvetica', 'B', 12)
        pdf.ln(3)
    else:
        pdf.set_font('Helvetica', 'B', 11)
        pdf.ln(2)
    pdf.multi_cell(0, 6, title)
    pdf.ln(2)

def add_text(pdf, text):
    """Add regular text."""
    pdf.set_font('Helvetica', '', 10)
    # Handle inline code
    text = text.replace('`', '"')
    # Replace Unicode characters that can't be encoded in latin-1
    text = text.replace('→', '->')
    text = text.replace('\\x1e', '<RS>')
    pdf.multi_cell(0, 5, text)

def add_code(pdf, code):
    """Add code block."""
    pdf.set_font('Courier', '', 9)
    pdf.set_fill_color(245, 245, 245)
    # Replace Unicode characters that can't be encoded in latin-1
    code = code.replace('→', '->')
    code = code.replace('\\x1e', '<RS>')
    for line in code.split('\n'):
        if line.strip():
            pdf.cell(0, 4, '  ' + line, 0, 1, fill=True)
        else:
            pdf.cell(0, 4, '', 0, 1, fill=True)
    pdf.ln(2)

def add_table(pdf, headers, rows):
    """Add a table."""
    pdf.set_font('Helvetica', 'B', 9)
    
    # Calculate column widths based on number of columns and content
    num_cols = len(headers)
    page_width = 190  # Usable page width
    
    if num_cols == 2:
        # Calculate max content length in each column
        max_first_col = max(len(headers[0]), max((len(row[0]) for row in rows), default=0))
        max_second_col = max(len(headers[1]), max((len(row[1]) for row in rows), default=0))
        
        # Determine widths based on content ratio
        total_content = max_first_col + max_second_col
        if total_content > 0:
            # Allocate proportionally with minimum widths
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
    
    # Headers - replace Unicode characters
    for i, header in enumerate(headers):
        header = header.replace('→', '->').replace('\\x1e', '<RS>')
        w = col_widths[i] if i < len(col_widths) else 40
        pdf.cell(w, 6, header, 1, 0, 'C')
    pdf.ln()
    
    # Rows
    pdf.set_font('Helvetica', '', 9)
    for row in rows:
        for i, cell in enumerate(row):
            # Replace Unicode characters
            cell = cell.replace('→', '->').replace('\\x1e', '<RS>')
            w = col_widths[i] if i < len(col_widths) else 40
            # Truncate if too long - allow more characters per unit width
            max_chars = int(w * 0.45)  # Approximately 0.45 chars per mm
            display = cell[:max_chars] + '...' if len(cell) > max_chars else cell
            pdf.cell(w, 5, display, 1, 0, 'L')
        pdf.ln()
    pdf.ln(3)

def process_markdown(pdf, content):
    """Process markdown content and add to PDF."""
    lines = content.split('\n')
    i = 0
    in_code_block = False
    code_buffer = []
    in_table = False
    table_headers = []
    table_rows = []
    
    while i < len(lines):
        line = lines[i]
        
        # Code blocks
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
        
        # Tables
        if '|' in line and not line.strip().startswith('|--'):
            cells = [c.strip() for c in line.split('|')[1:-1]]
            if cells:
                if not in_table:
                    table_headers = cells
                    in_table = True
                elif line.strip().startswith('|--') or line.strip().startswith('|-'):
                    pass  # Skip separator
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
        
        # Skip table separators
        if line.strip().startswith('|--') or line.strip().startswith('|-'):
            i += 1
            continue
        
        # Headers
        if line.startswith('# '):
            # Skip main title (handled in PDF header)
            i += 1
            continue
        elif line.startswith('## '):
            add_section(pdf, line[3:].strip(), 1)
        elif line.startswith('### '):
            add_section(pdf, line[4:].strip(), 2)
        elif line.startswith('#### '):
            add_section(pdf, line[5:].strip(), 3)
        # Bold text as mini headers
        elif line.strip().startswith('**') and line.strip().endswith('**'):
            pdf.set_font('Helvetica', 'B', 10)
            pdf.ln(2)
            pdf.multi_cell(0, 5, line.strip().replace('**', ''))
        # List items
        elif line.strip().startswith('- '):
            pdf.set_font('Helvetica', '', 10)
            text = line.strip()[2:].replace('`', '"')
            pdf.multi_cell(0, 5, '  - ' + text)
        # Numbered items
        elif re.match(r'^\d+\.', line.strip()):
            pdf.set_font('Helvetica', '', 10)
            text = line.strip().replace('`', '"')
            pdf.multi_cell(0, 5, '  ' + text)
        # Regular text
        elif line.strip():
            add_text(pdf, line.strip())
        
        i += 1
    
    # Handle any remaining table
    if in_table and table_headers and table_rows:
        add_table(pdf, table_headers, table_rows)

def main():
    pdf = PDF()
    pdf.set_auto_page_break(auto=True, margin=15)
    pdf.add_page()
    
    # Read and process markdown
    content = parse_markdown('MMBasic_Structures_Manual.md')
    process_markdown(pdf, content)
    
    # Save PDF
    output_file = 'MMBasic_Structures_Manual.pdf'
    pdf.output(output_file)
    print(f'Generated: {output_file}')

if __name__ == '__main__':
    main()
