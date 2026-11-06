LATEXMK ?= latexmk
PYTHON ?= python3
.PHONY: all clean export check
all:
	$(LATEXMK) -lualatex -interaction=nonstopmode -halt-on-error -outdir=build the-book-of-mudsl.ltx
export:
	$(PYTHON) export_manual.py
check:
	$(PYTHON) export_manual.py --check
clean:
	$(LATEXMK) -C -outdir=build the-book-of-mudsl.ltx
