/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   temporal_blender.c                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: lfarias- <leofariasrj25@gmail.com>         +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/03/19 16:30:36 by lfarias-          #+#    #+#             */
/*   Updated: 2025/03/19 16:33:27 by lfarias-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../../headers/mini_rt.h"

void blend_frames(t_appdata *app_data, t_threaddata *thread_data)
{
    t_tile *tiles = thread_data->tiles;
    int start_tile = thread_data->start_tile;
    int end_tile = thread_data->end_tile;
    int tile_size = TILE_SIZE;

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
                if (!(global_x >= 0 && global_x < SCREEN_WIDTH && global_y >= 0 && global_y < SCREEN_HEIGHT))
                {
                    continue;
                }

                int global_idx = global_y * SCREEN_WIDTH + global_x;
                int local_idx = local_tile_offset + y * tile_size + x;
                t_pixel *curr = &app_data->accum_buffer[global_idx];
                t_pixel *prev = &app_data->prev_accum_buffer[global_idx];
                t_pixel *local = &thread_data->local_buffer[local_idx];

                // Skip processing if pixel is converged
                if (!curr->converged)
                {
                    if (atomic_load(&app_data->is_moving))
                    {
                        // Blend with previous frame to reduce ghosting
                        float alpha = app_data->blend_alpha;
                        if (local->samples == 0) // Skipped pixel
                        {
                            // Use prev directly to avoid zero contribution
                            curr->r = prev->r;
                            curr->g = prev->g;
                            curr->b = prev->b;
                            curr->samples = prev->samples;
                        }
                        else
                        {
                            // Blend for sampled pixels
                            curr->r = local->r * (1.0f - alpha) + prev->r * alpha;
                            curr->g = local->g * (1.0f - alpha) + prev->g * alpha;
                            curr->b = local->b * (1.0f - alpha) + prev->b * alpha;
                            curr->samples = (int)(local->samples * (1.0f - alpha) + prev->samples * alpha);
                            if (curr->samples < 1) curr->samples = 1; // Avoid division by zero
                        }
                    }
                    else
                    {
                        // Accumulate new samples when stationary
                        if (app_data->sample_count == 0)
                        {
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
                }

                // Update previous buffer only for non-converged pixels to preserve converged values
                if (!curr->converged)
                {
                    prev->r = curr->r;
                    prev->g = curr->g;
                    prev->b = curr->b;
                    prev->samples = curr->samples;
                }
            }
        }
    }
}
