from __future__ import annotations

import argparse
import html
import re
import subprocess
from pathlib import Path


EDGE_CANDIDATES = [
    Path(r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"),
    Path(r"C:\Program Files\Microsoft\Edge\Application\msedge.exe"),
    Path(r"C:\Program Files\Google\Chrome\Application\chrome.exe"),
]


def resolve_browser(explicit: str | None) -> Path:
    if explicit:
        path = Path(explicit)
        if path.exists():
            return path
        raise FileNotFoundError(f"Navegador nao encontrado: {path}")

    for candidate in EDGE_CANDIDATES:
        if candidate.exists():
            return candidate

    raise FileNotFoundError("Nenhum navegador compatível encontrado para gerar o PDF.")


def inline_to_html(text: str, base_dir: Path) -> str:
    code_pattern = re.compile(r"`([^`]+)`")
    link_pattern = re.compile(r"\[([^\]]+)\]\(([^)]+)\)")

    def convert_links(segment: str) -> str:
        escaped = html.escape(segment)

        def repl(match: re.Match[str]) -> str:
            label = html.escape(match.group(1))
            target = match.group(2).strip()
            if re.match(r"^(https?://|mailto:|file://)", target, re.IGNORECASE):
                href = target
            else:
                href = (base_dir / target).resolve().as_uri()
            return f'<a href="{html.escape(href, quote=True)}">{label}</a>'

        return link_pattern.sub(repl, escaped)

    parts: list[str] = []
    last = 0
    for match in code_pattern.finditer(text):
        parts.append(convert_links(text[last : match.start()]))
        parts.append(f"<code>{html.escape(match.group(1))}</code>")
        last = match.end()
    parts.append(convert_links(text[last:]))
    return "".join(parts)


def image_line_to_html(line: str, base_dir: Path) -> str | None:
    match = re.fullmatch(r"!\[(.*?)\]\((.*?)\)", line.strip())
    if not match:
        return None

    alt = html.escape(match.group(1))
    target = (base_dir / match.group(2).strip()).resolve().as_uri()
    return (
        '<figure class="manual-figure">'
        f'<img src="{html.escape(target, quote=True)}" alt="{alt}">'
        f"<figcaption>{alt}</figcaption>"
        "</figure>"
    )


def markdown_to_html(markdown_path: Path, html_path: Path) -> None:
    source = markdown_path.read_text(encoding="utf-8")
    base_dir = markdown_path.parent
    lines = source.splitlines()

    body: list[str] = []
    paragraph: list[str] = []
    list_items: list[str] = []
    list_type: str | None = None
    in_code = False
    code_lang = ""
    code_lines: list[str] = []

    def flush_paragraph() -> None:
        nonlocal paragraph
        if paragraph:
            text = " ".join(item.strip() for item in paragraph).strip()
            if text:
                body.append(f"<p>{inline_to_html(text, base_dir)}</p>")
            paragraph = []

    def flush_list() -> None:
        nonlocal list_items, list_type
        if list_items and list_type:
            body.append(f"<{list_type}>")
            body.extend(list_items)
            body.append(f"</{list_type}>")
        list_items = []
        list_type = None

    def flush_code() -> None:
        nonlocal in_code, code_lang, code_lines
        classes = f' class="language-{html.escape(code_lang)}"' if code_lang else ""
        code = html.escape("\n".join(code_lines))
        body.append(f"<pre><code{classes}>{code}</code></pre>")
        in_code = False
        code_lang = ""
        code_lines = []

    for raw_line in lines:
        line = raw_line.rstrip()
        stripped = line.strip()

        if in_code:
            if stripped.startswith("```"):
                flush_code()
            else:
                code_lines.append(raw_line)
            continue

        if stripped.startswith("```"):
            flush_paragraph()
            flush_list()
            in_code = True
            code_lang = stripped[3:].strip()
            code_lines = []
            continue

        if not stripped:
            flush_paragraph()
            flush_list()
            continue

        image_html = image_line_to_html(stripped, base_dir)
        if image_html:
            flush_paragraph()
            flush_list()
            body.append(image_html)
            continue

        if stripped.startswith("### "):
            flush_paragraph()
            flush_list()
            body.append(f"<h3>{inline_to_html(stripped[4:], base_dir)}</h3>")
            continue

        if stripped.startswith("## "):
            flush_paragraph()
            flush_list()
            body.append(f"<h2>{inline_to_html(stripped[3:], base_dir)}</h2>")
            continue

        if stripped.startswith("# "):
            flush_paragraph()
            flush_list()
            body.append(f"<h1>{inline_to_html(stripped[2:], base_dir)}</h1>")
            continue

        if re.match(r"^\d+\.\s+", stripped):
            flush_paragraph()
            if list_type not in (None, "ol"):
                flush_list()
            list_type = "ol"
            content = re.sub(r"^\d+\.\s+", "", stripped)
            list_items.append(f"<li>{inline_to_html(content, base_dir)}</li>")
            continue

        if stripped.startswith("- "):
            flush_paragraph()
            if list_type not in (None, "ul"):
                flush_list()
            list_type = "ul"
            list_items.append(f"<li>{inline_to_html(stripped[2:], base_dir)}</li>")
            continue

        flush_list()
        paragraph.append(stripped)

    flush_paragraph()
    flush_list()

    html_text = f"""<!DOCTYPE html>
<html lang="pt-BR">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>19 - Manual de Operacao com Imagens</title>
  <style>
    @page {{
      size: A4;
      margin: 16mm 14mm 18mm 14mm;
    }}
    body {{
      font-family: "Segoe UI", Arial, sans-serif;
      color: #16202a;
      line-height: 1.55;
      font-size: 11.5pt;
      margin: 0;
      background: #fff;
    }}
    main {{
      max-width: 100%;
    }}
    h1, h2, h3 {{
      color: #0d5aa7;
      margin-top: 1.15em;
      margin-bottom: 0.45em;
      page-break-after: avoid;
    }}
    h1 {{
      font-size: 23pt;
      border-bottom: 2px solid #27c1f6;
      padding-bottom: 6px;
    }}
    h2 {{
      font-size: 17pt;
      border-left: 4px solid #27c1f6;
      padding-left: 8px;
    }}
    h3 {{
      font-size: 13pt;
    }}
    p, li {{
      margin: 0 0 0.55em 0;
    }}
    ul, ol {{
      margin: 0.35em 0 0.9em 1.4em;
      padding: 0;
    }}
    pre {{
      background: #0f172a;
      color: #e2e8f0;
      padding: 12px 14px;
      border-radius: 10px;
      overflow-x: auto;
      white-space: pre-wrap;
      word-break: break-word;
    }}
    code {{
      font-family: Consolas, "Courier New", monospace;
      background: #edf2f7;
      padding: 0.08em 0.32em;
      border-radius: 4px;
      font-size: 0.95em;
    }}
    pre code {{
      background: transparent;
      padding: 0;
      border-radius: 0;
      font-size: 0.92em;
    }}
    a {{
      color: #0d5aa7;
      text-decoration: none;
    }}
    .manual-figure {{
      margin: 14px 0 18px;
      page-break-inside: avoid;
      text-align: center;
    }}
    .manual-figure img {{
      max-width: 100%;
      max-height: 225mm;
      border: 1px solid #d8e1ea;
      border-radius: 10px;
      box-shadow: 0 8px 18px rgba(15, 23, 42, 0.08);
    }}
    .manual-figure figcaption {{
      font-size: 9.5pt;
      color: #475569;
      margin-top: 7px;
    }}
    .meta {{
      margin-top: -4px;
      margin-bottom: 18px;
      color: #475569;
      font-size: 10pt;
    }}
  </style>
</head>
<body>
  <main>
    <div class="meta">Versao renderizada para distribuicao interna do laboratorio YOU.</div>
    {''.join(body)}
  </main>
</body>
</html>
"""

    html_path.parent.mkdir(parents=True, exist_ok=True)
    html_path.write_text(html_text, encoding="utf-8")


def render_pdf(html_path: Path, pdf_path: Path, browser_path: Path) -> None:
    pdf_path.parent.mkdir(parents=True, exist_ok=True)
    cmd = [
        str(browser_path),
        "--headless",
        "--disable-gpu",
        "--no-first-run",
        "--print-to-pdf-no-header",
        f"--print-to-pdf={pdf_path}",
        "--virtual-time-budget=6000",
        html_path.resolve().as_uri(),
    ]
    subprocess.run(cmd, check=True)


def main() -> None:
    parser = argparse.ArgumentParser(description="Gera HTML e PDF do manual de operacao.")
    parser.add_argument(
        "--source",
        default=r"C:\www\YouSimuladorOBD\docs\19-manual-operacao.md",
        help="Arquivo markdown de origem.",
    )
    parser.add_argument(
        "--html",
        default=r"C:\www\YouSimuladorOBD\docs\19-manual-operacao.html",
        help="Arquivo HTML de saida.",
    )
    parser.add_argument(
        "--pdf",
        default=r"C:\www\YouSimuladorOBD\docs\19-manual-operacao.pdf",
        help="Arquivo PDF de saida.",
    )
    parser.add_argument(
        "--browser",
        default=None,
        help="Caminho explicito do navegador para impressao em PDF.",
    )
    args = parser.parse_args()

    markdown_path = Path(args.source)
    html_path = Path(args.html)
    pdf_path = Path(args.pdf)
    browser_path = resolve_browser(args.browser)

    markdown_to_html(markdown_path, html_path)
    render_pdf(html_path, pdf_path, browser_path)

    print(f"HTML gerado em: {html_path}")
    print(f"PDF gerado em: {pdf_path}")


if __name__ == "__main__":
    main()
