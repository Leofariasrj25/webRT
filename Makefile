# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: lfarias- <lfarias-@student.42.rio>         +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2023/02/09 15:25:00 by lfarias-          #+#    #+#              #
#    Updated: 2025/03/17 15:05:45 by lfarias-         ###   ########.fr        #
#                                                                              #
# **************************************************************************** #

#-----PROJECT VARIABLES-----#
NAME    =   miniRT
BUILD   =   UnitTests/build
CC      =   cc
RM      =   rm -rf
CFLAGS  =   -Wall -Wextra -Werror -O3 -g
HEADERS =   -I ./headers -I $(LIBFT_DIR) -I $(MLX_DIR)/include
LIBS    =   -L$(LIBFT_DIR) -lft $(MLX_DIR)/build/libmlx42.a -ldl -lglfw -pthread -lm

#-----LIBFT VARIABLES-----#
LIBFT_DIR   = ./libs/libft
LIBFT       = $(LIBFT_DIR)/libft.a  # Added explicit libft target

#-----MLX VARIABLES-----#
MLX_DIR     = ./libs/mlx42
MLX         = $(MLX_DIR)/build/libmlx42.a  # Added explicit MLX target

#-----C FILES-----#
# (Source file lists remain unchanged)
INPUT = $(addprefix input/, \
    arg_validator.c \
    scene_open.c \
    scene_loader.c \
    scene_checker.c \
    error.c \
    get_element.c \
    get_color.c \
    get_point.c \
    is_normalized.c \
)

TYPES = $(addprefix types/, \
    t_elist.c \
    init_camera.c \
    init_sphere.c \
    init_cylinder.c \
    init_plane.c \
    init_light.c \
    init_ambient_light.c \
    new_point.c \
    get_ray.c \
    t_aabb.c \
    bvh_tree.c \
)

RENDER = $(addprefix render/, \
    transform_scene.c \
    sphere_intersection.c \
    plane_intersection.c \
    cylinder_intersection.c \
    cone_intersection.c \
    render_scene.c \
    sampler.c	   \
    get_px_color.c \
    compute_diffuse.c \
    compute_ambient.c \
    compute_specular.c \
    get_intersection.c \
    move_scene.c \
    rotate_scene.c \
    interactive_z_axis.c \
)

OPERATIONS = $(addprefix operations/, \
    quadratic_solver.c \
    cylinder_quadratic.c \
    cone_quadratic.c \
    vector_operations.c \
    vector_operations2.c \
    cross_product.c \
    transform_vector.c \
    transform_elements.c \
    vtm.c \
    rtm.c \
    matrix.c \
    get_distance.c \
)

APP = $(addprefix app/, \
    scene_close.c \
    destroy_scene.c \
    logger.c \
    timer.c \
    events.c \
)

MAIN    = main.c
SRCS    = $(addprefix sources/, $(MAIN) $(APP) $(INPUT) $(TYPES) $(RENDER) $(OPERATIONS) $(MY_MLX))
OBJS    = $(SRCS:.c=.o)

#-----RULES-----#
all: $(LIBFT) $(MLX) $(NAME)

$(MLX):
	cmake $(MLX_DIR) -B $(MLX_DIR)/build && make -C $(MLX_DIR)/build -j4

$(LIBFT):
	make -C $(LIBFT_DIR)

$(NAME): $(LIBFT) $(MLX) $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(HEADERS) $(LIBS) -o $(NAME)

clean:
	$(RM) $(OBJS)
	make -C $(LIBFT_DIR) clean
	$(RM) $(MLX_DIR)/build

fclean: clean
	$(RM) $(NAME)
	make -C $(LIBFT_DIR) fclean

re: fclean all

%.o: %.c
	$(CC) $(CFLAGS) $(HEADERS) -c $< -o $@

$(BUILD):
	@cd UnitTests && cmake -S . -B build

test: $(LIBFT) $(BUILD)
	@cd UnitTests && cmake --build build && cd build && ctest --output-on-failure

cleantest:
	$(RM) $(BUILD)

retest: cleantest test

run1: $(NAME)
	./$(NAME) ./scenes/scene.rt

LIBFT_WEB = $(LIBFT_DIR)/libft_web.a
LDFLAGS += -s ASYNCIFY -s FETCH=1 -s ASYNCIFY_IMPORTS=['emscripten_set_main_loop_arg']

libft_web:
	@echo "Building libft for WebAssembly"
	@cd $(LIBFT_DIR) && make CC="emcc" CFLAGS="$(CFLAGS)" clean
	@cd $(LIBFT_DIR) && make CC="emcc" CFLAGS="$(CFLAGS)"
	@mv $(LIBFT) $(LIBFT_WEB)

web: libft_web
	@echo "Building MLX42 for WebAssembly"
	@cd $(MLX_DIR) && emcmake cmake -B build_web && emmake make -C build_web -j4
	
	@echo "Compiling miniRT WebAssembly"
	@emcc $(LDFLAGS) -O3 $(HEADERS) -pthread $(SRCS) -o ./web/demo.js \
	    $(MLX_DIR)/build_web/libmlx42.a \
	    $(LIBFT_WEB) \
	    --preload-file ./scenes@/scenes \
	    -s USE_GLFW=3 -s USE_WEBGL2=1 -s FULL_ES3=1 \
	    -s NO_EXIT_RUNTIME=1 -s EXPORTED_RUNTIME_METHODS='["ccall", "cwrap"]' \
	    -s ALLOW_MEMORY_GROWTH=1 -s USE_PTHREADS=1 -s PTHREAD_POOL_SIZE=4

cleanweb:
	@$(RM) $(OBJS)
	make -C $(LIBFT_DIR) clean
	@$(RM) $(MLX_DIR)/build_web
	@$(RM) $(LIBFT_WEB)
	@$(RM) ./web/demo.*

redoweb: clean web

.PHONY: web libft_web cleanweb all clean fclean re test cleantest retest run1 redoweb
