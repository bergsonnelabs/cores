/* Doubly-linked circular list — ported from ST STM32_WPAN */
#ifndef STM_LIST_H
#define STM_LIST_H

#include <stdint.h>

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

/* CMSIS intrinsics */
#ifndef __get_PRIMASK
static inline uint32_t __get_PRIMASK(void) {
    uint32_t r;
    __asm volatile ("mrs %0, primask" : "=r"(r));
    return r;
}
#endif
#ifndef __set_PRIMASK
static inline void __set_PRIMASK(uint32_t v) {
    __asm volatile ("msr primask, %0" :: "r"(v) : "memory");
}
#endif
#ifndef __disable_irq
static inline void __disable_irq(void) {
    __asm volatile ("cpsid i" ::: "memory");
}
#endif

typedef struct _tListNode {
    struct _tListNode *next;
    struct _tListNode *prev;
} tListNode;

void LST_init_head(tListNode *listHead);
uint8_t LST_is_empty(tListNode *listHead);
void LST_insert_head(tListNode *listHead, tListNode *node);
void LST_insert_tail(tListNode *listHead, tListNode *node);
void LST_remove_node(tListNode *node);
void LST_remove_head(tListNode *listHead, tListNode **node);
void LST_remove_tail(tListNode *listHead, tListNode **node);
void LST_insert_node_after(tListNode *node, tListNode *ref_node);
void LST_insert_node_before(tListNode *node, tListNode *ref_node);
int LST_get_size(tListNode *listHead);
void LST_get_next_node(tListNode *ref_node, tListNode **node);
void LST_get_prev_node(tListNode *ref_node, tListNode **node);

#endif /* STM_LIST_H */
