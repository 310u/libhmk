typedef unsigned int bitmap_t;
#define M_DIV_CEIL(n, d) (((n) + (d) - 1) / (d))
#define MAKE_BITMAP(len) ((bitmap_t[M_DIV_CEIL(len, 32)]){0})
#define NUM_KEYS 42
static bitmap_t key_disabled[] = MAKE_BITMAP(NUM_KEYS);
