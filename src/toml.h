#ifndef TOML_H
#define TOML_H

#include <stddef.h>

struct toml_table;

struct toml_table *toml_parse_file(const char *path);
void toml_free(struct toml_table *table);

const char *
toml_get_string(const struct toml_table *table, const char *section, const char *key, const char *default_value);
long toml_get_int(const struct toml_table *table, const char *section, const char *key, long default_value);
double toml_get_double(const struct toml_table *table, const char *section, const char *key, double default_value);
int toml_get_bool(const struct toml_table *table, const char *section, const char *key, int default_value);

#endif // !TOML_H
