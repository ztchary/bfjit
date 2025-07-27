// ZTCHARY'S C UTILS (ZCU) //

#ifndef ZCU_H_
#define ZCU_H_

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define DA_INIT_CAP 16
#define HT_INIT_CAP 2
#define FNV_OFFSET 0xcbf29ce484222325
#define FNV_PRIME 0x100000001b3

#define zcu_assert(cond, msg) \
do {                       \
	if (cond) break;       \
	printf("%s\n", (msg)); \
	result = 1;            \
	goto defer;            \
} while (0)

#define zcuda_reserve(da, new_cap)                                        \
do {                                                                      \
	if ((da)->cap > (new_cap)) break;                                     \
	if ((da)->cap == 0) (da)->cap = DA_INIT_CAP;                          \
	while ((da)->cap < (new_cap)) (da)->cap *= 2;                         \
	(da)->items = realloc((da)->items, (da)->cap * sizeof(*(da)->items)); \
} while (0)

#define zcuda_append(da, item)             \
do {                                       \
	zcuda_reserve((da), (da)->count + 1);  \
	(da)->items[(da)->count++] = (item);   \
} while (0)

#define zcuda_appends(da, add_i, add_c)                                       \
do {                                                                          \
	zcuda_reserve((da), (da)->count + (add_c));                               \
	memcpy((da)->items + (da)->count, (add_i), (add_c)*sizeof(*(da)->items)); \
	(da)->count += (add_c);                                                   \
} while (0)

#define zcuda_append_cstr(da, cstr) zcuda_appends((da), (cstr), strlen(cstr))

#define zcuda_free(da) free((da)->items);

typedef struct {
	char *key;
	void *value;
} zcuht_entry;

typedef struct {
	size_t count;
	size_t cap;
	zcuht_entry *entries;
} zcuht;

uint64_t zcu_fnv1a(char *data);
zcuht *zcuht_create();
void zcuht_destroy(zcuht *ht);
void zcuht_set_entry(zcuht_entry *hte, size_t cap, char *key, void *value);
void zcuht_resize(zcuht *ht);
void zcuht_set(zcuht *ht, char *key, void *value);
bool zcuht_get(zcuht *ht, char *key, void **value);
void zcuht_delete(zcuht *ht, char *key);

#ifdef ZCU_IMPLEMENTATION

uint64_t zcu_fnv1a(char *data) {
	uint64_t hash = FNV_OFFSET;
	while (*data) {
		hash ^= *(data++);
		hash *= FNV_PRIME;
	}
	return hash;
}

zcuht *zcuht_create() {
	zcuht *ht = (zcuht *)calloc(1, sizeof(zcuht));
	ht->cap = HT_INIT_CAP;
	ht->entries = (zcuht_entry *)calloc(HT_INIT_CAP, sizeof(zcuht_entry));
	return ht;
}

void zcuht_destroy(zcuht *ht) {
	for (size_t i = 0; i < ht->cap; i++) {
		if (ht->entries[i].key != NULL) free(ht->entries[i].key);
	}
	free(ht->entries);
	free(ht);
}

void zcuht_set_entry(zcuht_entry *hte, size_t cap, char *key, void *value) {
	uint64_t hash = zcu_fnv1a(key);
	size_t index = hash & (cap - 1);
	while (hte[index].key != NULL) {
		if (strcmp(hte[index].key, key) == 0) {
			hte[index].value = value;
			return;
		}
		index = (index + 1) & (cap - 1);
	}

	hte[index].key = strdup(key);
	hte[index].value = value;
}

void zcuht_resize(zcuht *ht) {
	size_t new_cap = ht->cap * 2;
	zcuht_entry *new_hte = calloc(new_cap, sizeof(zcuht_entry));
	for (size_t i = 0; i < ht->cap; i++) {
		if (ht->entries[i].key == NULL) continue;
		zcuht_set_entry(new_hte, ht->cap, ht->entries[i].key, ht->entries[i].value);
		free(ht->entries[i].key);
	}
	ht->cap = new_cap;
	free(ht->entries);
	ht->entries = new_hte;
}

void zcuht_set(zcuht *ht, char *key, void *value) {
	if (ht->cap/2 < (ht->count + 1)) zcuht_resize(ht);
	zcuht_set_entry(ht->entries, ht->cap, key, value);
	ht->count++;
}

bool zcuht_get(zcuht *ht, char *key, void **value) {
	uint64_t hash = zcu_fnv1a(key);
	size_t index = hash & (ht->cap - 1);
	while (ht->entries[index].key != NULL) {
		if (strcmp(ht->entries[index].key, key) == 0) {
			*value = ht->entries[index].value;		
			return true;
		}
	}
	return false;
}

void zcuht_delete(zcuht *ht, char *key) {
	uint64_t hash = zcu_fnv1a(key);
	size_t index = hash & (ht->cap - 1);
	while (ht->entries[index].key != NULL) {
		if (strcmp(ht->entries[index].key, key) == 0) {
			free(ht->entries[index].key);
			ht->entries[index].key = NULL;
			return;
		}
	}
}

#endif
#endif
