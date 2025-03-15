/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   render_scene.c                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gcorreia <gcorreia@student.42.rio>         +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/03/20 16:15:36 by gcorreia          #+#    #+#             */
/*   Updated: 2025/03/05 22:33:31 by lfarias-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../../headers/mini_rt.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>


static void         generate_samples(t_threaddata *thread_data, t_xorshift32 *rng);
static int          render_px(float x, float y, t_scene *s, mlx_image_t *image);
static t_ray        get_px_ray(float x, float y, mlx_image_t *image, t_scene *scene);
static void         progressive_render(t_threaddata *thread_data);
static inline void  accumulate_sample_local(t_pixel *buffer, int index, uint32_t color);
static void         merge_local_to_global(t_appdata *app_data, t_threaddata *thread_data);

// public
void render_frame(void *arg) {
    t_appdata   *app_data;
    bool        needs_more_samples;

    app_data = (t_appdata *)arg;
    needs_more_samples = (app_data->sample_count < 64);

    if (!needs_more_samples && !app_data->rendering_in_progress) {
        return;
    }

    pthread_mutex_lock(&app_data->render_mutex);

    if (!app_data->start_rendering) {
        app_data->start_rendering = true;
        atomic_store(&app_data->threads_done, 0);
        pthread_cond_broadcast(&app_data->start_render_cond);
    }

    while (atomic_load(&app_data->threads_done) < NUM_THREADS) {
        pthread_cond_wait(&app_data->frame_ready_cond, &app_data->render_mutex);
    }

    app_data->sample_count++;
    app_data->frame_offset += RAYS_PER_TILE;
    app_data->start_rendering = false;

    mlx_image_t *temp = app_data->render_image;
    app_data->render_image = app_data->display_image;
    app_data->display_image = temp;

    mlx_image_to_window(app_data->engine, app_data->display_image, 0, 0);

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

    while (1) {
        pthread_mutex_lock(&app_data->render_mutex);

        while (!app_data->start_rendering) {
            pthread_cond_wait(&app_data->start_render_cond, &app_data->render_mutex);
        }

        pthread_mutex_unlock(&app_data->render_mutex);

        generate_samples(thread_data, &rng);
        progressive_render(thread_data);
        
        prev_done = atomic_fetch_add(&app_data->threads_done, 1);
        if (prev_done + 1 == NUM_THREADS) 
        {
            pthread_mutex_lock(&app_data->render_mutex);
            printf("render_area: Frame complete, sample_count=%d\n", app_data->sample_count);
            pthread_cond_signal(&app_data->frame_ready_cond);
            app_data->start_rendering = false;
            pthread_mutex_unlock(&app_data->render_mutex);
        }
    }

    return NULL;
}

// private 

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
                    uint8_t r = pixel->samples > 0 ? (uint8_t)(pixel->r * inv_samples * 255) : 0;
                    uint8_t g = pixel->samples > 0 ? (uint8_t)(pixel->g * inv_samples * 255) : 0;
                    uint8_t b = pixel->samples > 0 ? (uint8_t)(pixel->b * inv_samples * 255) : 0;
                    uint32_t color = (255 << 24) | (r << 16) | (g << 8) | b;

                    mlx_put_pixel(app_data->render_image, global.x, global.y, color);
                }
            }
        }
    }
}


static void generate_samples(t_threaddata *thread_data, t_xorshift32 *rng)
{
    t_appdata   *app_data;
    t_tile      tile;
    t_point     jitter;
    t_point     sobol;
    t_point     coord;
    t_point     local;
    uint32_t    color;
    int         sobol_idx;
    int         local_idx;
    int         local_tile_offset;
    int         idx_x, idx_y;

    app_data = thread_data->app_data;

    for (int tile_idx = thread_data->start_tile; tile_idx < thread_data->end_tile; tile_idx++) 
    {
        tile = thread_data->tiles[tile_idx];
        local_tile_offset = (tile_idx - thread_data->start_tile) * TILE_SIZE * TILE_SIZE;

        for (int i = 0; i < RAYS_PER_TILE; i++)
        {
            sobol_idx = (i + app_data->frame_offset + (tile_idx * 31)) % SOBOL_SIZE;
            sobol.x = app_data->sobol_sequence[sobol_idx][0];
            sobol.y = app_data->sobol_sequence[sobol_idx][1];
            jitter.x = (xorshift32(rng) & 0xFFFF) * (1.0f / 65536.0f) * 1.0f - 0.5f; // [-0.5, 0.5]
            jitter.y = (xorshift32(rng) & 0xFFFF) * (1.0f / 65536.0f) * 1.0f - 0.5f; // [-0.5, 0.5]
            coord.x = tile.x + (sobol.x + jitter.x) * (TILE_SIZE - 1);
            coord.y = tile.y + (sobol.y + jitter.y) * (TILE_SIZE - 1);

            idx_x = (int)(coord.x + 0.5); // Round to nearest integer
            idx_y = (int)(coord.y + 0.5);

            if (idx_x >= tile.x && idx_x < tile.x + TILE_SIZE && idx_y >= tile.y && idx_y < tile.y + TILE_SIZE)
            {
                color = render_px((float)idx_x, (float)idx_y, app_data->scene_info, app_data->render_image);
                local.x = idx_x - tile.x;
                local.y = idx_y - tile.y;
                local_idx = local_tile_offset + local.y * TILE_SIZE + local.x;
                accumulate_sample_local(thread_data->local_buffer, local_idx, color);
            }
        }
    }

    merge_local_to_global(app_data, thread_data);
}

static int  render_px(float x, float y, t_scene *s, mlx_image_t *image)
{
	t_ray			ray;
	t_intersection	intersec;
	int				color;

	ray = get_px_ray(x, y, image, s);
	intersec = get_intersection_bvh(ray, s->root);
	// intersec = get_intersection(ray, s->elements);
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

static inline void accumulate_sample_local(t_pixel *buffer, int index, uint32_t color)
{
    buffer[index].r += ((color >> 16) & 0xFF) / 255.0f;
    buffer[index].g += ((color >> 8) & 0xFF) / 255.0f;
    buffer[index].b += (color & 0xFF) / 255.0f;
    buffer[index].samples += 1;
}

static void merge_local_to_global(t_appdata *app_data, t_threaddata *thread_data)
{
    t_pixel *local_buffer;
    t_tile  *tiles;
    t_tile  tile;
    int     start_tile;
    int     end_tile;
    int     tile_size;
    int     local_tile_offset;
    int     local_idx;
    t_point global;
    int     global_idx;

    local_buffer = thread_data->local_buffer;
    tiles = thread_data->tiles;
    start_tile = thread_data->start_tile;
    end_tile = thread_data->end_tile;
    tile_size = TILE_SIZE;

    pthread_mutex_lock(&app_data->accum_mutex);
    for (int tile_idx = start_tile; tile_idx < end_tile; tile_idx++)
    {
        local_tile_offset = (tile_idx - start_tile) * tile_size * tile_size;
        tile = tiles[tile_idx];

        for (int y = 0; y < tile_size; y++)
        {
            for (int x = 0; x < tile_size; x++)
            {
                local_idx = local_tile_offset + y * tile_size + x;
                global.x = tile.x + x;
                global.y = tile.y + y;

                // Ensure we’re within bounds (in case jittering pushed us out)
                if (global.x >= 0 && global.x < SCREEN_WIDTH && global.y >= 0 && global.y < SCREEN_HEIGHT)
                {
                    global_idx = global.y * SCREEN_WIDTH + global.x;
                    app_data->accum_buffer[global_idx].r += local_buffer[local_idx].r;
                    app_data->accum_buffer[global_idx].g += local_buffer[local_idx].g;
                    app_data->accum_buffer[global_idx].b += local_buffer[local_idx].b;
                    app_data->accum_buffer[global_idx].samples += local_buffer[local_idx].samples;

                    // Reset local buffer
                    local_buffer[local_idx].r = 0;
                    local_buffer[local_idx].g = 0;
                    local_buffer[local_idx].b = 0;
                    local_buffer[local_idx].samples = 0;
                }
            }
        }
    }
    pthread_mutex_unlock(&app_data->accum_mutex);
}
