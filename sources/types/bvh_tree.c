/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   bvh_tree.c                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: lfarias- <leofariasrj25@gmail.com>         +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/03/05 13:27:57 by lfarias-          #+#    #+#             */
/*   Updated: 2025/03/05 13:28:05 by lfarias-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../../headers/mini_rt.h"
#include <stdio.h>

# define X 0
# define Y 1
# define Z 2

static t_bvh_node   *build_bvh_helper(t_elist **array, int start, int end);
static int          count_list_elements(t_elist *elements);
static t_elist      **list_to_array(t_elist *elements, int count);

// public

t_bvh_node *build_bvh(t_elist *elements) {
    int     count;
    t_aabb  box;
    t_elist **array;
    t_elist **valid_array;

    count = count_list_elements(elements);

    if (count == 0) {
        return NULL;
    }

    array = list_to_array(elements, count);
    valid_array = malloc(sizeof(t_elist *) * count);
    int valid_count = 0;
    for (int i = 0; i < count; i++) {
        box = create_aabb(&array[i]->object, array[i]->type);

        if (!(box.min.x == box.max.x && box.min.y == box.max.y && box.min.z == box.max.z)) {
            valid_array[valid_count++] = array[i];
        }
    }

    if (valid_count == 0) {
        free(valid_array);
        free(array);
        return NULL;
    }

    t_bvh_node *root = build_bvh_helper(valid_array, 0, valid_count - 1);

    free(valid_array);
    free(array);
    return root;
}

// private

static int count_list_elements(t_elist *elements) 
{
    int     count;
    t_elist *current;

    count = 0;
    current = elements;

    while (current) 
    {
        count++;
        current = current->next;
    }

    return count;
}

// Helper to convert t_elist to array
static t_elist **list_to_array(t_elist *elements, int count) 
{
    t_elist **array;
    t_elist *current;

    array = malloc(sizeof(t_elist *) * count);
    current = elements;

    for (int i = 0; i < count && current; i++) {
        array[i] = current;
        current = current->next;
    }

    return array;
}

static int current_axis; // Static variable to pass axis to comparison function

static int compare_aabb_center(const void *a, const void *b) {
    t_elist *elem_a;
    t_elist *elem_b;
    t_aabb  aabb_a;
    t_aabb  aabb_b;
    float   center_a;
    float   center_b;

    elem_a = *(t_elist **)a;
    elem_b = *(t_elist **)b;
    aabb_a = create_aabb(&elem_a->object, elem_a->type);
    aabb_b = create_aabb(&elem_b->object, elem_b->type);

    if (current_axis == X) {
        center_a = (aabb_a.min.x + aabb_a.max.x) / 2.0f;
        center_b = (aabb_b.min.x + aabb_b.max.x) / 2.0f;
    } else if (current_axis == Y) {
        center_a = (aabb_a.min.y + aabb_a.max.y) / 2.0f;
        center_b = (aabb_b.min.y + aabb_b.max.y) / 2.0f;
    } else {
        center_a = (aabb_a.min.z + aabb_a.max.z) / 2.0f;
        center_b = (aabb_b.min.z + aabb_b.max.z) / 2.0f;
    }

    return (center_a < center_b) ? -1 : (center_a > center_b) ? 1 : 0;
}

static t_bvh_node *build_bvh_helper(t_elist **array, int start, int end) {
    t_bvh_node  *leaf;
    t_bvh_node  *node;
    t_bvh_node  *left;
    t_bvh_node  *right;
    t_aabb      bounds;
    t_aabb      obj_aabb;
    int         count;
    float       distance[3];
    int         mid;

    if (start > end) {
        return NULL;
    }

    count = end - start + 1;
    if (count == 1) {
        leaf = malloc(sizeof(t_bvh_node));
        leaf->bounds = create_aabb(&array[start]->object, array[start]->type);
        leaf->left = NULL;
        leaf->right = NULL;
        leaf->object = &array[start]->object;
        leaf->object_type = array[start]->type;
        return leaf;
    }

    bounds = create_aabb(&array[start]->object, array[start]->type);

    for (int i = start + 1; i <= end; i++) {
        obj_aabb = create_aabb(&array[i]->object, array[i]->type);
        bounds = merge_aabb(bounds, obj_aabb);
    }

    distance[X] = bounds.max.x - bounds.min.x;
    distance[Y] = bounds.max.y - bounds.min.y;
    distance[Z] = bounds.max.z - bounds.min.z;

    if (distance[X] > distance[Y] && distance[X] > distance[Z])
    {
        current_axis = X;
    }
    else if (distance[Y] > distance[Z])
    {
        current_axis = Y;
    }
    else 
    {
        current_axis = Z;
    }

    qsort(array + start, count, sizeof(t_elist *), compare_aabb_center);

    mid = start + (end - start) / 2;
    left = build_bvh_helper(array, start, mid);
    right = build_bvh_helper(array, mid + 1, end);

    node = malloc(sizeof(t_bvh_node));
    node->left = left;
    node->right = right;
    node->object = NULL;

    if (left && right) {
        node->bounds = merge_aabb(left->bounds, right->bounds);
    } else if (left) {
        node->bounds = left->bounds;
    } else if (right) {
        node->bounds = right->bounds;
    } else {
        free(node); // highly unlikely, but I like peace of mind.
        return NULL;
    }

    return node;
}
