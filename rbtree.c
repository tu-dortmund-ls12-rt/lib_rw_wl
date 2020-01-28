#include <stdio.h>
#include <uk_lib_so_wl/rbtree.h>

#define __WL_CODE __attribute((section(".wl_text")))
#define __WL_DATA __attribute((section(".wl_data")))

int __WL_CODE
uk_so_wl_rbtree_compare(struct uk_so_wl_rbtree_phys_page_handle node1,
                        struct uk_so_wl_rbtree_phys_page_handle node2) {
    if (node2.access_count != node1.access_count) {
        return node1.access_count - node2.access_count;
    }
    return node1.phys_address -
           node2.phys_address;  // Just to keep an absolute ordner in the tree
}

unsigned int __WL_CODE
uk_so_wl_rbtree_equals(struct uk_so_wl_rbtree_phys_page_handle node1,
                       struct uk_so_wl_rbtree_phys_page_handle node2) {
    return node1.access_count == node2.access_count &&
           node1.phys_address == node2.phys_address;
}

unsigned long __WL_CODE uk_so_wl_rbtree_fast_log2(unsigned long n) {
    static const int table[64] = {
        0,  58, 1,  59, 47, 53, 2,  60, 39, 48, 27, 54, 33, 42, 3,  61,
        51, 37, 40, 49, 18, 28, 20, 55, 30, 34, 11, 43, 14, 22, 4,  62,
        57, 46, 52, 38, 26, 32, 41, 50, 36, 17, 19, 29, 10, 13, 21, 56,
        45, 25, 31, 35, 16, 9,  12, 44, 24, 15, 8,  23, 7,  6,  5,  63};

    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n |= n >> 32;

    return table[(n * 0x03f6eaf2cd271461) >> 58];
}

void __WL_CODE uk_so_wl_rbtree_insert(struct uk_so_wl_rbtree *tree,
                                      struct uk_so_wl_rbtree_node *element) {
    // Maximum height
    unsigned long max_height =
        2 * uk_so_wl_rbtree_fast_log2(tree->element_count + 2) - 2;
    max_height++;
    // List of parent pointers, on the way to the target
    struct uk_so_wl_rbtree_node *parents[max_height];
    struct uk_so_wl_rbtree_node **pParent = 0;

    // search for the place to insert
    unsigned int pidx = 0;
    struct uk_so_wl_rbtree_node *act = tree->root;
    while (act != 0) {
        parents[pidx++] = act;
        pParent = &parents[pidx - 1];
        if (uk_so_wl_rbtree_compare(element->value, act->value) > 0) {
            act = act->right;
        } else {
            act = act->left;
        }
        if (pidx > max_height) {
            printf("RBTree size overflowed");
            while (1)
                ;
        }
    }

    element->color = uk_so_wl_rbtree_RED;
    element->right = 0;
    element->left = 0;

    tree->element_count++;
    // If the root was empty, pParent is 0
    if (pParent >= &parents[0]) {
        // Node to insert the new node at
        struct uk_so_wl_rbtree_node *p = *pParent;
        if (uk_so_wl_rbtree_compare(element->value, p->value) > 0) {
            p->right = element;
        } else {
            p->left = element;
        }

        /**
         * Now rebalance the tree, distinguish between 6 cases
         */
        do {
            p = *pParent;
            // Case 1: Parent node was black, no need to rebalance
            if (p->color == uk_so_wl_rbtree_BLACK) {
                return;
            }
            // Case 2: Parent is not black, but the root
            if (--pParent < &parents[0]) {
                p->color = uk_so_wl_rbtree_BLACK;
                return;
            }
            // Case 3: Parent is red and not the root, has none or a black uncle
            // and is inner child
            struct uk_so_wl_rbtree_node *g = *pParent;
            unsigned int pDirection = p == g->left;
            struct uk_so_wl_rbtree_node *u = pDirection ? g->right : g->left;
            if (u == 0 || u->color == uk_so_wl_rbtree_BLACK) {
                if (element != (pDirection ? p->left : p->right)) {
                    if (pDirection) {
                        p->right =
                            (pDirection ? element->left : element->right);
                        element->left = p;
                        g->left = element;
                    } else {
                        p->left = (pDirection ? element->left : element->right);
                        element->right = p;
                        g->right = element;
                    }
                    // (pDirection ? p->right : p->left) =
                    //     (pDirection ? element->left : element->right);
                    // (pDirection ? element->left : element->right) = p;
                    // (pDirection ? g->left : g->right) = element;
                    u = p;
                    p = element;
                    element = u;
                }
                // Case 4: ... is outer child
                if (pDirection) {
                    g->left = (pDirection ? p->right : p->left);
                    p->right = g;
                } else {
                    g->right = (pDirection ? p->right : p->left);
                    p->left = g;
                }
                // (pDirection ? g->left : g->right) =
                //     (pDirection ? p->right : p->left);
                // (pDirection ? p->right : p->left) = g;
                p->color = uk_so_wl_rbtree_BLACK;
                g->color = uk_so_wl_rbtree_RED;
                if (--pParent < &parents[0]) {
                    tree->root = p;
                } else {
                    element = *pParent;
                    if (g == element->left) {
                        element->left = p;
                    } else {
                        element->right = p;
                    }
                    // (g == element->left ? element->left : element->right) =
                    // p;
                }
                return;
            }
            // Case 5: Uncle and Parent are red
            p->color = uk_so_wl_rbtree_BLACK;
            u->color = uk_so_wl_rbtree_BLACK;
            g->color = uk_so_wl_rbtree_RED;
            element = g;
        } while (--pParent >= &parents[0]);
    }
    // Replace root
    tree->root = element;
}

void __WL_CODE uk_so_wl_rbtree_remove(struct uk_so_wl_rbtree *tree,
                                      struct uk_so_wl_rbtree_node *element) {
    // Find the searched node
    // Maximum height
    unsigned long max_height =
        2 * uk_so_wl_rbtree_fast_log2(tree->element_count + 2) - 2;
    max_height++;
    // List of parent pointers, on the way to the target
    struct uk_so_wl_rbtree_node *parents[max_height];
    struct uk_so_wl_rbtree_node **pParent = 0;

    // search for the matching node
    unsigned int pidx = 0;
    struct uk_so_wl_rbtree_node *act = tree->root;
    while (act != 0) {
        parents[pidx++] = act;
        pParent = &parents[pidx - 1];
        if (uk_so_wl_rbtree_equals(act->value, element->value)) {
            break;
        } else {
            if (uk_so_wl_rbtree_compare(element->value, act->value) > 0) {
                act = act->right;
            } else {
                act = act->left;
            }
        }
    }

    if (pidx == 0) {
        return;
    }

    struct uk_so_wl_rbtree_node *r = parents[pidx - 1];

    if (!uk_so_wl_rbtree_equals(r->value, element->value)) {
        return;
    }

    /**
     * Node has 2 childs, swap the content with the minimum of the right subtree
     * (will not destroy the ordering (cause the node will be deleted) nor the
     * rb balance, cause only the content will be swapped)
     */
    if (r->right != 0 && r->left != 0) {
        act = r->right;
        while (act != 0) {
            parents[pidx++] = act;
            pParent = &parents[pidx - 1];
            act = act->left;
        }

        struct uk_so_wl_rbtree_node *swap = parents[pidx - 1];
        r->value = swap->value;
        r = swap;
    }

    tree->element_count--;
    pParent--;

    // Simple cases: r is red (has no child then)
    if (r->color == uk_so_wl_rbtree_RED) {
        // Check if it was the root
        if (pParent < &parents[0]) {
            tree->root = 0;
        } else {
            if ((*pParent)->left == r) {
                (*pParent)->left = 0;
            } else {
                (*pParent)->right = 0;
            }
            // ((*pParent)->left == r ? (*pParent)->left : (*pParent)->right) =
            // 0;
        }
    }
    // r is black, and has a red child
    else if (r->right != 0 || r->left != 0) {
        unsigned int pDirection = (*pParent)->left == r;
        // log("Deleting a black node with a " <<
        // (r->right->color==RED?"RED":"BLACK") << " right child");
        // might be the root
        if (pParent < &parents[0]) {
            tree->root = r->right != 0 ? r->right : r->left;
        } else {
            if (pDirection) {
                (*pParent)->left = (r->right != 0 ? r->right : r->left);
            } else {
                (*pParent)->right = (r->right != 0 ? r->right : r->left);
            }
            // (pDirection ? (*pParent)->left : (*pParent)->right) =
            //     (r->right != 0 ? r->right : r->left);
        }
        if (r->right != 0) {
            r->right->color = uk_so_wl_rbtree_BLACK;
        } else {
            r->left->color = uk_so_wl_rbtree_BLACK;
        }
        // (r->right != 0 ? r->right : r->left)->color = BLACK;
    }
    // r is black, has no child
    else {
        struct uk_so_wl_rbtree_node *n = 0;
        struct uk_so_wl_rbtree_node *p, *s, *c;
        unsigned int pDirection;

        // Not delete root
        if (pParent >= &parents[0]) {
            p = *pParent;
            pDirection = p->left == r;
            if (pDirection) {
                p->left = 0;
            } else {
                p->right = 0;
            }
            // (pDirection ? p->left : p->right) = 0;
            s = (pDirection ? p->right : p->left);
            // log("Before s check");
            // log("Node " << found_node << " had sibling " << s->value);
            if (s->color == uk_so_wl_rbtree_RED) {
                // log("L2");
                goto del_L2;
                // log("DL2");
            }
            if ((pDirection ? s->right : s->left) != 0) {
                goto del_L5;
            }
            if ((pDirection ? s->left : s->right) != 0) {
                goto del_L4;
            }
            if (p->color == uk_so_wl_rbtree_RED) {
                goto del_L3;
            }

            s->color = uk_so_wl_rbtree_RED;
            n = p;
            while (--pParent >= &parents[0]) {
                p = *pParent;
                pDirection = n == p->left;
                s = (pDirection ? p->right : p->left);
                if (s->color == uk_so_wl_rbtree_RED) {
                    goto del_L2;
                }
                if ((pDirection ? s->right : s->left)->color ==
                    uk_so_wl_rbtree_RED) {
                    goto del_L5;
                }
                if ((pDirection ? s->left : s->right)->color ==
                    uk_so_wl_rbtree_RED) {
                    goto del_L4;
                }
                if (p->color == uk_so_wl_rbtree_RED) {
                    goto del_L3;
                }

                s->color = uk_so_wl_rbtree_RED;
                n = p;
            }
        }

        tree->root = n;
        return;

    del_L2:
        s->color = uk_so_wl_rbtree_BLACK;
        p->color = uk_so_wl_rbtree_RED;
        c = (pDirection ? s->left : s->right);
        // log("Siblings " << (pDirection?"left":"right") << " child is " << c);
        if (pDirection) {
            p->right = c;
            s->left = p;
        } else {
            p->left = c;
            s->right = p;
        }
        // (pDirection ? p->right : p->left) = c;
        // (pDirection ? s->left : s->right) = p;
        if (--pParent < &parents[0]) {
            tree->root = s;
        } else {
            struct uk_so_wl_rbtree_node *k = *pParent;
            unsigned int pDirection = p == k->left;
            if (pDirection) {
                k->left = s;
            } else {
                k->right = s;
            }
            // (pDirection ? k->left : k->right) = s;
            // Fix parent list
            pParent++;
            *pParent = s;
            pParent++;
        }
        s = c;
        if ((pDirection ? s->right : s->left) != 0 &&
            (pDirection ? s->right : s->left)->color == uk_so_wl_rbtree_RED) {
            goto del_L5;
        }
        if ((pDirection ? s->left : s->right) != 0 &&
            (pDirection ? s->left : s->right)->color == uk_so_wl_rbtree_RED) {
            goto del_L4;
        }

    del_L3:
        s->color = uk_so_wl_rbtree_RED;
        p->color = uk_so_wl_rbtree_BLACK;
        return;

    del_L4:
        c = (pDirection ? s->left : s->right);
        c->color = uk_so_wl_rbtree_BLACK;
        if (pDirection) {
            s->left = (pDirection ? c->right : c->left);
            c->right = s;
            p->right = c;
        } else {
            s->right = (pDirection ? c->right : c->left);
            c->left = s;
            p->left = c;
        }
        // (pDirection ? s->left : s->right) = (pDirection ? c->right :
        // c->left); (pDirection ? c->right : c->left) = s;
        // (pDirection ? p->right : p->left) = c;
        s->color = uk_so_wl_rbtree_RED;
        s = c;

    del_L5:
        s->color = p->color;
        p->color = uk_so_wl_rbtree_BLACK;
        if (pDirection) {
            p->right = (pDirection ? s->left : s->right);
            s->left = p;
        } else {
            p->left = (pDirection ? s->left : s->right);
            s->right = p;
        }
        // (pDirection ? p->right : p->left) = (pDirection ? s->left :
        // s->right); (pDirection ? s->left : s->right) = p;
        ((pDirection ? s->right : s->left))->color = uk_so_wl_rbtree_BLACK;
        if (--pParent < &parents[0]) {
            tree->root = s;
        } else {
            n = *pParent;
            unsigned int pDirection = p == n->left;
            if (pDirection) {
                n->left = s;
            } else {
                n->right = s;
            }
            // (pDirection ? n->left : n->right) = s;
        }

        return;
    }

    return;
}

struct __WL_CODE uk_so_wl_rbtree_phys_page_handle
uk_so_wl_rbtree_pop_minimum(struct uk_so_wl_rbtree *tree) {
    // Find the smallest node first
    // Maximum height
    unsigned long max_height =
        2 * uk_so_wl_rbtree_fast_log2(tree->element_count + 2) - 2;
    max_height++;
    // List of parent pointers, on the way to the target
    struct uk_so_wl_rbtree_node *parents[max_height];
    struct uk_so_wl_rbtree_node **pParent = 0;

    // search for the smallest node
    unsigned int pidx = 0;
    struct uk_so_wl_rbtree_node *act = tree->root;
    while (act != 0) {
        parents[pidx++] = act;
        pParent = &parents[pidx - 1];
        act = act->left;
    }
    struct uk_so_wl_rbtree_node *r = parents[pidx - 1];
    struct uk_so_wl_rbtree_phys_page_handle found_node = r->value;
    tree->element_count--;
    pParent--;

    // Simple cases: r is red (has no child then)
    if (r->color == uk_so_wl_rbtree_RED) {
        // Check if it was the root
        if (pParent < &parents[0]) {
            tree->root = 0;
        } else {
            (*pParent)->left = 0;
        }
    }
    // r is black, and has a red child
    else if (r->right != 0) {
        // log("Deleting a black node with a " <<
        // (r->right->color==RED?"RED":"BLACK") << " right child");
        // might be the root
        if (pParent < &parents[0]) {
            tree->root = r->right;
        } else {
            (*pParent)->left = r->right;
        }
        r->right->color = uk_so_wl_rbtree_BLACK;
    }
    // r is black, has no child
    else {
        struct uk_so_wl_rbtree_node *n = 0;
        struct uk_so_wl_rbtree_node *p, *s, *c;
        unsigned int pDirection;

        // Not delete root
        if (pParent >= &parents[0]) {
            p = *pParent;
            pDirection = 1;
            p->left = 0;
            s = p->right;
            // log("Before s check");
            // log("Node " << found_node << " had sibling " << s->value);
            if (s->color == uk_so_wl_rbtree_RED) {
                // log("L2");
                goto del_L2;
                // log("DL2");
            }
            if (s->right != 0) {
                goto del_L5;
            }
            if (s->left != 0) {
                goto del_L4;
            }
            if (p->color == uk_so_wl_rbtree_RED) {
                goto del_L3;
            }

            s->color = uk_so_wl_rbtree_RED;
            n = p;
            while (--pParent >= &parents[0]) {
                p = *pParent;
                pDirection = n == p->left;
                s = (pDirection ? p->right : p->left);
                if (s->color == uk_so_wl_rbtree_RED) {
                    goto del_L2;
                }
                if ((pDirection ? s->right : s->left)->color ==
                    uk_so_wl_rbtree_RED) {
                    goto del_L5;
                }
                if ((pDirection ? s->left : s->right)->color ==
                    uk_so_wl_rbtree_RED) {
                    goto del_L4;
                }
                if (p->color == uk_so_wl_rbtree_RED) {
                    goto del_L3;
                }

                s->color = uk_so_wl_rbtree_RED;
                n = p;
            }
        }

        tree->root = n;
        return found_node;

    del_L2:
        s->color = uk_so_wl_rbtree_BLACK;
        p->color = uk_so_wl_rbtree_RED;
        c = (pDirection ? s->left : s->right);
        // log("Siblings " << (pDirection?"left":"right") << " child is " << c);
        if (pDirection) {
            p->right = c;
            s->left = p;
        } else {
            p->left = c;
            s->right = p;
        }
        // (pDirection ? p->right : p->left) = c;
        // (pDirection ? s->left : s->right) = p;
        if (--pParent < &parents[0]) {
            tree->root = s;
        } else {
            struct uk_so_wl_rbtree_node *k = *pParent;
            unsigned int pDirection = p == k->left;
            if (pDirection) {
                k->left = s;
            } else {
                k->right = s;
            }
            // (pDirection ? k->left : k->right) = s;
            // Fix parent list
            pParent++;
            *pParent = s;
            pParent++;
        }
        s = c;
        if ((pDirection ? s->right : s->left) != 0 &&
            (pDirection ? s->right : s->left)->color == uk_so_wl_rbtree_RED) {
            goto del_L5;
        }
        if ((pDirection ? s->left : s->right) != 0 &&
            (pDirection ? s->left : s->right)->color == uk_so_wl_rbtree_RED) {
            goto del_L4;
        }

    del_L3:
        s->color = uk_so_wl_rbtree_RED;
        p->color = uk_so_wl_rbtree_BLACK;
        return found_node;

    del_L4:
        c = (pDirection ? s->left : s->right);
        c->color = uk_so_wl_rbtree_BLACK;
        if (pDirection) {
            s->left = (pDirection ? c->right : c->left);
            c->right = s;
            p->right = c;
        } else {
            s->right = (pDirection ? c->right : c->left);
            c->left = s;
            p->left = c;
        }
        // (pDirection ? s->left : s->right) = (pDirection ? c->right :
        // c->left); (pDirection ? c->right : c->left) = s; (pDirection ?
        // p->right : p->left) = c;
        s->color = uk_so_wl_rbtree_RED;
        s = c;

    del_L5:
        s->color = p->color;
        p->color = uk_so_wl_rbtree_BLACK;
        if (pDirection) {
            p->right = (pDirection ? s->left : s->right);
            s->left = p;
        } else {
            p->left = (pDirection ? s->left : s->right);
            s->right = p;
        }
        // (pDirection ? p->right : p->left) = (pDirection ? s->left :
        // s->right); (pDirection ? s->left : s->right) = p;
        ((pDirection ? s->right : s->left))->color = uk_so_wl_rbtree_BLACK;
        if (--pParent < &parents[0]) {
            tree->root = s;
        } else {
            n = *pParent;
            unsigned int pDirection = p == n->left;
            if (pDirection) {
                n->left = s;
            } else {
                n->right = s;
            }
            // (pDirection ? n->left : n->right) = s;
        }

        return found_node;
    }

    return found_node;
}

void __WL_CODE uk_so_wl_rbtree_print_tree(struct uk_so_wl_rbtree *tree) {
    unsigned long max_height =
        2 * uk_so_wl_rbtree_fast_log2(tree->element_count + 2) - 2;
    max_height++;
    unsigned int dist = tree->element_count * 3;

    printf("Printing tree:\n\n");

    for (unsigned int i = 0; i < max_height; i++) {
        uk_so_wl_rbtree_print_level(tree, i, dist, tree->root);
        dist /= 2;
        printf("\n");
    }
    printf("\n");
}

void __WL_CODE uk_so_wl_rbtree_print_level(struct uk_so_wl_rbtree *tree,
                                           unsigned int level, int dist,
                                           struct uk_so_wl_rbtree_node *root) {
    if (level == 0) {
        for (int i = 0; i < dist - 1; i++) {
            printf(" ");
        }
        if (root != 0) {
            printf(root->color == uk_so_wl_rbtree_RED ? "R" : "B");
        } else {
            printf("O");
        }
        for (int i = 0; i < dist + 1; i++) {
            printf(" ");
        }
    } else {
        uk_so_wl_rbtree_print_level(tree, level - 1, dist,
                                    root == 0 ? 0 : root->left);
        uk_so_wl_rbtree_print_level(tree, level - 1, dist,
                                    root == 0 ? 0 : root->right);
    }
}

unsigned long __WL_CODE
uk_so_wl_rbtree_get_element_count(struct uk_so_wl_rbtree *tree) {
    return tree->element_count;
}
