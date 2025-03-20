/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   destroy_scene.c                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: lfarias- <lfarias-@student.42.rio>         +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/03/13 14:43:27 by lfarias-          #+#    #+#             */
/*   Updated: 2025/03/19 22:36:43 by lfarias-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../../headers/mini_rt.h"

void	destroy_scene(t_scene *scene, int scene_fd)
{
	scene_close(scene_fd);
	free(scene->a_light);
	free(scene->light);
	free(scene->camera);
	free_bvh(scene->root);
	free_elist(&scene->elements);
}
