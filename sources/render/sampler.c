/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   sampler.c                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: lfarias- <leofariasrj25@gmail.com>         +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/03/17 14:17:42 by lfarias-          #+#    #+#             */
/*   Updated: 2025/03/17 17:47:55 by lfarias-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../../headers/mini_rt.h"

static inline void	accumulate_sample_local(t_pixel *buffer, int index, uint32_t color);
static int		compute_rays_per_tile(float variance);
static float		compute_tile_variance(t_pixel *local_buffer, int offset, int tile_size);
static void		merge_local_to_global(t_appdata *app_data, t_threaddata *thread_data);

// public
void generate_samples(t_threaddata *thread_data, t_xorshift32 *rng)
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
    float       temporal_factor;

    app_data = thread_data->app_data;
    temporal_factor = app_data->is_moving? 1.0f : 0.5f;

    for (int tile_idx = thread_data->start_tile; tile_idx < thread_data->end_tile; tile_idx++) 
    {
        tile = thread_data->tiles[tile_idx];
        local_tile_offset = (tile_idx - thread_data->start_tile) * TILE_SIZE * TILE_SIZE;
        float variance = compute_tile_variance(thread_data->local_buffer, local_tile_offset, TILE_SIZE);


        int rays = (int)(compute_rays_per_tile(variance) * temporal_factor * 2);

        if (rays * 2 > RAYS_PER_TILE)
        {
            rays = RAYS_PER_TILE * 2;
        } 
        else if (rays < 1) 
        {
            rays = 1;
        }

        for (int i = 0; i < rays; i++)
        {
            sobol_idx = (i + app_data->frame_offset + (tile_idx * 31)) % SOBOL_SIZE;
            sobol.x = app_data->sobol_sequence[sobol_idx][0];
            sobol.y = app_data->sobol_sequence[sobol_idx][1];
            jitter.x = (xorshift32(rng) & 0xFFFF) * (1.0f / 65536.0f) * 1.0f - 0.5f; // [-0.5, 0.5]
            jitter.y = (xorshift32(rng) & 0xFFFF) * (1.0f / 65536.0f) * 1.0f - 0.5f; // [-0.5, 0.5]
            coord.x = tile.x + (sobol.x + jitter.x) * (TILE_SIZE - 1);
            coord.y = tile.y + (sobol.y + jitter.y) * (TILE_SIZE - 1);

            idx_x = (int)(coord.x); // Round to nearest integer
            idx_y = (int)(coord.y);

            if (app_data->is_moving && (idx_x + idx_y) % 2 != app_data->sample_count % 2)
            {
                continue; // Skip this pixel, it will be filled by blending with the previous frame
            }

            if (idx_x >= tile.x && idx_x < tile.x + TILE_SIZE && idx_y >= tile.y && idx_y < tile.y + TILE_SIZE)
            {
                local.x = idx_x - tile.x;
                local.y = idx_y - tile.y;
                local_idx = local_tile_offset + local.y * TILE_SIZE + local.x;

                if (!thread_data->local_buffer[local_idx].converged)
                {
                    color = render_px(coord.x, coord.y, app_data->scene_info, app_data->render_image);
                    accumulate_sample_local(thread_data->local_buffer, local_idx, color);
                }
            }
        }
    }

    merge_local_to_global(app_data, thread_data);
}

// private 


static float    compute_tile_variance(t_pixel *local_buffer, int offset, int tile_size)
{
    int     total_tiles;
    t_color color;
    float   variance;

    total_tiles = tile_size * tile_size;
    variance = 0;

    for (int i = 0; i < total_tiles; i++)
    {
        if (local_buffer[i + offset].samples == 0) continue; // Skip if no samples
        color.r = local_buffer[i + offset].r / local_buffer[i + offset].samples;
        color.g = local_buffer[i + offset].g / local_buffer[i + offset].samples;
        color.b = local_buffer[i + offset].b / local_buffer[i + offset].samples;
        variance += color.r * color.r + color.g * color.g + color.b * color.b;
    }

    return variance / total_tiles;
}

static int  compute_rays_per_tile(float variance)
{
    const float scale_factor = 0.1f;
    const int   base_rays = RAYS_PER_TILE / 4;
    int         additional_rays;
    int         ray_amount;

    additional_rays = (int)(variance * scale_factor);
    ray_amount = base_rays + additional_rays;

    if (ray_amount > RAYS_PER_TILE)
    {
        ray_amount = RAYS_PER_TILE;
    }
   
    return ray_amount;
}
#include <stdio.h>
static inline void accumulate_sample_local(t_pixel *buffer, int index, uint32_t color)
{
    t_pixel     *pixel;
    t_pixel     prev;
    t_pixel     curr;
    t_pixel     diff;
    bool        under_threshold;
    const float threshold = 0.5f;

    pixel = &buffer[index];
    
    prev.r = pixel->r / (pixel->samples > 0 ? pixel->samples : 1);
    prev.g = pixel->g / (pixel->samples > 0 ? pixel->samples : 1);
    prev.b = pixel->b / (pixel->samples > 0 ? pixel->samples : 1);

    pixel->r += ((color >> 16) & 0xFF) / 255.0f;
    pixel->g += ((color >> 8) & 0xFF) / 255.0f;
    pixel->b += (color & 0xFF) / 255.0f;
    pixel->samples += 1;

    curr.r = pixel->r / pixel->samples; 
    curr.g = pixel->g / pixel->samples; 
    curr.b = pixel->b / pixel->samples; 

    diff.r = fabsf(curr.r - prev.r);
    diff.g = fabsf(curr.g - prev.g);
    diff.b = fabsf(curr.b - curr.b);
    under_threshold = diff.r < threshold && diff.g < threshold && diff.b < threshold;

    if (!pixel->converged && pixel->samples > 1 && under_threshold)
    {
        //printf(">>> converged pixel at index=%d\n", index);
        pixel->converged = true;
    }
}

static inline void atomic_add_float(float *ptr, float val)
{
    union { float f; uint32_t i; } old_val, new_val;
    do {
        old_val.f = *ptr;
        new_val.f = old_val.f + val;
    } while (!__sync_bool_compare_and_swap((uint32_t *)ptr, old_val.i, new_val.i));
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
    t_pixel *global_pixel;

    local_buffer = thread_data->local_buffer;
    tiles = thread_data->tiles;
    start_tile = thread_data->start_tile;
    end_tile = thread_data->end_tile;
    tile_size = TILE_SIZE;

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
                    global_pixel = &app_data->accum_buffer[global_idx];

                    __sync_fetch_and_add(&global_pixel->samples, local_buffer[local_idx].samples);
                    atomic_add_float(&global_pixel->r, local_buffer[local_idx].r);
                    atomic_add_float(&global_pixel->g, local_buffer[local_idx].g);
                    atomic_add_float(&global_pixel->b, local_buffer[local_idx].b);
                    if (local_buffer[local_idx].converged) // Propagate convergence
                    {
                        global_pixel->converged = true;
                        app_data->converged_pixels += 1;
                    }
                    // Reset local buffer
                    local_buffer[local_idx].r = 0;
                    local_buffer[local_idx].g = 0;
                    local_buffer[local_idx].b = 0;
                    local_buffer[local_idx].samples = 0;
                    local_buffer[local_idx].converged = false;
                }
            }
        }
    }
}
