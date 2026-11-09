LATEXMK ?= latexmk
PYTHON ?= python3
.PHONY: all clean export check generate typography
export PYTHONPATH := $(CURDIR)/build/python:$(PYTHONPATH)
all: generate
	$(LATEXMK) -lualatex -shell-escape -interaction=nonstopmode -halt-on-error -outdir=build the-book-of-mudsl.ltx
generate:
	$(PYTHON) prepare_highlighting.py
	$(PYTHON) typeset-manuals.py
	$(PYTHON) typeset-manuals.py --manual-dir ../mull/stdlib --output build/generated/stdlib.tex
	$(PYTHON) typeset-manifest.py
export:
	$(PYTHON) export_manual.py
check: generate
	$(PYTHON) export_manual.py --check
	$(PYTHON) typeset-manuals.py --check
	$(PYTHON) typeset-manuals.py --manual-dir ../mull/stdlib --output build/generated/stdlib.tex --check
	$(PYTHON) typeset-manifest.py --check
	$(PYTHON) test_book.py
typography: generate
	$(PYTHON) check_typography.py
clean:
	$(LATEXMK) -C -outdir=build the-book-of-mudsl.ltx
