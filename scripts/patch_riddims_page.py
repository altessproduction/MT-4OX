#!/usr/bin/env python3
# Insère la page Riddims dans web/index.html de MT-4OX (bloc idempotent entre
# les marqueurs riddims:begin / riddims:end, juste avant </body>).
#  - les données (window.RIDDB) régénérées par scripts/riddims_site.py
#  - le module de la page (scripts/riddims_page.js)
import sys
from pathlib import Path

WEB = Path("/home/z/my-project/build-fw/MT-4OX/web")
INDEX = WEB / "index.html"
DATA = WEB / "riddims.js"
PAGE = Path("/home/z/my-project/scripts/riddims_page.js")

BEGIN = "<!-- riddims:begin (Altess Production, GPL-3.0) -->"
END = "<!-- riddims:end -->"

def main():
    html = INDEX.read_text(encoding="utf-8")
    data = DATA.read_text(encoding="utf-8").strip()
    page = PAGE.read_text(encoding="utf-8").strip()
    block = (f"{BEGIN}\n<script>\n{data}\n</script>\n"
             f"<script>\n{page}\n</script>\n{END}")
    if BEGIN in html:
        if html.count(BEGIN) != 1 or html.count(END) != 1:
            print("ERREUR: marqueurs riddims dupliqués"); sys.exit(1)
        pre, rest = html.split(BEGIN, 1)
        _, post = rest.split(END, 1)
        html = pre + block + post
        print("bloc riddims remplacé")
    else:
        if "</body>" not in html:
            print("ERREUR: pas de </body>"); sys.exit(1)
        html = html.replace("</body></html>", block + "\n</body></html>") \
            if "</body></html>" in html else html.replace("</body>", block + "\n</body>")
        print("bloc riddims inséré")
    INDEX.write_text(html, encoding="utf-8")
    print(f"OK {INDEX} ({len(html)} octets)")

if __name__ == "__main__":
    main()
