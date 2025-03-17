/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   render_scene.c                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gcorreia <gcorreia@student.42.rio>         +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/03/20 16:15:36 by gcorreia          #+#    #+#             */
/*   Updated: 2025/03/17 15:08:40 by lfarias-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../../headers/mini_rt.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>


static t_ray        get_px_ray(float x, float y, mlx_image_t *image, t_scene *scene);
static void         progressive_render(t_threaddata *thread_data);
static void         blend_frames(t_appdata *app_data, t_threaddata *thread_data);

// public
void render_frame(void *arg) {
    t_appdata   *app_data;
    bool        needs_more_samples;

    app_data = (t_appdata *)arg;
    needs_more_samples = app_data->sample_count < MAX_SAMPLES;

    if (!needs_more_samples && !app_data->rendering_in_progress) 
    {
        return;
    }

    app_data->blend_alpha = app_data->is_moving ? 0.6f : 0.5f;

    pthread_mutex_lock(&app_data->render_mutex);

    if (!app_data->start_rendering) {
        app_data->start_rendering = true;
        atomic_store(&app_data->threads_done, 0);
        pthread_cond_broadcast(&app_data->start_render_cond);
    }

    while (atomic_load(&app_data->threads_done) < NUM_THREADS) 
    {
        pthread_cond_wait(&app_data->frame_ready_cond, &app_data->render_mutex);
    }

    if (app_data->sample_count == 0)
    {
        memset(app_data->accum_buffer, 0, SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(t_pixel));
    }

    pthread_mutex_unlock(&app_data->render_mutex);

    app_data->sample_count++;
    app_data->frame_offset += RAYS_PER_TILE;
    app_data->start_rendering = false;

    mlx_image_t *temp = app_data->render_image;
    app_data->render_image = app_data->display_image;
    app_data->display_image = temp;
    mlx_image_to_window(app_data->engine, app_data->display_image, 0, 0);

    memcpy(app_data->prev_accum_buffer, app_data->accum_buffer, SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(t_pixel));

    if (needs_more_samples) {
        app_data->rendering_in_progress = true;
    } else {
        app_data->rendering_in_progress = false;
        app_data->image_displayed = true;
    }

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

    while (1) {
        pthread_mutex_lock(&app_data->render_mutex);

        while (!app_data->start_rendering) {
            pthread_cond_wait(&app_data->start_render_cond, &app_data->render_mutex);
        }

        pthread_mutex_unlock(&app_data->render_mutex);

        if (app_data->sample_count == 0)
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
            //printf("render_area: Frame complete, sample_count=%d\n", app_data->sample_count);
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

static void blend_frames(t_appdata *app_data, t_threaddata *thread_data)
{
    t_tile *tiles = thread_data->tiles;
    int start_tile = thread_data->start_tile;
    int end_tile = thread_data->end_tile;
    int tile_size = TILE_SIZE;

    if (app_data->is_moving) printf("Motion detected, alpha=%f\n", app_data->blend_alpha);

    for (int tile_idx = start_tile; tile_idx < end_tile; tile_idx++)
    {
        t_tile tile = tiles[tile_idx];
        int local_tile_offset = (tile_idx - start_tile) * tile_size * tile_size;
        for (int y = 0; y < tile_size; y++)
        {
            for (int x = 0; x < tile_size; x++)
            {
                int global_x = tile.x + x;
                int global_y = tile.y + y;
                if (global_x >= 0 && global_x < SCREEN_WIDTH && global_y >= 0 && global_y < SCREEN_HEIGHT)
                {
                    int global_idx = global_y * SCREEN_WIDTH + global_x;
                    int local_idx = local_tile_offset + y * tile_size + x;
                    t_pixel *curr = &app_data->accum_buffer[global_idx];
                    t_pixel *prev = &app_data->prev_accum_buffer[global_idx];
                    t_pixel *local = &thread_data->local_buffer[local_idx];

                    if (app_data->is_moving)
                    {
                        // Blend with previous frame to reduce ghosting
                        float alpha = app_data->blend_alpha;
                        curr->r = local->r * (1.0f - alpha) + prev->r * alpha;
                        curr->g = local->g * (1.0f - alpha) + prev->g * alpha;
                        curr->b = local->b * (1.0f - alpha) + prev->b * alpha;
                        curr->samples = local->samples; // Reset sample count during motion
                        app_data->is_moving = false;
                    }
                    else
                    {
                        // Accumulate new samples when stationary
                        if (app_data->sample_count == 0)
                        {
                            // First frame: initialize with local buffer
                            curr->r = local->r;
                            curr->g = local->g;
                            curr->b = local->b;
                            curr->samples = local->samples;
                        }
                        else
                        {
                            // Subsequent frames: add new samples
                            curr->r += local->r;
                            curr->g += local->g;
                            curr->b += local->b;
                            curr->samples += local->samples;
                        }
                    }

                    // Update previous buffer for next frame
                    prev->r = curr->r;
                    prev->g = curr->g;
                    prev->b = curr->b;
                    prev->samples = curr->samples;
                }
            }
        }
    }
}


int  render_px(float x, float y, t_scene *s, mlx_image_t *image)
{
	t_ray			ray;
	t_intersection	intersec;
	int				color;

	ray = get_px_ray(x, y, image, s);
	intersec = get_intersection_bvh(ray, s->root);
        return (color = get_px_color(intersec, ray, s));
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
		fov_mult = tan(scene->camera->r_fov * 0.5); // Use r_fov, half angle
                initialized = true;
	}
	
        origin.x = (2.0 * (x / image->width) - 1.0) * a_ratio * fov_mult + scene->camera->origin.x;
        origin.y = (1.0 - 2.0 * (y / image->height)) * fov_mult + scene->camera->origin.y;
        origin.z = -1.0 + scene->camera->origin.z;

	// Generate ray from camera origin to target
	ray = get_ray(scene->camera->origin, origin);
	ray.origin = scene->camera->origin; // Ensure ray starts at camera position

	return ray;
}


