/*
 * Copyright (c) 2018 Juan Manuel Torres Palma
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel.h>
#include <semaphore.h>
#include <string.h>

#define mode_t int

#define SEM_ERRNO_SET(_code)      \
	do {                      \
		errno = (_code);  \
		goto error;       \
	} while (0)

/*
 * Private named semaphore
 */
struct _nsem {
	sem_t sem;
	const char *name;
	u32_t refs;   /* Counts how many threads have opened this sem */
	bool to_free; /* Flag set by unlink */
	bool in_use;  /* Used to check if this spot is available */
};

#define SEM_NAMED_N_LIMIT  (32)
static int sem_named_n;
static struct _nsem sem_named[SEM_NAMED_N_LIMIT];

/*
 * NULL if not found, pointer to the object if found.
 */
static inline struct _nsem *find_by_name_nsem(const char *name)
{
	struct _nsem *it, *lmt, *semp = NULL;

	it = sem_named;
	lmt = sem_named + SEM_NAMED_N_LIMIT;

	while (it != lmt && !semp) {
		if (!strncmp(name, it->name, PATH_MAX) && it->in_use)
			semp = it;
		++it;
	}

	return semp;
}

static inline struct _nsem *find_by_addr_nsem(sem_t *sem)
{
	struct _nsem *it, *lmt, *semp = NULL;

	it = sem_named;
	lmt = sem_named + SEM_NAMED_N_LIMIT;

	while (it != lmt && !semp) {
		if (&it->sem == sem && it->in_use)
			semp = it;
		++it;
	}

	return semp;
}

/*
 * FIXME: remove only current thread.
 * Returns number of references left
 */
static inline int dereference_nsem(struct _nsem *nsem)
{
	return --nsem->refs;
}

/*
 * Only called if we are sure it does not exist
 */
static inline struct _nsem *alloc_nsem(const char *name)
{
	struct _nsem *it, *lmt, *semp = NULL;

	it = sem_named;
	lmt = sem_named + SEM_NAMED_N_LIMIT;

	while (it != lmt && !semp) {
		if (!it->in_use) {
			semp = it;
			semp->in_use = true;
			semp->to_free = false;
			semp->name = name;
			semp->refs = 1;
		}
		++it;
	}

	return semp;
}

static inline void free_nsem(struct _nsem *nsem)
{
	nsem->in_use = false;
	sem_destroy(&nsem->sem); /* This should never fail */
}

int sem_init(sem_t *sem, int pshared, unsigned value)
{
	ARG_UNUSED(pshared); /* In Zephyr, process is not defined */
	k_sem_init(sem, value, SEM_VALUE_MAX);
	return 0;
}

int sem_destroy(sem_t *sem)
{
	if (k_sem_count_get(sem) != 0)
		SEM_ERRNO_SET(EBUSY);
	return 0;

error:
	return -1;
}

int sem_getvalue(sem_t *restrict sem, int *restrict sval)
{
	*sval = (int)k_sem_count_get(sem);
	return 0;
}

int sem_post(sem_t *sem)
{
	/* TODO: Care about process scheduling when unlocking */
	k_sem_give(sem);
	return 0;
}

int sem_wait(sem_t *sem)
{
	k_sem_take(sem, K_FOREVER);
	return 0;
}

int sem_trywait(sem_t *sem)
{
	if (k_sem_take(sem, K_NO_WAIT) == -EBUSY)
		SEM_ERRNO_SET(EAGAIN);
	return 0;

error:
	return -1;
}

int sem_timedwait(sem_t *restrict sem,
		  const struct timespec *restrict abs_timeout)
{
	s64_t wakeup; /* ms */
	s64_t now;

	if (abs_timeout->tv_nsec < 0 || abs_timeout->tv_nsec > 1000000000)
		SEM_ERRNO_SET(EINVAL);

	 /* TODO: division is not a good idea */
	wakeup = abs_timeout->tv_sec * 1000;
	wakeup += abs_timeout->tv_nsec / 1000000;

	/* ms ellapsed since power up or last overflow */
	now = __ticks_to_ms(k_cycle_get_32());

	if (k_sem_take(sem, (s32_t)(wakeup - now)) == -EBUSY)
		SEM_ERRNO_SET(ETIMEDOUT);
	return 0;

error:
	return -1;
}

/*
 * Flags control if the semaphore is created or simply retrieved
 * O_CREAT: Creates a new semaphore with mode_t permissions and count.
 * O_EXCL:  Passed along with O_CREAT, fails if a semaphore with name already
 *          exists.
 *
 * This function is atomic since a few variables are shared.
 * FIXME: Multiple calls to open by same thread will cause trouble.
 */
sem_t *sem_open(const char *name, int oflag, ...)
{
	struct _nsem *nsem;
	unsigned value;
	unsigned int key;
	va_list al;

	key = irq_lock();

	if (sem_named_n == SEM_NAMED_N_LIMIT)
		SEM_ERRNO_SET(ENFILE);

	nsem = find_by_name_nsem(name);

	if (nsem) {
		if (oflag & O_EXCL)
			SEM_ERRNO_SET(EEXIST);
	} else {
		if (!(oflag & O_CREAT))
			SEM_ERRNO_SET(ENOENT);

		va_start(al, oflag);
		va_arg(al, mode_t); /* permissions not used */
		value = va_arg(al, unsigned);
		va_end(al);

		nsem = alloc_nsem(name);
		sem_init(&nsem->sem, 0, value);
		sem_named_n++;
	}

	irq_unlock(key);
	return &nsem->sem;

error:
	irq_unlock(key);
	return SEM_FAILED;
}

/*
 * Close can destroy a semaphore too if sem_unlink has been called on that sem
 * before and it's the last reference to it.
 */
int sem_close(sem_t *sem)
{
	struct _nsem *nsem;
	unsigned int key;

	key = irq_lock();
	nsem = find_by_addr_nsem(sem);

	if (!nsem)
		SEM_ERRNO_SET(EINVAL);

	if (!dereference_nsem(nsem))
		if (nsem->to_free)
			free_nsem(nsem);

	irq_unlock(key);
	return 0;

error:
	irq_unlock(key);
	return -1;
}

/*
 * Free a semaphore if no references left. */
int sem_unlink(const char *name)
{
	struct _nsem *nsem;
	unsigned int key;

	key = irq_lock();
	nsem = find_by_name_nsem(name);

	if (!nsem)
		SEM_ERRNO_SET(ENOENT);

	nsem->to_free = true;

	if (!dereference_nsem(nsem))
		free_nsem(nsem);

	irq_unlock(key);
	return 0;

error:
	irq_unlock(key);
	return -1;
}
