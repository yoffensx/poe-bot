# 003 · C++ DXGI capture DLL + ctypes-обёртка

| | |
|---|---|
| **Статус** | To Do |
| **Приоритет** | high |
| **Волна** | Wave 1 — Ввод и вывод |
| **Блокеры** | 002 |
| **Блокирует** | 005, 006, 010 |
| **Obsidian ID** | `ca2c420d-0f71-4e07-896b-b3838c715bb8` |

## Контекст

`cpp/capture/` пуст. Нужен источник кадров для всех vision-модулей. По ADR-0002 захват выносится в C++ DLL,
по ADR-0003 — только пиксели, без чтения памяти процесса.

## Задача

- [ ] `cpp/capture/CMakeLists.txt` + исходники DLL: DXGI Desktop Duplication API, x64/Release, MSVC.
- [ ] Передача BGRA-кадра в Python: named shared memory или pipe + дескрипторы в структуре.
- [ ] `capture/frame_source.py`: ctypes-биндинг, выдача `numpy.ndarray` без копий где возможно.
- [ ] Контекстный менеджер, корректная обработка `DXGI_ERROR_ACCESS_LOST` (смена разрешения/фокуса).
- [ ] Dev-fallback (mss/GDI), чтобы слой работал без собранной DLL.
- [ ] Замер и логирование latency захвата.

## Инварианты

- External only: хендл `PathOfExile2.exe` не открываем, хуки не ставим.
- Захват идёт compositor-уровнем (DXGI), а не `PrintWindow`/`BitBlt` по окну.

## Гейт

```bash
cmake -B build/cpp cpp/ -DCMAKE_BUILD_TYPE=Release
cmake --build build/cpp --config Release
uv run pytest tests/unit/test_frame_source.py
```
