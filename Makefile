MAKEFLAGS += -rR
.SUFFIXES:

QEMUFLAGS := -m 2G
IMAGE_NAME := release/genoa

HOST_CC := cc
HOST_CFLAGS := -g -O2 -pipe

.PHONY: all
all: $(IMAGE_NAME).iso

.PHONY: run
run: $(IMAGE_NAME).iso ovmf/ovmf-code-x86_64.fd
	@qemu-system-x86_64 \
		-M q35 \
		-drive if=pflash,unit=0,format=raw,file=ovmf/ovmf-code-x86_64.fd,readonly=on \
		-cdrom $(IMAGE_NAME).iso \
		$(QEMUFLAGS)

ovmf/ovmf-code-x86_64.fd:
	@mkdir -p ovmf
	@curl -Lo $@ https://github.com/osdev0/edk2-ovmf-nightly/releases/latest/download/ovmf-code-x86_64.fd

limine/limine:
	@rm -rf limine
	@git clone https://github.com/limine-bootloader/limine.git --branch=v9.x-binary --depth=1
	@$(MAKE) -C limine \
		CC="$(HOST_CC)" \
		CFLAGS="$(HOST_CFLAGS)"

kernel-deps:
	@./kernel/get-deps
	@touch kernel-deps

.PHONY: kernel
kernel: kernel-deps
	@$(MAKE) -C kernel

$(IMAGE_NAME).iso: limine/limine kernel
	@rm -rf iso_root
	@mkdir -p release
	@mkdir -p iso_root/boot/limine iso_root/EFI/BOOT
	@cp -v kernel/bin/genoa.elf iso_root/boot/
	@cp -v limine.conf iso_root/boot/limine/
	@cp -v limine/limine-bios.sys limine/limine-bios-cd.bin limine/limine-uefi-cd.bin iso_root/boot/limine/
	@cp -v limine/BOOTX64.EFI iso_root/EFI/BOOT/
	@cp -v limine/BOOTIA32.EFI iso_root/EFI/BOOT/
	@xorriso -as mkisofs -R -r -J -b boot/limine/limine-bios-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table -hfsplus \
		-apm-block-size 2048 --efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		iso_root -o $(IMAGE_NAME).iso
	@./limine/limine bios-install $(IMAGE_NAME).iso
	@rm -rf iso_root

.PHONY: clean
clean:
	@$(MAKE) -C kernel clean
	@rm -rf iso_root $(IMAGE_NAME).iso

.PHONY: distclean
distclean:
	@$(MAKE) -C kernel distclean
	@rm -rf iso_root *.iso kernel-deps limine ovmf
