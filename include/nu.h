#ifndef NU_H
#define NU_H

#include <stddef.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <ucontext.h>

/* JSON Node Type Variant IDs */
#define NU_AST_JSON_NULL    100
#define NU_AST_JSON_BOOL    101
#define NU_AST_JSON_INT     102
#define NU_AST_JSON_FLOAT   103
#define NU_AST_JSON_STRING  104
#define NU_AST_JSON_ARRAY   105
#define NU_AST_JSON_OBJECT  106
#define NU_AST_JSON_PAIR    107  /* Represents "key": value. val.str = key, first_child = value */

#ifndef NUO_H

#define NU_SMASH_CFG()     _Static_assert(0, "Libnu Error: You must #include <nuo.h> to use NU_SMASH_CFG()")
#define NU_SMASH_DATA(var) _Static_assert(0, "Libnu Error: You must #include <nuo.h> to use NU_SMASH_DATA()")
#define NU_SMASH_FLAGS()   _Static_assert(0, "Libnu Error: You must #include <nuo.h> to use NU_SMASH_FLAGS()")
#define NU_SMASH_REGS()    _Static_assert(0, "Libnu Error: You must #include <nuo.h> to use NU_SMASH_REGS()")
#define NU_SMASH_SIMD()    _Static_assert(0, "Libnu Error: You must #include <nuo.h> to use NU_SMASH_SIMD()")
#define nu_obfs(...)       _Static_assert(0, "Libnu Error: You must #include <nuo.h> to use nu_obfs()")

#endif /* NUO_H */

typedef enum {
    NU_LOG_DEBUG,
    NU_LOG_INFO,
    NU_LOG_WARN,
    NU_LOG_ERROR
} nu_log_level_t;

typedef void (*nu_proc_io_cb)(const char *buffer, size_t len, void *data);

void nu_log_output(nu_log_level_t level, const char *file, int line, const char *fmt, ...);

#define NU_DEBUG(fmt, ...) nu_log_output(NU_LOG_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define NU_INFO(fmt, ...)  nu_log_output(NU_LOG_INFO,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define NU_WARN(fmt, ...)  nu_log_output(NU_LOG_WARN,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define NU_ERROR(fmt, ...) nu_log_output(NU_LOG_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

typedef struct nu_mm nu_mm_t;

typedef enum {
    NU_MM_ARENA,
    NU_MM_BUDDY,
    NU_MM_SLAB,
    NU_MM_SLOB
} nu_mm_type_t;

// Creates a new nu_mm instance with the specified type,
// backing memory (ptr. to the memory that is supposed to be managed) and size of the memory.
nu_mm_t* nu_mm_create(nu_mm_type_t type, void *backing_mem, size_t size);

// Destroys a nu_mm instance
void nu_mm_destroy(nu_mm_t *mm);

// Alloc, Free and Realloc for a specific instance.
void* nu_alloc(nu_mm_t *mm, size_t size);
void  nu_free(nu_mm_t *mm, void *ptr);
void* nu_realloc(nu_mm_t *mm, void *ptr, size_t size);

typedef struct nu_map nu_map_t;

// Creates a new hashmap using a specific allocator instance.
// arg1: The backing memory manager instance to handle allocations.
// arg2: Baseline slot count (must be a power of 2 for optimization).
nu_map_t* nu_map_create(nu_mm_t *mm, size_t initial_capacity);

// Tears down the map table and frees duplicated keys.
void nu_map_destroy(nu_map_t *map);

// Inserts or updates a key-value association.
// Automatically doubles table size when the load factor hits 70%.
bool nu_map_set(nu_map_t *map, const char *key, void *value);

// Looks up a key and returns its associated pointer value.
// Returns NULL if the key is not found.
void* nu_map_get(nu_map_t *map, const char *key);

// Removes a key from the map
bool nu_map_delete(nu_map_t *map, const char *key);

// Returns total number of active elements inside the map.
size_t nu_map_size(nu_map_t *map);

// Raw 64-bit block operations
void nu_xtea_encrypt_block(const uint32_t key[4], uint32_t block[2]);
void nu_xtea_decrypt_block(const uint32_t key[4], uint32_t block[2]);

// Derives a valid 128-bit key out of any string passphrase
void nu_xtea_derive_key(const char *passphrase, uint32_t key[4]);

// CTR Stream Mode.
// Handles arbitrary data lengths
// Passing encrypted data through this function decrypts it.
// Requires an 8-byte nonce/initialization vector (use a random number or counter).
void nu_xtea_crypt_ctr(const uint32_t key[4], uint64_t nonce, uint8_t *data, size_t size);

typedef struct {
    uint32_t type;         /* User-defined token type identifier */
    const char *text;      /* Direct pointer into source buffer string slice */
    size_t length;         /* Length of the token string slice */
    size_t line;           /* Line number tracker for diagnostics */
    size_t column;         /* Precise column start index for error pointers */
} nu_token_t;

typedef struct {
    const char *source;    /* Target code text buffer */
    size_t cursor;         /* Read index offset */
    size_t line;           /* Current line index tracker */
    size_t line_start;     /* Index offset where current line started */
} nu_lexer_t;

// Lexer Functions
void nu_lexer_init(nu_lexer_t *lexer, const char *source);
char nu_lexer_peek_char(nu_lexer_t *lexer);
char nu_lexer_advance_char(nu_lexer_t *lexer);
void nu_lexer_skip_whitespace(nu_lexer_t *lexer);

typedef struct nu_ast_node nu_ast_node_t;

typedef union {
    int64_t i64;
    double f64;
    const char *str;
    void *raw;
} nu_ast_val_t;

struct nu_ast_node {
    uint32_t type;                     /* User-defined AST node variant type */
    nu_ast_val_t val;                  /* Multi-type data storage variant slot */
    nu_ast_node_t *first_child;        /* Head of children sibling sequence chain */
    nu_ast_node_t *last_child;         /* Tail of children sequence chain */
    nu_ast_node_t *next_sibling;       /* Linked list pointer to adjacent scope node */
};

typedef struct {
    nu_mm_t *mm;                       /* Memory manager instance */
    nu_ast_node_t *root;               /* AST Entry execution target root */
} nu_ast_t;

// AST (Abstract Syntax Tree) Functions.
nu_ast_t* nu_ast_create(nu_mm_t *mm);
nu_ast_node_t* nu_ast_new_node(nu_ast_t *ast, uint32_t type);
nu_ast_node_t* nu_ast_new_branch(nu_ast_t *ast, uint32_t type, size_t child_count, ...);
void nu_ast_add_child(nu_ast_node_t *parent, nu_ast_node_t *child);
void nu_ast_set_str(nu_ast_t *ast, nu_ast_node_t *node, const char *str, size_t len);
void nu_ast_set_int(nu_ast_node_t *node, int64_t val);
void nu_ast_set_float(nu_ast_node_t *node, double val);
void nu_ast_destroy(nu_ast_t *ast);

typedef struct nu_parser nu_parser_t;

typedef nu_ast_node_t* (*nu_nud_fn)(nu_parser_t *parser, nu_token_t tok);
typedef nu_ast_node_t* (*nu_led_fn)(nu_parser_t *parser, nu_ast_node_t *left, nu_token_t tok);

typedef struct {
    nu_nud_fn nud;          /* Null Denotation handler (Prefix / Literals / Unary) */
    nu_led_fn led;          /* Left Denotation handler (Infix / Postfix operators) */
    uint32_t lbp;           /* Left Binding Power value (Precedence hierarchy weight) */
} nu_parser_rule_t;

struct nu_parser {
    nu_lexer_t *lexer;
    nu_ast_t *ast;
    nu_token_t current;
    nu_token_t peek;
    nu_token_t (*next_token_cb)(nu_lexer_t *lexer);
    const nu_parser_rule_t *rules;
    uint32_t max_token_id;
};

// Pratt Parser funcs.
void nu_parser_init(nu_parser_t *parser, nu_lexer_t *lexer, nu_ast_t *ast, nu_token_t (*next_token_cb)(nu_lexer_t *), const nu_parser_rule_t *rules, uint32_t max_token_id);
void nu_parser_advance(nu_parser_t *parser);
bool nu_parser_match(nu_parser_t *parser, uint32_t expected_type);
nu_ast_node_t* nu_parse_expression(nu_parser_t *parser, uint32_t binding_power);

typedef enum {
    NU_HASH_MD5,
    NU_HASH_SHA1,
    NU_HASH_SHA256,
    NU_HASH_SHA512
} nu_hash_type_t;

// Computes a cryptographic hash over a binary data buffer.
// arg1: The memory manager instance to allocate the resulting hex string.
// arg2: The hashing algorithm type variant (e.g., NU_HASH_SHA256).
// arg3: Pointer to the input data buffer.
// arg4: Size of the input data buffer in bytes.
// Returns a null-terminated, lowercase hexadecimal string containing the hash,
// or NULL if the allocation fails or an unsupported hash type is provided.
char* nu_hash_encode(nu_mm_t *mm, nu_hash_type_t type, const uint8_t *data, size_t len);


// Encodes an AST node tree into a raw JSON string stream.
// Allocates the resulting buffer from the provided memory manager.
char* nu_json_encode(nu_mm_t *mm, const nu_ast_node_t *root);

// Decodes a raw JSON string into a structured, executable AST node tree.
// All parsed nodes are completely managed by the provided memory manager.
nu_ast_node_t* nu_json_decode(nu_mm_t *mm, const char *json);

// Query an AST JSON object using a dot/index notation path string.
// Eg: nu_json_get(root, "users.0.profile.email")
// Returns the matching nu_ast_node_t pointer or NULL if missing.
nu_ast_node_t* nu_json_get(const nu_ast_node_t *root, const char *keypath);

typedef struct nu_trie_node nu_trie_node_t;

struct nu_trie_node {
    void *value;                   /* User payload data pointer */
    nu_trie_node_t *next_sibling;  /* Right sibling in the same level */
    nu_trie_node_t *first_child;   /* Downward edge to next characters */
    char edge_char;                /* Character label for this specific incoming path */
    bool is_terminal;              /* Flag checking if this node represents a complete entry */
};

typedef struct {
    nu_mm_t *mm;                   /* Attached memory manager instance */
    nu_trie_node_t *root;          /* Tree entry root pointer */
    size_t size;                   /* Total active items tracked */
} nu_trie_t;

// Creates an empty Trie using a specified memory manager instance
nu_trie_t* nu_trie_create(nu_mm_t *mm);

// Inserts or updates an association. Returns false if allocation fails.
bool nu_trie_insert(nu_trie_t *trie, const char *key, void *value);

// Looks up a specific exact key string. Returns payload pointer, or NULL if not found.
void* nu_trie_lookup(nu_trie_t *trie, const char *key);

// Removes a key association. Cleans up stale hanging branches automatically.
bool nu_trie_delete(nu_trie_t *trie, const char *key);

// Destroys all allocated branches and nodes within the tree structure
void nu_trie_destroy(nu_trie_t *trie);

// Returns the maximum possible size a compressed buffer could take for a given input size.
// Useful for allocating the destination buffer safely before compressing.
size_t nu_lz4_compress_bound(size_t src_size);

// Compresses 'src' buffer into 'dst' buffer.
// Returns the exact size of the compressed data written to 'dst', or 0 on failure.
size_t nu_lz4_compress(const uint8_t *src, size_t src_size, uint8_t *dst, size_t dst_max_size);

// Decompresses 'src' buffer into 'dst' buffer.
// 'dst_max_size' must be the exact expected uncompressed size or greater.
// Returns the exact size of the decompressed data, or 0 on failure.
size_t nu_lz4_decompress(const uint8_t *src, size_t src_size, uint8_t *dst, size_t dst_max_size);

// Returns current wall-clock time in nanoseconds since the Unix Epoch.
// Subject to system clock adjustments (NTP).
uint64_t nu_time_now(void);

// Returns monotonically increasing time in nanoseconds since boot.
// Strictly hardware-driven. Immune to NTP jumps.
uint64_t nu_time_monotonic(void);

// High-precision sleep (nanoseconds).
// Automatically resumes if interrupted by an OS signal,
// guaranteeing the full sleep duration.
void nu_sleep_ns(uint64_t nanoseconds);

// Formats a string into 'str' using a va_list argument pointer.
// Returns the number of characters generated (excluding the null terminator).
int nu_vsnprintf(char *str, size_t size, const char *format, va_list ap);

// Variadic wrapper for nu_vsnprintf.
int nu_snprintf(char *str, size_t size, const char *format, ...);

// Formats a string and prints it directly to standard output (stdout).
// Returns total characters printed.
int nu_printf(const char *format, ...);

// Writes a raw buffer to a target file descriptor (1=stdout, 2=stderr).
int nu_fd_write(int fd, const char *buf, size_t len);

typedef enum {
    NU_ARG_BOOL,
    NU_ARG_INT,
    NU_ARG_STR
} nu_arg_type_t;

typedef struct {
    nu_arg_type_t type;
    const char *short_flag; // e.g., "-p" (or NULL)
    const char *long_flag;  // e.g., "--port" (or NULL)
    const char *help;       // Help description text
    bool is_set;            // Flag indicating if it was supplied
    union {
        bool b;
        int64_t i;
        const char *s;
    } val;
} nu_arg_def_t;

typedef struct {
    nu_mm_t *mm;
    nu_trie_t *flag_trie;      // Matches flags directly to definition pointers
    const char **positionals;  // Array of non-flag arguments captured
    size_t positional_count;
    size_t positional_cap;
    const char *program_name;
} nu_arg_parser_t;

// Initializes the argument parsing layout structure using an allocator
nu_arg_parser_t* nu_arg_create(nu_mm_t *mm);

// Registers an argument definition configuration.
bool nu_arg_register(nu_arg_parser_t *ap, nu_arg_def_t *def);

// Parses raw system arguments.
// Returns false if an unknown flag is met or a value conversion fails.
bool nu_arg_parse(nu_arg_parser_t *ap, int argc, char *argv[]);

// Prints a formatted help layout to stdout.
void nu_arg_print_help(nu_arg_parser_t *ap, nu_arg_def_t defs[], size_t def_count);

// Cleans up the argument parser allocations cleanly
void nu_arg_destroy(nu_arg_parser_t *ap);

struct nu_loop;
typedef struct nu_loop nu_loop_t;

typedef struct nu_lua nu_lua_t;

// Instantiates a new Lua runner.
nu_lua_t* nu_lua_create(nu_mm_t *mm, nu_loop_t *loop);

// Executes a raw script string. Returns false on syntax or execution errors.
bool nu_lua_run_string(nu_lua_t *ctx, const char *script);

// Decrypts an XTEA-encrypted script/bytecode chunk directly into memory and runs it on a Lua runner.
bool nu_lua_run_encrypted(nu_lua_t *ctx, const uint8_t *enc_data, size_t size, const uint32_t key[4]);

// Registers a standard C function to be available inside the Lua runner's environment.
typedef int (*nu_lua_fn)(nu_lua_t *ctx);
void nu_lua_register(nu_lua_t *ctx, const char *name, nu_lua_fn fn);

// Destroys a Lua runner.
void nu_lua_destroy(nu_lua_t *ctx);

int         nu_lua_get_top(nu_lua_t *ctx);
int64_t     nu_lua_to_int(nu_lua_t *ctx, int index);
double      nu_lua_to_float(nu_lua_t *ctx, int index);
const char* nu_lua_to_str(nu_lua_t *ctx, int index, size_t *out_len);
bool        nu_lua_to_bool(nu_lua_t *ctx, int index);

void        nu_lua_push_int(nu_lua_t *ctx, int64_t val);
void        nu_lua_push_float(nu_lua_t *ctx, double val);
void        nu_lua_push_str(nu_lua_t *ctx, const char *str);
void        nu_lua_push_bool(nu_lua_t *ctx, bool val);

typedef struct nu_py nu_py_t;

// Instantiates a new Python runner.
nu_py_t* nu_py_create(nu_mm_t *mm, nu_loop_t *loop);

// Runs an arbitrary string inside a Python runner.
bool     nu_py_run_string(nu_py_t *ctx, const char *script);

// Destroys a Python runner.
void     nu_py_destroy(nu_py_t *ctx);

// Register functions to run in a Python runner's space.
typedef void (*nu_py_fn)(nu_py_t *ctx);
void     nu_py_register(nu_py_t *ctx, const char *name, nu_py_fn fn);

// Simple stack-unpacking functions for function builders
int         nu_py_get_argc(nu_py_t *ctx);
int64_t     nu_py_to_int(nu_py_t *ctx, int index);
double      nu_py_to_float(nu_py_t *ctx, int index);
const char* nu_py_to_str(nu_py_t *ctx, int index);
bool        nu_py_to_bool(nu_py_t *ctx, int index);

void        nu_py_push_int(nu_py_t *ctx, int64_t val);
void        nu_py_push_float(nu_py_t *ctx, double val);
void        nu_py_push_str(nu_py_t *ctx, const char *str);
void        nu_py_push_bool(nu_py_t *ctx, bool val);

typedef struct {
    nu_mm_t *mm;
    uint8_t *data;
    size_t size;
    size_t capacity;
} nu_buf_t;

// Buffer things
void   nu_buf_init(nu_buf_t *buf, nu_mm_t *mm, size_t initial_cap);
void   nu_buf_reserve(nu_buf_t *buf, size_t new_cap);
void   nu_buf_append(nu_buf_t *buf, const void *data, size_t len);
void   nu_buf_append_val(nu_buf_t *buf, uint8_t byte);
void   nu_buf_reset(nu_buf_t *buf);
void   nu_buf_free(nu_buf_t *buf);

typedef struct {
    const char *ptr;
    size_t len;
} nu_span_t;

// String Span functions
nu_span_t nu_span_from_str(const char *str);
nu_span_t nu_span_from_parts(const char *ptr, size_t len);
bool      nu_span_eq(nu_span_t span, const char *str);
bool      nu_span_eq_span(nu_span_t span1, nu_span_t span2);
nu_span_t nu_span_trim(nu_span_t span);
nu_span_t nu_span_split_next(nu_span_t *span, char delim);

typedef enum {
    NU_FIBER_READY,
    NU_FIBER_RUNNING,
    NU_FIBER_DEAD
} nu_fiber_state_t;

typedef struct nu_fiber {
    ucontext_t ctx;                 // OS-independent C context structure
    void *stack;                    // Allocated stack memory
    nu_fiber_state_t state;
    void (*entry)(void *);          // Target function
    void *arg;                      // Argument
    struct nu_fiber *next;
} nu_fiber_t;

typedef struct {
    nu_fiber_t *current;
    nu_fiber_t *idle;               // Main thread context
    nu_fiber_t *run_queue;
    nu_fiber_t *run_queue_tail;
} nu_scheduler_t;


// Memory-safe string split. Returns heap-allocated array of strings. Free with nu_str_free_list.
char** nu_str_split(const char *str, const char *delim, int *out_count);
void   nu_str_free_list(char **list, int count);

// Trims whitespace in-place (modifies the buffer)
char* nu_str_trim(char *str);

typedef void (*nu_event_cb)(int fd, void *data);

nu_loop_t* nu_loop_create(void);
void       nu_loop_destroy(nu_loop_t *loop);

// Add a standard file descriptor to watch for readability
bool       nu_loop_add_fd(nu_loop_t *loop, int fd, nu_event_cb cb, void *data);

// Add a timer (interval in milliseconds)
bool       nu_loop_add_timer(nu_loop_t *loop, int ms, nu_event_cb cb, void *data);

// Watch a file/directory path for modifications
bool       nu_loop_add_watch(nu_loop_t *loop, const char *path, nu_event_cb cb, void *data);

// Run the loop indefinitely. Returns false on catastrophic failure.
bool       nu_loop_run(nu_loop_t *loop);

typedef void (*nu_ipc_cb)(const char *msg, int client_fd, void *data);

// Starts a blocking IPC server. Typically spawned in its own thread or used as a standalone daemon.
// Automatically creates a UNIX socket at 'sock_path'.
void       nu_ipc_listen(const char *sock_path, nu_ipc_cb cb, void *data);

// Sends a quick message to a listening IPC server and returns response (or NULL)
char* nu_ipc_send(const char *sock_path, const char *message);

// Mask and monitor system signals via epoll loop.
// Pass signum (e.g., SIGINT, SIGTERM). Returns false on failure.
bool nu_loop_add_signal(nu_loop_t *loop, int signum, nu_event_cb cb, void *data);

// Exec background processes without hanging. Pipeline matches standard argv format.
bool nu_process_spawn(nu_loop_t *loop, char *const argv[], nu_proc_io_cb stdout_cb, void *data);

// Daemonize easily
bool nu_daemonize(void);

#endif // NU_H
