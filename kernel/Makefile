MAKEFLAGS += -rR
.SUFFIXES:

ARCH                ?= x86_64
KERNEL_ELF          := bin/genoa-$(subst x86_64,x86-64,$(ARCH)).elf
OBJDIR              := build
SRCDIR              := .
ARCHDIR             := arch/$(ARCH)
OUTDIRS             := $(OBJDIR) bin

CC                  := cc
CFLAGS              := -g -O2 -pipe
CPPFLAGS            :=
NASMFLAGS           := -F dwarf -g

CC_IS_CLANG         := $(shell ! $(CC) --version 2>/dev/null | grep 'clang' >/dev/null 2>&1; echo $$?)

CFLAGS              += -Wall \
                       -Wextra \
                       -std=gnu11 \
                       -ffreestanding \
                       -fno-stack-protector \
                       -fno-stack-check \
                       -fno-PIC \
                       -ffunction-sections \
                       -fdata-sections

CPPFLAGS            := -I $(SRCDIR) \
                       -I $(ARCHDIR) \
                       -DLIMINE_API_REVISION=3 \
                       -MMD \
                       -MP

NASMFLAGS           += -Wall

ifeq ($(CC_IS_CLANG),1)
    CC              += -target $(ARCH)-unknown-none
endif

ifeq ($(ARCH),x86_64)
    CFLAGS          += -m64 \
                       -march=x86-64 \
                       -mno-80387 \
                       -mno-mmx \
                       -mno-sse \
                       -mno-sse2 \
                       -mno-red-zone \
                       -mcmodel=kernel
    LDFLAGS         += -Wl,-m,elf_x86_64
    NASMFLAGS       += -f elf64
    LINKER_SCRIPT   = $(ARCHDIR)/linker.ld
else
    $(error Unsupported ARCH: $(ARCH))
endif

LDFLAGS             += -Wl,--build-id=none \
                       -nostdlib \
                       -static \
                       -z max-page-size=0x1000 \
                       -Wl,--gc-sections \
                       -T $(LINKER_SCRIPT)

ifneq ($(shell ( test '$(MAKECMDGOALS)' = clean || test '$(MAKECMDGOALS)' = distclean ); echo $$?),0)
    ifeq ($(shell ( ! test -f arch/limine.h ); echo $$?),0)
        $(error Please run the ./get-deps script first)
    endif
endif

SRCS               := $(shell find $(SRCDIR) -type f \( -name '*.c' -o -name '*.S' -o -name '*.asm' \) | LC_ALL=C sort) # TODO: Make it only build like arch/$(TARGET)/* and not other targets
OBJS               := $(patsubst %.c,$(OBJDIR)/%.o,$(filter %.c,$(SRCS)))
OBJS               += $(patsubst %.S,$(OBJDIR)/%.o,$(filter %.S,$(SRCS)))
OBJS               += $(patsubst %.asm,$(OBJDIR)/%.o,$(filter %.asm,$(SRCS)))
DEPS               := $(OBJS:.o=.d)

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

ifeq ($(ARCH),x86_64)
$(OBJDIR)/%.o: %.asm
	@mkdir -p "$$(dirname $@)"
	@echo "  NASM $<"
	@nasm $(NASMFLAGS) $< -o $@
endif

clean:
	rm -rf $(OBJDIR) $(KERNEL_ELF)

distclean: clean
	rm -rf bin
