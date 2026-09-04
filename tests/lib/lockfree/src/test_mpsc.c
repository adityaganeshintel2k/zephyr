/*
 * Copyright (c) 2023 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/irq.h>
#include <zephyr/ztest.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util_loops.h>
#include <zephyr/timing/timing.h>
#include <zephyr/sys/dlist.h>
#include <zephyr/sys/spsc_lockfree.h>
#include <zephyr/sys/mpsc_lockfree.h>
#include <zephyr/sys/mpsc_lockfree_priority.h>

static struct mpsc push_pop_q;
static struct mpsc_node push_pop_nodes[2];

/*
 * @brief Push and pop one element
 *
 * @see mpsc_push(), mpsc_pop()
 *
 * @ingroup tests
 */
ZTEST(mpsc, test_push_pop)
{

	mpsc_ptr_t node, head;
	struct mpsc_node *stub, *next, *tail;

	mpsc_init(&push_pop_q);

	head = mpsc_ptr_get(push_pop_q.head);
	tail = push_pop_q.tail;
	stub = &push_pop_q.stub;
	next = stub->next;

	zassert_equal(head, stub, "Head should point at stub");
	zassert_equal(tail, stub, "Tail should point at stub");
	zassert_is_null(next, "Next should be null");

	node = mpsc_pop(&push_pop_q);
	zassert_is_null(node, "Pop on empty queue should return null");

	mpsc_push(&push_pop_q, &push_pop_nodes[0]);

	head = mpsc_ptr_get(push_pop_q.head);

	zassert_equal(head, &push_pop_nodes[0], "Queue head should point at push_pop_node");
	next = mpsc_ptr_get(push_pop_nodes[0].next);
	zassert_is_null(next, "push_pop_node next should point at null");
	next = mpsc_ptr_get(push_pop_q.stub.next);
	zassert_equal(next, &push_pop_nodes[0], "Queue stub should point at push_pop_node");
	tail = push_pop_q.tail;
	stub = &push_pop_q.stub;
	zassert_equal(tail, stub, "Tail should point at stub");

	node = mpsc_pop(&push_pop_q);
	stub = &push_pop_q.stub;

	zassert_not_equal(node, stub, "Pop should not return stub");
	zassert_not_null(node, "Pop should not return null");
	zassert_equal(node, &push_pop_nodes[0],
		      "Pop should return push_pop_node %p, instead was %p",
		      &push_pop_nodes[0], node);

	node = mpsc_pop(&push_pop_q);
	zassert_is_null(node, "Pop on empty queue should return null");
}

#define MPSC_PRIORITY_LEVELS 3

struct test_mpsc_priority_node {
	struct mpsc_node node;
	uint32_t value;
};

ZTEST(mpsc, test_priority_ordering)
{
	struct mpsc queues[MPSC_PRIORITY_LEVELS];
	struct mpsc_priority priority_q;
	struct test_mpsc_priority_node nodes[] = {
		{ .value = 0U },
		{ .value = 1U },
		{ .value = 2U },
		{ .value = 3U },
	};
	const uint32_t expected[] = { 1U, 2U, 0U, 3U };

	mpsc_priority_init(&priority_q, queues, ARRAY_SIZE(queues));
	zassert_is_null(mpsc_priority_pop(&priority_q));

	mpsc_priority_push(&priority_q, &nodes[0].node, 2U);
	mpsc_priority_push(&priority_q, &nodes[1].node, 0U);
	mpsc_priority_push(&priority_q, &nodes[2].node, 1U);
	mpsc_priority_push(&priority_q, &nodes[3].node, 2U);

	for (size_t i = 0U; i < ARRAY_SIZE(expected); i++) {
		struct mpsc_node *node = mpsc_priority_pop(&priority_q);
		struct test_mpsc_priority_node *priority_node;

		zassert_not_null(node);
		priority_node = CONTAINER_OF(node, struct test_mpsc_priority_node, node);
		zassert_equal(priority_node->value, expected[i]);
	}

	zassert_is_null(mpsc_priority_pop(&priority_q));
}

#define MPSC_FREEQ_SZ 8
#define MPSC_ITERATIONS 100000
#define MPSC_STACK_SIZE (512 + CONFIG_TEST_EXTRA_STACK_SIZE)
#define MPSC_THREADS_NUM 4

struct thread_info {
	k_tid_t tid;
	int executed;
	int priority;
	int cpu_id;
};

static struct thread_info mpsc_tinfo[MPSC_THREADS_NUM];
static struct k_thread mpsc_thread[MPSC_THREADS_NUM];
static K_THREAD_STACK_ARRAY_DEFINE(mpsc_stack, MPSC_THREADS_NUM, MPSC_STACK_SIZE);

struct test_mpsc_node {
	uint32_t id;
	struct mpsc_node n;
	struct mpsc_node mpsc_node;
	sys_dnode_t dlist_node;
	uint8_t priority;
	uint8_t owner;
};


struct spsc_node_sq {
	struct spsc _spsc;
	struct test_mpsc_node *const buffer;
};

#define TEST_SPSC_DEFINE(n, sz) SPSC_DEFINE(_spsc_##n, struct test_mpsc_node, sz)
#define SPSC_NAME(n, _) (struct spsc_node_sq *)&_spsc_##n

LISTIFY(MPSC_THREADS_NUM, TEST_SPSC_DEFINE, (;), MPSC_FREEQ_SZ)

struct spsc_node_sq *node_q[MPSC_THREADS_NUM] = {
	LISTIFY(MPSC_THREADS_NUM, SPSC_NAME, (,))
};

static struct mpsc mpsc_q;

static void mpsc_consumer(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	struct mpsc_node *n;
	struct test_mpsc_node *nn;

	for (int i = 0; i < (MPSC_ITERATIONS)*(MPSC_THREADS_NUM - 1); i++) {
		do {
			n = mpsc_pop(&mpsc_q);
			if (n == NULL) {
				k_yield();
			}
		} while (n == NULL);

		zassert_not_equal(n, &mpsc_q.stub, "mpsc should not produce stub");

		nn = CONTAINER_OF(n, struct test_mpsc_node, n);

		/* Return node to producer's free queue - must retry if queue is full */
		while (spsc_acquire(node_q[nn->id]) == NULL) {
			k_yield();
		}
		spsc_produce(node_q[nn->id]);
	}
}

static void mpsc_producer(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	struct test_mpsc_node *n;
	uint32_t id = (uint32_t)(uintptr_t)p1;

	for (int i = 0; i < MPSC_ITERATIONS; i++) {
		do {
			n = spsc_consume(node_q[id]);
			if (n == NULL) {
				k_yield();
			}
		} while (n == NULL);

		spsc_release(node_q[id]);
		n->id = id;
		mpsc_push(&mpsc_q, &n->n);
	}
}

/**
 * @brief Test that the producer and consumer are indeed thread safe
 *
 * This can and should be validated on SMP machines where incoherent
 * memory could cause issues.
 */
ZTEST(mpsc, test_mpsc_threaded)
{
	mpsc_init(&mpsc_q);

	TC_PRINT("setting up mpsc producer free queues\n");
	/* Setup node free queues */
	for (int i = 0; i < MPSC_THREADS_NUM; i++) {
		for (int j = 0; j < MPSC_FREEQ_SZ; j++) {
			spsc_acquire(node_q[i]);
		}
		spsc_produce_all(node_q[i]);
	}

	TC_PRINT("starting consumer\n");
	mpsc_tinfo[0].tid =
		k_thread_create(&mpsc_thread[0], mpsc_stack[0], MPSC_STACK_SIZE,
				mpsc_consumer,
				NULL, NULL, NULL,
				K_PRIO_PREEMPT(5),
				K_INHERIT_PERMS, K_NO_WAIT);

	for (int i = 1; i < MPSC_THREADS_NUM; i++) {
		TC_PRINT("starting producer %i\n", i);
		mpsc_tinfo[i].tid =
			k_thread_create(&mpsc_thread[i], mpsc_stack[i], MPSC_STACK_SIZE,
					mpsc_producer,
					(void *)(uintptr_t)i, NULL, NULL,
					K_PRIO_PREEMPT(5),
					K_INHERIT_PERMS, K_NO_WAIT);
	}

	for (int i = 0; i < MPSC_THREADS_NUM; i++) {
		TC_PRINT("joining mpsc thread %d\n", i);
		k_thread_join(mpsc_tinfo[i].tid, K_FOREVER);
	}
}

#define THROUGHPUT_ITERS 100000

#ifndef MPSC_PRIORITY_BENCHMARK
#define MPSC_PRIORITY_BENCHMARK 1
#endif

#ifndef MPSC_BENCHMARK_ITERATIONS
#define MPSC_BENCHMARK_ITERATIONS 100000
#endif

#ifndef MPSC_BENCHMARK_PRODUCERS
#define MPSC_BENCHMARK_PRODUCERS 3
#endif

#define MPSC_BENCHMARK_PRIORITIES 3
#define MPSC_BENCHMARK_POOL_SIZE 8
#define MPSC_BENCHMARK_STACK_SIZE (1024 + CONFIG_TEST_EXTRA_STACK_SIZE)

#if MPSC_PRIORITY_BENCHMARK

enum mpsc_benchmark_mode {
	MPSC_BENCHMARK_PRIORITY,
	MPSC_BENCHMARK_LOCKED,
};

struct mpsc_benchmark_node {
	struct mpsc_node mpsc_node;
	sys_dnode_t dlist_node;
	uint8_t priority;
	uint8_t owner;
};

struct mpsc_benchmark_producer_arg {
	uint32_t id;
	enum mpsc_benchmark_mode mode;
};

static struct mpsc_priority benchmark_priority_q;
static struct mpsc benchmark_priority_queues[MPSC_BENCHMARK_PRIORITIES];
static struct {
	sys_dlist_t queues[MPSC_BENCHMARK_PRIORITIES];
	struct k_spinlock lock;
} benchmark_locked_q;

static K_SEM_DEFINE(benchmark_start, 0, MPSC_BENCHMARK_PRODUCERS + 1);
static struct k_thread benchmark_threads[MPSC_BENCHMARK_PRODUCERS + 1];
static K_THREAD_STACK_ARRAY_DEFINE(benchmark_stacks, MPSC_BENCHMARK_PRODUCERS + 1,
					   MPSC_BENCHMARK_STACK_SIZE);
static struct mpsc_benchmark_producer_arg benchmark_args[MPSC_BENCHMARK_PRODUCERS];
static uint64_t benchmark_push_cycles[MPSC_BENCHMARK_PRODUCERS];
static uint64_t benchmark_pop_cycles;

#define BENCHMARK_SPSC_DEFINE(id, _) \
	SPSC_DEFINE(benchmark_free_q_##id, struct test_mpsc_node, MPSC_BENCHMARK_POOL_SIZE)
#define BENCHMARK_SPSC_NAME(id, _) \
	(struct spsc_node_sq *)&benchmark_free_q_##id

LISTIFY(MPSC_BENCHMARK_PRODUCERS, BENCHMARK_SPSC_DEFINE, (;), MPSC_BENCHMARK_PRODUCERS)

static struct spsc_node_sq *benchmark_free_q[MPSC_BENCHMARK_PRODUCERS] = {
	LISTIFY(MPSC_BENCHMARK_PRODUCERS, BENCHMARK_SPSC_NAME, (,))
};

static void mpsc_benchmark_queue_init(enum mpsc_benchmark_mode mode)
{
	if (mode == MPSC_BENCHMARK_PRIORITY) {
		mpsc_priority_init(&benchmark_priority_q, benchmark_priority_queues,
				   MPSC_BENCHMARK_PRIORITIES);
		return;
	}

	for (uint8_t priority = 0U; priority < MPSC_BENCHMARK_PRIORITIES; priority++) {
		sys_dlist_init(&benchmark_locked_q.queues[priority]);
	}
}

static void mpsc_benchmark_push(enum mpsc_benchmark_mode mode,
				struct test_mpsc_node *node)
{
	if (mode == MPSC_BENCHMARK_PRIORITY) {
		mpsc_priority_push(&benchmark_priority_q, &node->mpsc_node, node->priority);
		return;
	}

	k_spinlock_key_t key = k_spin_lock(&benchmark_locked_q.lock);
	sys_dlist_append(&benchmark_locked_q.queues[node->priority], &node->dlist_node);
	k_spin_unlock(&benchmark_locked_q.lock, key);
}

static struct test_mpsc_node *mpsc_benchmark_pop(enum mpsc_benchmark_mode mode)
{
	if (mode == MPSC_BENCHMARK_PRIORITY) {
		struct mpsc_node *node = mpsc_priority_pop(&benchmark_priority_q);

		return node == NULL ? NULL : CONTAINER_OF(node, struct test_mpsc_node,
								mpsc_node);
	}

	k_spinlock_key_t key = k_spin_lock(&benchmark_locked_q.lock);
	struct test_mpsc_node *node = NULL;

	for (uint8_t priority = 0U; priority < MPSC_BENCHMARK_PRIORITIES; priority++) {
		sys_dnode_t *dnode = sys_dlist_peek_head(&benchmark_locked_q.queues[priority]);

		if (dnode != NULL) {
			sys_dlist_remove(dnode);
			node = CONTAINER_OF(dnode, struct test_mpsc_node, dlist_node);
			break;
		}
	}

	k_spin_unlock(&benchmark_locked_q.lock, key);
	return node;
}

static void mpsc_benchmark_producer(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	struct mpsc_benchmark_producer_arg *arg = p1;

	k_sem_take(&benchmark_start, K_FOREVER);

	for (uint32_t i = 0U; i < MPSC_BENCHMARK_ITERATIONS; i++) {
		struct test_mpsc_node *node;

		do {
			node = spsc_consume(benchmark_free_q[arg->id]);
			if (node == NULL) {
				k_yield();
			}
		} while (node == NULL);

		spsc_release(benchmark_free_q[arg->id]);
		node->priority = i % MPSC_BENCHMARK_PRIORITIES;

		timing_t start = timing_counter_get();
		mpsc_benchmark_push(arg->mode, node);
		timing_t finish = timing_counter_get();
		benchmark_push_cycles[arg->id] += timing_cycles_get(&start, &finish);
	}
}

static void mpsc_benchmark_consumer(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	enum mpsc_benchmark_mode mode = (enum mpsc_benchmark_mode)(uintptr_t)p1;

	k_sem_take(&benchmark_start, K_FOREVER);

	for (uint32_t i = 0U; i < MPSC_BENCHMARK_ITERATIONS * MPSC_BENCHMARK_PRODUCERS;
	     i++) {
		struct test_mpsc_node *node;

		do {
			timing_t start = timing_counter_get();
			node = mpsc_benchmark_pop(mode);
			timing_t finish = timing_counter_get();
			benchmark_pop_cycles += timing_cycles_get(&start, &finish);
			if (node == NULL) {
				k_yield();
			}
		} while (node == NULL);

		uint32_t owner = node->owner;

		spsc_acquire(benchmark_free_q[owner]);
		spsc_produce(benchmark_free_q[owner]);
	}
}

static void mpsc_benchmark_run(enum mpsc_benchmark_mode mode)
{
	mpsc_benchmark_queue_init(mode);
	benchmark_pop_cycles = 0U;

	for (uint32_t i = 0U; i < MPSC_BENCHMARK_PRODUCERS; i++) {
		benchmark_push_cycles[i] = 0U;
		spsc_reset(benchmark_free_q[i]);
		for (uint32_t j = 0U; j < MPSC_BENCHMARK_POOL_SIZE; j++) {
			struct test_mpsc_node *node = spsc_acquire(benchmark_free_q[i]);

			__ASSERT_NO_MSG(node != NULL);
			node->owner = i;
		}
		spsc_produce_all(benchmark_free_q[i]);
		benchmark_args[i] = (struct mpsc_benchmark_producer_arg) {
			.id = i,
			.mode = mode,
		};
	}

	k_thread_create(&benchmark_threads[0], benchmark_stacks[0], MPSC_BENCHMARK_STACK_SIZE,
			mpsc_benchmark_consumer, (void *)(uintptr_t)mode, NULL, NULL,
			K_PRIO_PREEMPT(5), K_INHERIT_PERMS, K_NO_WAIT);

	for (uint32_t i = 0U; i < MPSC_BENCHMARK_PRODUCERS; i++) {
		k_thread_create(&benchmark_threads[i + 1], benchmark_stacks[i + 1],
				MPSC_BENCHMARK_STACK_SIZE, mpsc_benchmark_producer,
				&benchmark_args[i], NULL, NULL, K_PRIO_PREEMPT(5),
				K_INHERIT_PERMS, K_NO_WAIT);
	}

	timing_start();
	for (uint32_t i = 0U; i < MPSC_BENCHMARK_PRODUCERS + 1; i++) {
		k_sem_give(&benchmark_start);
	}

	for (uint32_t i = 0U; i < MPSC_BENCHMARK_PRODUCERS + 1; i++) {
		k_thread_join(&benchmark_threads[i], K_FOREVER);
	}

	uint64_t push_total = 0U;

	for (uint32_t i = 0U; i < MPSC_BENCHMARK_PRODUCERS; i++) {
		push_total += benchmark_push_cycles[i];
	}

	uint64_t push_ops = (uint64_t)MPSC_BENCHMARK_ITERATIONS * MPSC_BENCHMARK_PRODUCERS;
	uint64_t pop_ops = push_ops;

	TC_PRINT("priority_mpsc_%s: producers=%u iterations=%u push_avg=%llu cycles (%u ns) "
		 "pop_avg=%llu cycles (%u ns)\n",
		 mode == MPSC_BENCHMARK_PRIORITY ? "lockfree" : "locked",
		 MPSC_BENCHMARK_PRODUCERS, MPSC_BENCHMARK_ITERATIONS,
		 push_total / push_ops, (uint32_t)timing_cycles_to_ns(push_total / push_ops),
		 benchmark_pop_cycles / pop_ops,
		 (uint32_t)timing_cycles_to_ns(benchmark_pop_cycles / pop_ops));
}

ZTEST(mpsc, test_priority_benchmark)
{
	TC_PRINT("benchmark config: producers=%u iterations=%u\n",
		 MPSC_BENCHMARK_PRODUCERS, MPSC_BENCHMARK_ITERATIONS);
	mpsc_benchmark_run(MPSC_BENCHMARK_PRIORITY);
	mpsc_benchmark_run(MPSC_BENCHMARK_LOCKED);
}

#endif /* MPSC_PRIORITY_BENCHMARK */

ZTEST(mpsc, test_mpsc_throughput)
{
	struct mpsc_node node;
	timing_t start_time, end_time;

	mpsc_init(&mpsc_q);
	timing_init();
	timing_start();

	start_time = timing_counter_get();

	int key = irq_lock();

	for (int i = 0; i < THROUGHPUT_ITERS; i++) {
		mpsc_push(&mpsc_q, &node);

		mpsc_pop(&mpsc_q);
	}

	irq_unlock(key);

	end_time = timing_counter_get();

	uint64_t cycles = timing_cycles_get(&start_time, &end_time);
	uint64_t ns = timing_cycles_to_ns(cycles);

	TC_PRINT("%llu ns for %d iterations, %llu ns per op\n", ns,
		 THROUGHPUT_ITERS, ns/THROUGHPUT_ITERS);
}

ZTEST_SUITE(mpsc, NULL, NULL, NULL, NULL, NULL);
