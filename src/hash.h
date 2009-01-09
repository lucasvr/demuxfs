#ifndef __hash_h
#define __hash_h

struct hash_item {
	ino_t key;
	void *data;
};

struct hash_table {
	int size;
	pthread_mutex_t mutex;
	struct hash_item **items;
};

typedef void (*hashtable_free_function_t)(void *data);

struct hash_table *hashtable_new(int size);
void hashtable_destroy(struct hash_table *table, hashtable_free_function_t free_function);
void hashtable_invalidate_contents(struct hash_table *table);
void *hashtable_get(struct hash_table *table, ino_t key);
bool hashtable_add(struct hash_table *table, ino_t key, void *data);
bool hashtable_del(struct hash_table *table, ino_t key);
void hashtable_lock(struct hash_table *hash);
void hashtable_unlock(struct hash_table *hash);

#endif /* __hash_h */
