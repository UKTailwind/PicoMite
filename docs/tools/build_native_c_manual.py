#!/usr/bin/env python3
"""Build a PicoMite native-C edition from the repository's DOCX and Markdown.

The original manual remains intact; a clearly identified, versioned native-C
appendix is added to a new copy. The Markdown source is kept authoritative.
"""
from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import tempfile
from pathlib import Path

from docx import Document
from docx.shared import Inches, Pt
from docx.enum.text import WD_BREAK
from docx.enum.style import WD_STYLE_TYPE
from docx.oxml import OxmlElement
from docx.oxml.ns import qn


def ensure_style(doc: Document, name: str, base: str, *, font='Consolas', size=8):
    styles = doc.styles
    if name not in styles:
        st = styles.add_style(name, WD_STYLE_TYPE.PARAGRAPH)
        st.base_style = styles[base]
        st.font.name = font
        st.font.size = Pt(size)
        st.paragraph_format.space_after = Pt(2)
    return styles[name]



def ensure_heading_styles(doc: Document):
    """Supply Word heading styles missing from the upstream manual template.

    python-docx's add_heading(level=N) requires a named 'Heading N' paragraph
    style. Existing styles are preserved; only absent ones are created.
    """
    styles = doc.styles
    for level in range(1, 10):
        name = f'Heading {level}'
        if name in styles:
            continue
        style = styles.add_style(name, WD_STYLE_TYPE.PARAGRAPH)
        style.base_style = styles['Normal']
        style.font.bold = True
        style.font.size = Pt(max(10, 18 - 2 * (level - 1)))
        style.paragraph_format.space_before = Pt(12 if level <= 2 else 6)
        style.paragraph_format.space_after = Pt(4)
        style.paragraph_format.keep_with_next = True
        # Mark the created paragraph style as a structural heading so that
        # Word/LibreOffice can use it in navigation and the document outline.
        ppr = style.element.get_or_add_pPr()
        outline = OxmlElement('w:outlineLvl')
        outline.set(qn('w:val'), str(level - 1))
        ppr.append(outline)


def field(paragraph, instruction: str):
    run = paragraph.add_run()
    start = OxmlElement('w:fldChar'); start.set(qn('w:fldCharType'), 'begin')
    code = OxmlElement('w:instrText'); code.set(qn('xml:space'), 'preserve'); code.text = ' ' + instruction + ' '
    sep = OxmlElement('w:fldChar'); sep.set(qn('w:fldCharType'), 'separate')
    placeholder = OxmlElement('w:t'); placeholder.text = 'Update field in Word'
    end = OxmlElement('w:fldChar'); end.set(qn('w:fldCharType'), 'end')
    for element in (start, code, sep, placeholder, end): run._r.append(element)


def plain_inline(text: str) -> str:
    # Keep command syntax and link targets rather than silently dropping them.
    text = re.sub(r'\[([^\]]+)\]\(([^)]+)\)', r'\1 (\2)', text)
    text = text.replace('`', '')
    text = re.sub(r'(?<!\\)\*\*([^*]+)\*\*', r'\1', text)
    text = re.sub(r'(?<!\\)\*([^*]+)\*', r'\1', text)
    return text.replace('\\<', '<').replace('\\>', '>')


def add_list_item(doc: Document, text: str, numbered: bool, marker: str):
    """Use the template's list style when present, otherwise preserve markers."""
    style = 'List Number' if numbered else 'List Bullet'
    if style in doc.styles:
        doc.add_paragraph(plain_inline(text), style=style)
    else:
        # Some upstream DOCX templates omit Word's built-in list styles.
        # Using an undefined style raises KeyError in python-docx.
        prefix = f'{marker} ' if numbered else '\u2022 '
        paragraph = doc.add_paragraph(prefix + plain_inline(text))
        paragraph.paragraph_format.left_indent = Pt(18)
        paragraph.paragraph_format.first_line_indent = Pt(-12)


def find_libreoffice():
    executable = shutil.which('libreoffice') or shutil.which('soffice')
    if executable and 'openoffice' not in str(executable).lower():
        return executable
    if os.name == 'nt':
        for variable in ('PROGRAMFILES', 'PROGRAMFILES(X86)'):
            base = os.environ.get(variable)
            if base:
                candidate = Path(base) / 'LibreOffice' / 'program' / 'soffice.exe'
                if candidate.is_file():
                    return str(candidate)
    return None



def find_openoffice():
    """Locate Apache OpenOffice, without requiring its optional Python-UNO bridge."""
    candidates = []
    for variable in ('PROGRAMFILES(X86)', 'PROGRAMFILES'):
        base = os.environ.get(variable)
        if base:
            candidates.append(Path(base) / 'OpenOffice 4' / 'program')
    candidates.extend([
        Path('C:/Program Files (x86)/OpenOffice 4/program'),
        Path('C:/Program Files/OpenOffice 4/program'),
    ])
    for directory in candidates:
        if (directory / 'soffice.exe').is_file():
            return directory / 'soffice.exe'
    return None


# Apache OpenOffice exposes the Windows COM Automation Bridge, independently
# of the optional Python-UNO component. Windows Script Host is built into
# Windows; the export therefore does not need pyuno, pywin32, or Word.
OPENOFFICE_COM_EXPORT = r'''Option Explicit
Dim serviceManager, desktop, document, sourceURL, targetURL
Dim loadOptions(1), exportOptions(1), hidden, readOnly, filterName, overwrite

If WScript.Arguments.Count <> 2 Then
    WScript.Echo "Expected source and target file URLs"
    WScript.Quit 2
End If
sourceURL = WScript.Arguments(0)
targetURL = WScript.Arguments(1)

Set serviceManager = CreateObject("com.sun.star.ServiceManager")
Set desktop = serviceManager.createInstance("com.sun.star.frame.Desktop")
Set hidden = serviceManager.Bridge_GetStruct("com.sun.star.beans.PropertyValue")
hidden.Name = "Hidden"
hidden.Value = True
Set loadOptions(0) = hidden
Set readOnly = serviceManager.Bridge_GetStruct("com.sun.star.beans.PropertyValue")
readOnly.Name = "ReadOnly"
readOnly.Value = True
Set loadOptions(1) = readOnly
Set document = desktop.loadComponentFromURL(sourceURL, "_blank", 0, loadOptions)
If document Is Nothing Then
    Err.Raise vbObjectError + 1, "OpenOffice PDF export", "Could not load source DOCX"
End If
Set filterName = serviceManager.Bridge_GetStruct("com.sun.star.beans.PropertyValue")
filterName.Name = "FilterName"
filterName.Value = "writer_pdf_Export"
Set exportOptions(0) = filterName
Set overwrite = serviceManager.Bridge_GetStruct("com.sun.star.beans.PropertyValue")
overwrite.Name = "Overwrite"
overwrite.Value = True
Set exportOptions(1) = overwrite
Call document.storeToURL(targetURL, exportOptions)
Call document.close(True)
WScript.Echo "PDF exported"
'''


def export_pdf_with_openoffice(office, source, target):
    """Use the documented OpenOffice COM bridge on Windows, not Python-UNO."""
    if os.name != 'nt':
        raise RuntimeError('OpenOffice COM PDF export is supported on Windows only')
    if not office.is_file():
        raise RuntimeError('OpenOffice executable missing: ' + str(office))
    cscript = Path(os.environ.get('SystemRoot', 'C:/Windows')) / 'System32/cscript.exe'
    if not cscript.is_file():
        raise RuntimeError('Windows Script Host not found: ' + str(cscript))
    with tempfile.TemporaryDirectory(prefix='pico-mm-aoo-') as directory:
        script = Path(directory) / 'export_pdf.vbs'
        script.write_text(OPENOFFICE_COM_EXPORT, encoding='ascii')
        command = [str(cscript), '//Nologo', str(script),
                   source.resolve().as_uri(), target.resolve().as_uri()]
        try:
            result = subprocess.run(command, capture_output=True,
                                    text=True, timeout=180)
        except subprocess.TimeoutExpired as exc:
            raise RuntimeError('OpenOffice COM PDF export timed out') from exc
        if result.returncode != 0:
            raise RuntimeError('OpenOffice COM PDF export failed (exit %s):\n%s\n%s\n'
                               'Check that the OpenOffice COM Automation Bridge is registered.'
                               % (result.returncode, result.stdout, result.stderr))
        if not target.is_file() or target.stat().st_size == 0:
            raise RuntimeError('OpenOffice did not create a nonempty PDF: ' + str(target)
                               + '\n' + result.stdout + '\n' + result.stderr)


def add_markdown(doc: Document, source: str):
    ensure_style(doc, 'PicoMite Code', 'Normal')
    ensure_style(doc, 'PicoMite Quote', 'Normal', font='Aptos', size=9)
    lines = source.splitlines()
    block, fence = [], None
    in_table = []

    def flush_table():
        nonlocal in_table
        if not in_table: return
        rows = []
        for x in in_table:
            values = [plain_inline(c.strip()) for c in x.strip().strip('|').split('|')]
            if all(re.fullmatch(r':?-{3,}:?', v) for v in values): continue
            rows.append(values)
        if rows:
            width = max(map(len, rows))
            table = doc.add_table(rows=0, cols=width)
            try: table.style = 'Table Grid'
            except KeyError: pass
            for row in rows:
                cells = table.add_row().cells
                for i, value in enumerate(row): cells[i].text = value
        in_table = []

    def flush_block():
        nonlocal block
        if block:
            doc.add_paragraph(plain_inline(' '.join(x.strip() for x in block)))
            block = []

    for line in lines + ['']:
        stripped = line.strip()
        if fence is not None:
            if stripped.startswith(fence): fence = None
            else: doc.add_paragraph(line.rstrip('\n'), style='PicoMite Code')
            continue
        if stripped.startswith('```') or stripped.startswith('~~~'):
            flush_block(); flush_table(); fence = stripped[:3]; continue
        if stripped.startswith('|') and stripped.endswith('|'):
            flush_block(); in_table.append(stripped); continue
        flush_table()
        heading = re.match(r'^(#{1,6})\s+(.*)$', stripped)
        if heading:
            flush_block()
            # Markdown title is the chapter title in the combined manual.
            doc.add_heading(plain_inline(heading.group(2)), level=min(len(heading.group(1))+1, 9))
        elif not stripped:
            flush_block()
        elif stripped.startswith('>'):
            flush_block(); doc.add_paragraph(plain_inline(stripped.lstrip('> ')), style='PicoMite Quote')
        elif re.match(r'^([-*+] |\d+[.)] )', stripped):
            flush_block()
            match = re.match(r'^([-*+] |(\d+)[.)] )', stripped)
            numbered = match.group(2) is not None
            t = stripped[match.end():]
            add_list_item(doc, t, numbered, match.group(2) + '.' if numbered else '-')
        elif re.match(r'^(-{3,}|\*{3,})$', stripped):
            flush_block()
        else:
            block.append(line)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--out', type=Path, default=None)
    parser.add_argument('--pdf', action='store_true', help='Also export a PDF using LibreOffice or Apache OpenOffice')
    args = parser.parse_args()
    root = args.repo.resolve()
    source = root / 'docs/PicoMite_User_Manual.docx'
    native = root / 'docs/PicoMite-native-C-support.md'
    out = args.out or root / 'docs/PicoMite_User_Manual_Native_C_RC6.docx'
    for path in (source, native):
        if not path.is_file(): parser.error(f'Missing source file: {path}')
    if out.resolve() == source.resolve(): parser.error('Refusing to overwrite upstream manual')
    out.parent.mkdir(parents=True, exist_ok=True)
    doc = Document(source)
    ensure_heading_styles(doc)
    doc.add_page_break()
    doc.add_heading('Native C / TCC / ELF — PicoMite V7.00.00 RC6', 1)
    doc.add_paragraph('Supplement for the DnCraptor/pico-mm fork, based on integration commit ff63ab4 and documentation commit 8a07cfa. The preceding chapters retain the upstream manual content.')
    doc.add_paragraph('This appendix is generated from docs/PicoMite-native-C-support.md. The upstream manual does not supersede the fork-specific Flash layout warning in docs/V7.00.00_release_notes.md.')
    add_markdown(doc, native.read_text(encoding='utf-8'))
    doc.save(out)
    print(f'DOCX: {out}')
    if args.pdf:
        libreoffice = find_libreoffice()
        pdf = out.with_suffix('.pdf')
        if libreoffice:
            with tempfile.TemporaryDirectory(prefix='pico-mm-pdf-') as profile:
                command = [libreoffice, '-env:UserInstallation=' + Path(profile).as_uri(), '--headless', '--convert-to', 'pdf', '--outdir', str(out.parent), str(out)]
                result = subprocess.run(command, capture_output=True, text=True, check=True)
            if not pdf.is_file():
                raise RuntimeError(f'LibreOffice did not create {pdf}: {result.stdout} {result.stderr}')
        else:
            openoffice = find_openoffice()
            if not openoffice:
                parser.error('PDF export requires LibreOffice or Apache OpenOffice 4 with its COM bridge')
            if pdf.exists():
                pdf.unlink()  # Do not mistake an old PDF for the new export.
            export_pdf_with_openoffice(openoffice, out, pdf)
            if not pdf.is_file():
                raise RuntimeError('OpenOffice UNO did not create ' + str(pdf))
        print(f'PDF: {pdf}')

if __name__ == '__main__': main()
