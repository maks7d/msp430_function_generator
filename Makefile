# =========================================================
# Configuration des chemins (Variables d'environnement ou Relatifs)
# =========================================================
# Par défaut, on cherche les outils dans le dossier parent (../) 
# pour que le repo soit portable. 
# Vous pouvez aussi surcharger ces variables à la volée :
# ex: make GCC_DIR=/autre/chemin/gcc flash
GCC_DIR     ?= ../../ti/msp430-gcc-9.3.1.11_linux64
SUPPORT_DIR ?= ../../ti/msp430-gcc-support-files/include
FLASHER_DIR ?= ../../ti/MSPFlasher_1.3.20

MCU = msp430g2553
CC = $(GCC_DIR)/bin/msp430-elf-gcc
OBJCOPY = $(GCC_DIR)/bin/msp430-elf-objcopy
FLASHER = $(FLASHER_DIR)/MSP430Flasher

CFLAGS = -mmcu=$(MCU) -O2 -g
INCLUDES = -I$(SUPPORT_DIR)
LDFLAGS = -L$(SUPPORT_DIR)

TARGET = main

all: $(TARGET).hex

$(TARGET).elf: $(TARGET).c
	$(CC) $(CFLAGS) $(INCLUDES) $(LDFLAGS) -o $@ $<

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

flash: $(TARGET).hex
	# L'outil MSP430Flasher de TI veut un format text (HEX ou TI-TXT), pas ELF
	LD_LIBRARY_PATH=$(FLASHER_DIR) $(FLASHER) -w $(TARGET).hex -v -z [VCC]

clean:
	rm -f *.elf *.hex *.o