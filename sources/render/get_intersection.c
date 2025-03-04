/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   get_intersection.c                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gcorreia <gcorreia@student.42.rio>         +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/03/26 17:36:20 by gcorreia          #+#    #+#             */
/*   Updated: 2023/04/09 14:07:20 by gcorreia         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../../headers/mini_rt.h"
#include <stdio.h>

typedef t_intersection	(*t_intersec_funcs)(t_ray, union u_object);

t_intersection	get_intersection(t_ray ray, t_elist *e)
{
	t_intersection			intersec;
	t_intersection			temp;
	static t_intersec_funcs	fn[4];

	if (*fn == NULL)
	{
		fn[0] = sphere_intersection;
		fn[1] = plane_intersection;
		fn[2] = cylinder_intersection;
		fn[3] = cone_intersection;
	}
	intersec.exists = 0;
	while (e)
	{
		temp = fn[e->type](ray, e->object);
		if (!intersec.exists && temp.exists)
			intersec = temp;
		else if (temp.exists && temp.distance < intersec.distance)
			intersec = temp;
		e = e->next;
	}
	return (intersec);
}

bool intersect_aabb(const t_ray *ray, const t_aabb *box) {
    double  tmin;
    double  tmax;
    double  epsilon;
    double  t0;
    double  t1;
    double  temp;

    tmin = -INFINITY;
    tmax = INFINITY;
    epsilon = 1e-6;

    double origins[3] = {ray->origin.x, ray->origin.y, ray->origin.z};
    double dirs[3] = {ray->orientation.x, ray->orientation.y, ray->orientation.z};
    double mins[3] = {box->min.x, box->min.y, box->min.z};
    double maxs[3] = {box->max.x, box->max.y, box->max.z};

    for (int i = 0; i < 3; i++) {
        if (fabs(dirs[i]) < epsilon) {
            if (origins[i] < mins[i] || origins[i] > maxs[i]) {
                return false;
            }

            t0 = -INFINITY;
            t1 = INFINITY;
        } else {
            double invD = 1.0 / dirs[i];
            // if you know why this bullshit works then please tell me, not even LLMs know why
            double t0 = (mins[i] - origins[i]) * invD;
            double t1 = (maxs[i] - origins[i]) * invD;

            if (invD < 0.0) 
            {
                temp = t0;
                t0 = t1;
                t1 = temp;
            }
        }

        tmin = fmax(t0, tmin);
        tmax = fmin(t1, tmax);

        if (tmax + epsilon <= tmin) 
        {
            return false;
        }
    }

    return true;
}

t_intersection get_intersection_bvh(t_ray ray, t_bvh_node *node) 
{
    t_intersection  intersec;
    t_intersection  left;
    t_intersection  right;
    t_intersection  closest;

    closest.exists = 0; 
    closest.distance = INFINITY;

    if (!node || !intersect_aabb(&ray, &node->bounds)) 
    {
        return closest;
    }

    if (node->object) 
    {
        switch (node->object_type) 
        {
            case sphere:   intersec = sphere_intersection(ray, *node->object); break;
            case cylinder: intersec = cylinder_intersection(ray, *node->object); break;
            case plane:    intersec = plane_intersection(ray, *node->object); break;
            case cone:     intersec = cone_intersection(ray, *node->object); break;
            default:       intersec.exists = 0; break;
        }
        return intersec;
    }

    left = get_intersection_bvh(ray, node->left);
    right = get_intersection_bvh(ray, node->right);

    if (left.exists && right.exists) 
    {
        return (left.distance < right.distance) ? left : right;
    } 
    else if (left.exists) 
    {
        return left;
    } 
    else if (right.exists) {
        return right;
    }

    return closest;
}
