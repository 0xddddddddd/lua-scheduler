#ifndef __LINUX_LIST_H__
#define __LINUX_LIST_H__

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
#	pragma once
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>

#ifdef _MSC_VER
#   define __always_inline __forceinline
#elif defined(__GNUC__)
#   define __always_inline inline __attribute__((always_inline))
#else
#   define __always_inline inline
#endif

#ifdef _MSC_VER
#   define typeof(x) __typeof__(x)
#endif

/* container_of: Get the containing structure from a member pointer */
#ifndef container_of
#   define container_of(ptr, type, member) ((type *)((char *)(ptr) - offsetof(type, member)))
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct list
{
    struct list *next, *prev;
};

struct hlist_head
{
    struct hlist_node *first;
};

struct hlist_node
{
    struct hlist_node *next, **pprev;
};

/**
 * LIST_HEAD_INIT - initialize a &struct list's links to point to itself
 * @name: name of the list_head
 */
#define LIST_HEAD_INIT(name) { &(name), &(name) }

/**
 * LIST_HEAD - definition of a &struct list with initialization values
 * @name: name of the list_head
 */
#define LIST_HEAD(name) \
    struct list name = LIST_HEAD_INIT(name)

/**
 * list_init - Initialize a list_head structure
 * @list: list_head structure to be initialized.
 */
static inline void list_init(struct list *link)
{
    link->next = link;
    link->prev = link;
}


static inline bool __list_add_valid(struct list *link,
                                    struct list *prev,
                                    struct list *next)
{
    return true;
}

static inline bool __list_del_entry_valid(struct list *entry)
{
    return true;
}

/*
 * Insert a new entry between two known consecutive entries.
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 *
 * Must be inlined to ensure it can be safely called
 * with initdata arguments.
 */
static __always_inline void __list_add(struct list *link,
                                       struct list *prev,
                                       struct list *next)
{
    if (!__list_add_valid(link, prev, next))
        return;

    next->prev = link;
    link->next = next;
    link->prev = prev;
    prev->next = link;
}

/**
 * Delete a list entry by making the prev/next entries point to each other.
 */
static inline void __list_del(struct list *prev, struct list *next)
{
    next->prev = prev;
    prev->next = next;
}

static inline void __list_del_entry(struct list *entry)
{
    if (!__list_del_entry_valid(entry))
        return;

    __list_del(entry->prev, entry->next);
}

/**
 * list_add - add a new entry
 * @new: new entry to be added
 * @head: list head to add it after
 *
 * Insert a new entry after the specified head.
 * This is good for implementing stacks.
 *
 * Must be inlined to ensure it can be safely called
 * with initdata arguments.
 */
static __always_inline void list_add(struct list *link,
                                     struct list *new_link)
{
    __list_add(new_link, link, link->next);
}

static struct list* list_ctor()
{
    struct list* link = (struct list*)malloc(sizeof(struct list));
    list_init(link);
    return link;
}

static inline void list_insert(struct list *link, struct list *new_link)
{
    new_link->prev = link->prev;
    new_link->next = link;
    new_link->prev->next = new_link;
    new_link->next->prev = new_link;
}

static inline void list_append(struct list *link, struct list *new_link)
{
    list_insert((struct list *)link, new_link);
}

static inline void list_prepend(struct list *link, struct list *new_link)
{
    list_insert(link->next, new_link);
}

/**
 * list_add_tail - add a new entry before the specified head
 * @new: new entry to be added
 * @head: list head to add it before
 *
 * Insert a new entry before the specified head.
 * This is useful for implementing queues.
 */
static inline void list_add_tail(struct list *head, struct list *link)
{
    __list_add(link, head->prev, head);
}

/**
 * list_add_tail_release - add a new entry with release barrier
 * @new: new entry to be added
 * @head: list head to add it before
 */
static inline void list_add_tail_release(struct list *head, 
                                         struct list *link)
{
    struct list *prev = head->prev;

    if (__list_add_valid(link, prev, head)) {
        link->next = head;
        link->prev = prev;
        head->prev = link;
        prev->next = link;
    }
}

/**
 * list_del - deletes entry from list
 * @entry: the element to delete from the list
 */
static inline void list_del(struct list *entry)
{
    __list_del_entry(entry);
    entry->next = NULL;
    entry->prev = NULL;
}

static inline void list_remove(struct list *link)
{
    link->prev->next = link->next;
    link->next->prev = link->prev;
}

/**
 * list_replace - replace old entry by new one
 * @old: the element to be replaced
 * @new: the new element to insert
 */
static inline void list_replace(struct list *old,
                                struct list *link)
{
    link->next = old->next;
    link->next->prev = link;
    link->prev = old->prev;
    link->prev->next = link;
}

/**
 * list_replace_init - replace old entry and initialize the old one
 * @old: the element to be replaced
 * @new: the new element to insert
 */
static inline void list_replace_init(struct list *old,
                                     struct list *link)
{
    list_replace(old, link);
    list_init(old);
}

/**
 * list_swap - replace entry1 with entry2 and re-add entry1 at entry2's position
 * @entry1: the location to place entry2
 * @entry2: the location to place entry1
 */
static inline void list_swap(struct list *entry1,
                             struct list *entry2)
{
    struct list *pos = entry2->prev;

    list_del(entry2);
    list_replace(entry1, entry2);
    if (pos == entry1)
        pos = entry2;
    list_add(pos, entry1);
}

static inline void list_swap2(struct list *new_link, struct list *old_link)
{
    list_remove(old_link);
    list_insert(new_link, old_link);
}

/**
 * list_del_init - deletes entry from list and reinitialize it
 * @entry: the element to delete from the list
 */
static inline void list_del_init(struct list *entry)
{
    __list_del_entry(entry);
    list_init(entry);
}

/**
 * list_move - delete from one list and add as another's head
 * @list: the entry to move
 * @head: the head that will precede our entry
 */
static inline void list_move(struct list *head, struct list *link)
{
    __list_del_entry(link);
    list_add(head, link);
}

/**
 * list_move_tail - delete from one list and add as another's tail
 * @list: the entry to move
 * @head: the head that will follow our entry
 */
static inline void list_move_tail(struct list *head,
                                  struct list *link)
{
    __list_del_entry(link);
    list_add_tail(head, link);
}

/**
 * list_bulk_move_tail - move a subsection of a list to its tail
 * @head: the head that will follow our entry
 * @first: first entry to move
 * @last: last entry to move
 */
static inline void list_bulk_move_tail(struct list *head,
                                       struct list *first,
                                       struct list *last)
{
    first->prev->next = last->next;
    last->next->prev = first->prev;

    head->prev->next = first;
    first->prev = head->prev;

    last->next = head;
    head->prev = last;
}

/* ========== List Tests ========== */

/**
 * list_is_first - tests whether @list is the first entry in list @head
 */
static inline int list_is_first(const struct list *head,
                                const struct list *link)
{
    return link->prev == head;
}

/**
 * list_is_last - tests whether @list is the last entry in list @head
 */
static inline int list_is_last(const struct list *head,
                               const struct list *link)
{
    return link->next == head;
}

/**
 * list_is_head - tests whether @list is the list @head
 */
static inline int list_is_head(const struct list *head,
                               const struct list *link)
{
    return link == head;
}

/**
 * list_empty - tests whether a list is empty
 * @head: the list to test
 */
static inline int list_empty(const struct list *head)
{
    return head->next == head;
}

/**
 * list_del_init_careful - deletes entry from list and reinitialize it
 * @entry: the element to delete from the list
 */
static inline void list_del_init_careful(struct list *entry)
{
    __list_del_entry(entry);
    entry->prev = entry;
    entry->next = entry;
}

/**
 * list_empty_careful - tests whether a list is empty
 * @head: the list to test
 */
static inline int list_empty_careful(const struct list *head)
{
    struct list *next = head->next;
    return list_is_head(next, head) && (next == head->prev);
}

/**
 * list_rotate_left - rotate the list to the left
 * @head: the head of the list
 */
static inline void list_rotate_left(struct list *head)
{
    struct list *first;

    if (!list_empty(head)) {
        first = head->next;
        list_move_tail(first, head);
    }
}

/**
 * list_rotate_to_front - Rotate list to specific item
 * @list: The desired new front of the list
 * @head: The head of the list
 */
static inline void list_rotate_to_front(struct list *head,
                                        struct list *link)
{
    list_move_tail(head, link);
}

/**
 * list_is_singular - tests whether a list has just one entry
 * @head: the list to test
 */
static inline int list_is_singular(const struct list *head)
{
    return !list_empty(head) && (head->next == head->prev);
}

static inline void __list_cut_position(struct list *head,
                                       struct list *link,
                                       struct list *entry)
{
    struct list *new_first = entry->next;
    link->next = head->next;
    link->next->prev = link;
    link->prev = entry;
    entry->next = link;
    head->next = new_first;
    new_first->prev = head;
}

/**
 * list_cut_position - cut a list into two
 * @list: a new list to add all removed entries
 * @head: a list with entries
 * @entry: an entry within head
 */
static inline void list_cut_position(struct list *head,
                                     struct list *link,
                                     struct list *entry)
{
    if (list_empty(head))
        return;
    if (list_is_singular(head) && !list_is_head(entry, head) && (entry != head->next))
        return;
    if (list_is_head(head, entry))
        list_init(link);
    else
        __list_cut_position(head, link, entry);
}

/**
 * list_cut_before - cut a list into two, before given entry
 * @list: a new list to add all removed entries
 * @head: a list with entries
 * @entry: an entry within head
 */
static inline void list_cut_before(struct list *head,
                                   struct list *link,
                                   struct list *entry)
{
    if (head->next == entry) {
        list_init(link);
        return;
    }
    link->next = head->next;
    link->next->prev = link;
    link->prev = entry->prev;
    link->prev->next = link;
    head->next = entry;
    entry->prev = head;
}

static inline void __list_splice(const struct list *link,
                                 struct list *prev,
                                 struct list *next)
{
    struct list *first = link->next;
    struct list *last = link->prev;

    first->prev = prev;
    prev->next = first;

    last->next = next;
    next->prev = last;
}

/**
 * list_splice - join two lists
 * @list: the new list to add
 * @head: the place to add it in the first list
 */
static inline void list_splice(struct list *head,
                               const struct list *link)
{
    if (!list_empty(link))
        __list_splice(link, head, head->next);
}

/**
 * list_splice_tail - join two lists, each list being a queue
 * @list: the new list to add
 * @head: the place to add it in the first list
 */
static inline void list_splice_tail(struct list *head,
                                    struct list *link)
{
    if (!list_empty(link))
        __list_splice(link, head->prev, head);
}

/**
 * list_splice_init - join two lists and reinitialise the emptied list
 * @list: the new list to add
 * @head: the place to add it in the first list
 */
static inline void list_splice_init(struct list *head,
                                    struct list *link)
{
    if (!list_empty(link)) {
        __list_splice(link, head, head->next);
        list_init(link);
    }
}

/**
 * list_splice_tail_init - join two lists and reinitialise the emptied list
 * @list: the new list to add
 * @head: the place to add it in the first list
 */
static inline void list_splice_tail_init(struct list *head,
                                         struct list *link)
{
    if (!list_empty(link)) {
        __list_splice(link, head->prev, head);
        list_init(link);
    }
}

/* ========== List Entry Macros ========== */

/**
 * list_entry - get the struct for this entry
 * @ptr: the &struct list pointer
 * @type: the type of the struct this is embedded in
 * @member: the name of the list_head within the struct
 */
#define list_entry(ptr, type, member) \
    container_of(ptr, type, member)

/**
 * list_first_entry - get the first element from a list
 * @ptr: the list head to take the element from
 * @type: the type of the struct this is embedded in
 * @member: the name of the list_head within the struct
 */
#define list_first_entry(ptr, type, member) \
    list_entry((ptr)->next, type, member)

/**
 * list_last_entry - get the last element from a list
 * @ptr: the list head to take the element from
 * @type: the type of the struct this is embedded in
 * @member: the name of the list_head within the struct
 */
#define list_last_entry(ptr, type, member) \
    list_entry((ptr)->prev, type, member)

/**
 * list_first_entry_or_null - get the first element from a list
 * @ptr: the list head to take the element from
 * @type: the type of the struct this is embedded in
 * @member: the name of the list_head within the struct
 */
#define list_first_entry_or_null(ptr, type, member) ({ \
    struct list *head__ = (ptr); \
    struct list *pos__ = head__->next; \
    pos__ != head__ ? list_entry(pos__, type, member) : NULL; \
})

/**
 * list_first_entry_or_null_acquire - get first element with acquire barrier
 */
#define list_first_entry_or_null_acquire(ptr, type, member) ({ \
    struct list *head__ = (ptr); \
    struct list *pos__ = head__->next; \
    pos__ != head__ ? list_entry(pos__, type, member) : NULL; \
})

/**
 * list_last_entry_or_null - get the last element from a list
 */
#define list_last_entry_or_null(ptr, type, member) ({ \
    struct list *head__ = (ptr); \
    struct list *pos__ = head__->prev; \
    pos__ != head__ ? list_entry(pos__, type, member) : NULL; \
})

/**
 * list_next_entry - get the next element in list
 * @pos: the type * to cursor
 * @member: the name of the list_head within the struct
 */
#define list_next_entry(pos, member) \
    list_entry((pos)->member.next, typeof(*(pos)), member)

/**
 * list_next_entry_circular - get the next element in list
 */
#define list_next_entry_circular(pos, head, member) \
    (list_is_last(&(pos)->member, head) ? \
    list_first_entry(head, typeof(*(pos)), member) : list_next_entry(pos, member))

/**
 * list_prev_entry - get the prev element in list
 * @pos: the type * to cursor
 * @member: the name of the list_head within the struct
 */
#define list_prev_entry(pos, member) \
    list_entry((pos)->member.prev, typeof(*(pos)), member)

/**
 * list_prev_entry_circular - get the prev element in list
 */
#define list_prev_entry_circular(pos, head, member) \
    (list_is_first(&(pos)->member, head) ? \
    list_last_entry(head, typeof(*(pos)), member) : list_prev_entry(pos, member))

/* ========== List Iteration Macros ========== */

/**
 * list_for_each - iterate over a list
 * @pos: the &struct list to use as a loop cursor
 * @head: the head for your list
 */
#define list_for_each(pos, head) \
    for (pos = (head)->next; !list_is_head(pos, (head)); pos = pos->next)

/**
 * list_for_each_continue - continue iteration over a list
 */
#define list_for_each_continue(pos, head) \
    for (pos = pos->next; !list_is_head(pos, (head)); pos = pos->next)

/**
 * list_for_each_prev - iterate over a list backwards
 */
#define list_for_each_prev(pos, head) \
    for (pos = (head)->prev; !list_is_head(pos, (head)); pos = pos->prev)

/**
 * list_for_each_safe - iterate over a list safe against removal
 * @pos: the &struct list to use as a loop cursor
 * @n: another &struct list to use as temporary storage
 * @head: the head for your list
 */
#define list_for_each_safe(pos, n, head) \
    for (pos = (head)->next, n = pos->next; \
         !list_is_head(pos, (head)); \
         pos = n, n = pos->next)

/**
 * list_for_each_prev_safe - iterate backwards safe against removal
 */
#define list_for_each_prev_safe(pos, n, head) \
    for (pos = (head)->prev, n = pos->prev; \
         !list_is_head(pos, (head)); \
         pos = n, n = pos->prev)

/**
 * list_count_nodes - count nodes in the list
 * @head: the head for your list
 */
static inline size_t list_count_nodes(struct list *head)
{
    struct list *pos;
    size_t count = 0;

    list_for_each(pos, head)
        count++;

    return count;
}

/**
 * list_entry_is_head - test if the entry points to the head
 */
#define list_entry_is_head(pos, head, member) \
    list_is_head(&pos->member, (head))

/**
 * list_for_each_entry - iterate over list of given type
 * @pos: the type * to use as a loop cursor
 * @head: the head for your list
 * @member: the name of the list_head within the struct
 */
#define list_for_each_entry(pos, head, member) \
    for (pos = list_first_entry(head, typeof(*pos), member); \
         !list_entry_is_head(pos, head, member); \
         pos = list_next_entry(pos, member))

/**
 * list_for_each_entry_reverse - iterate backwards over list
 */
#define list_for_each_entry_reverse(pos, head, member) \
    for (pos = list_last_entry(head, typeof(*pos), member); \
         !list_entry_is_head(pos, head, member); \
         pos = list_prev_entry(pos, member))

/**
 * list_prepare_entry - prepare a pos entry for use
 */
#define list_prepare_entry(pos, head, member) \
    ((pos) ? : list_entry(head, typeof(*pos), member))

/**
 * list_for_each_entry_continue - continue iteration over list
 */
#define list_for_each_entry_continue(pos, head, member) \
    for (pos = list_next_entry(pos, member); \
         !list_entry_is_head(pos, head, member); \
         pos = list_next_entry(pos, member))

/**
 * list_for_each_entry_continue_reverse - iterate backwards from given point
 */
#define list_for_each_entry_continue_reverse(pos, head, member) \
    for (pos = list_prev_entry(pos, member); \
         !list_entry_is_head(pos, head, member); \
         pos = list_prev_entry(pos, member))

/**
 * list_for_each_entry_from - iterate from current point
 */
#define list_for_each_entry_from(pos, head, member) \
    for (; !list_entry_is_head(pos, head, member); \
         pos = list_next_entry(pos, member))

/**
 * list_for_each_entry_from_reverse - iterate backwards from current point
 */
#define list_for_each_entry_from_reverse(pos, head, member) \
    for (; !list_entry_is_head(pos, head, member); \
         pos = list_prev_entry(pos, member))

/**
 * list_for_each_entry_safe - iterate safe against removal
 * @pos: the type * to use as a loop cursor
 * @n: another type * to use as temporary storage
 * @head: the head for your list
 * @member: the name of the list_head within the struct
 */
#define list_for_each_entry_safe(pos, n, head, member) \
    for (pos = list_first_entry(head, typeof(*pos), member), \
            n = list_next_entry(pos, member); \
         !list_entry_is_head(pos, head, member); \
         pos = n, n = list_next_entry(n, member))

/**
 * list_for_each_entry_safe_continue - continue list iteration safe
 */
#define list_for_each_entry_safe_continue(pos, n, head, member) \
    for (pos = list_next_entry(pos, member), \
            n = list_next_entry(pos, member); \
         !list_entry_is_head(pos, head, member); \
         pos = n, n = list_next_entry(n, member))

/**
 * list_for_each_entry_safe_from - iterate from current point safe
 */
#define list_for_each_entry_safe_from(pos, n, head, member) \
    for (n = list_next_entry(pos, member); \
         !list_entry_is_head(pos, head, member); \
         pos = n, n = list_next_entry(n, member))

/**
 * list_for_each_entry_safe_reverse - iterate backwards safe
 */
#define list_for_each_entry_safe_reverse(pos, n, head, member) \
    for (pos = list_last_entry(head, typeof(*pos), member), \
            n = list_prev_entry(pos, member); \
         !list_entry_is_head(pos, head, member); \
         pos = n, n = list_prev_entry(n, member))

/**
 * list_safe_reset_next - reset a stale loop
 */
#define list_safe_reset_next(pos, n, member) \
    n = list_next_entry(pos, member)

/* ========== Hash List (HLIST) Operations ========== */

#define HLIST_HEAD_INIT { .first = NULL }
#define HLIST_HEAD(name) struct hlist_head name = { .first = NULL }
#define INIT_HLIST_HEAD(ptr) ((ptr)->first = NULL)

static inline void INIT_HLIST_NODE(struct hlist_node *h)
{
    h->next = NULL;
    h->pprev = NULL;
}

/**
 * hlist_unhashed - Has node been removed?
 */
static inline int hlist_unhashed(const struct hlist_node *h)
{
    return !h->pprev;
}

/**
 * hlist_unhashed_lockless - lockless version
 */
static inline int hlist_unhashed_lockless(const struct hlist_node *h)
{
    return !h->pprev;
}

/**
 * hlist_empty - Is the hlist empty?
 */
static inline int hlist_empty(const struct hlist_head *h)
{
    return !h->first;
}

static inline void __hlist_del(struct hlist_node *n)
{
    struct hlist_node *next = n->next;
    struct hlist_node **pprev = n->pprev;

    *pprev = next;
    if (next)
        next->pprev = pprev;
}

/**
 * hlist_del - Delete node from list
 */
static inline void hlist_del(struct hlist_node *n)
{
    __hlist_del(n);
    n->next = NULL;
    n->pprev = NULL;
}

/**
 * hlist_del_init - Delete and reinitialize
 */
static inline void hlist_del_init(struct hlist_node *n)
{
    if (!hlist_unhashed(n)) {
        __hlist_del(n);
        INIT_HLIST_NODE(n);
    }
}

/**
 * hlist_add_head - add at the beginning
 */
static inline void hlist_add_head(struct hlist_node *n, struct hlist_head *h)
{
    struct hlist_node *first = h->first;
    n->next = first;
    if (first)
        first->pprev = &n->next;
    h->first = n;
    n->pprev = &h->first;
}

/**
 * hlist_add_before - add before specified node
 */
static inline void hlist_add_before(struct hlist_node *n,
                                    struct hlist_node *next)
{
    n->pprev = next->pprev;
    n->next = next;
    next->pprev = &n->next;
    *(n->pprev) = n;
}

/**
 * hlist_add_behind - add after specified node
 */
static inline void hlist_add_behind(struct hlist_node *n,
                                    struct hlist_node *prev)
{
    n->next = prev->next;
    prev->next = n;
    n->pprev = &prev->next;

    if (n->next)
        n->next->pprev = &n->next;
}

/**
 * hlist_add_fake - create a fake hlist
 */
static inline void hlist_add_fake(struct hlist_node *n)
{
    n->pprev = &n->next;
}

/**
 * hlist_fake - Is this a fake hlist?
 */
static inline bool hlist_fake(struct hlist_node *h)
{
    return h->pprev == &h->next;
}

/**
 * hlist_is_singular_node - is node the only element?
 */
static inline bool hlist_is_singular_node(struct hlist_node *n,
                                          struct hlist_head *h)
{
    return !n->next && n->pprev == &h->first;
}

/**
 * hlist_move_list - Move an hlist
 */
static inline void hlist_move_list(struct hlist_head *old,
                                   struct hlist_head *link)
{
    link->first = old->first;
    if (link->first)
        link->first->pprev = &link->first;
    old->first = NULL;
}

/**
 * hlist_splice_init - move all entries from one list to another
 */
static inline void hlist_splice_init(struct hlist_head *from,
                                     struct hlist_node *last,
                                     struct hlist_head *to)
{
    if (to->first)
        to->first->pprev = &last->next;
    last->next = to->first;
    to->first = from->first;
    from->first->pprev = &to->first;
    from->first = NULL;
}

#define hlist_entry(ptr, type, member) container_of(ptr, type, member)

#define hlist_for_each(pos, head) \
    for (pos = (head)->first; pos ; pos = pos->next)

#define hlist_for_each_safe(pos, n, head) \
    for (pos = (head)->first; pos && ({ n = pos->next; 1; }); pos = n)

#define hlist_entry_safe(ptr, type, member) \
    ({ typeof(ptr) ____ptr = (ptr); \
       ____ptr ? hlist_entry(____ptr, type, member) : NULL; })

/**
 * hlist_for_each_entry - iterate over list of given type
 */
#define hlist_for_each_entry(pos, head, member) \
    for (pos = hlist_entry_safe((head)->first, typeof(*(pos)), member); \
         pos; \
         pos = hlist_entry_safe((pos)->member.next, typeof(*(pos)), member))

/**
 * hlist_for_each_entry_continue - continue iteration
 */
#define hlist_for_each_entry_continue(pos, member) \
    for (pos = hlist_entry_safe((pos)->member.next, typeof(*(pos)), member); \
         pos; \
         pos = hlist_entry_safe((pos)->member.next, typeof(*(pos)), member))

/**
 * hlist_for_each_entry_from - iterate from current point
 */
#define hlist_for_each_entry_from(pos, member) \
    for (; pos; \
         pos = hlist_entry_safe((pos)->member.next, typeof(*(pos)), member))

/**
 * hlist_for_each_entry_safe - iterate safe against removal
 */
#define hlist_for_each_entry_safe(pos, n, head, member) \
    for (pos = hlist_entry_safe((head)->first, typeof(*pos), member); \
         pos && ({ n = pos->member.next; 1; }); \
         pos = hlist_entry_safe(n, typeof(*pos), member))

/**
 * hlist_count_nodes - count nodes in the hlist
 */
static inline size_t hlist_count_nodes(struct hlist_head *head)
{
    struct hlist_node *pos;
    size_t count = 0;

    hlist_for_each(pos, head)
        count++;

    return count;
}

#ifdef __cplusplus
}
#endif

#endif