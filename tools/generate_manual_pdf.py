from __future__ import annotations

import argparse
import html
import re
import subprocess
from datetime import datetime
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

    rendered_at = datetime.now().strftime("%d/%m/%Y %H:%M")

    html_text = f"""<!DOCTYPE html>
<html lang="pt-BR">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>19 - Manual de Operacao com Imagens</title>
  <style>
    @page {{
      size: A4;
      margin: 18mm 14mm 18mm 14mm;
    }}
    body {{
      font-family: "Segoe UI", Arial, sans-serif;
      color: #16202a;
      line-height: 1.55;
      font-size: 11.5pt;
      margin: 0;
      background: #fff;
      counter-reset: page;
    }}
    main {{
      max-width: 100%;
    }}
    .cover {{
      min-height: 250mm;
      display: flex;
      flex-direction: column;
      justify-content: space-between;
      background:
        radial-gradient(circle at top right, rgba(39, 193, 246, 0.22), transparent 34%),
        linear-gradient(180deg, #0f172a 0%, #0b2942 60%, #0d5aa7 100%);
      color: #f8fafc;
      border-radius: 20px;
      padding: 20mm 18mm;
      box-sizing: border-box;
      page-break-after: always;
      box-shadow: inset 0 0 0 1px rgba(255, 255, 255, 0.08);
    }}
    .cover-kicker {{
      color: #67e8f9;
      font-weight: 700;
      letter-spacing: 0.14em;
      font-size: 10pt;
      text-transform: uppercase;
    }}
    .cover h1 {{
      color: #ffffff;
      border-bottom: none;
      font-size: 28pt;
      line-height: 1.15;
      margin: 0.35em 0 0.25em;
      padding-bottom: 0;
    }}
    .cover-subtitle {{
      font-size: 13pt;
      color: #dbeafe;
      max-width: 135mm;
    }}
    .cover-grid {{
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 10px;
      margin-top: 14mm;
    }}
    .cover-card {{
      background: rgba(15, 23, 42, 0.35);
      border: 1px solid rgba(148, 163, 184, 0.2);
      border-radius: 14px;
      padding: 10px 12px;
    }}
    .cover-card strong {{
      display: block;
      color: #67e8f9;
      font-size: 9.5pt;
      margin-bottom: 4px;
      text-transform: uppercase;
      letter-spacing: 0.06em;
    }}
    .cover-footer {{
      display: flex;
      justify-content: space-between;
      gap: 16px;
      align-items: flex-end;
      color: #dbeafe;
      font-size: 10pt;
    }}
    .cover-footer-right {{
      text-align: right;
    }}
    .toc {{
      background: #f8fbfe;
      border: 1px solid #d7e8f3;
      border-radius: 16px;
      padding: 14px 16px;
      margin: 10px 0 18px;
      page-break-inside: avoid;
    }}
    .toc h2 {{
      margin-top: 0;
    }}
    .toc ul {{
      margin-bottom: 0;
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
    .page-footer {{
      position: fixed;
      left: 0;
      right: 0;
      bottom: 0;
      height: 10mm;
      font-size: 9pt;
      color: #64748b;
      display: flex;
      justify-content: space-between;
      align-items: center;
      border-top: 1px solid #d7e8f3;
      padding-top: 4px;
      background: #fff;
    }}
    .page-footer .page-number::after {{
      content: counter(page);
    }}
  </style>
</head>
<body>
  <section class="cover">
    <div>
      <div class="cover-kicker">YOU OBD Simulator</div>
      <h1>Manual de Operacao com Imagens</h1>
      <div class="cover-subtitle">
        Guia de uso do simulador para bancada, OTA, API, plugin de laboratorio e validacao com apps Android reais.
      </div>
      <div class="cover-grid">
        <div class="cover-card">
          <strong>Escopo</strong>
          Protocolos, perfis, modos, cenarios, DTCs, OTA, credenciais e fluxo com YOU OBD Lab.
        </div>
        <div class="cover-card">
          <strong>Publico</strong>
          Engenharia, QA, bancada, desenvolvimento Android e homologacao de scanner.
        </div>
      </div>
    </div>
    <div class="cover-footer">
      <div>
        <div><strong>Projeto:</strong> YouSimuladorOBD</div>
        <div><strong>Documento:</strong> 19-manual-operacao</div>
      </div>
      <div class="cover-footer-right">
        <div><strong>Renderizado em:</strong> {rendered_at}</div>
        <div><strong>Uso:</strong> distribuicao interna e laboratorio</div>
      </div>
    </div>
  </section>
  <main>
    <div class="meta">Versao renderizada para distribuicao interna do laboratorio YOU.</div>
    <section class="toc">
      <h2>Sumario</h2>
      <ul>
        <li>1. Objetivo</li>
        <li>2. Acesso ao simulador</li>
        <li>3. Painel principal</li>
        <li>4. Logica de operacao</li>
        <li>5. Perfis, modos e cenarios</li>
        <li>6. Pagina OTA e configuracao de campo</li>
        <li>7. Salvar configuracao e reiniciar</li>
        <li>8. Rotacionar senha da API</li>
        <li>9. Plugin YOU OBD Lab</li>
        <li>10. Fluxo recomendado de bancada</li>
        <li>11. OTA online</li>
        <li>12. Troubleshooting rapido</li>
        <li>13. Referencias cruzadas</li>
      </ul>
    </section>
    {''.join(body)}
  </main>
  <div class="page-footer">
    <span>YouSimuladorOBD • Manual operacional</span>
    <span class="page-number"></span>
  </div>
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
