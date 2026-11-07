# Ловушка вылета SHARVIEW: модуль ядра sharexc.prx (viewer/kexc/main.c). Сборка: make -C viewer/kexc (PSPDEV — pspdev/)
TARGET = sharexc
OBJS = main.o
BUILD_PRX = 1
PRX_EXPORTS =
USE_PSPSDK_LIBC = 1
USE_KERNEL_LIBS = 1
CFLAGS = -O2 -G0 -Wall
LIBS = -lpspdebug -lpspge_driver -lpspdisplay_driver -lpspkernel
PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

# №1208б/в: картинка экрана вылета вшивается в модуль (сжатая). Источник по порядку: kexc/crashpic.shx (явный файл), запасная БЕЗ рамки роликов из картинки Роба (assets/crash/technical_difficulties_source.png,
# tools/crashpic_noframe.py; есть всегда), заглушка crashpic_placeholder.shx (метка SHXPIC1:PLACEHOLDER; гейт tools/crashpic_gate.sh --final её не пропускает). Рядом с .shx — .txt со строкой «code_rect x y w h».
PY ?= $(firstword $(wildcard ../../.venv/bin/python) $(wildcard $(HOME)/Downloads/shar-psp/.venv/bin/python) python3)
SRCPNG = ../../assets/crash/technical_difficulties_source.png
.PHONY: crashpic_noframe.shx crashpic_data.h
crashpic_noframe.shx:
	@if [ -f $(SRCPNG) ]; then $(PY) ../../tools/crashpic_noframe.py $(SRCPNG) crashpic_noframe.shx; else rm -f crashpic_noframe.shx crashpic_noframe.txt; fi
crashpic_data.h: crashpic_noframe.shx
	@S=crashpic_placeholder.shx; PH=--placeholder; for f in crashpic.shx crashpic_noframe.shx; do if [ -f $$f ]; then S=$$f; PH=; break; fi; done; python3 ../../tools/crashpic_embed.py $$S crashpic_data.h $$PH
main.o: crashpic_data.h
