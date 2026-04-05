MCU = msp430g2553
CC = /Users/macmax/Downloads/msp430-gcc-9.3.1.11_macos/bin/msp430-elf-gcc
SUPPORT = /Users/macmax/Downloads/msp430-gcc-support-files/include

CFLAGS = -mmcu=$(MCU) -O2 -g
INCLUDES = -I$(SUPPORT)
LDFLAGS = -L$(SUPPORT)

TARGET = main

all: $(TARGET).hex

$(TARGET).elf: $(TARGET).c
	$(CC) $(CFLAGS) $(INCLUDES) $(LDFLAGS) -o $@ $<

$(TARGET).hex: $(TARGET).elf
	/Users/macmax/Downloads/msp430-gcc-9.3.1.11_macos/bin/msp430-elf-objcopy -O ihex $< $@

flash: $(TARGET).hex
	# L'outil MSP430Flasher de TI veut un format text (HEX ou TI-TXT), pas ELF
	/Users/macmax/ti/MSPFlasher_1.3.20/MSP430Flasher -w $(TARGET).hex -v -z [VCC]

clean:
	rm -f *.elf *.hex *.o