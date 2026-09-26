# ADR-0001: Выбор External-архитектуры бота

- **Статус:** Принято
- **Дата:** 2026-09-26
- **Авторы:** Maik

---

## Контекст

Path of Exile 2 использует собственный лёгкий клиентский античит без ring-0 драйвера.  
Защита строится на:
- Проверках целостности памяти процесса
- Базовом обнаружении инъекций (WriteProcessMemory, remote thread creation)
- Серверной телеметрии паттернов ввода (скорость кликов, регулярность действий)
- Скриншотах и сравнении рендера

Основная поверхность детектирования — серверная телеметрия поведения, а не ring-0 kernel callbacks.

## Решение

**Использовать строго External-архитектуру:**

- Screen capture через DXGI Desktop Duplication API (захват compositor, не хук D3D)
- Ввод через SendInput / driver-level device (KMBox/VmMulti опционально)
- Никакого `OpenProcess` с `PROCESS_VM_READ` к `PathOfExile2.exe`
- Никаких инъекций (LoadLibrary, manual map, APC, etc.)
- Никаких хуков D3D/DirectInput/RawInput внутри процесса игры

## Обоснование

| Вариант | Pros | Cons |
|---------|------|------|
| **External (выбрано)** | Нет intra-process footprint, нет инъекций, обходит integrity checks | Не читает память → OCR/CV сложнее |
| Internal (DLL injection) | Прямой доступ к game state, быстрее | Детектируется integrity checks, CreateRemoteThread ban-wave |
| Kernel driver | Полный доступ, невидимость от юзерленда | GGG может добавить ring-0 AC в любой патч; огромный риск |
| Memory reading (external) | Точные данные без OCR | `OpenProcess(PROCESS_VM_READ)` — стандартная точка детектирования |

External + Pixel-based — наиболее устойчивый вариант при текущей защите GGG.  
При добавлении GGG ring-0 драйвера — External остаётся рабочим без изменений архитектуры.

## Последствия

- Vision layer должен быть достаточно robust для чтения всей нужной информации из пикселей
- OCR должен точно читать текст модов, имён итемов, чисел HP/MP
- Скорость OCR и CV критична — frame processing < 33ms (30 FPS бота)
- DXGI Duplication API требует C++ реализации (оборачивается Python DLL-binding)

## Связанные ADR

- ADR-0002: Python + C++ DLL split
- ADR-0003: Pixel-based vision
