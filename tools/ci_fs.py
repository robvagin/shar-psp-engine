"""Поиск файла без учёта регистра букв: общий помощник сборки для Linux и WSL.

macOS не различает регистр в именах файлов, Linux различает. Распаковка диска (hit-and-run-web/tools/extract.py) кладёт имена строчными,
а в скриптах экспорта часть имён набрана по-другому (имена файлов из данных игры: «TVscreen», «Sunday_Drive_Intro»). На macOS это работало
само, на Linux файл «не найден».

Здесь одно место, где это чинится:
  resolve(путь)  -> настоящий путь на диске (точное имя, если оно есть; иначе поиск по каждой части пути без учёта регистра) или None
  install()      -> на файловой системе с учётом регистра подменяет чтение (open, os.stat, os.lstat, os.access, os.listdir, os.scandir):
                    если точного имени нет, берётся единственное совпадение без учёта регистра. Запись и создание файлов не трогаются.
                    На файловой системе без учёта регистра (macOS по умолчанию) не делает ничего: поведение прежнее.
Включение: tools/_ci/sitecustomize.py (его подхватывает Python через PYTHONPATH, которую выставляют setup.sh-цепочка: build_level.sh, ship.sh,
build_all_levels.sh). SHAR_CI_FS=1 — включить принудительно (проверка на macOS), SHAR_CI_FS=0 — выключить.
Только стандартная библиотека. Проверка: python3 tests/ci_fs/check.py (с красным контролем).
"""
import builtins, io, os

_real_open = builtins.open
_real_stat = os.stat
_real_lstat = os.lstat
_real_access = os.access
_real_listdir = os.listdir
_real_scandir = os.scandir
_cache = {}          # каталог -> {имя в нижнем регистре: [настоящие имена]}
_installed = False
SELF_DIR = os.path.dirname(os.path.abspath(__file__))


def _names(d, listdir=None):
    """имена в каталоге d по нижнему регистру; None — каталога нет"""
    if listdir is None and d in _cache:
        return _cache[d]
    try:
        names = (listdir or _real_listdir)(d or '.')
    except OSError:
        return None
    m = {}
    for n in names:
        m.setdefault(n.lower(), []).append(n)
    if listdir is None:
        _cache[d] = m
    return m


def resolve(path, listdir=None):
    """настоящий путь на диске для path без учёта регистра букв; None — не нашли или нашли два имени, отличающихся только регистром.
    Точное совпадение имени всегда главнее. Части «.» и «..» не трогаются. listdir — подмена для проверки."""
    p = os.fspath(path)
    if not isinstance(p, str) or not p:
        return None
    cur = os.sep if p.startswith(os.sep) else ''
    for part in p.split(os.sep):
        if part == '':
            continue
        if part in ('.', '..'):
            cur = os.path.join(cur, part); continue
        d = cur or '.'
        names = _names(d, listdir)
        reals = names.get(part.lower()) if names is not None else None
        if not reals and listdir is None and names is not None:
            _cache.pop(d, None)          # файл мог появиться после кэша
            names = _names(d)
            reals = names.get(part.lower()) if names is not None else None
        if not reals:
            return None
        if part in reals:
            pick = part
        elif len(reals) == 1:
            pick = reals[0]
        else:
            return None
        cur = os.path.join(cur, pick)
    if p.endswith(os.sep) and not cur.endswith(os.sep):
        cur += os.sep
    return cur or '.'


def _fix(path):
    """замена пути, если точного имени нет, а совпадение без учёта регистра есть; иначе None"""
    if not isinstance(path, (str, os.PathLike)):
        return None
    try:
        p = os.fspath(path)
    except TypeError:
        return None
    if not isinstance(p, str):
        return None
    r = resolve(p)
    return r if r is not None and r != p else None


def _open(file, mode='r', *a, **k):
    try:
        return _real_open(file, mode, *a, **k)
    except (FileNotFoundError, NotADirectoryError):
        if isinstance(mode, str) and any(c in mode for c in 'wax+'):
            raise
        alt = _fix(file)
        if alt is None:
            raise
        return _real_open(alt, mode, *a, **k)


def _wrap(real):
    def f(*args, **k):
        try:
            return real(*args, **k)
        except (FileNotFoundError, NotADirectoryError):
            path = args[0] if args else k.get('path')
            if path is None or k.get('dir_fd') is not None:
                raise
            alt = _fix(path)
            if alt is None:
                raise
            return real(alt, *args[1:], **k) if args else real(**dict(k, path=alt))
    f.__name__ = getattr(real, '__name__', 'f')
    return f


def case_sensitive_fs(probe=SELF_DIR):
    """True, если файловая система под probe различает регистр (папка в другом регистре не открывается)"""
    sw = probe.swapcase()
    if sw == probe:
        return False
    try:
        return not os.path.samefile(probe, sw)
    except OSError:
        return True


def install(force=None):
    """включить подмену чтения; возвращает True, если включено"""
    global _installed
    f = os.environ.get('SHAR_CI_FS') if force is None else str(int(bool(force)))
    if f == '0' or _installed:
        return _installed
    if f != '1' and not case_sensitive_fs():
        return False
    builtins.open = _open
    io.open = _open
    os.stat = _wrap(_real_stat)
    os.lstat = _wrap(_real_lstat)
    os.access = _wrap(_real_access)
    os.listdir = _wrap(_real_listdir)
    os.scandir = _wrap(_real_scandir)
    _installed = True
    return True


def uninstall():
    """вернуть настоящие функции (для проверки)"""
    global _installed
    builtins.open = _real_open
    io.open = _real_open
    os.stat = _real_stat
    os.lstat = _real_lstat
    os.access = _real_access
    os.listdir = _real_listdir
    os.scandir = _real_scandir
    _installed = False
    _cache.clear()
