#include "demuxfs.h"
#include "backend.h"
#include "filesrc.h"

/**
 * Load the requested backend and return its backend_ops and its handle.
 * @param backend_name path to loadable backend.
 * @param backend_handle pointer to the loadable library.
 * @return a pointer to the backend's backend_ops on success or NULL on error.
 */
struct backend_ops *backend_load(const char *backend_name, void **backend_handle)
{
	int i;
	char backend[PATH_MAX];
	struct stat statbuf;
	struct backend_ops *ops;
	struct backend_ops *(*backend_get_ops)(void);

	if (! backend_name) {
		fprintf(stderr, "Error: empty backend name received\n");
		return NULL;
	} else if (! backend_handle) {
		fprintf(stderr, "Error: invalid backend_handle pointer received\n");
		return NULL;
	}

	if (backend_name[0] == '/' || stat(backend_name, &statbuf) == 0)
		snprintf(backend, sizeof(backend), "%s", backend_name);
	else
		snprintf(backend, sizeof(backend), "%s/demuxfs/backends/lib%s.so", LIBDIR, backend_name);
	
	*backend_handle = dlopen(backend, RTLD_LAZY);
	if (! *backend_handle) {
		fprintf(stderr, "Failed to load backend: %s\n", dlerror());
		return NULL;
	}

	/* Clean existing errors, if any */
	dlerror();

	/* Load 'backend_get_ops' symbol */
	backend_get_ops = dlsym(*backend_handle, "backend_get_ops");
	if (! backend_get_ops) {
		fprintf(stderr, "Failed to load symbol 'backend_get_ops': %s\n", dlerror());
		goto out_error;
	}
	
	ops = backend_get_ops();
	if (! ops) {
		fprintf(stderr, "Broken backend returns NULL on backend_get_ops()\n");
		goto out_error;
	}

	/* Verify that backend implements all required operations */
	for (i=0; i<sizeof(struct backend_ops)/sizeof(void *); ++i) {
		if (((void *) &(ops)[i]) == NULL) {
			fprintf(stderr, "Backend doesn't implement all methods of struct backend_ops");
			goto out_error;
		}
	}

	return ops;

out_error:
	dlclose(*backend_handle);
	*backend_handle = NULL;
	return NULL;
}

/**
 * Unloads a backend.
 * @param backend_handle handle to the library, as returned from backend_load().
 */
void backend_unload(void *backend_handle)
{
	if (backend_handle)
		dlclose(backend_handle);
}

/**
 * For each installed backend print its usage message.
 */
void backend_print_usage(void)
{
	int ret;
	DIR *dir;
	struct stat statbuf;
	struct dirent *entry;
	void *backend_handle = NULL;
	struct backend_ops *ops = NULL;
	struct backend_ops *(*backend_get_ops)(void);
	char backends_dir[PATH_MAX], libname[PATH_MAX];

	snprintf(backends_dir, sizeof(backends_dir), "%s/demuxfs/backends", LIBDIR);
	dir = opendir(backends_dir);
	if (! dir) {
		fprintf(stderr, "%s: %s\n", backends_dir, strerror(errno));
		return;
	}

	while ((entry = readdir(dir))) {
		snprintf(libname, sizeof(libname), "%s/%s", backends_dir, entry->d_name);
		ret = lstat(libname, &statbuf);
		if (ret < 0 || !S_ISREG(statbuf.st_mode))
			continue;
		
		backend_handle = dlopen(libname, RTLD_LAZY);
		if (! backend_handle)
			continue;

		/* Clean existing errors, if any */
		dlerror();

		/* Load 'backend_get_ops' symbol */
		backend_get_ops = dlsym(backend_handle, "backend_get_ops");
		if (! backend_get_ops) {
			dlclose(backend_handle);
			continue;
		}

		ops = backend_get_ops();
		if (! ops) {
			fprintf(stderr, "Broken backend returns NULL on backend_get_ops()\n");
			dlclose(backend_handle);
			continue;
		}

		/* Invoke the backend's usage() function */
		if (ops->usage)
			ops->usage();

		dlclose(backend_handle);
	}

	closedir(dir);
}
