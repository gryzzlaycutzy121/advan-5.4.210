# SPDX-License-Identifier: GPL-2.0
# Makefile modul BFKRT -- VERSI 5.4.210 (kernel stok Advan UMS512)
#
# Dibangun terhadap pohon vanilla /e/linux-5.4.210 + config perangkat,
# sehingga vermagic-nya persis: 5.4.210 SMP preempt mod_unload modversions aarch64
#
# Pemakaian (dari build.sh agar semua jebakan Windows ditangani):
#   sh build.sh
#
# Pemakaian manual (env lengkap wajib, lihat build.sh):
#   make KDIR=/e/linux-5.4.210 KBUILD_MODPOST_WARN=1
ifneq ($(KERNELRELEASE),)

obj-m := berryfetcher_kernel.o berry_trig_all.o berry_m_virtkrn_obs.o berry_symdump.o berry_lkvm.o
ccflags-y := -Wall
# CFI: kernel stock (Unisoc/Android, clang 12.0.5) instrumented CFI_CLANG_SHADOW.
# Modul TANPA __cfi_check => mod->cfi_check=NULL => cfi_slowpath_handler
# -> "Don't allow unchecked modules" -> panic(). Jadi modul WAJIB di-CFI.
# clang major sama (12) => type-ID hash sama => cek lolos.
ccflags-y += -fsanitize=cfi-icall
# cross-DSO: modul punya __cfi_check sendiri + indirect call lewat __cfi_slowpath
# kernel (diekspor stock). Tanpa ini clang inline-check lokal (tanpa __cfi_check).
ccflags-y += -fsanitize-cfi-cross-dso

else

KDIR ?= /e/linux-5.4.210
PWD  := $(shell pwd)

all:
	@test -e "$(KDIR)/Makefile" || { \
	  echo "KDIR tidak valid: $(KDIR)"; exit 1; }
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=arm64 \
	    CROSS_COMPILE=aarch64-none-linux-gnu- KBUILD_MODPOST_WARN=1 modules
	@echo "---"
	@for f in $(PWD)/*.ko; do \
	   printf "%-28s " "$$f"; \
	   strings -a "$$f" | grep -m1 '^vermagic=' || echo "(tanpa vermagic)"; \
	done

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean 2>/dev/null || true

.PHONY: all clean
endif
