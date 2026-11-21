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
# -Wno-error: kernel build menambah -Werror; modul diagnosa lebih baik
# warning (mis. prototype kprobe) tidak menggagalkan artifact.
ccflags-y += -Wno-error
# CFI: kernel stock (Unisoc/Android, clang 12.0.5) instrumented CFI_CLANG_SHADOW.
# Modul TANPA __cfi_check => mod->cfi_check=NULL => cfi_slowpath_handler
# -> "Don't allow unchecked modules" -> panic(). Fix: ekspor __cfi_check STUB
# (lihat bfkrt_ko.h). -fsanitize=cfi-icall TIDAK dipakai: butuh -flto
# ("invalid argument '-fsanitize=cfi-icall' only allowed with '-flto'") dan
# flavor stock = KCFI (-fsanitize=kcfi) yang tidak ada di Ubuntu clang-12.

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
