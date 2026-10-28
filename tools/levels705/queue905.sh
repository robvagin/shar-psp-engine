#!/bin/bash
# очередь экспортов, не больше 3 одновременно; формат строки: ИМЯ N флаги
cd ~/Downloads/shar-psp-t75-lv
while read -r name n flags; do
  [ -z "$name" ] && continue
  while [ $(pgrep -f "t75lv/lvx.sh" | wc -l) -ge 3 ]; do sleep 5; done
  nohup build/t75lv/lvx.sh $name $n $flags >/dev/null 2>&1 &
  sleep 2
done < build/t75lv/jobs905.txt
wait
