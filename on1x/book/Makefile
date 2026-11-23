SHELL := /bin/sh
export TEXINPUTS := $(CURDIR)/tex:$(TEXINPUTS)
.DEFAULT_GOAL := book
.PHONY: book example check clean
book:
	latexmk -pdf -outdir=build -interaction=nonstopmode -halt-on-error on1x-book.ltx

example:
	cd examples && latexmk -pdf -interaction=nonstopmode -halt-on-error main.tex
check: book
	python3 tests/check_modules.py
	$(MAKE) example
	cd tests && pdflatex -interaction=nonstopmode -halt-on-error feature-smoke.tex > /dev/null
clean:
	latexmk -C -outdir=build on1x-book.ltx
	cd examples && latexmk -C main.tex
	cd tests && latexmk -C feature-smoke.tex
	cd tests && latexmk -C light-smoke.tex
