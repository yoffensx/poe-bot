# ADR-0002: Python + C++ DLL разделение

- **Статус:** Принято
- **Дата:** 2026-09-26
- **Авторы:** Maik

---

## Контекст

Бот требует:
1. **Низкую задержку** для захвата экрана (DXGI требует C++ API)
2. **Высокую точность** input humanizer (timing критичен)
3. **Быструю итерацию** логики (частые изменения FSM, конфигов, фильтров)
4. **Богатые CV-библиотеки** (OpenCV, Tesseract — лучшие биндинги в Python)

## Решение

**Двухуровневый стек:**

```
Python 3.12 (логика, CV, OCR, FSM, конфиги)
    ↕ ctypes / cffi
C++ DLL (DXGI capture, input humanizer)
```

### `poe2_capture.dll`
- DXGI Desktop Duplication API
- Выдаёт raw BGRA frames в shared memory / numpy array via ctypes
- Экспортирует: `init_capture()`, `grab_frame(buf, w, h)`, `release()`

### `poe2_input.dll`
- Bezier curve mouse movement
- Gaussian jitter
- Humanized keyboard (random inter-key delays)
- Экспортирует: `move_mouse(x, y, duration_ms)`, `click(x, y, btn)`, `key_press(vk, delay_ms)`

## Обоснование

| Компонент | Язык | Причина |
|-----------|------|---------|
| DXGI Capture | C++ | IDXGIOutputDuplication — нет Python биндингов, нужен COM |
| Input Humanizer | C++ | Точный timing, QueryPerformanceCounter, нет GIL |
| OCR | Python | pytesseract, easyocr — лучшие биндинги |
| OpenCV | Python | cv2 — полные биндинги, быстрее разработка |
| FSM / Logic | Python | Частые изменения, читаемость важна |
| Config | Python | tomllib (stdlib), Pydantic v2 |

### Альтернативы отклонены

- **Pure Python capture (mss/d3dshot):** задержка 15-30ms vs 1-2ms у DXGI Duplication; mss не поддерживает HDR
- **Pure C++ бот:** медленная итерация, нет хороших OCR-биндингов, сложнее поддерживать конфиги
- **Rust:** нет выигрыша над C++ DLL + Python; меньше CV-экосистема

## Последствия

- C++ DLL компилируется через CMake + MSVC (Visual Studio Build Tools 2022)
- Биндинг через Python `ctypes` (не Cython — меньше сложности)
- Shared memory для frames (избежать копирования 1080p BGRA = ~8MB/frame при 30fps)
- Сборка DLL — отдельный шаг в setup (`cmake --build`)

## Связанные ADR

- ADR-0001: External architecture
- ADR-0003: Pixel-based vision
