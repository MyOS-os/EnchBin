# scripts/Makefile — команды сборки EnchBin.
# Вызывается builder.c из корня репозитория (make -f scripts/Makefile <цель>).

CC ?= cc
LD ?= ld
AR ?= ar

CFLAGS_K = -std=c11 -ffreestanding -fno-stack-protector -fno-pic -mno-red-zone -mcmodel=kernel -O2 -Wall -Wextra -Iinclude
CFLAGS_U = -std=c11 -ffreestanding -fno-stack-protector -fno-pic -mno-red-zone -mcmodel=small -O2 -Wall -Wextra -Iinclude
# ring0-единицы (.mke/.b_d/приложения Apps и MyOS): ET_REL, релокации против
# ksyms ядра; fno-pic (без GOT), mcmodel=kernel (рядом с ядром, PC32 валиден).
CFLAGS_P = -std=c11 -ffreestanding -fno-stack-protector -fno-pic -mno-red-zone -mcmodel=kernel -O2 -Wall -Wextra -Iinclude

.PHONY: obj_k obj_u obj_p link_kernel link_reloc link_app ar embed

obj_k:
	$(CC) $(CFLAGS_K) $(EXTRA_INC) $(EXTRA_DEF) -c $(SRC) -o $(OBJ)

obj_u:
	$(CC) $(CFLAGS_U) $(EXTRA_INC) $(EXTRA_DEF) -c $(SRC) -o $(OBJ)

obj_p:
	$(CC) $(CFLAGS_P) $(EXTRA_INC) $(EXTRA_DEF) -c $(SRC) -o $(OBJ)

link_kernel:
	$(LD) -T $(LINKER) -o $(OUT) $(OBJS) $(EXTRA)

link_reloc:
	$(LD) -r -o $(OUT) $(OBJS)

link_app:
	$(CC) $(CFLAGS_U) -nostdlib -static -Wl,--build-id=none -Wl,-T,$(LINKER) $(OBJS) $(LIB) -o $(OUT)

ar:
	$(AR) rcs $(OUT) $(OBJS)

embed:
	cd $(DIR) && objcopy -I binary -O elf64-x86-64 -B i386:x86-64 --rename-section .data=.enchmke,alloc,load,readonly,data,contents $(BIN) $(OUT)
