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
#include "drawable.h"
#include "color.h"
#include "collide.h"

#define xy_of(v) v[0], v[1]

#define VIEW_WIDTH			320
#define VIEW_HEIGHT			180

#define TILES_X				128
#define TILES_Y				128

#define TILE_SIZE			16

#define LERP_FACTOR				0.0015f
#define PLAYER_SPEED_MAX		1.0f
#define PLAYER_ACCELERATION		0.75f
#define	PLAYER_FRICTION			0.5f
#define PLAYER_ROLL_DIST_MAX	92.0f
#define PLAYER_ROLL_SPEED		400.0f

#define HP_MAX				100.0f

#define STA_MAX				100.0f
#define STA_COST_ROLL		15.0f
#define STA_COST_ATTACK		30.0f

#define HP_COST_TOUCH		10.0f
#define HURT_COOLDOWN		500.0f

#define STA_BAR_WIDTH_MAX	(STA_MAX/128.0f)

#define MONSTER_WIDTH		(4*16.0f)
#define MONSTER_HEIGHT		(4*16.0f)

#define PROJECTILE_TTL		5000.0f

#define SHADER				&assets->shaders.basic_shader
#define TEXTURES			assets->textures.textures
#define NO_TEXTURE			&core_global->textures.none

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
	int					invincible;
	int					flinching;
	int					rolling;
	int					attacking;
	struct sprite		sprite;
	struct sprite		shadow;
	struct sprite		sta_bar;
	struct sprite		sta_bar_empty;
	struct sprite		hp_bar;
	struct sprite		hp_bar_empty;
	vec2				v;
	struct rect			hitbox;
	float				stamina;
	float				hitpoints;
	float				acceleration;
	float				speed_max;
	float				friction;
	float				roll_start;			/* When rolling started. */
	vec2				roll_src;			/* Where roll began. */
	vec2				roll_dst;			/* Where to roll to. */
	float				roll_distance_max;	/* How far to roll. */
	float				roll_speed;			/* Time required to complete a roll. */
	float				swing_distance;		/* Sword reach. */
	float				last_hurt;
	struct drawable		draw_hitbox;
	struct drawable		draw_line;
};

struct monster_anims {
	int				phase;
	struct anim		head_left;
	struct anim		head_right;
	struct anim		arm_left;
	struct anim		eye_left;
	struct anim		eye_right;
	struct anim		arm_right;
	struct anim		belly_1;
	struct anim		belly_2;
	struct anim		belly_3;
	struct anim		belly_4;
	struct anim		feet_1;
	struct anim		feet_2;
	struct anim		feet_3;
	struct anim		feet_4;
};

struct monster_sprites {
	struct sprite		head_left;
	struct sprite		head_right;
	struct sprite		arm_left;
	struct sprite		eye_left;
	struct sprite		eye_right;
	struct sprite		arm_right;
	struct sprite		belly_1;
	struct sprite		belly_2;
	struct sprite		belly_3;
	struct sprite		belly_4;
	struct sprite		feet_1;
	struct sprite		feet_2;
	struct sprite		feet_3;
	struct sprite		feet_4;
};

struct projectile {
	int					dead;
	float				spawned;
	vec2				v;
	struct sprite		sprite;
	struct rect			hitbox;
};

struct monster_phase0 {
	int			hand_toggle;
	float		last_projectile;
};

struct monster {
	int						phase;
	struct animatedsprites	*batcher;
	float					scale;
	struct monster_anims	anims;
	struct monster_sprites	sprites;
	struct rect				hitbox;
	struct drawable			draw_hitbox;
	vec2					base_pos;
	struct alist			*projectiles;
	struct animatedsprites	*projectiles_batch;
	struct monster_phase0	phase0;
};

struct game {
	float					time;
	struct player			player;
	struct atlas			atlas;
	struct animatedsprites	*batcher;
	struct animatedsprites	*ui;
	struct anim				anim_idle;
	struct anim				anim_walk_left;
	struct anim				anim_walk_right;
	struct anim				anim_walk_down;
	struct anim				anim_walk_up;
	struct anim				anim_grass;
	struct anim				anim_shadow;
	struct anim				anim_bar_sta;
	struct anim				anim_bar_empty;
	struct anim				anim_bar_hp;
	struct anim				anim_projectile;
	struct monster			monster;
	struct tiles			tiles;
	struct anim				*tiles_data[TILES_X][TILES_Y];
	vec2					view_offset;
	vec2					target_view_offset;
	float					lerp_factor;
	float					debug;
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

static float distancef(float x, float y)
{
	return sqrtf((x*x) + (y*y));
}

static void player_facing_angle(struct player *p, float *angle, float *distance)
{
	float mx = 0, my = 0;
	input_view_get_cursor(core_global->graphics.window, &mx, &my);
	
	float world_mouse_x = mx + game->view_offset[0];
	float world_mouse_y = my + game->view_offset[1];

	float dx = world_mouse_x - p->hitbox.pos[0];
	float dy = world_mouse_y - p->hitbox.pos[1];

	if(distance != NULL) {
		*distance = distancef(dx, dy);
	}
	*angle = atan2f(dy, dx);
}

void player_push(struct player *p, float angle, float dist_max)
{
	float roll_dst_x = p->hitbox.pos[0] + cosf(angle) * dist_max;
	float roll_dst_y = p->hitbox.pos[1] + sinf(angle) * dist_max;

	p->roll_start = game->time;
	set2f(p->roll_src, xy_of(p->hitbox.pos));
	set2f(p->roll_dst, roll_dst_x, roll_dst_y);
	p->rolling = 1;
}

void player_flinch(struct player *p, vec2 pos, float knockback)
{
	float dx = p->hitbox.pos[0] - pos[0];
	float dy = p->hitbox.pos[1] - pos[1];

	float angle = atan2f(dy, dx);

	/* Push player back from monster. */
	p->flinching = 1;
	player_push(p, angle, knockback);
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

void camera_set(vec2 src, int immediately)
{
	float x = src[0] - VIEW_WIDTH/2.0f;
	float y = src[1] - VIEW_HEIGHT/2.0f;

	if(immediately) {
		set2f(game->view_offset, x, y);
		set2f(game->target_view_offset, x, y);
	} else {
		set2f(game->target_view_offset, x, y);
	}
}

static void player_think(struct player *p, float dt)
{
	if(p->hitpoints <= 0) {
		return;
	}

	int follow = 0;
	float sta_regen = 0.0f;

	if(p->rolling || p->flinching) {
		/* Still rolling? */
		float roll_dt = (game->time - p->roll_start) / p->roll_speed;

		/* Invincibility frames. */
		if(!p->flinching && (roll_dt >= 0.2f && roll_dt <= 0.7f)) {
			p->invincible = 1;
		} else {
			p->invincible = 0;
		}

		// TODO: roll anims

		if(roll_dt > 1.0f) {
			p->rolling = 0;
			p->flinching = 0;
			printf("roll complete\n");
		} else {
			p->sprite.position[0] = p->roll_src[0] + (p->roll_dst[0] - p->roll_src[0]) * sinf( roll_dt * M_PI / 2.0f );
			p->sprite.position[1] = p->roll_src[1] + (p->roll_dst[1] - p->roll_src[1]) * sinf( roll_dt * M_PI / 2.0f );
		}
	}

	if(!p->rolling) {
		/* Key presses. */
		if(key_down(GLFW_KEY_D)) {
			p->v[0] += p->acceleration * dt;
			follow = 1;
		} else if(key_down(GLFW_KEY_A)) {
			p->v[0] -= p->acceleration * dt;
			follow = 1;
		}

		if(key_down(GLFW_KEY_W)) {
			p->v[1] += p->acceleration * dt;
			follow = 1;
		} else if(key_down(GLFW_KEY_S)) {
			p->v[1] -= p->acceleration * dt;
			follow = 1;
		}

		float facing_angle = 0;
		float dist_to_cursor = 0;
		player_facing_angle(p, &facing_angle, &dist_to_cursor);

		/* Set direction animation. */
		float facing_x = cosf(facing_angle);
		float facing_y = sinf(facing_angle);
		if(fabs(facing_x) > fabs(facing_y)) {
			if(facing_x < 0) {
				animatedsprites_switchanim(&p->sprite, &game->anim_walk_left);
			} else {
				animatedsprites_switchanim(&p->sprite, &game->anim_walk_right);
			}
		} else {
			if(facing_y < 0) {
				animatedsprites_switchanim(&p->sprite, &game->anim_walk_down);
			} else {
				animatedsprites_switchanim(&p->sprite, &game->anim_walk_up);
			}
		}
		
		/* Friction */
		friction(p, &p->v[0], dt);
		friction(p, &p->v[1], dt);

		/* Limits */
		p->v[0] = clamp(p->v[0], -p->speed_max, +p->speed_max);
		p->v[1] = clamp(p->v[1], -p->speed_max, +p->speed_max);

		/* Idle? */
		if(p->v[0] == 0 && p->v[1] == 0) {
			animatedsprites_switchanim(&p->sprite, &game->anim_idle);
			sta_regen = 0.01f;
		} else {
			sta_regen = 0.001f;
		}

		/* Apply movement. */
		p->sprite.position[0] += p->v[0];
		p->sprite.position[1] += p->v[1];
	}

	/* Gets hurt? */
	if((game->time - p->last_hurt >= HURT_COOLDOWN)) {
		if(!p->invincible) {
			if(collide_rect(&p->hitbox, &game->monster.hitbox)) {
				printf("ouch!\n");
				p->hitpoints -= HP_COST_TOUCH;
				p->last_hurt = game->time;
				player_flinch(p, &game->monster.hitbox.pos, 32.0f);
			}
		}

		p->hitpoints = clamp(p->hitpoints, 0, HP_MAX);
	}

	/* Refill stamina. */
	p->stamina += sta_regen * dt;
	p->stamina = clamp(p->stamina, 0, STA_MAX);
	
	/* Update UI elements */
	p->sta_bar.scale[0] = p->stamina;
	p->hp_bar.scale[0] = p->hitpoints;

	if(follow) {
		camera_set(game->player.sprite.position, 0);
	}

	/* Propagate position */
	set2f(p->shadow.position, p->sprite.position[0], p->sprite.position[1]);
	set2f(p->hitbox.pos, p->sprite.position[0], p->sprite.position[1]);
}

void view_offset_think(float dt)
{
	lerp2f(game->view_offset, game->target_view_offset, game->lerp_factor * dt);
}

void projectile_init(struct projectile *p, float x, float y,
		float vx, float vy, struct animatedsprites *batch)
{
	p->spawned = game->time;
	set2f(p->hitbox.pos, x, y);
	set2f(p->sprite.position, x, y);
	set2f(p->v, vx, vy);
	set2f(p->sprite.scale, 1.0, 1.0);
	animatedsprites_switchanim(&p->sprite, &game->anim_projectile);
}

void monster_spawn_projectile(struct monster *m, float x, float y, float vx, float vy)
{
	struct projectile *p = calloc(1, sizeof(struct projectile));
	projectile_init(p, x, y, vx, vy, m->projectiles_batch);
	alist_append(m->projectiles, p);
}

void projectile_think(struct projectile *p, float dt)
{
	if(p->dead) {
		return;
	}

	float age = game->time - p->spawned;

	if(age >= PROJECTILE_TTL) {
		p->dead = 1;
		return;
	}

	/* Wobble */
	if(age <= 200.0f) {
		p->sprite.scale[0] = 0.35 + (age / 200.0f * 0.75f);
		p->sprite.scale[1] = 0.35 + (age / 200.0f * 0.75f);
	} else {
		p->sprite.scale[0] = 1.0f + sinf(game->time * 0.01f) * 0.2f + sinf(game->time * 0.001f) * 0.1f;
		p->sprite.scale[1] = 1.0f + sinf(M_PI + game->time * 0.01f) * 0.2f + sinf(M_PI + game->time * 0.001f) * 0.1f;
	}

	/* Move */
	p->hitbox.pos[0] += p->v[0] * dt;
	p->hitbox.pos[1] += p->v[1] * dt;

	set2f(p->hitbox.size, p->sprite.scale[0] * 16.0f, p->sprite.scale[1] * 16.0f);
	set2f(p->sprite.position, xy_of(p->hitbox.pos));
}

void projectiles_think(struct monster *m, float dt)
{
	animatedsprites_clear(m->projectiles_batch);

	foreach_alist(struct projectile *, p, i, m->projectiles) {
		projectile_think(p, dt);

		if(p->dead) {
			alist_delete_at(m->projectiles, i, 1);
			i--;
		} else {
			animatedsprites_add(m->projectiles_batch, &p->sprite);
		}
	}
}

float angle_to(vec2 a, vec2 b)
{
	return atan2f(a[0]-b[0], a[1]-b[1]);
}

void monster_think_phase0(struct monster *m, float dt)
{
	struct monster_phase0 *p = &m->phase0;

	if(game->time - p->last_projectile >= 250.0f) {
		//float x = m->hitbox.pos[0] + p->hand_toggle ? -26.0f : 26.0f;
		float x = m->hitbox.pos[0] + (p->hand_toggle ? -26.0f : 26.0f);
		float y = m->hitbox.pos[1] + 8.0f;
		p->hand_toggle = !p->hand_toggle;

		/* Target player */
		vec2 src = { x, y };
		float angle = angle_to(&game->player.hitbox.pos, &src);

		monster_spawn_projectile(m, x, y, sinf(angle) * 0.05f, cosf(angle) * 0.05f);
		p->last_projectile = game->time;
	}
}

void monster_think_phase1(struct monster *m, float dt)
{
}

void monster_think_phase2(struct monster *m, float dt)
{
}

void monster_think(struct monster *m, float dt)
{
	m->hitbox.pos[0] = m->base_pos[0] + sinf(game->time*0.0005) * 16.0f + sinf(game->time*0.00025) * 8.0f;
	m->hitbox.pos[1] = m->base_pos[1] + cosf(game->time*0.0005) * 16.0f + sinf(game->time*0.00025) * 8.0f;

	switch(m->phase) {
		case 0:
			monster_think_phase0(m, dt);
			break;
		case 1:
			monster_think_phase1(m, dt);
			break;
		case 2:
			monster_think_phase2(m, dt);
			break;
	}

	projectiles_think(m, dt);
}

void game_think(struct core *core, struct graphics *g, float dt)
{
	/* FIXME: float is too tiny for time. */
	game->time += dt;

	/* Go player! */
	player_think(&game->player, dt);

	/* Go monster! */
	monster_think(&game->monster, dt);

	/* View offset */
	view_offset_think(dt);

	/* Sprites */
	animatedsprites_update(game->batcher, &game->atlas, dt);
	animatedsprites_update(game->ui, &game->atlas, dt);
	animatedsprites_update(game->monster.batcher, &game->atlas, dt);
	animatedsprites_update(game->monster.projectiles_batch, &game->atlas, dt);

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
	
	/* Tiles */
	tiles_render(&game->tiles, SHADER, g, TEXTURES, id);

	/* View */
	mat4 view;
	translate(view, -game->view_offset[0], -game->view_offset[1], 0);

	/* Monster */
	mat4 monster_view;
	mat4 monster_translate;
	translate(monster_translate,
			game->monster.hitbox.pos[0] - (MONSTER_WIDTH/2.0f - TILE_SIZE/2.0f) * game->monster.scale ,
			game->monster.hitbox.pos[1] - (MONSTER_HEIGHT/2.0f - TILE_SIZE/2.0f) * game->monster.scale ,
			0.0f);
	mult_same(monster_translate, view);
	
	mat4 monster_scale;
	scale(monster_scale, game->monster.scale, game->monster.scale, 1.0f);

	copym(monster_view, monster_translate);
	mult_same(monster_view, monster_scale);

	/* Transpose for OpenGL. */
	transpose_same(view);
	transpose_same(monster_view);

	/* Render */
	animatedsprites_render(game->batcher, SHADER, g, TEXTURES, view);
	animatedsprites_render(game->monster.batcher, SHADER, g, TEXTURES, monster_view);
	animatedsprites_render(game->monster.projectiles_batch, SHADER, g, TEXTURES, view);
	animatedsprites_render(game->ui, SHADER, g, TEXTURES, id);

	if(game->debug) {
		struct player *p = &game->player;
		drawable_new_rect_outline(&p->draw_hitbox, &p->hitbox, SHADER);
		drawable_render(&p->draw_hitbox, SHADER, g, NO_TEXTURE, p->invincible ? COLOR_RED : COLOR_BLACK, view);

		float view_x, view_y;
		input_view_get_cursor(core_global->graphics.window, &view_x, &view_y);
		view_x += game->view_offset[0];
		view_y += game->view_offset[1];
		drawable_new_linef(&p->draw_line, xy_of(p->hitbox.pos), view_x, view_y, SHADER);
		drawable_render(&p->draw_line, SHADER, g, NO_TEXTURE, COLOR_MAGENTA, view);

		/* Monster */
		drawable_new_rect_outline(&game->monster.draw_hitbox, &game->monster.hitbox, SHADER);
		drawable_render(&game->monster.draw_hitbox, SHADER, g, NO_TEXTURE, COLOR_CYAN, view);
	}
}

void player_roll(struct player *p, float angle)
{
	if(p->stamina < STA_COST_ROLL) {
		printf("out of stamina (%f)\n", p->stamina);
		return;
	}
	if(p->rolling || p->attacking) {
		printf("busy\n");
		return;
	}

	p->stamina -= STA_COST_ROLL;

	player_push(p, angle, p->roll_distance_max);
}

void player_attack(struct player *p, float angle)
{
	if(p->stamina < STA_COST_ATTACK) {
		printf("out of stamina\n");
		return;
	}
	if(p->rolling || p->attacking) {
		printf("action busy\n");
		return;
	}

	p->stamina -= STA_COST_ATTACK;
	
	float d = 100; //distance2f( p->hitbox.pos, enemy->hitbox.pos );

	if(d <= p->swing_distance) {
		printf("hit!\n");
	}
}

void game_mousebutton_callback(struct core *core, GLFWwindow *window, int button, int action, int mods)
{
	if(action == GLFW_PRESS) {
		float mx = 0, my = 0;
		input_view_get_cursor(window, &mx, &my);
		
		struct player *p = &game->player;

		float world_mouse_x = mx + game->view_offset[0];
		float world_mouse_y = my + game->view_offset[1];

		float dx = world_mouse_x - p->hitbox.pos[0];
		float dy = world_mouse_y - p->hitbox.pos[1];

		/* Dont do anything in deadzone */
		if(distancef(dx, dy) <= 8.0f) {
			printf("cursor in deadzone\n");
			return;
		}

		float angle = atan2f(dy, dx);

		switch(button) {
			case GLFW_MOUSE_BUTTON_LEFT:
				player_attack(p, angle);
				break;
			case GLFW_MOUSE_BUTTON_RIGHT:
				player_roll(p, angle);
				break;
		}
	}
}

void game_console_init(struct console *c)
{
	console_env_bind_1f(c, "player_speed_max", &(game->player.speed_max));
	console_env_bind_1f(c, "player_friction", &(game->player.friction));
	console_env_bind_1f(c, "player_acceleration", &(game->player.acceleration));
	console_env_bind_1f(c, "player_x", &(game->player.sprite.position[0]));
	console_env_bind_1f(c, "player_y", &(game->player.sprite.position[1]));
	console_env_bind_1f(c, "player_roll_dist", &(game->player.roll_distance_max));
	console_env_bind_1f(c, "player_roll_speed", &(game->player.roll_speed));
	
	console_env_bind_1f(c, "monster_x", &(game->monster.hitbox.pos[0]));
	console_env_bind_1f(c, "monster_y", &(game->monster.hitbox.pos[1]));
	console_env_bind_1f(c, "monster_scale", &(game->monster.scale));
	
	console_env_bind_1f(c, "off_lerp", &(game->lerp_factor));
	console_env_bind_1f(c, "debug", &(game->debug));
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
	p->stamina = STA_MAX;
	p->hitpoints = HP_MAX;
	p->speed_max = PLAYER_SPEED_MAX;
	p->acceleration = PLAYER_ACCELERATION;
	p->friction = PLAYER_FRICTION;
	p->roll_distance_max = PLAYER_ROLL_DIST_MAX;
	p->roll_speed = PLAYER_ROLL_SPEED;

	/* Player sprite */
	set3f(p->sprite.position, TILES_X*TILE_SIZE/2.0f - 96.0f, TILES_Y*TILE_SIZE/2.0f - 96.0f, 0);
	set2f(p->sprite.scale, 1.0f, 1.0f);
	animatedsprites_switchanim(&p->sprite, &game->anim_idle);
	animatedsprites_add(game->batcher, &p->sprite);

	/* Shadow */
	set2f(p->shadow.scale, 1.0f, 1.0f);
	animatedsprites_switchanim(&p->shadow, &game->anim_shadow);
	animatedsprites_add(game->batcher, &p->shadow);

	/* Stamina bar */
	set3f(p->sta_bar_empty.position, STA_MAX/2.0f + 4.0f, VIEW_HEIGHT - 8.0f - 16/2.0f, 0);
	set2f(p->sta_bar_empty.scale, STA_MAX, 1.0f);
	animatedsprites_switchanim(&p->sta_bar_empty, &game->anim_bar_empty);
	animatedsprites_add(game->ui, &p->sta_bar_empty);

	set3f(p->sta_bar.position, STA_MAX/2.0f + 4.0f, VIEW_HEIGHT - 8.0f - 16/2.0f, 0);
	set2f(p->sta_bar.scale, STA_MAX, 1.0f);
	animatedsprites_switchanim(&p->sta_bar, &game->anim_bar_sta);
	animatedsprites_add(game->ui, &p->sta_bar);

	/* HP bar */
	set3f(p->hp_bar_empty.position, HP_MAX/2.0f + 4.0f, VIEW_HEIGHT - 16/2.0f, 0);
	set2f(p->hp_bar_empty.scale, HP_MAX, 1.0f);
	animatedsprites_switchanim(&p->hp_bar_empty, &game->anim_bar_empty);
	animatedsprites_add(game->ui, &p->hp_bar_empty);

	set3f(p->hp_bar.position, HP_MAX/2.0f + 4.0f, VIEW_HEIGHT - 16/2.0f, 0);
	set2f(p->hp_bar.scale, HP_MAX, 1.0f);
	animatedsprites_switchanim(&p->hp_bar, &game->anim_bar_hp);
	animatedsprites_add(game->ui, &p->hp_bar);

	/* Hitbox */
	set2f(p->hitbox.pos, p->sprite.position[0], p->sprite.position[1]);
	set2f(p->hitbox.size, 16, 16);

	camera_set(p->sprite.position, 1);
}

void monster_anims_init(struct monster_anims *a)
{
	animatedsprites_setanim(&a->head_left,		1, atlas_frame_index(&game->atlas, "monster_head_left"),	1, 150.0f);
	animatedsprites_setanim(&a->head_right,		1, atlas_frame_index(&game->atlas, "monster_head_right"),	1, 150.0f);
	animatedsprites_setanim(&a->arm_left,		1, atlas_frame_index(&game->atlas, "monster_arm_left_1"),	2, 150.0f);
	animatedsprites_setanim(&a->eye_left,		1, atlas_frame_index(&game->atlas, "monster_eye_left"),		1, 150.0f);
	animatedsprites_setanim(&a->eye_right,		1, atlas_frame_index(&game->atlas, "monster_eye_right"),	1, 150.0f);
	animatedsprites_setanim(&a->arm_right,		1, atlas_frame_index(&game->atlas, "monster_arm_right_1"),	2, 150.0f);
	animatedsprites_setanim(&a->belly_1,		1, atlas_frame_index(&game->atlas, "monster_belly_1"),		1, 150.0f);
	animatedsprites_setanim(&a->belly_2,		1, atlas_frame_index(&game->atlas, "monster_belly_2"),		1, 150.0f);
	animatedsprites_setanim(&a->belly_3,		1, atlas_frame_index(&game->atlas, "monster_belly_3"),		1, 150.0f);
	animatedsprites_setanim(&a->belly_4,		1, atlas_frame_index(&game->atlas, "monster_belly_4"),		1, 150.0f);
	animatedsprites_setanim(&a->feet_1,			1, atlas_frame_index(&game->atlas, "monster_feet_1"),		2, 200.0f);
	animatedsprites_setanim(&a->feet_2,			1, atlas_frame_index(&game->atlas, "monster_feet_2"),		2, 200.0f);
	animatedsprites_setanim(&a->feet_3,			1, atlas_frame_index(&game->atlas, "monster_feet_3"),		2, 200.0f);
	animatedsprites_setanim(&a->feet_4,			1, atlas_frame_index(&game->atlas, "monster_feet_4"),		2, 200.0f);

	animatedsprites_setanim(&game->anim_projectile,		1, atlas_frame_index(&game->atlas, "projectile_1"),			3, 50.0f);
}

#define monster_switchanim(m, x) animatedsprites_switchanim(&m->sprites.x, &m->anims.x)

void monster_init(struct monster *m)
{
	struct monster_sprites *s = &m->sprites;

	/* Create batcher. */
	m->scale = 1.0f;
	set2f(m->base_pos, TILES_X*TILE_SIZE/2.0f, TILES_Y*TILE_SIZE/2.0f);
	set2f(m->hitbox.size, MONSTER_WIDTH * m->scale, MONSTER_HEIGHT * m->scale);
	m->batcher = animatedsprites_create();

	/* Position */
	set2f(s->head_left.position,	16 * 1,	16 * 3);
	set2f(s->head_right.position,	16 * 2,	16 * 3);
	
	set2f(s->arm_left.position,		16 * 0, 16 * 2);
	set2f(s->eye_left.position,		16 * 1, 16 * 2);
	set2f(s->eye_right.position,	16 * 2, 16 * 2);
	set2f(s->arm_right.position,	16 * 3, 16 * 2);
	
	set2f(s->belly_1.position,		16 * 0, 16 * 1);
	set2f(s->belly_2.position,		16 * 1, 16 * 1);
	set2f(s->belly_3.position,		16 * 2, 16 * 1);
	set2f(s->belly_4.position,		16 * 3, 16 * 1);

	set2f(s->feet_1.position,		16 * 0, 16 * 0);
	set2f(s->feet_2.position,		16 * 1, 16 * 0);
	set2f(s->feet_3.position,		16 * 2, 16 * 0);
	set2f(s->feet_4.position,		16 * 3, 16 * 0);

	/* Scale */
	set2f(s->head_left.scale,		1.0f,	1.0f);
	set2f(s->head_right.scale,		1.0f,	1.0f);
	set2f(s->arm_left.scale,		1.0f,	1.0f);
	set2f(s->eye_left.scale,		1.0f,	1.0f);
	set2f(s->eye_right.scale,		1.0f,	1.0f);
	set2f(s->arm_right.scale,		1.0f,	1.0f);
	set2f(s->belly_1.scale,			1.0f,	1.0f);
	set2f(s->belly_2.scale,			1.0f,	1.0f);
	set2f(s->belly_3.scale,			1.0f,	1.0f);
	set2f(s->belly_4.scale,			1.0f,	1.0f);
	set2f(s->feet_1.scale,			1.0f,	1.0f);
	set2f(s->feet_2.scale,			1.0f,	1.0f);
	set2f(s->feet_3.scale,			1.0f,	1.0f);
	set2f(s->feet_4.scale,			1.0f,	1.0f);

	monster_anims_init(&m->anims);

	/* Start animations. */
	monster_switchanim(m, head_left);
	monster_switchanim(m, head_right);
	monster_switchanim(m, arm_left);
	monster_switchanim(m, eye_left);
	monster_switchanim(m, eye_right);
	monster_switchanim(m, arm_right);
	monster_switchanim(m, belly_1);
	monster_switchanim(m, belly_2);
	monster_switchanim(m, belly_3);
	monster_switchanim(m, belly_4);
	monster_switchanim(m, feet_1);
	monster_switchanim(m, feet_2);
	monster_switchanim(m, feet_3);
	monster_switchanim(m, feet_4);

	/* Add sprites to batcher */
	animatedsprites_add(m->batcher, &s->head_left);
	animatedsprites_add(m->batcher, &s->head_right);
	animatedsprites_add(m->batcher, &s->arm_left);
	animatedsprites_add(m->batcher, &s->eye_left);
	animatedsprites_add(m->batcher, &s->eye_right);
	animatedsprites_add(m->batcher, &s->arm_right);
	animatedsprites_add(m->batcher, &s->belly_1);
	animatedsprites_add(m->batcher, &s->belly_2);
	animatedsprites_add(m->batcher, &s->belly_3);
	animatedsprites_add(m->batcher, &s->belly_4);
	animatedsprites_add(m->batcher, &s->feet_1);
	animatedsprites_add(m->batcher, &s->feet_2);
	animatedsprites_add(m->batcher, &s->feet_3);
	animatedsprites_add(m->batcher, &s->feet_4);

	/* Projectile(s) */
	if(m->projectiles == NULL) {
		m->projectiles = alist_new(128);
	} else {
		alist_clear(m->projectiles, 1);
	}

	if(m->projectiles_batch == NULL) {
		m->projectiles_batch = animatedsprites_create();
	} else {
		animatedsprites_clear(m->projectiles_batch);
	}
}

void player_anims_init()
{
	animatedsprites_setanim(&game->anim_idle,		1,  0,  2, 150.0f);
	animatedsprites_setanim(&game->anim_walk_right, 1,  2,  2, 150.0f);
	animatedsprites_setanim(&game->anim_walk_left,	1,  4,  2, 150.0f);
	animatedsprites_setanim(&game->anim_walk_up,	1,  6,  2, 150.0f);
	animatedsprites_setanim(&game->anim_walk_down,	1,  8,  2, 150.0f);
	animatedsprites_setanim(&game->anim_shadow,		1, 10,  1, 150.0f);
	animatedsprites_setanim(&game->anim_bar_hp,		1, 11,  1, 150.0f);
	animatedsprites_setanim(&game->anim_bar_empty,	1, 12,  1, 150.0f);
	animatedsprites_setanim(&game->anim_bar_sta,	1, 13,  1, 150.0f);
}

void game_init()
{
	game->debug = 1.0f;
	game->lerp_factor = LERP_FACTOR;

	/* Create animated sprite batcher. */
	if(game->batcher == NULL) {
		game->batcher = animatedsprites_create();
	}
	if(game->ui == NULL) {
		game->ui = animatedsprites_create();
	}

	/* Create animations. */
	player_anims_init();
	animatedsprites_setanim(&game->anim_grass, 1, 16,  1, 150.0f);

	/* Create sprite. */
	player_init(&game->player);

	/* Create monster. */
	monster_init(&game->monster);

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
