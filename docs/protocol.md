# TermiHUI WebSocket Protocol

## Обзор

TermiHUI использует WebSocket соединение для связи между клиентами и сервером. Все сообщения передаются в формате JSON.

**Адрес подключения:** `ws://localhost:37854`

## Структура сообщений

Все сообщения имеют поле `type`, определяющее тип операции:

```json
{
  "type": "имя_операции",
  "...": "дополнительные поля"
}
```

## Операции от клиента к серверу

### 1. Выполнение команды
Запускает новую команду в терминале.

**Запрос:**
```json
{
  "type": "execute",
  "command": "ls -la"
}
```

**Ответы:**
```json
{
  "type": "output",
  "data": "total 16\ndrwxr-xr-x  4 user  staff  128 Jan 15 10:30 .\n..."
}
```

```json
{
  "type": "status",
  "running": false,
  "exit_code": 0
}
```

### 2. Отправка ввода
Отправляет текст в активный терминал (эмулирует ввод с клавиатуры).

**Запрос:**
```json
{
  "type": "input",
  "text": "hello world\n"
}
```

**Ответ:**
```json
{
  "type": "input_sent",
  "bytes": 12
}
```



## Сообщения от сервера к клиенту

### 1. Вывод команды
Передаёт данные, выведенные командой в терминал. Для одной команды может быть отправлено несколько сообщений `output` по мере поступления данных.

```json
{
  "type": "output",
  "data": "Hello World\n"
}
```

### 2. Статус процесса
Уведомляет о завершении команды. Отправляется только один раз в конце выполнения.

```json
{
  "type": "status",
  "running": false,
  "exit_code": 0
}
```

### 3. Ошибки
Сообщает об ошибках выполнения операций.

```json
{
  "type": "error",
  "message": "Command execution failed",
  "error_code": "COMMAND_FAILED"
}
```

### 4. Подтверждение подключения
Отправляется при успешном подключении к серверу.

```json
{
  "type": "connected",
  "server_version": "1.0.0"
}
```

## Коды ошибок

| Код | Описание |
|-----|----------|
| `COMMAND_FAILED` | Не удалось выполнить команду |
| `INVALID_REQUEST` | Некорректный формат запроса |
| `SERVER_ERROR` | Внутренняя ошибка сервера |

## Примеры использования

### Простое выполнение команды
```json
// Клиент → Сервер
{"type": "execute", "command": "echo 'Hello World'"}

// Сервер → Клиент  
{"type": "output", "data": "Hello World\n"}
{"type": "status", "running": false, "exit_code": 0}
```

### Последовательное выполнение команд
```json
// Выполнить первую команду
{"type": "execute", "command": "pwd"}

// Получить результат
{"type": "output", "data": "/Users/username\n"}
{"type": "status", "running": false, "exit_code": 0}

// Выполнить вторую команду
{"type": "execute", "command": "ls -la"}
```

### Команды с частичным выводом
```json
// Запустить команду с длительным выводом
{"type": "execute", "command": "journalctl -f"}

// Сервер отправляет данные по частям
{"type": "output", "data": "Jan 15 10:30:01 host systemd[1]: Starting service...\n"}
{"type": "output", "data": "Jan 15 10:30:02 host systemd[1]: Service started.\n"}
{"type": "output", "data": "Jan 15 10:30:03 host app[1234]: Processing request...\n"}
// ... продолжается до завершения команды

// Когда команда завершается (например, Ctrl+C)
{"type": "status", "running": false, "exit_code": 130}
```

### Интерактивная работа
```json
// Запустить интерактивную программу
{"type": "execute", "command": "python3"}

// Сервер отправляет приглашение
{"type": "output", "data": "Python 3.9.0\n>>> "}

// Отправить код
{"type": "input", "text": "print('Hello')\n"}

// Получить результат
{"type": "output", "data": "Hello\n>>> "}
```

## Версионирование

Текущая версия протокола: **1.0.0**

При изменении API версия будет обновляться согласно [Semantic Versioning](https://semver.org/):
- **MAJOR** - несовместимые изменения
- **MINOR** - новая функциональность с обратной совместимостью  
- **PATCH** - исправления багов

Клиенты должны проверять поддерживаемую версию при подключении.