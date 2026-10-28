№1064 гейт (ручной запуск tools/emu_run.sh, тур-EBOOT с флагами build_g76tour, SET = test76tour):
 view_default.txt — по умолчанию (Original), view_tinted.txt — flag curbtint 1 до загрузки уровня; уровни 1, 3, 6 (подставить в golevel); красный/эталон — EBOOT TEST76 с тем же сценарием.
 Зелёный: Tinted против TEST76 = 0 пикселей (L1, L3, L6); Original против TEST76 — только бордюр/ступени (L1 667 и 1079 px), тротуар без изменений.
 menu.txt — пауза → OPTIONS → PICTURE → 7 раз вниз (Curbs), вправо; лог OPT ... curb=1, settings.bin ver 14. persist.txt — повторный запуск с сохранённым settings.bin (OVL): ver 14 Tinted = Tinted, ver 13 (без поля) = Original.
 pmenu с новой строкой ×7: build/c64/ovl (tools/menu_export.py --pack pmenu; CURB1064_OLD=1 даёт файлы TEST76 побайтно).
