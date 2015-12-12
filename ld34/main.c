/**
 * Ludum Dare 34 entry.
 *
 * Author: Tim Sjöstrand <tim.sjostrand at gmail.com>
 * Date: 2015
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "math4.h"
#include "assets.h"
#include "game.h"
#include "core.h"
#include "core_reload.h"
#include "input.h"
#include "atlas.h"
#include "animatedsprites.h"
#include "color.h"
#include "top-down/tiles.h"

#define VIEW_WIDTH			320
#define VIEW_HEIGHT			92

#define TILES_X				128
#define TILES_Y				128

#define TILE_SIZE			16

#define PLAYER_SPEED_MAX	0.2f
#define PLAYER_ACCELERATION	0.01f
#define	PLAYER_FRICTION		0.008f;

#define SHADER				&assets->shaders.basic_shader
#define TEXTURES			assets->textures.textures

struct game_settings settings = {
	.view_width			= VIEW_WIDTH,
	.view_height		= VIEW_HEIGHT,
	.window_width		= VIEW_WIDTH * 3,
	.window_height		= VIEW_HEIGHT * 3,
	.window_title		= "ld34",
	.sound_listener		= { VIEW_WIDTH / 2.0f, VIEW_HEIGHT / 2.0f, 0.0f },
	.sound_distance_max = 500.0f, // distance3f(vec3(0), sound_listener)
};

struct player {
	struct sprite	sprite;
	struct sprite	shadow;
	vec2			v;
	float			acceleration;
	float			speed_max;
	float			friction;
};

struct game {
	struct player			player;
	struct atlas			atlas;
	struct animatedsprites	*batcher;
	struct anim				anim_idle;
	struct anim				anim_walk_left;
	struct anim				anim_walk_right;
	struct anim				anim_walk_down;
	struct anim				anim_walk_up;
	struct anim				anim_grass;
	struct anim				anim_shadow;
	struct tiles			tiles;
	struct anim				*tiles_data[TILES_X][TILES_Y];
	vec2					view_offset;
	vec2					target_view_offset;
	float					lerp_factor;
} *game = NULL;

struct game_settings* game_get_settings()
{
	return &settings;
}

void game_init_memory(struct shared_memory *shared_memory, int reload)
{
	if(!reload) {
		memset(shared_memory->game_memory, 0, sizeof(struct game));
	}

	game = (struct game *) shared_memory->game_memory;
	core_global = shared_memory->core;
	assets = shared_memory->assets;
	vfs_global = shared_memory->vfs;
	input_global = shared_memory->input;
}

static void friction(struct player *p, float *v, float dt)
{
	if(fabs(*v) < 0.0001f) {
		*v = 0.0f;
	} else if(*v > 0.0f) {
		if(*v - p->friction * dt < 0.0f) {
			*v = 0.0f;
		} else {
			*v -= p->friction * dt;
		}
	} else if(*v < 0.0f) {
		if(*v + p->friction * dt > 0.0f) {
			*v = 0.0f;
		} else {
			*v += p->friction * dt;
		}
	}
}

static void player_think(struct player *p, float dt)
{
	/* Key presses. */
	if(key_down(GLFW_KEY_D)) {
		p->v[0] += p->acceleration * dt;
		animatedsprites_switchanim(&p->sprite, &game->anim_walk_right);
	} else if(key_down(GLFW_KEY_A)) {
		p->v[0] -= p->acceleration * dt;
		animatedsprites_switchanim(&p->sprite, &game->anim_walk_left);
	}

	if(key_down(GLFW_KEY_W)) {
		p->v[1] += p->acceleration * dt;
		animatedsprites_switchanim(&p->sprite, &game->anim_walk_up);
	} else if(key_down(GLFW_KEY_S)) {
		p->v[1] -= p->acceleration * dt;
		animatedsprites_switchanim(&p->sprite, &game->anim_walk_down);
	}

	/* Friction */
	friction(p, &p->v[0], dt);
	friction(p, &p->v[1], dt);

	/* Limits */
	p->v[0] = clamp(p->v[0], -p->speed_max, +p->speed_max);
	p->v[1] = clamp(p->v[1], -p->speed_max, +p->speed_max);

	if(p->v[0] == 0 && p->v[1] == 0) {
		animatedsprites_switchanim(&p->sprite, &game->anim_idle);
	}

	/* Apply movement. */
	p->sprite.position[0] += p->v[0] * dt;
	p->sprite.position[1] += p->v[1] * dt;

	/* Copy shadow */
	set2f(p->shadow.position, p->sprite.position[0], p->sprite.position[1]);
}

void view_offset_think(float dt)
{
	lerp2f(game->view_offset, game->target_view_offset, game->lerp_factor * dt);
}

void follow_view_offset(vec2 dst, vec2 src)
{
	dst[0] = src[0] - VIEW_WIDTH/2.0f;
	dst[1] = src[1] - VIEW_HEIGHT/2.0f;
}

void game_think(struct core *core, struct graphics *g, float dt)
{
	player_think(&game->player, dt);

	/* View offset */
	follow_view_offset(game->target_view_offset, game->player.sprite.position);
	view_offset_think(dt);

	/* Sprites */
	animatedsprites_update(game->batcher, &game->atlas, dt);

	/* Tiles */
	tiles_think(&game->tiles, game->view_offset, &game->atlas, dt);

	/* Shader */
	shader_uniforms_think(&assets->shaders.basic_shader, dt);
}

void game_render(struct core *core, struct graphics *g, float dt)
{
	glClearColor(rgba(COLOR_BLACK));
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	mat4 id;
	identity(id);
	
	tiles_render(&game->tiles, SHADER, g, TEXTURES, id);

	mat4 trans_offset;
	translate(trans_offset, -game->view_offset[0], -game->view_offset[1], 0);
	transpose_same(trans_offset);

	animatedsprites_render(game->batcher, &assets->shaders.basic_shader, g, assets->textures.textures, trans_offset);
}

void game_mousebutton_callback(struct core *core, GLFWwindow *window, int button, int action, int mods)
{
	if(action == GLFW_PRESS) {
		float x = 0, y = 0;
		input_view_get_cursor(window, &x, &y);
		console_debug("Click at %.0fx%.0f\n", x, y);
	}
}

void game_console_init(struct console *c)
{
	console_env_bind_1f(c, "player_speed_max", &(game->player.speed_max));
	console_env_bind_1f(c, "player_friction", &(game->player.friction));
	console_env_bind_1f(c, "player_acceleration", &(game->player.acceleration));
	console_env_bind_1f(c, "player_x", &(game->player.sprite.position[0]));
	console_env_bind_1f(c, "player_y", &(game->player.sprite.position[1]));
	console_env_bind_1f(c, "off_lerp", &(game->lerp_factor));
}

void map_init()
{
	for(int x=0; x<TILES_X; x++) {
		for(int y=0; y<TILES_Y; y++) {
			game->tiles_data[x][y] = &game->anim_grass;
		}
	}
}

static void player_init(struct player *p)
{
	p->speed_max = PLAYER_SPEED_MAX;
	p->acceleration = PLAYER_ACCELERATION;
	p->friction = PLAYER_FRICTION;

	set3f(p->sprite.position, TILES_X*TILE_SIZE/2.0f, TILES_Y*TILE_SIZE/2.0f, 0);
	set2f(p->sprite.scale, 1.0f, 1.0f);
	animatedsprites_switchanim(&p->sprite, &game->anim_idle);
	animatedsprites_add(game->batcher, &p->sprite);

	set2f(p->shadow.scale, 1.0f, 1.0f);
	animatedsprites_switchanim(&p->shadow, &game->anim_shadow);
	animatedsprites_add(game->batcher, &p->shadow);
}

void game_init()
{
	game->lerp_factor = 0.006f;

	/* Create animated sprite batcher. */
	game->batcher = animatedsprites_create();

	/* Create animations. */
	animatedsprites_setanim(&game->anim_idle,		1,  0,  2, 150.0f);
	animatedsprites_setanim(&game->anim_walk_right, 1,  2,  2, 150.0f);
	animatedsprites_setanim(&game->anim_walk_left,	1,  4,  2, 150.0f);
	animatedsprites_setanim(&game->anim_walk_up,	1,  6,  2, 150.0f);
	animatedsprites_setanim(&game->anim_walk_down,	1,  8,  2, 150.0f);
	animatedsprites_setanim(&game->anim_grass,		1, 16,  1, 150.0f);
	animatedsprites_setanim(&game->anim_shadow,		1, 10,  1, 150.0f);

	/* Create sprite. */
	player_init(&game->player);

	/* Create tiles. */
	map_init();
	tiles_init(&game->tiles, (struct anim **) game->tiles_data, TILE_SIZE, VIEW_WIDTH, VIEW_HEIGHT, TILES_X, TILES_Y);
}

void game_key_callback(struct core *core, struct input *input, GLFWwindow *window, int key,
	int scancode, int action, int mods)
{
	if(action == GLFW_PRESS) {
		switch(key) {
			case GLFW_KEY_ESCAPE:
				glfwSetWindowShouldClose(window, 1);
				break;
		}
	}
}

void game_assets_load()
{
	assets_load();
	vfs_register_callback("textures.json", core_reload_atlas, &game->atlas);
}

void game_assets_release()
{
	assets_release();
	atlas_free(&game->atlas);
}

void game_fps_callback(struct frames *f)
{
	core_debug("FPS:% 5d, MS:% 3.1f/% 3.1f/% 3.1f\n", f->frames,
			f->frame_time_min, f->frame_time_avg, f->frame_time_max);
}
