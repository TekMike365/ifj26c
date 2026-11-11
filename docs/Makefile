LATEXMK	:= latexmk

.PHONY: compile
compile:
	$(LATEXMK) -pdf
	$(LATEXMK) -c

.PHONY: watch
watch:
	$(LATEXMK) -pdf -pvc -interaction=nonstopmode

.PHONY: clean
clean:
	$(LATEXMK) -C
