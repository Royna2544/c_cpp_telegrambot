# Find all .scss files in the SCSS directory
SCSS_FILES := $(wildcard **/[!_]*.scss [!_]*.scss)
PHP_FILES := $(wildcard *.php)
SCSS_FRAGMENTS := $(wildcard **/_*.scss)

# Define the target CSS files by replacing the .scss extension with .css
CSS_FILES := $(patsubst %.scss,%.css,$(SCSS_FILES))
CSS_MAP_FILES := $(patsubst %.scss,%.css.map,$(SCSS_FILES))
HTML_FILES := $(patsubst %.php,%.html,$(PHP_FILES))

# Default target
all: $(CSS_FILES) $(HTML_FILES)

# Rule to compile SCSS to CSS
%.css: %.scss $(SCSS_FRAGMENTS)
	@echo Compiling $< into $@
	sass $< $@

%.html: %.php
	@echo Compiling $< into $@
	php $< > $@

# This project's C++ side uses cmake; It should be present in PATH
clean:
	cmake -E rm -f $(CSS_FILES) $(HTML_FILES) $(CSS_MAP_FILES)

# Phony targets 
.PHONY: all clean
