/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   events.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gcorreia <gcorreia@student.42.rio>         +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/02/14 11:13:45 by gcorreia          #+#    #+#             */
/*   Updated: 2023/04/16 11:39:00 by lfarias-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../../headers/mini_rt.h"
#include <string.h>

static bool handle_wasd(mlx_key_data_t keydata, t_camera *camera);

void	shutdown(void *arg)
{
        t_appdata   *app_data;

        app_data = (t_appdata *)arg;
	destroy_scene(app_data->scene_info, app_data->scene_fd);
	log_msg("Good Bye :-)", INFO);
        mlx_terminate(app_data->engine);
}

void key_hook(mlx_key_data_t keydata, void* param) 
{
    static struct timespec  last_render = {0};
    struct timespec         now;
    double                  elapsed;
    t_appdata               *app_data;
    t_camera                *camera;
    bool                    moved;

    clock_gettime(CLOCK_MONOTONIC, &now);
    elapsed = (now.tv_sec - last_render.tv_sec) + 
                     (now.tv_nsec - last_render.tv_nsec) / 1e9;

    // avoid overloading the event queue with repetitive key presses
    if (elapsed < 0.1) 
    {
        return;
    }

    app_data = (t_appdata*)param;
    camera = app_data->scene_info->camera;
    moved = false;

    if (keydata.action == MLX_PRESS || keydata.action == MLX_REPEAT) 
    {
        if (keydata.key == MLX_KEY_ESCAPE)
        {
            shutdown(app_data);
            return;
        }

        app_data->keys[keydata.key] = true;
        moved = handle_wasd(keydata, camera);
        
    }
    if (keydata.action == MLX_RELEASE) 
    {
        app_data->keys[keydata.key] = false;
    }

    pthread_mutex_lock(&app_data->render_mutex);
    if (moved) 
    {
        app_data->sample_count = 0;
        app_data->image_displayed = false;
        app_data->rendering_in_progress = true; // Keep true for progressive rendering
        app_data->frame_offset = 0;
        memset(app_data->accum_buffer, 0, SCREEN_HEIGHT * SCREEN_WIDTH * sizeof(t_pixel));
        pthread_mutex_unlock(&app_data->render_mutex);
        render_frame(app_data);
        last_render = now;
    } else {
        pthread_mutex_unlock(&app_data->render_mutex);
    }
}

static bool handle_wasd(mlx_key_data_t keydata, t_camera *camera)
{
    bool    moved;
    float   move_speed;

    moved = false;
    move_speed = 1.0f;

    if (keydata.key == MLX_KEY_W) 
    {
        camera->origin.z -= move_speed; 
        moved = true; 
    }
    else if (keydata.key == MLX_KEY_S) 
    {
        camera->origin.z += move_speed; 
        moved = true; 
    }
    else if (keydata.key == MLX_KEY_A) 
    {
        camera->origin.x -= move_speed; 
        moved = true; 
    }
    else if (keydata.key == MLX_KEY_D) 
    {
        camera->origin.x += move_speed; 
        moved = true; 
    }

    return moved;
}
