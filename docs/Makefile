# Minimal Sphinx Makefile for the SOEM fork documentation.
# Warnings are treated as errors so broken links/toctrees fail the build.

SPHINXOPTS    ?= -W --keep-going
SPHINXBUILD   ?= sphinx-build
SOURCEDIR      = .
BUILDDIR       = _build

.PHONY: help html clean

help:
	@$(SPHINXBUILD) -M help "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)

html:
	@$(SPHINXBUILD) -b html "$(SOURCEDIR)" "$(BUILDDIR)/html" $(SPHINXOPTS) $(O)

clean:
	@rm -rf "$(BUILDDIR)"
