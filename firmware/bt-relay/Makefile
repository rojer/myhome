.DEFAULT_GOAL = build
.PHONY: build flash flash-keep-fs
MAKEFLAGS += --warn-undefined-variables --no-builtin-rules

SHOS ?= shos

build:
	$(SHOS) build --platform=esp32c3 --verbose

flash:
	$(SHOS) flash build/fw.zip

flash-keep-fs:
	$(SHOS) flash --keep-fs=true build/fw.zip
