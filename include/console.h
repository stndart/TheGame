#pragma once

#include <fstream>

static std::ofstream log_file;
static bool console_created = false;

void create_console();

void log_message(const char *message);

void logf(const char *format, ...);
