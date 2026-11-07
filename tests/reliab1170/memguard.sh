#!/bin/bash
# №1170: гейт отказа памяти при загрузке уровня. Красные: (1) прежний порядок без проверки (LEGACY_MUST_FAIL=1), (2) разбор исходников на копии с вырезанными проверками. Потом зелёные.
cd "$(dirname "$0")/../.."; export DEVELOPER_DIR=${DEVELOPER_DIR:-/Library/Developer/CommandLineTools}
O=build/reliab1170; mkdir -p $O
cc -std=c11 -O1 -Wall -I viewer -DLEGACY_MUST_FAIL=1 tests/reliab1170/memguard.c -o $O/mg_red 2> $O/cc_mg_red.txt || { echo "FAIL: сборка"; exit 1; }
$O/mg_red > $O/mg_red.txt && { echo "FAIL: красный контроль (прежний порядок) не упал"; exit 1; }
cc -std=c11 -O1 -Wall -I viewer -DLEGACY_MUST_FAIL=0 tests/reliab1170/memguard.c -o $O/mg_green 2>> $O/cc_mg_red.txt && $O/mg_green > $O/mg_green.txt || { cat $O/mg_green.txt; echo "FAIL: mgCallocN"; exit 1; }
T=$O/old_tree; rm -rf $T; mkdir -p $T/viewer; cp viewer/main_levels.h viewer/main_collect.h $T/viewer/
sed -i.bak 's/mgCallocN(/mgOldCallocN(/; s/!cxTex \&\& !LOADMEM1170_OLD/0/' $T/viewer/main_levels.h $T/viewer/main_collect.h
python3 tests/reliab1170/memguard.py $T > $O/mg_src_red.txt && { echo "FAIL: красный контроль разбора исходников не упал"; exit 1; }
python3 tests/reliab1170/memguard.py . > $O/mg_src.txt || { cat $O/mg_src.txt; echo "FAIL: проверки в main_levels.h/main_collect.h не подключены"; exit 1; }
echo "RESULT: OK — красные: $(tail -1 $O/mg_red.txt | cut -c1-60); $(cut -c1-70 $O/mg_src_red.txt); зелёные: $(tail -1 $O/mg_green.txt); $(cat $O/mg_src.txt)"
