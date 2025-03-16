/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: lfarias- <lfarias-@student.42.rio>         +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/02/12 20:56:33 by lfarias-          #+#    #+#             */
/*   Updated: 2025/03/12 20:12:18 by lfarias-         ###   ########.fr       */
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
float		**generate_sobol_sequence(void);

long start_time = 0;

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
	    scene_filepath = "./scenes/simple_scene3.rt"; //default scene
	#endif

	app_data.scene_info = &scene;
	log_msg("starting application", INFO);

	if (init_scene(&app_data, scene_filepath) != 0 \
		|| init_engine(&app_data) != 0 \
		|| init_render_loop(&app_data) != 0) {
		
		return (2);
	}

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
    const int	    tile_size = 16;
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

    start_time = get_currtime_ms();
    mlx_key_hook(app_data->engine, key_hook, app_data);
    mlx_close_hook(app_data->engine, shutdown, app_data);
    mlx_loop_hook(app_data->engine, render_frame, app_data);
    #ifdef __EMSCRIPTEN__
	emscripten_set_main_loop_arg(emscripten_main_loop, app_data->engine, 0, true);
    #else
	mlx_loop(app_data->engine);
    #endif
	
    // Cleanup
    pthread_mutex_lock(&app_data->render_mutex);
    pthread_cond_broadcast(&app_data->frame_ready_cond);
    pthread_mutex_unlock(&app_data->render_mutex);

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(app_data->threads[i], NULL);
    }

    pthread_mutex_destroy(&app_data->render_mutex);
    pthread_mutex_destroy(&app_data->accum_mutex);
    pthread_cond_destroy(&app_data->frame_ready_cond);
    pthread_cond_destroy(&app_data->start_render_cond);
    mlx_delete_image(app_data->engine, app_data->render_image);
    mlx_delete_image(app_data->engine, app_data->display_image);
    free(app_data->accum_buffer);
    mlx_terminate(app_data->engine);

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
	app_data->engine = mlx_init(SCREEN_WIDTH, SCREEN_HEIGHT, "MiniRT", true);

	if(!app_data->engine) {
		// error handling logic
		puts(mlx_strerror(mlx_errno));
		return(EXIT_FAILURE);
	}

	log_msg("initializing frame buffers", INFO);
	app_data->render_image = mlx_new_image(app_data->engine, SCREEN_WIDTH, SCREEN_HEIGHT);
	app_data->display_image = mlx_new_image(app_data->engine, SCREEN_WIDTH, SCREEN_HEIGHT);
	if (!app_data->display_image || !app_data->render_image) {
		// error handling logic
		puts(mlx_strerror(mlx_errno));
		return(EXIT_FAILURE);
	}
	app_data->refresh_interval = 1.0 / 60.0; // 60 Hz default, adjust as needed
	app_data->accum_buffer = calloc(SCREEN_WIDTH * SCREEN_HEIGHT, sizeof(t_pixel));
	app_data->sample_count = 0;
	app_data->frame_offset = 0;
	app_data->image_displayed = true;
	app_data->sobol_sequence = generate_sobol_sequence();
	log_msg("engine started", INFO);
	return (0);
}

float **generate_sobol_sequence(void)
{
    float   **sobol;

    // Direction numbers for first two dimensions (Joe and Kuo, 32-bit)
    const uint32_t V[2][SOBOL_SIZE] = {
        // Dimension 0: Polynomial x + 1
        { 0x80000000, 0x40000000, 0x20000000, 0x10000000, 0x08000000, 0x04000000, 0x02000000, 0x01000000,
          0x00800000, 0x00400000, 0x00200000, 0x00100000, 0x00080000, 0x00040000, 0x00020000, 0x00010000,
          0x00008000, 0x00004000, 0x00002000, 0x00001000, 0x00000800, 0x00000400, 0x00000200, 0x00000100,
          0x00000080, 0x00000040, 0x00000020, 0x00000010, 0x00000008, 0x00000004, 0x00000002, 0x00000001 },
        // Dimension 1: Polynomial x^3 + x^2 + 1
        { 0x80000000, 0xc0000000, 0x60000000, 0x50000000, 0x28000000, 0x14000000, 0x0a000000, 0x05000000,
          0x02800000, 0x01400000, 0x00a00000, 0x00500000, 0x00280000, 0x00140000, 0x000a0000, 0x00050000,
          0x00028000, 0x00014000, 0x0000a000, 0x00005000, 0x00002800, 0x00001400, 0x00000a00, 0x00000500,
          0x00000280, 0x00000140, 0x000000a0, 0x00000050, 0x00000028, 0x00000014, 0x0000000a, 0x00000005 }
    };

    sobol = malloc(sizeof(float *) * SOBOL_SIZE); 

    if (!sobol)
    {
	return NULL;
    }

    uint32_t x[SOBOL_SIZE] = {0};
    uint32_t y[SOBOL_SIZE] = {0};

    for (uint32_t i = 0; i < SOBOL_SIZE; i++) {
	sobol[i] = malloc(sizeof(float) * 2);

	if (i == 0)
	{
	    sobol[0][0] = 0.0f;
	    sobol[0][1] = 0.0f;
	    continue;
	}

        int j = __builtin_ctz(i);
        x[i] = x[i - 1] ^ V[0][j];
        y[i] = y[i - 1] ^ V[1][j];
        sobol[i][0] = (float)x[i] * (1.0f / 4294967296.0f);
        sobol[i][1] = (float)y[i] * (1.0f / 4294967296.0f);
    }

    return sobol;
}
