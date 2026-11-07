# Python подхватывает этот файл сам, если папка tools/_ci есть в PYTHONPATH (её выставляют build_level.sh, ship.sh, build_all_levels.sh).
# На Linux и WSL включает поиск файлов без учёта регистра (tools/ci_fs.py); на macOS с обычной файловой системой ничего не делает.
import os, sys
try:
    sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))   # tools/
    import ci_fs
    ci_fs.install()
except Exception:   # подмена не должна ронять сам интерпретатор
    pass
finally:
    try: sys.path.remove(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    except ValueError: pass
