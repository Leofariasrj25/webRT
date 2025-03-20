/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   app.h                                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: lfarias- <lfarias-@student.42.rio>         +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/03/13 14:50:58 by lfarias-          #+#    #+#             */
/*   Updated: 2025/03/19 21:29:41 by lfarias-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef APP_H
# define APP_H

# include "types.h"

inline uint32_t xorshift32(t_xorshift32 *rng) 
{
    uint32_t x = rng->state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng->state = x;
    return x;
}

void			close_app(void *arg);

/* *********************** FREE FUNCTIONS ***************************** */

void	destroy_scene(t_scene *scene, int scene_fd);
void	scene_close(int scene_fd);

/* *********************** LOG FUNCTIONS ***************************** */
void	log_msg(char *msg, enum e_loglevel level);
void	log_render_time(long time);
void	log_scene(t_scene *scene);
long	get_currtime_ms(void);

/* ************************ Key ************************************** */
void	key_hook(mlx_key_data_t keydata, void* param);
#endif
