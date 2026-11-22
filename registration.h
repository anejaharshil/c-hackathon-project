#ifndef REGISTRATION_H
#define REGISTRATION_H

#include <stddef.h>

int registration_is_valid(const char *text);
int registration_normalize(char *out, size_t size, const char *text);
int sap_id_is_valid(const char *text);
int teacher_id_is_valid(const char *text);

#endif /* REGISTRATION_H */