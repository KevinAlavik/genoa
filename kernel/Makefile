MAKEFLAGS += -rR
.SUFFIXES:

KERNEL_ELF := bin/genoa.elf
OBJDIR := build
SRCDIR := src
INCDIR := include
OUTDIRS := $(OBJDIR) bin

CC := gcc
CFLAGS := -g -O2 -pipe
CPPFLAGS := -I $(SRCDIR) -I $(INCDIR) -MMD -MP
NASMFLAGS := -F dwarf -g

CFLAGS += -g -O2 -pipe -Wall -Wextra -Werror -std=gnu11 -ffreestanding -fno-stack-protector -fno-stack-check \
		  -fno-PIC -ffunction-sections -fdata-sections -m64 -march=x86-64 -mno-80387 -mno-mmx -mno-sse -mno-sse2 -mno-red-zone -mcmodel=kernel \
		  -Wno-unused-variable -I $(INCDIR) -I $(SRCDIR) -MMD -MP -DLIMINE_API_REVISION=3

LDFLAGS := -nostdlib -static -z max-page-size=0x1000 -Wl,--gc-sections \
           -T linker.ld -Wl,-m,elf_x86_64

SRCS := $(shell find $(SRCDIR) -name '*.c' -o -name '*.S' -o -name '*.asm')
OBJS := $(patsubst %.c,$(OBJDIR)/%.o,$(filter %.c,$(SRCS)))
OBJS += $(patsubst %.S,$(OBJDIR)/%.o,$(filter %.S,$(SRCS)))
OBJS += $(patsubst %.asm,$(OBJDIR)/%.o,$(filter %.asm,$(SRCS)))
DEPS := $(OBJS:.o=.d)

.PHONY: all clean distclean

all: $(OUTDIRS) $(KERNEL_ELF)

$(OUTDIRS):
	mkdir -p $@

$(KERNEL_ELF): $(OBJS)
	@echo "  LD $@"
	@$(CC) $(LDFLAGS) -o $@ $(OBJS)

$(OBJDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "  CC $<"
	@$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.o: %.S
	@mkdir -p $(dir $@)
	@echo "  CC $<"
	@$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.o: %.asm
	@mkdir -p "$$(dirname $@)"
	@echo "  NASM $<"
	@nasm $(NASMFLAGS) $< -o $@

clean:
	rm -rf $(OBJDIR) $(KERNEL_ELF)

distclean: clean
	rm -rf bin
