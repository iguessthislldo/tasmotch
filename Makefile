NAME ?= tasmotch
INO ?= $(NAME).ino
BOARD ?= esp8266:esp8266:nodemcuv2
PORT ?= /dev/ttyUSB0
BUILD ?= build
BIN ?= $(BUILD)/$(INO).elf

.PHONY: all
all: $(BIN)
	@if [ ! -c $(PORT) ]; then \
		echo "TTY $(PORT) doesn't exist. Not uploading. Please plug in a board or set PORT"; \
	else \
		arduino-cli upload --fqbn $(BOARD) --port $(PORT) --input-dir $(BUILD); \
		arduino-cli monitor --port $(PORT); \
	fi

config.h:
	@echo "Please copy config.template.h to config.h and fill it out"
	@exit 1

$(BIN): $(INO) utils.h config.h
	arduino-cli compile --fqbn $(BOARD) --build-path $(BUILD) --warnings all $(INO)
