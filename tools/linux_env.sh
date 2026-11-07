# source tools/linux_env.sh — на Linux кладёт в PATH обёртки stat/cp/sed под синтаксис macOS (BSD): тесты и служебные скрипты репозитория
# написаны на Маке (stat -f %z, cp -c, sed -i ''). Сборка ROM (tools/ship.sh) в обёртках не нуждается — она использует tools/portable.sh.
if [ "$(uname -s)" = Linux ]; then
  _SHAR_SHIM="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)/linux_shim/bin"
  case ":$PATH:" in *":$_SHAR_SHIM:"*) ;; *) export PATH="$_SHAR_SHIM:$PATH" ;; esac
  unset _SHAR_SHIM
fi
