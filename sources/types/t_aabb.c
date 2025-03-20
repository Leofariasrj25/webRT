/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   t_aabb.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: lfarias- <leofariasrj25@gmail.com>         +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/03/05 13:28:22 by lfarias-          #+#    #+#             */
/*   Updated: 2025/03/05 13:28:23 by lfarias-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../../headers/mini_rt.h"

t_aabb   create_aabb(union u_object *object, enum e_element type)
{
    static t_aabb_creator   aabb_creators[nae] = {0};
    static bool             is_init = false;

    if (!is_init) {
        aabb_creators[sphere] = create_aabb_sphere;
        aabb_creators[plane] = create_aabb_plane;
        aabb_creators[cylinder] = create_aabb_cylinder;
        aabb_creators[cone] = create_aabb_cone;
        is_init = true;
    }

    if (type >= 0 && type < nae && aabb_creators[type] != NULL) {
        return aabb_creators[type](object);
    } else {
        t_aabb default_box = { {0, 0, 0}, {0, 0, 0} };
        return default_box;
    }
}

t_aabb merge_aabb(t_aabb a, t_aabb b) {
    t_aabb result;

    result.min.x = fmin(a.min.x, b.min.x);
    result.min.y = fmin(a.min.y, b.min.y);
    result.min.z = fmin(a.min.z, b.min.z);
    result.max.x = fmax(a.max.x, b.max.x);
    result.max.y = fmax(a.max.y, b.max.y);
    result.max.z = fmax(a.max.z, b.max.z);
    return result;
}

t_aabb create_aabb_plane(void *object)
{
    //log_msg("Creating AABB for plane", WARN);
    (void)object;

    t_aabb box = {
        .min = { -100, -100, -100 },
        .max = { 100, 100, 100 }
    };
    return box;
}

t_aabb create_aabb_sphere(void *object)
{
	t_aabb	    box;
        t_sphere    *sphere;
	double	    radius;

        //log_msg("Creating AABB for sphere", WARN);
        sphere = (t_sphere *)object;
	radius = sphere->diameter / 2.0;

	box.min.x = sphere->origin.x - radius;
        box.min.y = sphere->origin.y - radius;
        box.min.z = sphere->origin.z - radius;
        box.max.x = sphere->origin.x + radius;
        box.max.y = sphere->origin.y + radius;
        box.max.z = sphere->origin.z + radius;

	return box;
}

t_aabb create_aabb_cylinder(void *object) {
    t_aabb      box;
    t_cylinder  *cylinder;
    double      radius;
    double      half_height;
    t_point     start;
    t_point     end;

    //log_msg("Creating AABB for cylinder", WARN);
    cylinder = (t_cylinder *)object;
    radius = cylinder->diameter / 2.0;
    half_height = cylinder->height / 2.0;

    t_point orient = cylinder->orientation;

    start.x = cylinder->origin.x - orient.x * half_height;
    start.y = cylinder->origin.y - orient.y * half_height;
    start.z = cylinder->origin.z - orient.z * half_height;

    end.x = cylinder->origin.x + orient.x * half_height;
    end.y = cylinder->origin.y + orient.y * half_height;
    end.z = cylinder->origin.z + orient.z * half_height;

    // Project radius onto each axis
    double perp_x = sqrt(orient.y*orient.y + orient.z*orient.z);
    double proj_x = radius * perp_x;

    double perp_y = sqrt(orient.x*orient.x + orient.z*orient.z);
    double proj_y = radius * perp_y;

    double perp_z = sqrt(orient.x*orient.x + orient.y*orient.y);
    double proj_z = radius * perp_z;

    // Compute AABB bounds
    box.min.x = fmin(start.x, end.x) - proj_x;
    box.max.x = fmax(start.x, end.x) + proj_x;

    box.min.y = fmin(start.y, end.y) - proj_y;
    box.max.y = fmax(start.y, end.y) + proj_y;

    box.min.z = fmin(start.z, end.z) - proj_z;
    box.max.z = fmax(start.z, end.z) + proj_z;

    return box;
}


t_aabb create_aabb_cone(void *object)
{
    //log_msg("Creating AABB for cone", WARN);
    return create_aabb_cylinder(object);
}
