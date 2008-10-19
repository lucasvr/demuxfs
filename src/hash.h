#ifndef __hash_h
#define __hash_h

struct hash_item {
	ino_t key;
	void *data;
};

struct hash_table {
	int size;
	struct hash_item **items;
};

struct hash_table *hashtable_new(int size);
void hashtable_destroy(struct hash_table *table);
void *hashtable_get(struct hash_table *table, ino_t key);
bool hashtable_add(struct hash_table *table, ino_t key, void *data);
bool hashtable_del(struct hash_table *table, ino_t key);

#endif /* __hash_h */
