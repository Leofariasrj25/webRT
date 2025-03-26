/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   types.h                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: lfarias- <lfarias-@student.42.rio>         +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/02/09 15:26:53 by lfarias-          #+#    #+#             */
/*   Updated: 2025/03/23 02:26:32 by lfarias-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef TYPES_H
# define TYPES_H

#include "../libs/mlx42/include/MLX42/MLX42.h"
#include <pthread.h>
#include <stdatomic.h>
/* *************************** CONSTANTS ************************************ */

# ifndef M_PI
#  define M_PI 3.14159265358979323846
# endif

# ifndef MLX_KEYSET_SIZE
#  define MLX_KEYSET_SIZE 350
# endif

/* ************************************************************************** */

/* *********************** SPACE RELATED TYPES ****************************** */

typedef struct s_point
{
	double	x;
	double	y;
	double	z;
}	t_point;

typedef struct
{
	uint8_t r;
	uint8_t g;
	uint8_t b;
} t_color;

typedef struct s_ray
{
	t_point	origin;
	t_point	orientation;
}	t_ray;

typedef struct s_intersection
{
	int		exists;
	int		color;
	t_point	location;
	t_point	normal;
	double	distance;
}	t_intersection;

enum	e_element
{
	sphere,
	plane,
	cylinder,
	cone,
	ambient_light,
	camera,
	light,
	nae
};

typedef struct s_a_light
{
	double	ratio;
	int		color;
}	t_a_light;

typedef struct s_camera
{
	t_point	origin;
	t_point	orientation;
	int		fov;
	double	r_fov;
}	t_camera;

typedef struct s_light
{
	t_point	origin;
	double	brightness;
	int		color;
}	t_light;

typedef struct s_sphere
{
	t_point	origin;
	double	diameter;
	int		color;
}	t_sphere;

typedef struct s_plane
{
	t_point	origin;
	t_point	normal;
	int		color;
}	t_plane;

typedef struct s_cylinder
{
	t_point	origin;
	t_point	orientation;
	double	diameter;
	double	height;
	int		color;
}	t_cylinder;

union u_object
{
	t_sphere	sphere;
	t_plane		plane;
	t_cylinder	cylinder;
};

typedef struct s_elist
{
	enum e_element		type;
	union u_object		object;
	struct s_elist		*next;

}	t_elist;

typedef struct s_scene
{
	t_light			*light;
	t_a_light		*a_light;
	t_camera		*camera;
	t_elist			*elements;
	struct s_scene	*next;
	struct s_bvh_node	*root;

}	t_scene;

typedef struct s_aabb {
    t_point min; // Minimum x, y, z
    t_point max; // Maximum x, y, z
} t_aabb;

typedef struct s_bvh_node {
    t_aabb		bounds;
    struct s_bvh_node	*left;
    struct s_bvh_node	*right;
    union u_object	*object;        // Leaf node object (NULL for internal nodes)
    enum e_element	object_type;
} t_bvh_node;

typedef t_aabb (*t_aabb_creator)(void *element);

void		elist_addback(t_elist **lst, t_elist *n);
void		free_elist(t_elist **head);
t_elist		*elist_new(enum e_element type, char **content, int *op_code);

t_ray		get_ray(t_point origin, t_point destination);
t_point		new_point(int x, int y, int z);

int		init_camera(char **attributes, t_camera **camera);
int		init_ambient_light(char **attributes, t_a_light **amb_light);
int		init_sphere(char **attributes, union u_object *sphere);
int		init_cylinder(char **attributes, union u_object *cylinder);
int		init_plane(char **attributes, union u_object *element);
int		init_light(char **attributes, t_light **light);

t_aabb		merge_aabb(t_aabb a, t_aabb b);
t_aabb		create_aabb(union u_object *object, enum e_element type);
t_aabb		create_aabb_plane(void *object);
t_aabb		create_aabb_sphere(void *object);
t_aabb		create_aabb_cylinder(void *object);
t_aabb		create_aabb_cone(void *object);

t_bvh_node	*build_bvh(t_elist *elements); 
void		free_bvh(t_bvh_node *node);
/* ************************************************************************** */

/* ************************ MLX RELATED TYPES ******************************* */

enum e_loglevel {
	INFO,
	WARN,
	ERROR
};

typedef struct {
    uint32_t state;
} t_xorshift32;

typedef struct {
	float	r;
	float	g;
	float	b;
	int	samples;
	bool	converged;
	char	padding[16]; // align
} t_pixel;

typedef struct {
	int	x;
	int	y;
} t_tile;

typedef struct {
	struct s_data		*app_data;
	int			thread_id;
	t_tile			*tiles;
	t_pixel			*local_buffer;
	int			start_tile;
	int			end_tile;
	int			total_pixels;
} t_threaddata;

typedef struct s_data
{
	mlx_t			*engine;
	uint32_t		window_height;
	uint32_t		window_width;
	t_scene			*scene_info;
	int			scene_fd;
	bool			*keys;
	atomic_bool		is_app_running;
	atomic_bool		is_moving;
	bool			stopped_moving;

	// render
	mlx_image_t		*render_image;   // Image being rendered to
	float			**sobol_sequence;
	atomic_int		frame_offset;
	t_pixel			*accum_buffer;
	t_pixel			*prev_accum_buffer;
	atomic_int		sample_count;
	float			blend_alpha;	// used for temporal blending
	uint32_t		converged_pixels;

	// multi-thread 
	t_threaddata		*thread_data;
	pthread_t		*threads;
	pthread_mutex_t		render_mutex;
	pthread_mutex_t		accum_mutex;
	pthread_cond_t		start_render_cond;
	pthread_cond_t		frame_ready_cond;
	atomic_bool		start_rendering;
	atomic_bool		rendering_in_progress;
	atomic_bool		image_displayed;
	atomic_int		threads_done;
} t_appdata;
/* ************************************************************************** */

#endif
