#pragma once

struct uk_so_wl_rbtree_phys_page_handle {
    unsigned long phys_address;
    unsigned long mapped_vm_page;
    unsigned long access_count;
};

enum uk_so_wl_rbtree_node_color { uk_so_wl_rbtree_RED, uk_so_wl_rbtree_BLACK };

struct uk_so_wl_rbtree_node {
    struct uk_so_wl_rbtree_phys_page_handle value;
    enum uk_so_wl_rbtree_node_color color;
    struct uk_so_wl_rbtree_node *left;
    struct uk_so_wl_rbtree_node *right;
};

struct uk_so_wl_rbtree {
    struct uk_so_wl_rbtree_node *root;
    unsigned long element_count;
};

void uk_so_wl_rbtree_insert(struct uk_so_wl_rbtree *tree,
                            struct uk_so_wl_rbtree_node *element);
void uk_so_wl_rbtree_remove(struct uk_so_wl_rbtree *tree,
                            struct uk_so_wl_rbtree_node *element);
struct uk_so_wl_rbtree_phys_page_handle uk_so_wl_rbtree_pop_minimum(
    struct uk_so_wl_rbtree *tree);
unsigned long uk_so_wl_rbtree_get_element_count(struct uk_so_wl_rbtree *tree);

void uk_so_wl_rbtree_print_tree(struct uk_so_wl_rbtree *tree);

unsigned long uk_so_wl_rbtree_fast_log2(unsigned long n);

void uk_so_wl_rbtree_print_level(struct uk_so_wl_rbtree *tree,
                                 unsigned int level, int dist,
                                 struct uk_so_wl_rbtree_node *root);
