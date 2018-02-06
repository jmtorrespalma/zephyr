/*
 * Copyright (c) 2018 Juan Manuel Torres Palma
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __SEMAPHORE_H__
#define __SEMAPHORE_H_

#ifdef CONFIG_NEWLIB_LIBC
#include <time.h>
#else
/* TODO: Move to common header posix_time.h with pthread.h */
struct timespec {
	s32_t tv_sec;
	s32_t tv_nsec;
};
#endif

#define SEM_FAILED     (NULL)
#define SEM_VALUE_MAX  UINT_MAX /* Maximum value allowed for a semaphore */
#define SEM_NSEMS_MAX  UINT_MAX /* Maximum number of semaphores per process */

/* TODO: Move to common header posix_common.h */
#define PATH_MAX       (64)
#define NAME_MAX       (64)
#define O_CREAT        (1 << 0)
#define O_EXCL         (1 << 1)

typedef struct k_sem sem_t;

/**
 * @brief POSIX semaphores API
 *
 * See IEEE 1003.1
 */
int sem_init(sem_t *sem, int pshared, unsigned value);

/**
 * @brief POSIX semaphores API
 *
 * See IEEE 1003.1
 */
int sem_destroy(sem_t *sem);

/**
 * @brief POSIX semaphores API
 *
 * See IEEE 1003.1
 */
int sem_getvalue(sem_t *restrict sem, int *restrict sval);

/**
 * @brief POSIX semaphores API
 *
 * See IEEE 1003.1
 */
int sem_post(sem_t *sem);

/**
 * @brief POSIX semaphores API
 *
 * See IEEE 1003.1
 */
int sem_wait(sem_t *sem);

/**
 * @brief POSIX semaphores API
 *
 * See IEEE 1003.1
 */
int sem_trywait(sem_t *sem);

/**
 * @brief POSIX semaphores API
 *
 * See IEEE 1003.1
 */
int sem_timedwait(sem_t *restrict sem,
		  const struct timespec *restrict abs_timeout);

/**
 * @brief POSIX semaphores API
 *
 * See IEEE 1003.1
 */
sem_t *sem_open(const char *name, int oflag, ...);

/**
 * @brief POSIX semaphores API
 *
 * See IEEE 1003.1
 */
int sem_close(sem_t *sem);

/**
 * @brief POSIX semaphores API
 *
 * See IEEE 1003.1
 */
int sem_unlink(const char *name);

#endif /* __SEMAPHORE_H__ */
