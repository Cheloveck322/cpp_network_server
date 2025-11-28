#!/usr/bin/env bash
set -e

# Собираем всё
make test

# Запускаем сервер в фоне
./bin/cpp-network-server &
SERVER_PID=$!

# Небольшая пауза, чтобы сервер поднялся
sleep 1

# Запускаем тестовый клиент
./bin/test-server
STATUS=$?

# Гасим сервер
kill "$SERVER_PID" || true
wait "$SERVER_PID" 2>/dev/null || true

exit "$STATUS"
