/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: lfarias- <lfarias-@student.42.rio>         +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/02/12 20:56:33 by lfarias-          #+#    #+#             */
/*   Updated: 2025/03/23 02:25:10 by lfarias-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../headers/mini_rt.h"

#include <time.h>
#include <stdio.h>
#include <sched.h>

#ifdef __EMSCRIPTEN__
# include <emscripten/html5.h>
# include <emscripten/emscripten.h>

void emscripten_main_loop(void* arg);
#endif

static int	init_scene(t_appdata *scene, char *filepath);
static int	init_engine(t_appdata *app_data);
static int	init_render_loop(t_appdata *app_data);
static void	shutdown_app(t_appdata *app_data);

int	main(int argc, char **argv)
{
	t_scene		scene;
	char		*scene_filepath;
	t_appdata	app_data;
		
	#ifndef __EMSCRIPTEN__
	    if (!validate_args(argc, argv)) {
		    return (1);
	    }

	    scene_filepath = argv[1];
	#else 
	    scene_filepath = "./scenes/showcase.rt"; //default scene
	#endif

	app_data.scene_info = &scene;
	log_msg("starting application", INFO);

	if (init_scene(&app_data, scene_filepath) != 0 \
		|| init_engine(&app_data) != 0 \
		|| init_render_loop(&app_data) != 0) {
		
		return (2);
	}

	shutdown_app(&app_data);
	return (0);
}

#ifdef __EMSCRIPTEN__
    void emscripten_main_loop(void* arg) {
	mlx_t* mlx = (mlx_t*)arg;
	    log_msg("Render start!", INFO);
	mlx_loop(mlx);
    }
#endif

static int	init_render_loop(t_appdata *app_data) {
    static bool     keyset[MLX_KEYSET_SIZE] = {0};
    const int	    tile_size = TILE_SIZE;
    const int	    total_tiles_x = SCREEN_WIDTH / tile_size;
    const int	    total_tiles_y = SCREEN_HEIGHT / tile_size;
    const int	    total_tiles = total_tiles_x * total_tiles_y;
    const int	    tiles_per_thread = total_tiles / NUM_THREADS;
    t_tile	    *tiles;

    log_msg("start rendering", INFO);
    transform_scene(app_data->scene_info);

    pthread_mutex_init(&app_data->render_mutex, NULL);
    pthread_mutex_init(&app_data->accum_mutex, NULL);
    pthread_cond_init(&app_data->start_render_cond, NULL);
    pthread_cond_init(&app_data->frame_ready_cond, NULL);
    app_data->keys = keyset;
    app_data->start_rendering = true;
    app_data->rendering_in_progress = true;
    atomic_store(&app_data->threads_done, 0);
	
    // calculate the amount of tile (already done on the lines above).
    // create the array of tiles that will be shared across all threads
    tiles = malloc(total_tiles * sizeof(t_tile));

    for (int tile_y = 0; tile_y < total_tiles_y; tile_y++)
    {
	for (int tile_x = 0; tile_x < total_tiles_x; tile_x++)
	{
	   tiles[tile_y * total_tiles_x + tile_x] = (t_tile){tile_x * tile_size, tile_y * tile_size};
	}

    }

    app_data->threads = malloc(sizeof(pthread_t) * NUM_THREADS);
    app_data->thread_data = malloc(sizeof(t_threaddata) * NUM_THREADS);

    for (int i = 0; i < NUM_THREADS; i++) 
    {
	app_data->thread_data[i].thread_id = i + 1;
        app_data->thread_data[i].app_data = app_data;
	app_data->thread_data[i].tiles = tiles;
	app_data->thread_data[i].start_tile = i * tiles_per_thread;
	app_data->thread_data[i].end_tile = (i + 1) * tiles_per_thread;
	app_data->thread_data[i].total_pixels = (app_data->thread_data[i].end_tile - app_data->thread_data[i].start_tile) * tile_size * tile_size;
	app_data->thread_data[i].local_buffer = calloc(app_data->thread_data[i].total_pixels, sizeof(t_pixel));

        pthread_create(&app_data->threads[i], NULL, render_area, &app_data->thread_data[i]);
    }

    mlx_key_hook(app_data->engine, key_hook, app_data);
    mlx_loop_hook(app_data->engine, render_frame, app_data);
    mlx_image_to_window(app_data->engine, app_data->render_image, 0, 0);
    #ifdef __EMSCRIPTEN__
	emscripten_set_main_loop_arg(emscripten_main_loop, app_data->engine, 0, true);
    #else
	mlx_loop(app_data->engine);
    #endif

    return (EXIT_SUCCESS);
}

static int	init_scene(t_appdata *app_data, char* filepath)
{
	app_data->scene_info->a_light = NULL;
	app_data->scene_info->light = NULL;
	app_data->scene_info->camera = NULL;
	app_data->scene_info->elements = NULL;
	app_data->scene_fd = scene_open(filepath);

	if (app_data->scene_fd < 0) {
		return (2);
	}

	log_msg("loading the scene elements", INFO);

	if (scene_load(app_data->scene_fd, app_data->scene_info) || scene_check(app_data->scene_info))
	{
		destroy_scene(app_data->scene_info, app_data->scene_fd);
		return (3);
	}

	return (0);
}

static int	init_engine(t_appdata *app_data)
{
	log_msg("starting the graphics engine", INFO);
	app_data->engine = mlx_init(SCREEN_WIDTH, SCREEN_HEIGHT, "WebRT", true);

	if(!app_data->engine) {
		puts(mlx_strerror(mlx_errno));
		return(EXIT_FAILURE);
	}

	atomic_store(&app_data->is_app_running, true);
	log_msg("initializing frame buffer", INFO);
	app_data->render_image = mlx_new_image(app_data->engine, SCREEN_WIDTH, SCREEN_HEIGHT);
	if (!app_data->render_image) {
		puts(mlx_strerror(mlx_errno));
		return(EXIT_FAILURE);
	}

	app_data->accum_buffer = calloc(SCREEN_WIDTH * SCREEN_HEIGHT, sizeof(t_pixel));
	app_data->prev_accum_buffer = calloc(SCREEN_WIDTH * SCREEN_HEIGHT, sizeof(t_pixel));
	for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++)
	{
	    app_data->prev_accum_buffer[i].r = 0.5f; // Gray
	    app_data->prev_accum_buffer[i].g = 0.5f;
	    app_data->prev_accum_buffer[i].b = 0.5f;
	    app_data->prev_accum_buffer[i].samples = 1;
	    app_data->prev_accum_buffer[i].converged = false;
	}
	
	app_data->sample_count = 0;
	app_data->frame_offset = 0;
	app_data->converged_pixels = 0;
	app_data->image_displayed = true;
	app_data->sobol_sequence = generate_sobol_sequence();
	log_msg("engine started", INFO);
	return (0);
}

static void shutdown_app(t_appdata *app_data)
{
    atomic_store(&app_data->is_app_running, false);

    pthread_mutex_lock(&app_data->render_mutex);
    app_data->start_rendering = true;
    pthread_cond_broadcast(&app_data->start_render_cond); // wake up waiting threads
    pthread_mutex_unlock(&app_data->render_mutex);

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(app_data->threads[i], NULL);
	free(app_data->thread_data[i].local_buffer);
    }

    free(app_data->thread_data[0].tiles);
    free(app_data->threads);
    free(app_data->thread_data);
    pthread_mutex_destroy(&app_data->render_mutex);
    pthread_mutex_destroy(&app_data->accum_mutex);
    pthread_cond_destroy(&app_data->frame_ready_cond);
    pthread_cond_destroy(&app_data->start_render_cond);

    mlx_delete_image(app_data->engine, app_data->render_image);
    free(app_data->accum_buffer);
    free(app_data->prev_accum_buffer);
    
    for (int j = 0; j < SOBOL_SIZE; j++)
    {
	free(app_data->sobol_sequence[j]);
    }

    free(app_data->sobol_sequence);
    destroy_scene(app_data->scene_info, app_data->scene_fd);
    mlx_terminate(app_data->engine);
}
