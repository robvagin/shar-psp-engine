# Общий список исходников «игры без экрана» (tools/bots/world.c + общий код viewer/): его берут tools/bots/build.sh,
# tests/collect/host.sh, tests/savegame/host.sh. Один список — новый модуль мира добавляется здесь, а не в трёх местах
# (28.09: traffic.c попал только в build.sh — tests/collect/host.sh перестал собираться). Вход: V — папка viewer/.
BOTS_LIB="tools/bots/world.c $V/game.c $V/car.c $V/col.c $V/props.c $V/stateprops.c $V/coins.c $V/homer.c $V/watchdog.c $V/watchdog_game.c $V/skin.c $V/rooms.c $V/roomcam.c \
  $V/collect.c $V/traffic.c $V/msncar.c $V/game_msn.c $V/shop.c $V/mission/msn_file.c $V/mission/msn_run.c $V/mission/msn_ops.c $V/mission/msn_game.c"
[ -f "$V/msn_audio.c" ] && BOTS_LIB="$BOTS_LIB $V/msn_audio.c tools/bots/msn_audio_host.c"   # звук миссий (голос, музыка)
[ -f "$V/animobj.c" ] && BOTS_LIB="$BOTS_LIB $V/animobj.c"   # правка 116: объекты ANM1 (подъёмник Duff, двери, платформы АЭС) шагают тем же кодом
