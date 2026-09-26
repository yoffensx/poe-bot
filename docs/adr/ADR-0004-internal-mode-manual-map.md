# ADR-0004: Internal Mode — Manual Map Injection & Headless GameState (SSOT)

- **Статус:** Принято (Глобальный архитектурный SSOT проекта)
- **Дата обновления:** 2026-09-27
- **Первоначальная дата:** 2026-09-26
- **Авторы:** Maik

---

## Контекст

Главная цель poe2-bot — обеспечение масштабируемой работы **крупных ферм с десятками и сотнями параллельных ботов на одном физическом или выделенном сервере**.

При запуске 50-100+ клиентов PoE2 на одном сервере подход External (захват кадров через DXGI + Tesseract OCR) технически нежизнеспособен:
- Требует полноценного рендера графики каждым клиентом игры, перегружая GPU и VRAM.
- Потребляет гигантские ресурсы CPU на обработку кадров и распознавание текста.
- Даёт высокую задержку (15-40ms) и приблизительные данные.

Для промышленной фермы требуется **Zero-Render Headless Architecture**: игра запускается без прорисовки полигонов/текстур (или с минимальным dummy-контекстом), а бот общается с игрой на уровне чистых структур данных со сверхнизким потреблением ресурсов (<0.5% CPU на инстанс).

---

## Архитектурное решение

**Internal Mode является основным и единственным целевым режимом работы (Primary SSOT):**

```
+-------------------------------------------------------------------------+
|                              СЕРВЕРНАЯ ФЕРМА                             |
|                                                                         |
|  [PoE2 Process 1 (PID 1042)] <--- manual-mapped stealth payload.dll     |
|         │ writes at 30 Hz (no render required)                          |
|         ▼                                                               |
|  [Shared Memory: Local\poe2_gs_1042]                                    |
|         ▲                                                               |
|         │ reads zero-copy (<1ms)                                        |
|  [Python InternalBridge 1] ──► [Bot FSM 1] ──► [Humanized Input 1]      |
|                                                                         |
|  [PoE2 Process N (PID 8920)] <--- manual-mapped stealth payload.dll     |
|         │ writes at 30 Hz (no render required)                          |
|         ▼                                                               |
|  [Shared Memory: Local\poe2_gs_8920]                                    |
|         ▲                                                               |
|         │ reads zero-copy (<1ms)                                        |
|  [Python InternalBridge N] ──► [Bot FSM N] ──► [Humanized Input N]      |
+-------------------------------------------------------------------------+
```

Конфигурация по умолчанию (`configs/build_configs/*.toml`):
```toml
[bot]
mode = "internal"           # "internal" (SSOT) | "external" (deprecated fallback)
poe2_pid = 0                # 0 = автоопределение PID
instance_id = 0             # 0 = использовать PID для изоляции shm
loader_path = "cpp/build/Release/poe2_loader.exe"
payload_path = "cpp/build/Release/poe2_payload.dll"
```

---

## Технический стек и компоненты

### 1. Manual Map Loader (`cpp/loader/mapper.cpp`)
Инжектирует payload DLL в процесс PoE2 полностью скрытно:
- **Прямые системные вызовы (Direct Syscalls):** Реализация Hell's Gate с динамическим извлечением SSN из ntdll и Halo's Gate fallback в случае перехвата прологов функций античитом/EDR.
- **Минимальные привилегии:** `OpenProcess` запрашивает только `PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION`.
- **Чистый PEB:** DLL не регистрируется в списках загрузчика Windows (`InLoadOrderModuleList` / `InMemoryOrderModuleList`).
- **Скрытие заголовков:** DOS magic (`MZ`) и NT-заголовки затираются нулями в целевой памяти сразу после завершения релокаций.
- **Скрытие потока:** Создание потока через `NtCreateThreadEx` с флагом `THREAD_CREATE_FLAGS_HIDE_FROM_DEBUGGER` (0x04).
- **Изоляция инстансов:** Loader передаёт `instance_id` (по умолчанию PID) payload-у через стартовый блок памяти.

### 2. Stealth Payload DLL (`cpp/internal/payload.cpp`)
Работает внутри процесса PoE2:
- **Zero-CRT:** Скомпилирована с `/NODEFAULTLIB`, `/GS-`, `/GR-`, `/MT`, `/O2`. Не тянет зависимости от C-runtime.
- **Hashed Imports:** Все системные функции разрешаются вручную через обход PEB/EAT по хэшам djb2 (никаких строковых литералов в бинарнике).
- **Безопасное чтение памяти (SEH):** Обернуто в `__try / __except`, что предотвращает краш клиента при смене указателей или переходе между локациями.
- **AOB Pattern Scanning:** Динамический поиск базы `GameState` в памяти процесса.
- **Strict Read-Only:** Payload только читает состояние игры и упаковывает его в `GameStatePacket` (POD-структуру). Никаких изменений игровой логики или хуков D3D.
- **30 Hz Update:** Фоновый поток выполняет запись в shared memory и засыпает на 33ms, практически не создавая нагрузки на процессор.

### 3. Партиционированная Shared Memory (`cpp/common/game_state.h`)
- Имя сегмента: `Local\poe2_gs_<instance_id>` (гарантирует бесконфликтность сотен ботов).
- Размер буфера: 64 КБ (хватает на игрока, фласки, до 128 монстров, 32 мода карты, маску инвентаря).
- Контроль целостности: `magic = 0x504F4532` ('POE2'), `version = 1`, `frameSeq` (монотонный счетчик кадров), флаг `writerReady`.

### 4. Python Host Bridge (`src/capture/internal_bridge.py` & `capture_service.py`)
- Zero-copy чтение через стандартный модуль `mmap` без сторонних тяжелых зависимостей.
- `BridgeRegistry`: реестр мостов для управления N инстансами.
- Фабрика `make_services_for_pids([pid1, pid2, ...])` для запуска фермы.
- Латентность чтения данных: **< 1 миллисекунды**.

---

## Преимущества для серверных ферм

| Показатель | Старый подход (External / OCR) | Новый подход (Internal Headless SSOT) |
|---|---|---|
| Плотность ботов на сервер | 2-4 процесса (упирается в GPU/VRAM) | **100+ процессов на 1 сервер** |
| Необходимость GPU / рендера | Обязательно (3D полигоны, шрифты) | **НЕТ (Zero-render / dummy context)** |
| Нагрузка CPU на 1 бота | 15-30% от ядра (OpenCV + Tesseract) | **< 0.5% одного потока** |
| Скорость реакции FSM | 30-50 ms | **< 1 ms** |
| Точность навигации | Пиксели на миникарте с шумом | **Точные float координаты (x, y, z)** |
| Анализ монстров | Цветные пятна (нет данных по HP) | **ID, точные координаты, тип и текущее HP** |

---

## Связанные решения

- [ADR-0001: Выбор архитектуры](ADR-0001-external-architecture.md) (заменён на настоящий документ)
- [ADR-0002: Python + C++ разделение](ADR-0002-python-cpp-split.md)
- [ADR-0003: Pixel-based vision (Deprecated)](ADR-0003-pixel-based-vision.md)
