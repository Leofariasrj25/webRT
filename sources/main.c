/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: lfarias- <lfarias-@student.42.rio>         +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/02/12 20:56:33 by lfarias-          #+#    #+#             */
/*   Updated: 2023/04/17 11:44:43 by lfarias-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../headers/mini_rt.h"

#include <time.h>
#include <stdio.h>

#ifdef __EMSCRIPTEN__
# include <emscripten/html5.h>
# include <emscripten/emscripten.h>

void emscripten_main_loop(void* arg);
#endif

static int	init_scene(t_appdata *scene, char *filepath);
static int	init_engine(t_appdata *app_data);
static int	init_render_loop(t_appdata *app_data);

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
    int             strip_height;

    log_msg("start rendering", INFO);
    transform_scene(app_data->scene_info);

    pthread_mutex_init(&app_data->render_mutex, NULL);
    pthread_cond_init(&app_data->start_render_cond, NULL);
    pthread_cond_init(&app_data->frame_ready_cond, NULL);
    app_data->keys = keyset;
    app_data->start_rendering = true;
    app_data->rendering_in_progress = true;
    atomic_store(&app_data->threads_done, 0);
	
    strip_height = SCREEN_HEIGHT / NUM_THREADS;

    for (int i = 0; i < NUM_THREADS; i++) {
	app_data->thread_data[i].thread_id = i + 1;
        app_data->thread_data[i].app_data = app_data;
        app_data->thread_data[i].start_y = i * strip_height;
        app_data->thread_data[i].end_y = (i + 1) * strip_height;
        if (i == NUM_THREADS - 1) {
            app_data->thread_data[i].end_y = SCREEN_HEIGHT; // Cover remainder
        }

        pthread_create(&app_data->threads[i], NULL, render_area, &app_data->thread_data[i]);
    }

    mlx_key_hook(app_data->engine, key_hook, app_data);
    mlx_loop_hook(app_data->engine, trigger_render, app_data);
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
	app_data->accum_buffer = calloc(SCREEN_WIDTH * SCREEN_HEIGHT * 4, sizeof(float));  
	app_data->variance_buffer = calloc(SCREEN_WIDTH * SCREEN_HEIGHT, sizeof(float));
	app_data->pixel_sample_counts = calloc(SCREEN_WIDTH * SCREEN_HEIGHT, sizeof(uint32_t));
	app_data->sample_count = 0;
	app_data->image_displayed = true;
	log_msg("engine started", INFO);
	return (0);
}
