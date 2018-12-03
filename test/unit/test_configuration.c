#include "../../src/binary.h"
#include "../../src/configuration.h"

#include "../lib/heap.h"
#include "../lib/munit.h"

/**
 * Helpers
 */

struct fixture
{
    struct raft_heap heap;
    struct raft_configuration configuration;
};

/**
 * Add a server to the fixture's configuration and check that no error occurs.
 */
#define __add(F, ID, ADDRESS, VOTING)                                        \
    {                                                                        \
        int rv;                                                              \
                                                                             \
        rv = raft_configuration_add(&F->configuration, ID, ADDRESS, VOTING); \
        munit_assert_int(rv, ==, 0);                                         \
    }

/**
 * Remove a server from the fixture's configuration and check that no error
 * occurs.
 */
#define __remove(F, ID)                                        \
    {                                                          \
        int rv;                                                \
                                                               \
        rv = raft_configuration_remove(&F->configuration, ID); \
        munit_assert_int(rv, ==, 0);                           \
    }

/**
 * Assert that the fixture's configuration has n servers.
 */
#define __assert_n_servers(F, N)                                 \
    {                                                            \
        munit_assert_int(F->configuration.n, ==, N);             \
                                                                 \
        if (N == 0) {                                            \
            munit_assert_ptr_null(f->configuration.servers);     \
        } else {                                                 \
            munit_assert_ptr_not_null(f->configuration.servers); \
        }                                                        \
    }

/**
 * Assert that the I'th server in the fixture's configuration equals the given
 * value.
 */
#define __assert_server_equal(F, I, ID, ADDRESS, VOTING)     \
    {                                                        \
        struct raft_server *server;                          \
                                                             \
        munit_assert_int(I, <, F->configuration.n);          \
                                                             \
        server = &f->configuration.servers[I];               \
                                                             \
        munit_assert_int(server->id, ==, ID);                \
        munit_assert_string_equal(server->address, ADDRESS); \
        munit_assert_int(server->voting, ==, VOTING);        \
    }

/**
 * Setup and tear down
 */

static void *setup(const MunitParameter params[], void *user_data)
{
    struct fixture *f = munit_malloc(sizeof *f);

    (void)user_data;
    (void)params;

    test_heap_setup(params, &f->heap);

    raft_configuration_init(&f->configuration);

    return f;
}

static void tear_down(void *data)
{
    struct fixture *f = data;

    raft_configuration_close(&f->configuration);

    test_heap_tear_down(&f->heap);

    free(f);
}

/**
 * raft_configuration_add
 */

/* Add a server to the configuration. */
static MunitResult test_add_one(const MunitParameter params[], void *data)
{
    struct fixture *f = data;

    (void)params;

    __add(f, 1, "127.0.0.1:666", true);

    __assert_n_servers(f, 1);
    __assert_server_equal(f, 0, 1, "127.0.0.1:666", true);

    return MUNIT_OK;
}

/* Add two servers to the configuration. */
static MunitResult test_add_two(const MunitParameter params[], void *data)
{
    struct fixture *f = data;

    (void)params;

    __add(f, 1, "127.0.0.1:666", true);
    __add(f, 2, "192.168.1.1:666", false);

    __assert_n_servers(f, 2);
    __assert_server_equal(f, 0, 1, "127.0.0.1:666", true);
    __assert_server_equal(f, 1, 2, "192.168.1.1:666", false);

    return MUNIT_OK;
}

/* Add a server with invalid ID. */
static MunitResult test_add_invalid_id(const MunitParameter params[],
                                       void *data)
{
    struct fixture *f = data;
    int rv;

    (void)params;

    rv = raft_configuration_add(&f->configuration, 0, "127.0.0.1:666", true);
    munit_assert_int(rv, ==, RAFT_ERR_BAD_SERVER_ID);

    munit_assert_string_equal(raft_strerror(rv), "server ID is not valid");

    return MUNIT_OK;
}

/* Add a server with no address. */
static MunitResult test_add_no_address(const MunitParameter params[],
                                       void *data)
{
    struct fixture *f = data;
    int rv;

    (void)params;

    rv = raft_configuration_add(&f->configuration, 1, NULL, true);
    munit_assert_int(rv, ==, RAFT_ERR_NO_SERVER_ADDRESS);

    munit_assert_string_equal(raft_strerror(rv), "server has no address");

    return MUNIT_OK;
}

/* Add a server with an ID which is already in use. */
static MunitResult test_add_dup_id(const MunitParameter params[], void *data)
{
    struct fixture *f = data;
    int rv;

    (void)params;

    __add(f, 1, "127.0.0.1:666", true);

    rv = raft_configuration_add(&f->configuration, 1, "192.168.1.1:666", false);
    munit_assert_int(rv, ==, RAFT_ERR_DUP_SERVER_ID);

    munit_assert_string_equal(raft_strerror(rv), "server ID already in use");

    return MUNIT_OK;
}

/* Add a server with an address which is already in use. */
static MunitResult test_add_dup_address(const MunitParameter params[],
                                        void *data)
{
    struct fixture *f = data;
    int rv;

    (void)params;

    __add(f, 1, "127.0.0.1:666", true);

    rv = raft_configuration_add(&f->configuration, 2, "127.0.0.1:666", false);
    munit_assert_int(rv, ==, RAFT_ERR_DUP_SERVER_ADDRESS);

    munit_assert_string_equal(raft_strerror(rv),
                              "server address already in use");

    return MUNIT_OK;
}

static char *add_oom_heap_fault_delay[] = {"0", "1", NULL};
static char *add_oom_heap_fault_repeat[] = {"1", NULL};

static MunitParameterEnum add_oom_params[] = {
    {TEST_HEAP_FAULT_DELAY, add_oom_heap_fault_delay},
    {TEST_HEAP_FAULT_REPEAT, add_oom_heap_fault_repeat},
    {NULL, NULL},
};

/* Out of memory. */
static MunitResult test_add_oom(const MunitParameter params[], void *data)
{
    struct fixture *f = data;
    int rv;

    (void)params;

    test_heap_fault_enable(&f->heap);

    rv = raft_configuration_add(&f->configuration, 1, "127.0.0.1:666", true);
    munit_assert_int(rv, ==, RAFT_ERR_NOMEM);

    munit_assert_string_equal(raft_strerror(rv), "out of memory");

    return MUNIT_OK;
}

static MunitTest add_tests[] = {
    {"/one", test_add_one, setup, tear_down, 0, NULL},
    {"/two", test_add_two, setup, tear_down, 0, NULL},
    {"/invalid-id", test_add_invalid_id, setup, tear_down, 0, NULL},
    {"/no-address", test_add_no_address, setup, tear_down, 0, NULL},
    {"/dup-id", test_add_dup_id, setup, tear_down, 0, NULL},
    {"/dup-address", test_add_dup_address, setup, tear_down, 0, NULL},
    {"/oom", test_add_oom, setup, tear_down, 0, add_oom_params},
    {NULL, NULL, NULL, NULL, 0, NULL},
};

/**
 * raft_configuration_remove
 */

/* Attempts to remove a server with an unknown ID result in an error. */
static MunitResult test_remove_unknown(const MunitParameter params[],
                                       void *data)
{
    struct fixture *f = data;
    int rv;

    (void)params;

    rv = raft_configuration_remove(&f->configuration, 1);
    munit_assert_int(rv, ==, RAFT_ERR_UNKNOWN_SERVER_ID);

    return MUNIT_OK;
}

/* Remove the last and only server. */
static MunitResult test_remove_last(const MunitParameter params[], void *data)
{
    struct fixture *f = data;

    (void)params;

    __add(f, 1, "127.0.0.1:666", true);

    __remove(f, 1);

    __assert_n_servers(f, 0);

    return MUNIT_OK;
}

/* Remove the first server. */
static MunitResult test_remove_first(const MunitParameter params[], void *data)
{
    struct fixture *f = data;

    (void)params;

    __add(f, 1, "127.0.0.1:666", true);
    __add(f, 2, "192.168.1.1:666", false);

    __remove(f, 1);

    __assert_n_servers(f, 1);
    __assert_server_equal(f, 0, 2, "192.168.1.1:666", false);
    ;

    return MUNIT_OK;
}

/* Remove a server in the middle. */
static MunitResult test_remove_middle(const MunitParameter params[], void *data)
{
    struct fixture *f = data;

    (void)params;

    __add(f, 1, "127.0.0.1:666", true);
    __add(f, 2, "192.168.1.1:666", false);
    __add(f, 3, "10.0.1.1:666", true);

    __remove(f, 2);

    __assert_n_servers(f, 2);
    __assert_server_equal(f, 0, 1, "127.0.0.1:666", true);
    ;
    __assert_server_equal(f, 1, 3, "10.0.1.1:666", true);
    ;

    return MUNIT_OK;
}

/* Out of memory. */
static MunitResult test_remove_oom(const MunitParameter params[], void *data)
{
    struct fixture *f = data;
    int rv;

    (void)params;

    __add(f, 1, "127.0.0.1:666", true);
    __add(f, 2, "192.168.1.1:666", false);

    test_heap_fault_config(&f->heap, 0, 1);
    test_heap_fault_enable(&f->heap);

    rv = raft_configuration_remove(&f->configuration, 2);
    munit_assert_int(rv, ==, RAFT_ERR_NOMEM);

    return MUNIT_OK;
}

static MunitTest remove_tests[] = {
    {"/unknown", test_remove_unknown, setup, tear_down, 0, NULL},
    {"/last", test_remove_last, setup, tear_down, 0, NULL},
    {"/first", test_remove_first, setup, tear_down, 0, NULL},
    {"/middle", test_remove_middle, setup, tear_down, 0, NULL},
    {"/oom", test_remove_oom, setup, tear_down, 0, NULL},
    {NULL, NULL, NULL, NULL, 0, NULL},
};

/**
 * raft_configuration__n_voting
 */

/* Return only voting nodes. */
static MunitResult test_n_voting(const MunitParameter params[], void *data)
{
    struct fixture *f = data;
    size_t n;

    (void)params;

    __add(f, 1, "127.0.0.1:666", true);

    n = raft_configuration__n_voting(&f->configuration);
    munit_assert_int(n, ==, 1);

    return MUNIT_OK;
}

static MunitTest n_voting_tests[] = {
    {"/", test_n_voting, setup, tear_down, 0, NULL},
    {NULL, NULL, NULL, NULL, 0, NULL},
};

/**
 * raft_configuration__index
 */

/* If no matching server is found, the length of the configuration is
   returned. */
static MunitResult test_index_no_match(const MunitParameter params[],
                                       void *data)
{
    struct fixture *f = data;
    size_t i;

    (void)params;

    __add(f, 1, "127.0.0.1:666", true);

    i = raft_configuration__index(&f->configuration, 3);
    munit_assert_int(i, ==, f->configuration.n);

    return MUNIT_OK;
}

static MunitTest index_tests[] = {
    {"/no-match", test_index_no_match, setup, tear_down, 0, NULL},
    {NULL, NULL, NULL, NULL, 0, NULL},
};

/**
 * raft_configuration__voting_index
 */

/* The index of the matching voting server (relative to the number of voting
   servers) is returned. */
static MunitResult test_voting_index_match(const MunitParameter params[],
                                           void *data)
{
    struct fixture *f = data;
    size_t i;

    (void)params;

    __add(f, 1, "192.168.1.1:666", false);
    __add(f, 2, "192.168.1.2:666", true);
    __add(f, 3, "192.168.1.3:666", true);

    i = raft_configuration__voting_index(&f->configuration, 3);
    munit_assert_int(i, ==, 1);

    return MUNIT_OK;
}

/* If no matching server is found, the length of the configuration is
 * returned. */
static MunitResult test_voting_index_no_match(const MunitParameter params[],
                                              void *data)
{
    struct fixture *f = data;
    size_t i;

    (void)params;

    __add(f, 1, "192.168.1.1:666", true);

    i = raft_configuration__voting_index(&f->configuration, 3);
    munit_assert_int(i, ==, f->configuration.n);

    return MUNIT_OK;
}

/* If the server exists but is non-voting, the length of the configuration is
   returned. . */
static MunitResult test_voting_index_non_voting(const MunitParameter params[],
                                                void *data)
{
    struct fixture *f = data;
    size_t i;

    (void)params;

    __add(f, 1, "192.168.1.1:666", false);

    i = raft_configuration__voting_index(&f->configuration, 1);
    munit_assert_int(i, ==, f->configuration.n);

    return MUNIT_OK;
}

static MunitTest voting_index_tests[] = {
    {"/match", test_voting_index_match, setup, tear_down, 0, NULL},
    {"/no-match", test_voting_index_no_match, setup, tear_down, 0, NULL},
    {"/non-voting", test_voting_index_non_voting, setup, tear_down, 0, NULL},
    {NULL, NULL, NULL, NULL, 0, NULL},
};

/**
 * raft_configuration__copy
 */

/* Out of memory */
static MunitResult test_copy_oom(const MunitParameter params[], void *data)
{
    struct fixture *f = data;
    struct raft_configuration configuration;
    int rv;

    (void)params;

    __add(f, 1, "192.168.1.1:666", false);

    test_heap_fault_config(&f->heap, 0, 1);
    test_heap_fault_enable(&f->heap);

    raft_configuration_init(&configuration);

    rv = raft_configuration__copy(&f->configuration, &configuration);
    munit_assert_int(rv, ==, RAFT_ERR_NOMEM);

    return MUNIT_OK;
}

static MunitTest copy_tests[] = {
    {"/oom", test_copy_oom, setup, tear_down, 0, NULL},
    {NULL, NULL, NULL, NULL, 0, NULL},
};

/**
 * Test suite
 */

MunitSuite raft_configuration_suites[] = {
    {"/add", add_tests, NULL, 1, 0},
    {"/remove", remove_tests, NULL, 1, 0},
    {"/n_voting", n_voting_tests, NULL, 1, 0},
    {"/index", index_tests, NULL, 1, 0},
    {"/voting-index", voting_index_tests, NULL, 1, 0},
    {"/copy", copy_tests, NULL, 1, 0},
    {NULL, NULL, NULL, 0, 0},
};
