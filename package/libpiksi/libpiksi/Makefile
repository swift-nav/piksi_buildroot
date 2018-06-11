.PHONY: all src test docs .FORCE

all: src

src: .FORCE
	$(MAKE) -C src

test: src .FORCE
	$(MAKE) -C test

docs: .FORCE
	doxygen Doxyfile
