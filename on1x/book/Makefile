SHELL := /bin/sh
export TEXINPUTS := $(CURDIR)/tex:$(TEXINPUTS)
.PHONY: example check clean
example:
	cd examples && latexmk -pdf -interaction=nonstopmode -halt-on-error main.tex
check:
	python3 tests/check_modules.py
	$(MAKE) example
	cd tests && pdflatex -interaction=nonstopmode -halt-on-error feature-smoke.tex > /dev/null
clean:
	cd examples && latexmk -C main.tex
	cd tests && latexmk -C feature-smoke.tex
	cd tests && latexmk -C light-smoke.tex
