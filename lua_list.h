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
 *
 * Initializes the list_head to point to itself.  If it is a list header,
 * the result is an empty list.
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

/*
 * Delete a list entry by making the prev/next entries
 * point to each other.
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
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

/**
 * list_ctor - allocate and initialize a new list node
 *
 * Allocates memory for a new list node and initializes it to point to itself.
 * Caller is responsible for freeing the allocated memory.
 *
 * Returns a pointer to the newly allocated and initialized list node.
 */
static struct list* list_ctor()
{
    struct list* link = (struct list*)malloc(sizeof(struct list));
    list_init(link);
    return link;
}

/**
 * list_insert - insert a new entry before the specified node
 * @link: existing list node to insert before
 * @new_link: new entry to be added
 *
 * Insert a new entry before the specified node.
 */
static inline void list_insert(struct list *link, struct list *new_link)
{
    new_link->prev = link->prev;
    new_link->next = link;
    new_link->prev->next = new_link;
    new_link->next->prev = new_link;
}

/**
 * list_append - append a new entry before the specified node
 * @link: existing list node to append before
 * @new_link: new entry to be added
 *
 * Appends a new entry before the specified node (same as list_insert).
 */
static inline void list_append(struct list *link, struct list *new_link)
{
    list_insert(link, new_link);
}

/**
 * list_prepend - prepend a new entry after the specified node
 * @link: existing list node to prepend after
 * @new_link: new entry to be added
 *
 * Prepends a new entry by inserting it after the specified node.
 */
static inline void list_prepend(struct list *link, struct list *new_link)
{
    list_insert(link->next, new_link);
}

/**
 * list_add_tail - add a new entry
 * @head: list head to add it before
 * @link: new entry to be added
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
 * @head: list head to add it before
 * @link: new entry to be added
 *
 * Insert a new entry before the specified head, using a release barrier to set
 * the ->next pointer that points to it.  This is useful for implementing
 * queues, in particular one that the elements will be walked through forwards
 * locklessly.
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
 * list_del - deletes entry from list.
 * @entry: the element to delete from the list.
 * Note: list_empty() on entry does not return true after this, the entry is
 * in an undefined state.
 */
static inline void list_del(struct list *entry)
{
    __list_del_entry(entry);
    entry->next = NULL;
    entry->prev = NULL;
}

/**
 * list_remove - remove entry from list
 * @link: the element to remove from the list.
 *
 * Removes the entry from the list by making prev/next entries
 * point to each other. The node itself is not reinitialized.
 */
static inline void list_remove(struct list *link)
{
    link->prev->next = link->next;
    link->next->prev = link->prev;
}

/**
 * list_replace - replace old entry by new one
 * @old : the element to be replaced
 * @link : the new element to insert
 *
 * If @old was empty, it will be overwritten.
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
 * list_replace_init - replace old entry by new one and initialize the old one
 * @old : the element to be replaced
 * @link : the new element to insert
 *
 * If @old was empty, it will be overwritten.
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

/**
 * list_move_before - move an entry before another entry
 * @target: the position marker where to insert before
 * @entry: the entry to be moved
 *
 * Removes @entry from its current position and inserts it
 * before @target. Unlike list_swap(), this does not exchange
 * positions but simply moves one entry.
 */
static inline void list_move_before(struct list *target, struct list *entry)
{
    list_remove(entry);
    list_insert(target, entry);
}

/**
 * list_del_init - deletes entry from list and reinitialize it.
 * @entry: the element to delete from the list.
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
 * @last: last entry to move, can be the same as first
 *
 * Move all entries between @first and including @last before @head.
 * All three entries must belong to the same linked list.
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
 * list_is_first -- tests whether @list is the first entry in list @head
 * @head: the head of the list
 * @link: the entry to test
 */
static inline int list_is_first(const struct list *head,
                                const struct list *link)
{
    return link->prev == head;
}

/**
 * list_is_last - tests whether @list is the last entry in list @head
 * @head: the head of the list
 * @link: the entry to test
 */
static inline int list_is_last(const struct list *head,
                               const struct list *link)
{
    return link->next == head;
}

/**
 * list_is_head - tests whether @list is the list @head
 * @head: the head of the list
 * @link: the entry to test
 */
static inline int list_is_head(const struct list *head,
                               const struct list *link)
{
    return link == head;
}

/**
 * list_empty - tests whether a list is empty
 * @head: the list to test.
 */
static inline int list_empty(const struct list *head)
{
    return head->next == head;
}

/**
 * list_del_init_careful - deletes entry from list and reinitialize it.
 * @entry: the element to delete from the list.
 *
 * This is the same as list_del_init(), except designed to be used
 * together with list_empty_careful() in a way to guarantee ordering
 * of other memory operations.
 *
 * Any memory operations done before a list_del_init_careful() are
 * guaranteed to be visible after a list_empty_careful() test.
 */
static inline void list_del_init_careful(struct list *entry)
{
    __list_del_entry(entry);
    entry->prev = entry;
    entry->next = entry;
}

/**
 * list_empty_careful - tests whether a list is empty and not being modified
 * @head: the list to test
 *
 * Description:
 * tests whether a list is empty _and_ checks that no other CPU might be
 * in the process of modifying either member (next or prev)
 *
 * NOTE: using list_empty_careful() without synchronization
 * can only be safe if the only activity that can happen
 * to the list entry is list_del_init(). Eg. it cannot be used
 * if another CPU could re-list_add() it.
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
 * list_rotate_to_front() - Rotate list to specific item.
 * @head: The head of the list.
 * @link: The desired new front of the list.
 *
 * Rotates list so that @link becomes the new front of the list.
 */
static inline void list_rotate_to_front(struct list *head,
                                        struct list *link)
{
    /*
     * Deletes the list head from the list denoted by @head and
     * places it as the tail of @link, this effectively rotates the
     * list so that @link is at the front.
     */
    list_move_tail(head, link);
}

/**
 * list_is_singular - tests whether a list has just one entry.
 * @head: the list to test.
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
 * @head: a list with entries
 * @link: a new list to add all removed entries
 * @entry: an entry within head, could be the head itself
 *	and if so we won't cut the list
 *
 * This helper moves the initial part of @head, up to and
 * including @entry, from @head to @link. You should
 * pass on @entry an element you know is on @head. @link
 * should be an empty list or a list you do not care about
 * losing its data.
 *
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
 * @head: a list with entries
 * @link: a new list to add all removed entries
 * @entry: an entry within head, could be the head itself
 *
 * This helper moves the initial part of @head, up to but
 * excluding @entry, from @head to @link.  You should pass
 * in @entry an element you know is on @head.  @link should
 * be an empty list or a list you do not care about losing
 * its data.
 * If @entry == @head, all entries on @head are moved to
 * @link.
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
 * list_splice - join two lists, this is designed for stacks
 * @head: the place to add it in the first list.
 * @link: the new list to add.
 */
static inline void list_splice(struct list *head,
                               const struct list *link)
{
    if (!list_empty(link))
        __list_splice(link, head, head->next);
}

/**
 * list_splice_tail - join two lists, each list being a queue
 * @head: the place to add it in the first list.
 * @link: the new list to add.
 */
static inline void list_splice_tail(struct list *head,
                                    struct list *link)
{
    if (!list_empty(link))
        __list_splice(link, head->prev, head);
}

/**
 * list_splice_init - join two lists and reinitialise the emptied list.
 * @head: the place to add it in the first list.
 * @link: the new list to add.
 *
 * The list at @link is reinitialised
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
 * @head: the place to add it in the first list.
 * @link: the new list to add.
 *
 * Each of the lists is a queue.
 * The list at @link is reinitialised
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
 * @ptr:	the &struct list_head pointer.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the list_head within the struct.
 */
#define list_entry(ptr, type, member) \
    container_of(ptr, type, member)

/**
 * list_first_entry - get the first element from a list
 * @ptr:	the list head to take the element from.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the list_head within the struct.
 *
 * Note, that list is expected to be not empty.
 */
#define list_first_entry(ptr, type, member) \
    list_entry((ptr)->next, type, member)

/**
 * list_last_entry - get the last element from a list
 * @ptr:	the list head to take the element from.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the list_head within the struct.
 *
 * Note, that list is expected to be not empty.
 */
#define list_last_entry(ptr, type, member) \
    list_entry((ptr)->prev, type, member)

/**
 * list_first_entry_or_null - get the first element from a list
 * @ptr:	the list head to take the element from.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the list_head within the struct.
 *
 * Note that if the list is empty, it returns NULL.
 */
#define list_first_entry_or_null(ptr, type, member) ({ \
    struct list *head__ = (ptr); \
    struct list *pos__ = head__->next; \
    pos__ != head__ ? list_entry(pos__, type, member) : NULL; \
})

/**
 * list_first_entry_or_null_acquire - get the first element from a list with barrier
 * @ptr:	the list head to take the element from.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the list_head within the struct.
 *
 * Note that if the list is empty, it returns NULL.
 */
#define list_first_entry_or_null_acquire(ptr, type, member) ({ \
    struct list *head__ = (ptr); \
    struct list *pos__ = head__->next; \
    pos__ != head__ ? list_entry(pos__, type, member) : NULL; \
})

/**
 * list_last_entry_or_null - get the last element from a list
 * @ptr:	the list head to take the element from.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the list_head within the struct.
 *
 * Note that if the list is empty, it returns NULL.
 */
#define list_last_entry_or_null(ptr, type, member) ({ \
    struct list *head__ = (ptr); \
    struct list *pos__ = head__->prev; \
    pos__ != head__ ? list_entry(pos__, type, member) : NULL; \
})

/**
 * list_next_entry - get the next element in list
 * @pos:	the type * to cursor
 * @member:	the name of the list_head within the struct.
 */
#define list_next_entry(pos, member) \
    list_entry((pos)->member.next, typeof(*(pos)), member)

/**
 * list_next_entry_circular - get the next element in list
 * @pos:	the type * to cursor.
 * @head:	the list head to take the element from.
 * @member:	the name of the list_head within the struct.
 *
 * Wraparound if pos is the last element (return the first element).
 * Note, that list is expected to be not empty.
 */
#define list_next_entry_circular(pos, head, member) \
    (list_is_last(&(pos)->member, head) ? \
    list_first_entry(head, typeof(*(pos)), member) : list_next_entry(pos, member))

/**
 * list_prev_entry - get the prev element in list
 * @pos:	the type * to cursor
 * @member:	the name of the list_head within the struct.
 */
#define list_prev_entry(pos, member) \
    list_entry((pos)->member.prev, typeof(*(pos)), member)

/**
 * list_prev_entry_circular - get the prev element in list
 * @pos:	the type * to cursor.
 * @head:	the list head to take the element from.
 * @member:	the name of the list_head within the struct.
 *
 * Wraparound if pos is the first element (return the last element).
 * Note, that list is expected to be not empty.
 */
#define list_prev_entry_circular(pos, head, member) \
    (list_is_first(&(pos)->member, head) ? \
    list_last_entry(head, typeof(*(pos)), member) : list_prev_entry(pos, member))

/* ========== List Iteration Macros ========== */

/**
 * list_for_each	-	iterate over a list
 * @pos:	the &struct list_head to use as a loop cursor.
 * @head:	the head for your list.
 */
#define list_for_each(pos, head) \
    for (pos = (head)->next; !list_is_head(pos, (head)); pos = pos->next)

/**
 * list_for_each_continue - continue iteration over a list
 * @pos:	the &struct list_head to use as a loop cursor.
 * @head:	the head for your list.
 *
 * Continue to iterate over a list, continuing after the current position.
 */
#define list_for_each_continue(pos, head) \
    for (pos = pos->next; !list_is_head(pos, (head)); pos = pos->next)

/**
 * list_for_each_prev	-	iterate over a list backwards
 * @pos:	the &struct list_head to use as a loop cursor.
 * @head:	the head for your list.
 */
#define list_for_each_prev(pos, head) \
    for (pos = (head)->prev; !list_is_head(pos, (head)); pos = pos->prev)

/**
 * list_for_each_safe - iterate over a list safe against removal of list entry
 * @pos:	the &struct list_head to use as a loop cursor.
 * @n:		another &struct list_head to use as temporary storage
 * @head:	the head for your list.
 */
#define list_for_each_safe(pos, n, head) \
    for (pos = (head)->next, n = pos->next; \
         !list_is_head(pos, (head)); \
         pos = n, n = pos->next)

/**
 * list_for_each_prev_safe - iterate over a list backwards safe against removal of list entry
 * @pos:	the &struct list_head to use as a loop cursor.
 * @n:		another &struct list_head to use as temporary storage
 * @head:	the head for your list.
 */
#define list_for_each_prev_safe(pos, n, head) \
    for (pos = (head)->prev, n = pos->prev; \
         !list_is_head(pos, (head)); \
         pos = n, n = pos->prev)

/**
 * list_count_nodes - count nodes in the list
 * @head:	the head for your list.
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
 * list_entry_is_head - test if the entry points to the head of the list
 * @pos:	the type * to cursor
 * @head:	the head for your list.
 * @member:	the name of the list_head within the struct.
 */
#define list_entry_is_head(pos, head, member) \
    list_is_head(&pos->member, (head))

/**
 * list_for_each_entry	-	iterate over list of given type
 * @pos:	the type * to use as a loop cursor.
 * @head:	the head for your list.
 * @member:	the name of the list_head within the struct.
 */
#define list_for_each_entry(pos, head, member) \
    for (pos = list_first_entry(head, typeof(*pos), member); \
         !list_entry_is_head(pos, head, member); \
         pos = list_next_entry(pos, member))

/**
 * list_for_each_entry_reverse - iterate backwards over list of given type.
 * @pos:	the type * to use as a loop cursor.
 * @head:	the head for your list.
 * @member:	the name of the list_head within the struct.
 */
#define list_for_each_entry_reverse(pos, head, member) \
    for (pos = list_last_entry(head, typeof(*pos), member); \
         !list_entry_is_head(pos, head, member); \
         pos = list_prev_entry(pos, member))

/**
 * list_prepare_entry - prepare a pos entry for use in list_for_each_entry_continue()
 * @pos:	the type * to use as a start point
 * @head:	the head of the list
 * @member:	the name of the list_head within the struct.
 *
 * Prepares a pos entry for use as a start point in list_for_each_entry_continue().
 */
#define list_prepare_entry(pos, head, member) \
    ((pos) ? : list_entry(head, typeof(*pos), member))

/**
 * list_for_each_entry_continue - continue iteration over list of given type
 * @pos:	the type * to use as a loop cursor.
 * @head:	the head for your list.
 * @member:	the name of the list_head within the struct.
 *
 * Continue to iterate over list of given type, continuing after
 * the current position.
 */
#define list_for_each_entry_continue(pos, head, member) \
    for (pos = list_next_entry(pos, member); \
         !list_entry_is_head(pos, head, member); \
         pos = list_next_entry(pos, member))

/**
 * list_for_each_entry_continue_reverse - iterate backwards from the given point
 * @pos:	the type * to use as a loop cursor.
 * @head:	the head for your list.
 * @member:	the name of the list_head within the struct.
 *
 * Start to iterate over list of given type backwards, continuing after
 * the current position.
 */
#define list_for_each_entry_continue_reverse(pos, head, member) \
    for (pos = list_prev_entry(pos, member); \
         !list_entry_is_head(pos, head, member); \
         pos = list_prev_entry(pos, member))

/**
 * list_for_each_entry_from - iterate over list of given type from the current point
 * @pos:	the type * to use as a loop cursor.
 * @head:	the head for your list.
 * @member:	the name of the list_head within the struct.
 *
 * Iterate over list of given type, continuing from current position.
 */
#define list_for_each_entry_from(pos, head, member) \
    for (; !list_entry_is_head(pos, head, member); \
         pos = list_next_entry(pos, member))

/**
 * list_for_each_entry_from_reverse - iterate backwards over list of given type
 *                                    from the current point
 * @pos:	the type * to use as a loop cursor.
 * @head:	the head for your list.
 * @member:	the name of the list_head within the struct.
 *
 * Iterate backwards over list of given type, continuing from current position.
 */
#define list_for_each_entry_from_reverse(pos, head, member) \
    for (; !list_entry_is_head(pos, head, member); \
         pos = list_prev_entry(pos, member))

/**
 * list_for_each_entry_safe - iterate over list of given type safe against removal of list entry
 * @pos:	the type * to use as a loop cursor.
 * @n:		another type * to use as temporary storage
 * @head:	the head for your list.
 * @member:	the name of the list_head within the struct.
 */
#define list_for_each_entry_safe(pos, n, head, member) \
    for (pos = list_first_entry(head, typeof(*pos), member), \
            n = list_next_entry(pos, member); \
         !list_entry_is_head(pos, head, member); \
         pos = n, n = list_next_entry(n, member))

/**
 * list_for_each_entry_safe_continue - continue list iteration safe against removal
 * @pos:	the type * to use as a loop cursor.
 * @n:		another type * to use as temporary storage
 * @head:	the head for your list.
 * @member:	the name of the list_head within the struct.
 *
 * Iterate over list of given type, continuing after current point,
 * safe against removal of list entry.
 */
#define list_for_each_entry_safe_continue(pos, n, head, member) \
    for (pos = list_next_entry(pos, member), \
            n = list_next_entry(pos, member); \
         !list_entry_is_head(pos, head, member); \
         pos = n, n = list_next_entry(n, member))

/**
 * list_for_each_entry_safe_from - iterate over list from current point safe against removal
 * @pos:	the type * to use as a loop cursor.
 * @n:		another type * to use as temporary storage
 * @head:	the head for your list.
 * @member:	the name of the list_head within the struct.
 *
 * Iterate over list of given type from current point, safe against
 * removal of list entry.
 */
#define list_for_each_entry_safe_from(pos, n, head, member) \
    for (n = list_next_entry(pos, member); \
         !list_entry_is_head(pos, head, member); \
         pos = n, n = list_next_entry(n, member))

/**
 * list_for_each_entry_safe_reverse - iterate backwards over list safe against removal
 * @pos:	the type * to use as a loop cursor.
 * @n:		another type * to use as temporary storage
 * @head:	the head for your list.
 * @member:	the name of the list_head within the struct.
 *
 * Iterate backwards over list of given type, safe against removal
 * of list entry.
 */
#define list_for_each_entry_safe_reverse(pos, n, head, member) \
    for (pos = list_last_entry(head, typeof(*pos), member), \
            n = list_prev_entry(pos, member); \
         !list_entry_is_head(pos, head, member); \
         pos = n, n = list_prev_entry(n, member))

/**
 * list_safe_reset_next - reset a stale list_for_each_entry_safe loop
 * @pos:	the loop cursor used in the list_for_each_entry_safe loop
 * @n:		temporary storage used in list_for_each_entry_safe
 * @member:	the name of the list_head within the struct.
 *
 * list_safe_reset_next is not safe to use in general if the list may be
 * modified concurrently (eg. the lock is dropped in the loop body). An
 * exception to this is if the cursor element (pos) is pinned in the list,
 * and list_safe_reset_next is called after re-taking the lock and before
 * completing the current iteration of the loop body.
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
 * hlist_unhashed - Has node been removed from list and reinitialized?
 * @h: Node to be checked
 *
 * Not that not all removal functions will leave a node in unhashed
 * state.  For example, hlist_nulls_del_init_rcu() does leave the
 * node in unhashed state, but hlist_nulls_del() does not.
 */
static inline int hlist_unhashed(const struct hlist_node *h)
{
    return !h->pprev;
}

/**
 * hlist_unhashed_lockless - Version of hlist_unhashed for lockless use
 * @h: Node to be checked
 *
 * This variant of hlist_unhashed() must be used in lockless contexts
 * to avoid potential load-tearing.  The READ_ONCE() is paired with the
 * various WRITE_ONCE() in hlist helpers that are defined below.
 */
static inline int hlist_unhashed_lockless(const struct hlist_node *h)
{
    return !h->pprev;
}

/**
 * hlist_empty - Is the specified hlist_head structure an empty hlist?
 * @h: Structure to check.
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
 * hlist_del - Delete the specified hlist_node from its list
 * @n: Node to delete.
 *
 * Note that this function leaves the node in hashed state.  Use
 * hlist_del_init() or similar instead to unhash @n.
 */
static inline void hlist_del(struct hlist_node *n)
{
    __hlist_del(n);
    n->next = NULL;
    n->pprev = NULL;
}

/**
 * hlist_del_init - Delete the specified hlist_node from its list and initialize
 * @n: Node to delete.
 *
 * Note that this function leaves the node in unhashed state.
 */
static inline void hlist_del_init(struct hlist_node *n)
{
    if (!hlist_unhashed(n)) {
        __hlist_del(n);
        INIT_HLIST_NODE(n);
    }
}

/**
 * hlist_add_head - add a new entry at the beginning of the hlist
 * @n: new entry to be added
 * @h: hlist head to add it after
 *
 * Insert a new entry after the specified head.
 * This is good for implementing stacks.
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
 * hlist_add_before - add a new entry before the one specified
 * @n: new entry to be added
 * @next: hlist node to add it before, which must be non-NULL
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
 * hlist_add_behind - add a new entry after the one specified
 * @n: new entry to be added
 * @prev: hlist node to add it after, which must be non-NULL
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
 * hlist_add_fake - create a fake hlist consisting of a single headless node
 * @n: Node to make a fake list out of
 *
 * This makes @n appear to be its own predecessor on a headless hlist.
 * The point of this is to allow things like hlist_del() to work correctly
 * in cases where there is no list.
 */
static inline void hlist_add_fake(struct hlist_node *n)
{
    n->pprev = &n->next;
}

/**
 * hlist_fake: Is this node a fake hlist?
 * @h: Node to check for being a self-referential fake hlist.
 */
static inline bool hlist_fake(struct hlist_node *h)
{
    return h->pprev == &h->next;
}

/**
 * hlist_is_singular_node - is node the only element of the specified hlist?
 * @n: Node to check for singularity.
 * @h: Header for potentially singular list.
 *
 * Check whether the node is the only node of the head without
 * accessing head, thus avoiding unnecessary cache misses.
 */
static inline bool hlist_is_singular_node(struct hlist_node *n,
                                          struct hlist_head *h)
{
    return !n->next && n->pprev == &h->first;
}

/**
 * hlist_move_list - Move an hlist
 * @old: hlist_head for old list.
 * @link: hlist_head for new list.
 *
 * Move a list from one list head to another. Fixup the pprev
 * reference of the first entry if it exists.
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
 * hlist_splice_init() - move all entries from one list to another
 * @from: hlist_head from which entries will be moved
 * @last: last entry on the @from list
 * @to:   hlist_head to which entries will be moved
 *
 * @to can be empty, @from must contain at least @last.
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
 * hlist_for_each_entry	- iterate over list of given type
 * @pos:	the type * to use as a loop cursor.
 * @head:	the head for your list.
 * @member:	the name of the hlist_node within the struct.
 */
#define hlist_for_each_entry(pos, head, member) \
    for (pos = hlist_entry_safe((head)->first, typeof(*(pos)), member); \
         pos; \
         pos = hlist_entry_safe((pos)->member.next, typeof(*(pos)), member))

/**
 * hlist_for_each_entry_continue - iterate over a hlist continuing after current point
 * @pos:	the type * to use as a loop cursor.
 * @member:	the name of the hlist_node within the struct.
 */
#define hlist_for_each_entry_continue(pos, member) \
    for (pos = hlist_entry_safe((pos)->member.next, typeof(*(pos)), member); \
         pos; \
         pos = hlist_entry_safe((pos)->member.next, typeof(*(pos)), member))

/**
 * hlist_for_each_entry_from - iterate over a hlist continuing from current point
 * @pos:	the type * to use as a loop cursor.
 * @member:	the name of the hlist_node within the struct.
 */
#define hlist_for_each_entry_from(pos, member) \
    for (; pos; \
         pos = hlist_entry_safe((pos)->member.next, typeof(*(pos)), member))

/**
 * hlist_for_each_entry_safe - iterate over list of given type safe against removal of list entry
 * @pos:	the type * to use as a loop cursor.
 * @n:		a &struct hlist_node to use as temporary storage
 * @head:	the head for your list.
 * @member:	the name of the hlist_node within the struct.
 */
#define hlist_for_each_entry_safe(pos, n, head, member) \
    for (pos = hlist_entry_safe((head)->first, typeof(*pos), member); \
         pos && ({ n = pos->member.next; 1; }); \
         pos = hlist_entry_safe(n, typeof(*pos), member))

/**
 * hlist_count_nodes - count nodes in the hlist
 * @head:	the head for your hlist.
 */
static inline size_t hlist_count_nodes(struct hlist_head *head)
{
    struct hlist_node *pos;
    size_t count = 0;

    hlist_for_each(pos, head)
        count++;

    return count;
}

/* ============================================================================
 * Single-linked list macros
 * ============================================================================ */

/**
 * Single-linked list node structure
 *
 * Usage in your structure:
 *   struct node {
 *       int data;
 *       struct node *next;
 *   };
 */

/* Initialize single-linked list head */
#define slist_init(head) \
    (head)->next = NULL

/* Check if single-linked list is empty */
#define slist_empty(head) \
    ((head)->next == NULL)

/* Add node at the beginning of single-linked list */
#define slist_add_head(head, node) \
    (node)->next = (head)->next; \
    (head)->next = (node)

/* Add node at the end of single-linked list */
#define slist_add_tail(head, node) \
    typeof(head) __p = (head); \
    while (__p->next != NULL) { \
    __p = __p->next; \
    } \
    __p->next = (node); \
    (node)->next = NULL

/* Insert node after prev node */
#define slist_insert_after(prev, node) \
    (node)->next = (prev)->next; \
    (prev)->next = (node);

/* Remove node from single-linked list (requires prev pointer) */
#define slist_remove(prev, node) \
    (prev)->next = (node)->next; \
    (node)->next = NULL

/**
 * slist_for_each - iterate over single-linked list
 * @head: the head pointer
 * @pos: the loop cursor (pointer to current node)
 *
 * Usage:
 *   struct node *pos;
 *   slist_for_each(&head, pos) {
 *       // use pos->data
 *   }
 */
#define slist_for_each(head, pos) \
    for ((pos) = (head)->next; (pos) != NULL; (pos) = (pos)->next)

/**
 * slist_for_each_prev - iterate over single-linked list with prev pointer
 * @head: the head pointer
 * @prev: pointer to previous node
 * @pos: the loop cursor (pointer to current node)
 *
 * Usage:
 *   struct node *prev, *pos;
 *   slist_for_each_prev(&head, prev, pos) {
 *       // can use prev to remove pos
 *       if (condition) {
 *           slist_remove(prev, pos);
 *           pos = prev;  // reset pos
 *       }
 *   }
 */
#define slist_for_each_prev(head, prev, pos) \
    for ((prev) = (head), (pos) = (head)->next; \
         (pos) != NULL; \
         (prev) = (pos), (pos) = (pos)->next)

/**
 * slist_for_each_safe - iterate over single-linked list safe against removal
 * @head: the head pointer
 * @prev: pointer to previous node
 * @pos: the loop cursor (pointer to current node)
 * @n: temporary storage for next pointer
 *
 * Usage:
 *   struct node *prev, *pos, *n;
 *   slist_for_each_safe(&head, prev, pos, n) {
 *       if (should_remove) {
 *           slist_remove(prev, pos);
 *           free(pos);
 *           pos = prev;  // important: reset pos to prev
 *       }
 *   }
 */
#define slist_for_each_safe(head, prev, pos, n) \
    for ((prev) = (head), (pos) = (head)->next, (n) = (pos) ? (pos)->next : NULL; \
         (pos) != NULL; \
         (prev) = (pos), (pos) = (n), (n) = (pos) ? (pos)->next : NULL)

/**
 * slist_entry - get the container structure from slist node pointer
 * @ptr: pointer to the next field
 * @type: the type of the container struct
 * @member: the name of the next field within the struct
 *
 * Usage:
 *   struct my_struct {
 *       int data;
 *       struct my_struct *next;
 *   };
 *   struct my_struct *obj = slist_entry(ptr, struct my_struct, next);
 */
#define slist_entry(ptr, type, member) \
    container_of(ptr, type, member)

/**
 * slist_for_each_entry - iterate over single-linked list of given type
 * @head: the head pointer
 * @pos: the loop cursor (pointer to container structure)
 * @member: the name of the next field within the struct
 *
 * Usage:
 *   struct my_struct {
 *       int data;
 *       struct my_struct *next;
 *   };
 *   struct my_struct head = {0, NULL};
 *   struct my_struct *obj;
 *   slist_for_each_entry(&head, obj, next) {
 *       printf("%d\n", obj->data);
 *   }
 */
#define slist_for_each_entry(head, pos, member) \
    for ((pos) = (head)->member; \
         (pos) != NULL; \
         (pos) = (pos)->member)

/**
 * slist_for_each_entry_safe - iterate over single-linked list of given type safe against removal
 * @head: the head pointer (container structure with next field)
 * @pos: the loop cursor (pointer to container structure)
 * @n: temporary pointer to next node
 * @member: the name of the next field within the struct
 *
 * Usage:
 *   struct my_struct head = {0, NULL};
 *   struct my_struct *obj, *n;
 *   slist_for_each_entry_safe(&head, obj, n, next) {
 *       if (should_remove) {
 *           // safe to remove obj
 *       }
 *   }
 */
#define slist_for_each_entry_safe(head, pos, n, member) \
    for ((pos) = (head)->member, (n) = (pos) ? (pos)->member : NULL; \
         (pos) != NULL; \
         (pos) = (n), (n) = (pos) ? (pos)->member : NULL)

/**
 * slist_count_nodes - count nodes in the single-linked list
 * @head: the head pointer
 * @member: the name of the next field
 *
 * Returns the number of nodes in the list
 */
#define slist_count_nodes(head, member) ({ \
    size_t __count = 0; \
    typeof(head) __pos; \
    for (__pos = (head)->member; __pos != NULL; __pos = __pos->member) \
        __count++; \
    __count; \
})

/**
 * Pointer head version - for when the list head is just a pointer
 *
 * Example:
 *   struct node {
 *       int data;
 *       struct node *next;
 *   };
 *   struct node *list_head = NULL;  // Just a pointer
 */

/**
 * slist_ptr_add_head - add node at the beginning
 * @head_ptr: pointer to the list head pointer (e.g., &ctx->paused_threads)
 * @node: node to add
 */
#define slist_ptr_add_head(head_ptr, node) \
    (node)->next = *(head_ptr); \
    *(head_ptr) = (node)

/**
 * slist_ptr_add_tail - add node at the end
 * @head_ptr: pointer to the list head pointer
 * @node: node to add
 */
#define slist_ptr_add_tail(head_ptr, node) \
    (node)->next = NULL; \
    if (*(head_ptr) == NULL) { \
        *(head_ptr) = (node); \
    } else { \
        typeof(*(head_ptr)) __p = *(head_ptr); \
        while (__p->next != NULL) { \
            __p = __p->next; \
        } \
        __p->next = (node); \
    }

/**
 * slist_ptr_insert_after - insert node after another node
 * @prev: the node to insert after
 * @node: node to insert
 */
#define slist_ptr_insert_after(prev, node) \
    (node)->next = (prev)->next; \
    (prev)->next = (node)

/**
 * slist_ptr_remove - remove node from list
 * @head_ptr: pointer to the list head pointer
 * @prev: previous node (NULL if removing head)
 * @node: node to remove
 */
#define slist_ptr_remove(head_ptr, prev, node) \
    if ((prev) == NULL) { \
        *(head_ptr) = (node)->next; \
    } else { \
        (prev)->next = (node)->next; \
    } \
    (node)->next = NULL

/**
 * slist_ptr_for_each - iterate over list
 * @head_ptr: pointer to the list head pointer
 * @pos: loop cursor
 */
#define slist_ptr_for_each(head_ptr, pos) \
    for ((pos) = (head_ptr); (pos) != NULL; (pos) = (pos)->next)

/**
 * slist_ptr_for_each_prev - iterate with previous pointer
 * @head_ptr: pointer to the list head pointer
 * @prev: previous node pointer
 * @pos: loop cursor
 */
#define slist_ptr_for_each_prev(head_ptr, prev, pos) \
    for ((prev) = NULL, (pos) = (head_ptr); \
         (pos) != NULL; \
         (prev) = (pos), (pos) = (pos)->next)

/**
 * slist_ptr_for_each_safe - iterate safe against removal
 * @head_ptr: pointer to the list head pointer
 * @prev: previous node pointer
 * @pos: loop cursor
 * @n: temporary next pointer
 */
#define slist_ptr_for_each_safe(head_ptr, prev, pos, n) \
    for ((prev) = NULL, (pos) = (head_ptr), (n) = (pos) ? (pos)->next : NULL; \
         (pos) != NULL; \
         (prev) = (pos), (pos) = (n), (n) = (pos) ? (pos)->next : NULL)

/**
 * slist_ptr_empty - check if list is empty
 * @head_ptr: pointer to the list head pointer
 */
#define slist_ptr_empty(head_ptr) \
    (*(head_ptr) == NULL)

/**
 * slist_ptr_count - count nodes in list
 * @head_ptr: pointer to the list head pointer
 */
#define slist_ptr_count(head_ptr) ({ \
    size_t __count = 0; \
    typeof(*(head_ptr)) __pos; \
    for (__pos = *(head_ptr); __pos != NULL; __pos = __pos->next) \
        __count++; \
    __count; \
})

#ifdef __cplusplus
}
#endif

#endif