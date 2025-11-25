#pragma once

#include <fstream>

static std::ofstream log_file;
static std::ofstream netlog_file;
static bool console_created = false;

void create_console();

void log_message(const char *message);

void logf(const char *format, ...);
void logns(int socket, const char *addr, int port);
void logn(int socket, size_t len, char *data, bool in = false);