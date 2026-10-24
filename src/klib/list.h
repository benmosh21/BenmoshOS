#ifndef LIST_H
#define LIST_H

#include <stddef.h>
#include <stdint.h>
#include "../kernel/panic/panic.h"

struct list_head {
    struct list_head *next, *prev;
};

#define LIST_HEAD_INIT(name) { &(name), &(name) }

#define LIST_HEAD(name) \
    struct list_head name = LIST_HEAD_INIT(name)

// Initialize a list head
static inline void list_init(struct list_head *list) {
    list->next = list;
    list->prev = list;
}

// Internal function to add a new node between two known consecutive nodes
static inline void __list_add(struct list_head *new_node,
                              struct list_head *prev,
                              struct list_head *next) {
    next->prev = new_node;
    new_node->next = next;
    new_node->prev = prev;
    prev->next = new_node;
}

// Add a new node at the end of the list
static inline void list_add_tail(struct list_head *new_node, struct list_head *head) {
    __list_add(new_node, head->prev, head);
}

// Add a new node at the beginning of the list
static inline void list_add(struct list_head *new_node, struct list_head *head) {
    __list_add(new_node, head, head->next);
}

// Remove a node from the list
static inline void list_del(struct list_head *entry) {
    /* Hardened check against pointer manipulation */
    if (entry->next->prev != entry || entry->prev->next != entry) {
        kernel_panic("Kernel List Corruption: Tampered pointers in list_del!");
    }
    entry->prev->next = entry->next;
    entry->next->prev = entry->prev;
    entry->next = (struct list_head*)0;
    entry->prev = (struct list_head*)0;
}

// Check if the list is empty
static inline int list_empty(const struct list_head *head) {
    return head->next == head;
}


#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

// Get the container structure from a member pointer
#define container_of(ptr, type, member) ({                      \
        const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
        (type *)( (char *)__mptr - offsetof(type,member) );})

// Get the struct for this entry
#define list_entry(ptr, type, member) \
    container_of(ptr, type, member)

// Get the first entry from a list
#define list_first_entry(ptr, type, member) \
    list_entry((ptr)->next, type, member)

// Iterate over the list of given type
#define list_for_each(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

// Iterate over the list of given type safe against removal of list entry
#define list_for_each_safe(pos, n, head) \
    for (pos = (head)->next, n = pos->next; pos != (head); \
         pos = n, n = pos->next)

#endif /* LIST_H */