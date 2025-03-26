/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   render_scene.c                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gcorreia <gcorreia@student.42.rio>         +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/03/20 16:15:36 by gcorreia          #+#    #+#             */
/*   Updated: 2025/03/23 02:17:59 by lfarias-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../../headers/mini_rt.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>


static t_ray        get_px_ray(float x, float y, mlx_image_t *image, t_scene *scene);
static void         progressive_render(t_threaddata *thread_data);

// public
void render_frame(void *arg) {
    t_appdata   *app_data;
    bool        needs_more_samples;

    app_data = (t_appdata *)arg;
    needs_more_samples = app_data->converged_pixels < (SCREEN_WIDTH * SCREEN_HEIGHT);

    if (!needs_more_samples && !app_data->rendering_in_progress) 
    {
        return;
    }

    app_data->blend_alpha = app_data->is_moving ? 0.1f : 0.5f;

    pthread_mutex_lock(&app_data->render_mutex);

    if (!app_data->start_rendering) {
        app_data->start_rendering = true;
        atomic_store(&app_data->threads_done, 0);
        pthread_cond_broadcast(&app_data->start_render_cond);
    }

    while (atomic_load(&app_data->threads_done) < NUM_THREADS) 
    {
        printf(">>> main thread waiting\n");
        pthread_cond_wait(&app_data->frame_ready_cond, &app_data->render_mutex);
    }

    if (app_data->sample_count == 0)
    {
        memset(app_data->accum_buffer, 0, SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(t_pixel));
    }

    app_data->sample_count++;
    app_data->frame_offset += RAYS_PER_TILE;
    app_data->start_rendering = false;

    memcpy(app_data->prev_accum_buffer, app_data->accum_buffer, SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(t_pixel));

    if (atomic_load(&app_data->is_moving)) 
    {
        atomic_store(&app_data->is_moving, false);
    }

    if (needs_more_samples) {
        app_data->rendering_in_progress = true;
    } else {
        app_data->rendering_in_progress = false;
        app_data->image_displayed = true;
    }

    pthread_mutex_unlock(&app_data->render_mutex);
}


void *render_area(void* arg) 
{
    t_threaddata    *thread_data;
    t_appdata       *app_data;
    t_xorshift32      rng;
    int             prev_done;

    thread_data = (t_threaddata *)arg;
    app_data = thread_data->app_data;
    rng.state = 12345 + thread_data->thread_id;

    while (atomic_load(&app_data->is_app_running)) 
    {
        pthread_mutex_lock(&app_data->render_mutex);

        while (!app_data->start_rendering)
        {
            printf(">>> thread %d waiting\n", thread_data->thread_id);
            pthread_cond_wait(&app_data->start_render_cond, &app_data->render_mutex);
        }

        pthread_mutex_unlock(&app_data->render_mutex);
        
        if (!atomic_load(&app_data->is_app_running))
        {
                return NULL;
        }

        if (atomic_load(&app_data->sample_count) == 0)
        {
            memset(thread_data->local_buffer, 0, thread_data->total_pixels);
        }

        generate_samples(thread_data, &rng);
        blend_frames(app_data, thread_data);
        progressive_render(thread_data);

        prev_done = atomic_fetch_add(&app_data->threads_done, 1);
        if (prev_done + 1 == NUM_THREADS) 
        {
            pthread_mutex_lock(&app_data->render_mutex);
            printf("render_area: Frame complete, sample_count=%d, converged_pixels=%d\n", app_data->sample_count, app_data->converged_pixels);
            pthread_cond_signal(&app_data->frame_ready_cond);
            app_data->start_rendering = false;
            pthread_mutex_unlock(&app_data->render_mutex);
        }
    }

    return NULL;
}


static void progressive_render(t_threaddata *thread_data)
{
    t_appdata   *app_data;
    t_tile      tile;
    t_pixel     *pixel;
    t_point     global;
    int         global_idx;
    float       inv_samples;
     
    app_data = thread_data->app_data; 

    for (int tile_idx = thread_data->start_tile; tile_idx < thread_data->end_tile; tile_idx++) 
    {
        tile = thread_data->tiles[tile_idx];

        for (int ly = 0; ly < TILE_SIZE; ly++) 
        {
            for (int lx = 0; lx < TILE_SIZE; lx++) 
            {
                global.x = tile.x + lx;
                global.y = tile.y + ly;

                if (global.x < SCREEN_WIDTH && global.y < SCREEN_HEIGHT)
                {
                    global_idx = global.y * SCREEN_WIDTH + global.x;
                    pixel = &app_data->accum_buffer[global_idx];

                    inv_samples = 1.0f / pixel->samples;

                    // Normalize colors by sample count and clamp to [0, 1] to prevent overflow
                    float r_normalized = pixel->samples > 0 ? pixel->r * inv_samples : 0.0f;
                    float g_normalized = pixel->samples > 0 ? pixel->g * inv_samples : 0.0f;
                    float b_normalized = pixel->samples > 0 ? pixel->b * inv_samples : 0.0f;

                    // Clamp normalized values to ensure they stay in [0, 1]
                    if (r_normalized > 1.0f) r_normalized = 1.0f;
                    if (r_normalized < 0.0f) r_normalized = 0.0f;
                    if (g_normalized > 1.0f) g_normalized = 1.0f;
                    if (g_normalized < 0.0f) g_normalized = 0.0f;
                    if (b_normalized > 1.0f) b_normalized = 1.0f;
                    if (b_normalized < 0.0f) b_normalized = 0.0f;

                    // Scale to uint8_t for display
                    uint8_t r = (uint8_t)(r_normalized * 255);
                    uint8_t g = (uint8_t)(g_normalized * 255);
                    uint8_t b = (uint8_t)(b_normalized * 255);
                    
                    uint32_t color = (255 << 24) | (r << 16) | (g << 8) | b;

                    mlx_put_pixel(app_data->render_image, global.x, global.y, color);
                }
            }
        }
    }
}

int  render_px(float x, float y, t_scene *s, mlx_image_t *image)
{
	t_ray			ray;
	t_intersection	        intersec;

	ray = get_px_ray(x, y, image, s);
	intersec = get_intersection_bvh(ray, s->root);
        return (get_px_color(intersec, ray, s));
}

static t_ray    get_px_ray(float x, float y, mlx_image_t *image, t_scene *scene)
{
	static double	a_ratio;
	static double	fov_mult;
	t_point		origin;
	t_ray		ray;
	static bool	initialized = false;

	if (!initialized) 
        {
		a_ratio = (double)image->width / image->height;
		fov_mult = tan(scene->camera->r_fov * 0.5);
                initialized = true;
	}
	
        origin.x = (2.0 * (x / image->width) - 1.0) * a_ratio * fov_mult + scene->camera->origin.x;
        origin.y = (1.0 - 2.0 * (y / image->height)) * fov_mult + scene->camera->origin.y;
        origin.z = -1.0 + scene->camera->origin.z;

	ray = get_ray(scene->camera->origin, origin);
	ray.origin = scene->camera->origin;

	return ray;
}
