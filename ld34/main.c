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
#include "sound.h"

#define xy_of(v) v[0], v[1]

#define VIEW_WIDTH				320
#define VIEW_HEIGHT				180

#define TILES_X					128
#define TILES_Y					128

#define TILE_SIZE				16

#define LERP_FACTOR				0.0015f

#define PLAYER_SPEED_MAX		0.05f
#define PLAYER_ACCELERATION		(PLAYER_SPEED_MAX/2.0f)
#define	PLAYER_FRICTION			(PLAYER_SPEED_MAX/4.0f)
#define PLAYER_ATTACK_WIDTH		16.0f		/* When facing up/down */
#define PLAYER_ATTACK_HEIGHT	24.0f
#define PLAYER_ATTACK_TIME		250.0f
#define PLAYER_ATTACK_DMG		15.0f

#define PLAYER_ROLL_DIST_MAX	92.0f
#define PLAYER_ROLL_SPEED		400.0f
#define PLAYER_ROLL_DODGE_START	0.1f
#define PLAYER_ROLL_DODGE_END	0.7f

#define HP_MAX					100.0f

#define STA_MAX					100.0f
#define STA_COST_ROLL			15.0f
#define STA_COST_ATTACK_MAX		30.0f
#define STA_COST_ATTACK_MIN		10.0f

#define HP_COST_TOUCH			10.0f
#define HP_COST_PROJECTILE		25.0f
#define HURT_COOLDOWN			500.0f

#define STA_BAR_WIDTH_MAX		(STA_MAX/128.0f)

#define MONSTER_HP_MAX			500.0f
#define MONSTER_WIDTH			(4*16.0f)
#define MONSTER_HEIGHT			(4*16.0f)
#define MONSTER_PHASE0_RANGE	(VIEW_HEIGHT)

#define PROJECTILE_TTL			5000.0f
#define PROJECTILE_EXPLODE_TIME 150.0f

#define GAME_STATE_MENU			0
#define GAME_STATE_PLAY			1
#define GAME_STATE_OVER			2
#define GAME_STATE_WIN			3

#define SCREEN_SHAKE_TIME			500.0f
#define SCREEN_SHAKE_POWER_ATTACK	0.25f
#define SCREEN_SHAKE_BIG_DMG		1.0f
#define SCREEN_SHAKE_SMALL_DMG		0.25f

#define DIR_DOWN	0
#define DIR_UP		1
#define DIR_LEFT	2
#define DIR_RIGHT	3

#define SHADER					&assets->shaders.basic_shader
#define TEXTURES				assets->textures.textures
#define NO_TEXTURE				&core_global->textures.none

struct game_settings settings = {
	.view_width			= VIEW_WIDTH,
	.view_height		= VIEW_HEIGHT,
	.window_width		= VIEW_WIDTH * 3,
	.window_height		= VIEW_HEIGHT * 3,
	.window_title		= "ld34",
	.sound_listener		= { 0, 0, 0.0f },
	.sound_distance_max = 9999999999.0f, // distance3f(vec3(0), sound_listener)
};

struct player_anims {
	struct anim		attack_left;
	struct anim		attack_right;
	struct anim		attack_up;
	struct anim		attack_down;
	struct anim		arrow;
};

struct player_weapon {
	float				last_attack;
	struct sprite		attack;
	struct rect			attack_hitbox;
	struct drawable		draw_attack_hitbox;
	int					miss;
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
	struct player_weapon weapon;
	struct player_anims	anims;
	struct basic_sprite	arrow;

	/* Tunables. */
	float				sta_cost_roll;
	float				sta_cost_attack_max;
	float				sta_cost_attack_min;
	float				attack_time;
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
	struct anim		shadow_1;
	struct anim		shadow_2;
	struct anim		shadow_3;
	struct anim		shadow_4;
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
	struct sprite		shadow_1;
	struct sprite		shadow_2;
	struct sprite		shadow_3;
	struct sprite		shadow_4;
};

struct projectile {
	int					dead;
	float				spawned;
	vec2				v;
	struct sprite		sprite;
	struct rect			hitbox;
	float				explode_time;
};

struct monster_phase0 {
	int					sequence;
	float				sequence_start;
	int					hand_toggle;
	float				last_projectile;
	int					circles_fired;
	float				spiral_angle;
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
	float					hitpoints;
	struct sprite			hp_bar;
	struct sprite			hp_bar_empty;
};

struct state_menu {
	float					time;
	struct basic_sprite		sprite;
	struct rect				btn_start;
	struct rect				btn_quit;
	struct drawable			draw_start;
	int						quit_selected;
};

struct state_over {
	float					time;
	struct basic_sprite		sprite;
};

struct state_win {
	float					time;
	struct basic_sprite		sprite;
};

struct game {
	float					time;
	float					started;
	struct player			player;
	struct atlas			atlas;
	struct animatedsprites	*batcher;
	struct animatedsprites	*ui;
	struct anim				anim_idle_left;
	struct anim				anim_idle_right;
	struct anim				anim_idle_up;
	struct anim				anim_idle_down;
	struct anim				anim_walk_left;
	struct anim				anim_walk_right;
	struct anim				anim_walk_down;
	struct anim				anim_walk_up;
	struct anim				anim_grass_1;
	struct anim				anim_grass_2;
	struct anim				anim_grass_3;
	struct anim				anim_grass_stone;
	struct anim				anim_grass_sand_1;
	struct anim				anim_grass_sand_2;
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
	float					screen_shake_start;
	float					screen_shake_power;
	vec2					sound_pos;

	/* State */
	think_func_t			think;
	render_func_t			render;
	struct state_menu		state_menu;
	struct state_over		state_over;
	struct state_win		state_win;
} *game = NULL;

/**** Game states ****/
void game_state_menu_think(struct core *core, struct graphics *g, float dt);
void game_state_menu_render(struct core *core, struct graphics *g, float dt);

void game_state_play_think(struct core *core, struct graphics *g, float dt);
void game_state_play_render(struct core *core, struct graphics *g, float dt);

void game_state_over_think(struct core *core, struct graphics *g, float dt);
void game_state_over_render(struct core *core, struct graphics *g, float dt);

void game_state_win_think(struct core *core, struct graphics *g, float dt);
void game_state_win_render(struct core *core, struct graphics *g, float dt);
/************/

struct game_settings* game_get_settings()
{
	return &settings;
}

void game_set_state(int state)
{
	core_global->graphics.delta_time_factor = 1.0f;

	switch(state) {
		case GAME_STATE_MENU:
			game->think = &game_state_menu_think;
			game->render = &game_state_menu_render;
			break;
		case GAME_STATE_PLAY:
			game->started = game->time;
			game->think = &game_state_play_think;
			game->render = &game_state_play_render;
			break;
		case GAME_STATE_OVER:
			game->think = &game_state_over_think;
			game->render = &game_state_over_render;
			break;
		case GAME_STATE_WIN:
			game->think = &game_state_win_think;
			game->render = &game_state_win_render;
			break;
		default:
			errorf("Game", "Unknown game state: %d\n", state);
			break;
	}
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

static float distance2f(vec2 a, vec2 b)
{
	return distancef(b[0]-a[0], b[0]-a[0]);
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

	sound_buf_play_pitched(&core_global->sound, assets->sounds.hurt, game->sound_pos, 0.2f);
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

float game_elapsed(float before)
{
	return game->time - before;
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

void screen_shake(float power)
{
	game->screen_shake_start = game->time;
	game->screen_shake_power = power;
}


int player_facing_dir()
{
	float facing_angle = 0;
	float dist_to_cursor = 0;
	player_facing_angle(&game->player, &facing_angle, &dist_to_cursor);

	/* Set direction animation. */
	float facing_x = cosf(facing_angle);
	float facing_y = sinf(facing_angle);
	if(fabs(facing_x) > fabs(facing_y)) {
		if(facing_x < 0) {
			return DIR_LEFT;
		} else {
			return DIR_RIGHT;
		}
	} else {
		if(facing_y < 0) {
			return DIR_DOWN;
		} else {
			return DIR_UP;
		}
	}

	return DIR_DOWN;
}

void player_weapon_think(struct player *p, float dt)
{
	struct player_weapon *w = &p->weapon;

	/* Update graphics */
	float attack_off_x = 0;
	float attack_off_y = 0;
	int player_dir = player_facing_dir();

	switch(player_facing_dir()) {
		case DIR_DOWN:
			attack_off_x = 0;
			attack_off_y = -8;
			animatedsprites_switchanim(&p->weapon.attack, &p->anims.attack_down);
			set2f(p->weapon.attack_hitbox.size, PLAYER_ATTACK_WIDTH, PLAYER_ATTACK_HEIGHT);
			break;
		case DIR_UP:
			attack_off_x = 0;
			attack_off_y = 8;
			animatedsprites_switchanim(&p->weapon.attack, &p->anims.attack_up);
			set2f(p->weapon.attack_hitbox.size, PLAYER_ATTACK_WIDTH, PLAYER_ATTACK_HEIGHT);
			break;
		case DIR_LEFT:
			attack_off_x = -8;
			attack_off_y = 0;
			animatedsprites_switchanim(&p->weapon.attack, &p->anims.attack_left);
			set2f(p->weapon.attack_hitbox.size, PLAYER_ATTACK_HEIGHT, PLAYER_ATTACK_WIDTH);
			break;
		case DIR_RIGHT:
			attack_off_x = 8;
			attack_off_y = 0;
			animatedsprites_switchanim(&p->weapon.attack, &p->anims.attack_right);
			set2f(p->weapon.attack_hitbox.size, PLAYER_ATTACK_HEIGHT, PLAYER_ATTACK_WIDTH);
			break;
	}
	set2f(p->weapon.attack_hitbox.pos, p->sprite.position[0] + attack_off_x * 1.5f, p->sprite.position[1] + attack_off_y *1.5f);
	set2f(p->weapon.attack.position, p->sprite.position[0] + attack_off_x, p->sprite.position[1] + attack_off_y);

	/* Attack logic. */
	if(w->last_attack != 0 && (game->time - w->last_attack <= p->attack_time)) {
		p->attacking = 1;

		if(w->miss && collide_rect(&w->attack_hitbox, &game->monster.hitbox)) {
			printf("hit!\n");
			w->miss = 0;
			game->monster.hitpoints -= PLAYER_ATTACK_DMG;
			screen_shake(SCREEN_SHAKE_POWER_ATTACK);
			sound_buf_play_pitched(&core_global->sound, assets->sounds.attack, game->sound_pos, 0.2f);
		}

		/* Show effect */
		if(w->miss) {
			w->attack.scale[0] = p->weapon.attack_hitbox.size[0] / 16.0f * 1.5f;
			w->attack.scale[1] = p->weapon.attack_hitbox.size[1] / 16.0f * 1.5f;
		} else {
			w->attack.scale[0] = p->weapon.attack_hitbox.size[0] / 16.0f * 1.5f;
			w->attack.scale[1] = p->weapon.attack_hitbox.size[1] / 16.0f * 1.5f;
		}
	} else if(p->attacking == 1) {
		/* attacking changes to 0 here. */
		p->attacking = 0;

		if(w->miss) {
			printf("missed - stamina penalty\n");
			p->stamina -= (p->sta_cost_attack_max - p->sta_cost_attack_min);
			sound_buf_play_pitched(&core_global->sound, assets->sounds.miss, game->sound_pos, 0.2f);
		}

		/* Hide effect */
		w->attack.scale[0] = 0;
		w->attack.scale[1] = 0;
	}
}

static void player_think(struct player *p, float dt)
{
	if(p->hitpoints <= 0) {
		game_set_state(GAME_STATE_OVER);
		return;
	}

	int follow = 0;
	float sta_regen = 0.0f;

	if(p->rolling || p->flinching) {
		/* Still rolling? */
		float roll_dt = (game->time - p->roll_start) / p->roll_speed;

		/* Invincibility frames. */
		if(!p->flinching && (roll_dt >= PLAYER_ROLL_DODGE_START  && roll_dt <= PLAYER_ROLL_DODGE_END)) {
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
			switch(player_facing_dir()) {
				case DIR_DOWN:
					animatedsprites_switchanim(&p->sprite, &game->anim_idle_down);
					break;
				case DIR_UP:
					animatedsprites_switchanim(&p->sprite, &game->anim_idle_up);
					break;
				case DIR_LEFT:
					animatedsprites_switchanim(&p->sprite, &game->anim_idle_left);
					break;
				case DIR_RIGHT:
					animatedsprites_switchanim(&p->sprite, &game->anim_idle_right);
					break;
			}
			sta_regen = 0.02f;
		} else {
			/* Stamina regen penalty. */
			sta_regen = 0.005f;
		}

		/* Apply movement. */
		p->sprite.position[0] += p->v[0] * dt;
		p->sprite.position[1] += p->v[1] * dt;
	}

	/* Gets hurt? */
	if((game->time - p->last_hurt >= HURT_COOLDOWN)) {
		if(!p->invincible) {
			/* Touches monster? */
			if(collide_rect(&p->hitbox, &game->monster.hitbox)) {
				printf("ouch!\n");
				p->hitpoints -= HP_COST_TOUCH;
				p->last_hurt = game->time;
				player_flinch(p, &game->monster.hitbox.pos, 32.0f);
				screen_shake(SCREEN_SHAKE_BIG_DMG);
			}

			/* Touches projectile? */
			foreach_alist(struct projectile *, projectile, i, game->monster.projectiles) {
				/* Ignore if dead or exploding. */
				if(projectile->dead || projectile->explode_time != 0) {
					continue;
				}
				if(collide_rect(&p->hitbox, &projectile->hitbox)) {
					printf("collide with projectile\n");
					projectile->explode_time = game->time;
					set2f(projectile->v, 0, 0);
					p->hitpoints -= HP_COST_PROJECTILE;
					p->last_hurt = game->time;
					player_flinch(p, &projectile->hitbox.pos, 16.0f);
					screen_shake(SCREEN_SHAKE_SMALL_DMG);
				}
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

	/* Do attack logic. */
	player_weapon_think(p, dt);
}

void view_offset_think(float dt)
{
	lerp2f(game->view_offset, game->target_view_offset, game->lerp_factor * dt);

	/* HACK: Player arrow */
	set4f(game->player.arrow.pos, -game->view_offset[0] + game->player.sprite.position[0], -game->view_offset[1] + game->player.sprite.position[1] - 2.0f, 0.0f, 1.0f);
	float player_angle = 0;
	player_facing_angle(&game->player, &player_angle, 0);
	game->player.arrow.rotation = player_angle;

	if(game_elapsed(game->screen_shake_start) <= SCREEN_SHAKE_TIME) {
		game->view_offset[0] += randr(-game->screen_shake_power, +game->screen_shake_power) * dt;
		game->view_offset[1] += randr(-game->screen_shake_power, +game->screen_shake_power) * dt;
		game->screen_shake_power = game->screen_shake_power * (1.0f - game_elapsed(game->screen_shake_start) / SCREEN_SHAKE_TIME);
	}
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

void monster_spawn_projectile(struct monster *m, float x, float y, float vx, float vy, int sound)
{
	struct projectile *p = calloc(1, sizeof(struct projectile));
	projectile_init(p, x, y, vx, vy, m->projectiles_batch);
	alist_append(m->projectiles, p);

	if(sound) {
		sound_buf_play_detailed(&core_global->sound, assets->sounds.laser, game->sound_pos, game->sound_pos, 0, 0.5f, randr(0.8f, 1.0f), 0);
	}
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

	if(p->explode_time != 0) {
		float explode_dt = (game->time - p->explode_time) / PROJECTILE_EXPLODE_TIME;
		if(explode_dt < 1.0f) {
			// TODO: maybe custm anim here?
			p->sprite.scale[0] = (1.0f + explode_dt) * 1.5f;
			p->sprite.scale[1] = (1.0f + explode_dt) * 1.5f;
		} else {
			p->dead = 1;
		}
	} else if(age <= 200.0f) {
		/* Spawn anim */
		p->sprite.scale[0] = 0.35 + (age / 200.0f * 0.75f);
		p->sprite.scale[1] = 0.35 + (age / 200.0f * 0.75f);
	} else {
		/* Wobble anim */
		p->sprite.scale[0] = 1.0f + sinf(game->time * 0.01f) * 0.2f + sinf(game->time * 0.001f) * 0.1f;
		p->sprite.scale[1] = 1.0f + sinf(M_PI + game->time * 0.01f) * 0.2f + sinf(M_PI + game->time * 0.001f) * 0.1f;
	}

	/* Move */
	p->hitbox.pos[0] += p->v[0] * dt;
	p->hitbox.pos[1] += p->v[1] * dt;

	/* Slightly smaller than image */
	set2f(p->hitbox.size, p->sprite.scale[0] * 8.0f, p->sprite.scale[1] * 12.0f);
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

void projectiles_clear(struct monster *m)
{
	animatedsprites_clear(game->monster.projectiles_batch);
	alist_clear(game->monster.projectiles, 1);
}

float angle_from_to(vec2 a, vec2 b)
{
	return atan2f(a[0]-b[0], a[1]-b[1]);
}

#define MONSTER_PHASE0_SPAM_INTERVAL 400.0f

void monster_think_phase0(struct monster *m, float dt)
{
	struct monster_phase0 *phase = &m->phase0;

	float player_dist = distance2f(m->hitbox.pos, game->player.hitbox.pos);

	if(player_dist <= MONSTER_PHASE0_RANGE) {
		if(phase->sequence >= 3) {
			phase->sequence = 0;
		}

		switch(phase->sequence) {
			case 0:
				/* 1st circle */
				if(phase->circles_fired == 0 && game_elapsed(phase->sequence_start) >= 800.0f) {
					/* Ring of projectiles. */
					for(int i=0; i<8; i++) {
						float vx = sinf(i * (2.0f * M_PI / 8.0f)) * 0.025f;
						float vy = cosf(i * (2.0f * M_PI / 8.0f)) * 0.025f;
						monster_spawn_projectile(m, m->hitbox.pos[0], m->hitbox.pos[1], vx, vy, i==0);
					}
					phase->circles_fired = 1;
				}

				/* 2nd circle */
				if(phase->circles_fired == 1 && game_elapsed(phase->sequence_start) >= 1600.0f) {
					/* Ring of projectiles. */
					for(int i=0; i<8; i++) {
						float vx = sinf(M_PI/4.0f + i * (2.0f * M_PI / 8.0f)) * 0.025f;
						float vy = cosf(M_PI/4.0f + i * (2.0f * M_PI / 8.0f)) * 0.025f;
						monster_spawn_projectile(m, m->hitbox.pos[0], m->hitbox.pos[1], vx, vy, i==0);
					}
					phase->circles_fired = 2;
				}

				/* 3rd circle */
				if(phase->circles_fired == 2 && game_elapsed(phase->sequence_start) >= 2400.0f) {
					/* Ring of projectiles. */
					for(int i=0; i<8; i++) {
						float vx = sinf(M_PI/8.0f + i * (2.0f * M_PI / 8.0f)) * 0.025f;
						float vy = cosf(M_PI/8.0f + i * (2.0f * M_PI / 8.0f)) * 0.025f;
						monster_spawn_projectile(m, m->hitbox.pos[0], m->hitbox.pos[1], vx, vy, i==0);
					}
					phase->circles_fired = 3;
				}

				/* A little downtime before proceeding. */
				if(game_elapsed(phase->sequence_start) >= 5000.0f) {
					printf("next sequence\n");
					phase->circles_fired = 0;
					phase->sequence ++;
					phase->sequence_start = game->time;
				}
				break;
			case 1:
				/* Spam projectiles for 5 sec */
				if(game_elapsed(phase->sequence_start) >= 500.0f) {
					if(game->time - phase->last_projectile >= MONSTER_PHASE0_SPAM_INTERVAL) {
						float x = m->hitbox.pos[0] + (phase->hand_toggle ? -26.0f : 26.0f);
						float y = m->hitbox.pos[1] + 8.0f;
						phase->hand_toggle = !phase->hand_toggle;

						/* Target player */
						vec2 src = { x, y };
						float angle = angle_from_to(&game->player.hitbox.pos, &src);

						monster_spawn_projectile(m, x, y, sinf(angle) * 0.05f, cosf(angle) * 0.05f, 1);
						phase->last_projectile = game->time;
					}

					if(game_elapsed(phase->sequence_start) >= 5000.0f) {
						printf("next sequence\n");
						phase->sequence ++;
						phase->sequence_start = game->time;
					}
				}
				break;
			case 2:
				/* Slow spiral */
				if(game_elapsed(phase->sequence_start) >= 1000.0f) {
					if(game_elapsed(phase->last_projectile) >= 100.0f) {
						monster_spawn_projectile(m, m->hitbox.pos[0], m->hitbox.pos[1],
								cosf(phase->spiral_angle) * 0.035f,
								sinf(phase->spiral_angle) * 0.035f, 1);
						phase->last_projectile = game->time;
						phase->spiral_angle += (2*M_PI) / 16.0f;
					}
				}

				if(game_elapsed(phase->sequence_start) >= 5000.0f) {
					printf("next sequence\n");
					phase->sequence ++;
					phase->sequence_start = game->time;
				}
				break;
		}
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
	/* Is dead? */
	if(m->hitpoints <= 0.0f) {
		game_set_state(GAME_STATE_WIN);
	} else {
		/* Shuffle around */
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

	/* Update UI */
	m->hp_bar.scale[0] = MONSTER_WIDTH * (m->hitpoints / MONSTER_HP_MAX);

	/* Sync shadow position */
	struct monster_sprites *s = &m->sprites;
	set2f(s->shadow_1.position,		m->hitbox.pos[0] - 16 * 1.5f, m->hitbox.pos[1] + 16 * -2.0f);
	set2f(s->shadow_2.position,		m->hitbox.pos[0] - 16 * 0.5f, m->hitbox.pos[1] + 16 * -2.0f);
	set2f(s->shadow_3.position,		m->hitbox.pos[0] + 16 * 0.5f, m->hitbox.pos[1] + 16 * -2.0f);
	set2f(s->shadow_4.position,		m->hitbox.pos[0] + 16 * 1.5f, m->hitbox.pos[1] + 16 * -2.0f);
}

void game_state_play_think(struct core *core, struct graphics *g, float dt)
{
	/* FIXME: float is too tiny for time. */
	game->time += dt;

	/* Go to player if not already there. */
	if(game_elapsed(game->started) >= 3000.0f) {
		camera_set(game->player.sprite.position, 0);
	}

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
}

void game_reset()
{
	game_init();
}

void game_state_over_think(struct core *core, struct graphics *g, float dt)
{
	game_state_play_think(core, g, dt);

	struct state_over *state = &game->state_over;
	state->time += dt;

	/* Wobble wobble */
	state->sprite.scale[0] = VIEW_WIDTH + (1 + sinf(state->time * 0.005f)) * 2.0f;
	state->sprite.scale[1] = VIEW_HEIGHT + (1 + sinf(state->time * 0.0025f)) * 2.0f;

	if(key_pressed(GLFW_KEY_SPACE)) {
		game_reset();
		game_set_state(GAME_STATE_MENU);
	}
}

void game_state_over_render(struct core *core, struct graphics *g, float dt)
{
	glClearColor(rgba(COLOR_BLACK));
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	game_state_play_render(core, g, dt);

	sprite_render(&game->state_over.sprite, SHADER, g);
}

#define WIN_FADE_IN_TIME	1000.0f

void game_state_win_think(struct core *core, struct graphics *g, float dt)
{
	game_state_play_think(core, g, dt);

	struct state_win *state = &game->state_win;

	state->time += dt;

	/* Fade in text */
	float fade_in = clamp(state->time/WIN_FADE_IN_TIME, 0.0f, 1.0f);
	if(fade_in < 1.0f) {
		state->sprite.scale[0] = VIEW_WIDTH * sinf(M_PI/2.0f * fade_in);
		state->sprite.scale[1] = VIEW_HEIGHT * sinf(M_PI/2.0f * fade_in);
	} else {
		/* Wobble */
		//state->sprite.scale[0] = VIEW_WIDTH + sinf((state->time - WIN_FADE_IN_TIME) * 0.01f) * 8.0f;
		//state->sprite.scale[1] = VIEW_HEIGHT + sinf((state->time - WIN_FADE_IN_TIME) * 0.001f) * 8.0f;
	}

	/* Slow down time */
	float fade_out = clamp(state->time/4000.0f, 0.0f, 1.0f);
	g->delta_time_factor = 1.0f - fade_out;

	/* Destroy dangerous projectiles. */
	projectiles_clear(&game->monster);

	if(key_pressed(GLFW_KEY_SPACE)) {
		game_reset();
		game_set_state(GAME_STATE_MENU);
	}
}

void game_state_win_render(struct core *core, struct graphics *g, float dt)
{
	glClearColor(rgba(COLOR_BLACK));
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	game_state_play_render(core, g, dt);

	sprite_render(&game->state_win.sprite, SHADER, g);
}

void game_state_menu_think(struct core *core, struct graphics *g, float dt)
{
	struct state_menu *menu = &game->state_menu;

	menu->time += dt;

	/* Wobble wobble */
	menu->sprite.scale[0] = VIEW_WIDTH + (1.0f + sinf(menu->time * 0.008f)) * 1.5f; 
	menu->sprite.scale[1] = VIEW_HEIGHT + (1.0f + sinf(menu->time * 0.004f)) * 1.5f; 

	if(key_pressed(GLFW_KEY_DOWN)
			|| key_pressed(GLFW_KEY_W)
			|| key_pressed(GLFW_KEY_S)
			|| key_pressed(GLFW_KEY_UP)) {
		sound_buf_play(&core_global->sound, assets->sounds.select, game->sound_pos);
		menu->quit_selected = !menu->quit_selected;
		menu->sprite.texture = menu->quit_selected ? &assets->textures.menu_quit : &assets->textures.menu_start;
	}

	if(key_pressed(GLFW_KEY_ENTER)
			|| key_pressed(GLFW_KEY_SPACE)) {
		if(menu->quit_selected) {
			glfwSetWindowShouldClose(g->window, 1);
		} else {
			sound_buf_play(&core_global->sound, assets->sounds.coin, game->sound_pos);
			/* Switch to game play state */
			game_set_state(GAME_STATE_PLAY);
		}
	}
}

void game_state_play_render(struct core *core, struct graphics *g, float dt)
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
	sprite_render(&game->player.arrow, SHADER, g);
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

		drawable_new_rect_outline(&p->weapon.draw_attack_hitbox, &p->weapon.attack_hitbox, SHADER);
		drawable_render(&p->weapon.draw_attack_hitbox, SHADER, g, NO_TEXTURE, COLOR_YELLOW, view);

		/* Monster */
		drawable_new_rect_outline(&game->monster.draw_hitbox, &game->monster.hitbox, SHADER);
		drawable_render(&game->monster.draw_hitbox, SHADER, g, NO_TEXTURE, COLOR_CYAN, view);
	}
}

void game_state_menu_render(struct core *core, struct graphics *g, float dt)
{
	glClearColor(rgba(COLOR_BLACK));
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	sprite_render(&game->state_menu.sprite, SHADER, g);
}

void game_think(struct core *core, struct graphics *g, float dt)
{
	/* Sanity check. */
	if(game->think == NULL) {
		game_set_state(GAME_STATE_MENU);
	}
	
	/* Run game state. */
	game->think(core, g, dt);

	/* Shader */
	shader_uniforms_think(&assets->shaders.basic_shader, dt);
}

void game_render(struct core *core, struct graphics *g, float dt)
{
	/* Sanity check. */
	if(game->render == NULL) {
		game_set_state(GAME_STATE_MENU);
	}
	
	/* Run game state. */
	game->render(core, g, dt);
}

void player_roll(struct player *p, float angle)
{
	if(p->stamina < p->sta_cost_roll) {
		printf("out of stamina (%f)\n", p->stamina);
		return;
	}
	if(p->rolling || p->attacking) {
		printf("busy\n");
		return;
	}

	p->stamina -= p->sta_cost_roll;

	player_push(p, angle, p->roll_distance_max);
	sound_buf_play_pitched(&core_global->sound, assets->sounds.roll, game->sound_pos, 0.2f);
}

void player_attack(struct player *p, float angle)
{
	if(p->stamina < p->sta_cost_attack_min) {
		printf("out of stamina\n");
		return;
	}
	if(p->rolling || p->attacking) {
		printf("action busy\n");
		return;
	}

	p->stamina -= p->sta_cost_attack_min;
	p->weapon.miss = 1;
	p->weapon.last_attack = game->time;
	sound_buf_play_pitched(&core_global->sound, assets->sounds.swing, game->sound_pos, 0.2f);
}

void game_mousebutton_callback(struct core *core, GLFWwindow *window, int button, int action, int mods)
{
	if(action == GLFW_PRESS) {
		if(game->think == &game_state_play_think) {
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
		} else if(game->think == &game_state_menu_think) {
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
	console_env_bind_1f(c, "player_hp", &(game->player.hitpoints));
	console_env_bind_1f(c, "sta_cost_roll", &(game->player.sta_cost_roll));
	console_env_bind_1f(c, "sta_cost_attack_max", &(game->player.sta_cost_attack_max));
	console_env_bind_1f(c, "sta_cost_attack_min", &(game->player.sta_cost_attack_min));
	console_env_bind_1f(c, "player_attack_time", &(game->player.attack_time));
	
	console_env_bind_1f(c, "monster_x", &(game->monster.hitbox.pos[0]));
	console_env_bind_1f(c, "monster_y", &(game->monster.hitbox.pos[1]));
	console_env_bind_1f(c, "monster_scale", &(game->monster.scale));
	console_env_bind_1f(c, "monster_hp", &(game->monster.hitpoints));
	
	console_env_bind_1f(c, "off_lerp", &(game->lerp_factor));
	console_env_bind_1f(c, "debug", &(game->debug));
}

void map_init()
{
	for(int x=0; x<TILES_X; x++) {
		for(int y=0; y<TILES_Y; y++) {
			if(rand() % 100 == 0) {
				game->tiles_data[x][y] = &game->anim_grass_2;
			} else if(rand() % 100 == 0) {
				game->tiles_data[x][y] = &game->anim_grass_3;
			} else if(rand() % 200 == 0) {
				game->tiles_data[x][y] = &game->anim_grass_stone;
			} else if(rand() % 300 == 0) {
				game->tiles_data[x][y] = &game->anim_grass_sand_1;
			} else if(rand() % 300 == 0) {
				game->tiles_data[x][y] = &game->anim_grass_sand_2;
			} else {
				game->tiles_data[x][y] = &game->anim_grass_1;
			}
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
	p->sta_cost_roll = STA_COST_ROLL;
	p->sta_cost_attack_max = STA_COST_ATTACK_MAX;
	p->sta_cost_attack_min = STA_COST_ATTACK_MIN;
	p->attack_time = PLAYER_ATTACK_TIME;

	/* Attack animation */
	set2f(p->weapon.attack.scale, 0.0f, 0.0f);
	animatedsprites_switchanim(&p->weapon.attack, &p->anims.attack_left);
	animatedsprites_add(game->batcher, &p->weapon.attack);

	/* Shadow */
	set2f(p->shadow.scale, 1.0f, 1.0f);
	animatedsprites_switchanim(&p->shadow, &game->anim_shadow);
	animatedsprites_add(game->batcher, &p->shadow);

	/* Player sprite */
	set3f(p->sprite.position, TILES_X*TILE_SIZE/2.0f, TILES_Y*TILE_SIZE/2.0f - MONSTER_PHASE0_RANGE - 64.0f, 0);
	set2f(p->sprite.scale, 1.0f, 1.0f);
	animatedsprites_switchanim(&p->sprite, &game->anim_idle_down);
	animatedsprites_add(game->batcher, &p->sprite);

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
	set2f(p->hitbox.size, 11, 11);

	/* Attack hitbox */
	set2f(p->weapon.attack_hitbox.pos, p->sprite.position[0], p->sprite.position[1]);
	set2f(p->weapon.attack_hitbox.size, 16, 8);

	/* Arrow */
	p->arrow.type = 0;
	p->arrow.texture = &assets->textures.arrow;
	set4f(p->arrow.pos, p->hitbox.pos[0], p->hitbox.pos[1], 0.0f, 1.0f);
	set4f(p->arrow.scale, 48, 16, 1.0f, 1.0f);
	copyv(p->arrow.color, COLOR_WHITE);
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
	animatedsprites_setanim(&a->shadow_1,		1, atlas_frame_index(&game->atlas, "monster_shadow_1"),		1, 200.0f);
	animatedsprites_setanim(&a->shadow_2,		1, atlas_frame_index(&game->atlas, "monster_shadow_2"),		1, 200.0f);
	animatedsprites_setanim(&a->shadow_3,		1, atlas_frame_index(&game->atlas, "monster_shadow_3"),		1, 200.0f);
	animatedsprites_setanim(&a->shadow_4,		1, atlas_frame_index(&game->atlas, "monster_shadow_4"),		1, 200.0f);


	animatedsprites_setanim(&game->anim_projectile,		1, atlas_frame_index(&game->atlas, "projectile_1"),			3, 50.0f);
}

#define monster_switchanim(m, x) animatedsprites_switchanim(&m->sprites.x, &m->anims.x)

void monster_init(struct monster *m)
{
	struct monster_sprites *s = &m->sprites;

	m->hitpoints = MONSTER_HP_MAX;

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
	set2f(s->shadow_1.scale,		1.0f,	1.0f);
	set2f(s->shadow_2.scale,		1.0f,	1.0f);
	set2f(s->shadow_3.scale,		1.0f,	1.0f);
	set2f(s->shadow_4.scale,		1.0f,	1.0f);

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
	monster_switchanim(m, shadow_1);
	monster_switchanim(m, shadow_2);
	monster_switchanim(m, shadow_3);
	monster_switchanim(m, shadow_4);

	/* HP bar */
	float hp_bar_x = MONSTER_WIDTH / 2.0f - TILE_SIZE/2.0f;
	float hp_bar_y = MONSTER_HEIGHT + 0.0f;

	set3f(m->hp_bar_empty.position, hp_bar_x, hp_bar_y, 0);
	set2f(m->hp_bar_empty.scale, MONSTER_WIDTH, 1.0f);
	animatedsprites_switchanim(&m->hp_bar_empty, &game->anim_bar_empty);

	set3f(m->hp_bar.position, hp_bar_x, hp_bar_y, 0);
	set2f(m->hp_bar.scale, MONSTER_WIDTH, 1.0f);
	animatedsprites_switchanim(&m->hp_bar, &game->anim_bar_hp);

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

	animatedsprites_add(m->batcher, &m->hp_bar_empty);
	animatedsprites_add(m->batcher, &m->hp_bar);

	animatedsprites_add(game->batcher, &s->shadow_1);
	animatedsprites_add(game->batcher, &s->shadow_2);
	animatedsprites_add(game->batcher, &s->shadow_3);
	animatedsprites_add(game->batcher, &s->shadow_4);

	/* Projectile(s) */
	if(m->projectiles != NULL) {
		alist_free(m->projectiles, 1);
	}
	m->projectiles = alist_new(128);

	if(m->projectiles_batch != NULL) {
		animatedsprites_destroy(m->projectiles_batch);
	}
	m->projectiles_batch = animatedsprites_create();
}

void player_anims_init(struct player_anims *a)
{
	/* Use 1 frame from walk frames for idle anim. */
	animatedsprites_setanim(&game->anim_idle_left,	1, atlas_frame_index(&game->atlas, "player_walk_left_2"),	1, 300.0f);
	animatedsprites_setanim(&game->anim_idle_right,	1, atlas_frame_index(&game->atlas, "player_walk_right_2"),	1, 300.0f);
	animatedsprites_setanim(&game->anim_idle_up,	1, atlas_frame_index(&game->atlas, "player_walk_up_2"),		1, 300.0f);
	animatedsprites_setanim(&game->anim_idle_down,	1, atlas_frame_index(&game->atlas, "player_idle_1"),		2, 300.0f);

	animatedsprites_setanim(&game->anim_walk_right, 1, atlas_frame_index(&game->atlas, "player_walk_right_1"),	4, 150.0f);
	animatedsprites_setanim(&game->anim_walk_left,	1, atlas_frame_index(&game->atlas, "player_walk_left_1"),	4, 150.0f);
	animatedsprites_setanim(&game->anim_walk_up,	1, atlas_frame_index(&game->atlas, "player_walk_up_1"),		4, 150.0f);
	animatedsprites_setanim(&game->anim_walk_down,	1, atlas_frame_index(&game->atlas, "player_walk_down_1"),	4, 150.0f);
	animatedsprites_setanim(&game->anim_shadow,		1, atlas_frame_index(&game->atlas, "player_shadow"),		1, 150.0f);
	animatedsprites_setanim(&game->anim_bar_hp,		1, atlas_frame_index(&game->atlas, "bar_hp"),				1, 150.0f);
	animatedsprites_setanim(&game->anim_bar_empty,	1, atlas_frame_index(&game->atlas, "bar_empty"),			1, 150.0f);
	animatedsprites_setanim(&game->anim_bar_sta,	1, atlas_frame_index(&game->atlas, "bar_sta"),				1, 150.0f);

	animatedsprites_setanim(&a->attack_left,		1, atlas_frame_index(&game->atlas, "attack_left"),			1, 150.0f);
	animatedsprites_setanim(&a->attack_right,		1, atlas_frame_index(&game->atlas, "attack_right"),			1, 150.0f);
	animatedsprites_setanim(&a->attack_down,		1, atlas_frame_index(&game->atlas, "attack_down_1"),		3, PLAYER_ATTACK_TIME/3.0f);
	animatedsprites_setanim(&a->attack_up,			1, atlas_frame_index(&game->atlas, "attack_up"),			1, 150.0f);
	
	animatedsprites_setanim(&a->arrow,				1, atlas_frame_index(&game->atlas, "arrow"),				1, 150.0f);
}

void game_state_menu_init(struct state_menu *menu)
{
	memset(menu, 0, sizeof(struct state_menu));

	/* Menu state */
	menu->quit_selected = 0;

	/* Menu sprite */
	struct basic_sprite *s = &menu->sprite;
	s->type = 0;
	s->texture = &assets->textures.menu_start;
	set4f(s->pos, VIEW_WIDTH / 2.0f, VIEW_HEIGHT / 2.0f, 0.0f, 1.0f);
	set4f(s->scale, VIEW_WIDTH, VIEW_HEIGHT, 1.0f, 1.0f);
	copyv(s->color, COLOR_WHITE);
}

void game_state_over_init(struct state_over *state)
{
	memset(state, 0, sizeof(struct state_over));

	struct basic_sprite *s = &state->sprite;

	/* Menu sprite */
	s->type = 0;
	s->texture = &assets->textures.game_over;
	set4f(s->pos, VIEW_WIDTH / 2.0f, VIEW_HEIGHT / 2.0f, 0.0f, 1.0f);
	set4f(s->scale, VIEW_WIDTH, VIEW_HEIGHT, 1.0f, 1.0f);
	copyv(s->color, COLOR_WHITE);
}

void game_state_win_init(struct state_win *state)
{
	memset(state, 0, sizeof(struct state_win));

	struct basic_sprite *s = &state->sprite;

	/* Menu sprite */
	s->type = 0;
	s->texture = &assets->textures.win;
	set4f(s->pos, VIEW_WIDTH / 2.0f, VIEW_HEIGHT / 2.0f, 0.0f, 1.0f);
	//set4f(s->scale, VIEW_WIDTH, VIEW_HEIGHT, 1.0f, 1.0f);
	set4f(s->scale, 0, 0, 1.0f, 1.0f);
	copyv(s->color, COLOR_WHITE);
}

void game_init()
{
	game->screen_shake_start = 0;
	game->debug = 0.0f;
	game->lerp_factor = LERP_FACTOR;
	set3f(game->sound_pos, 0, 0, 0);

	/* Init game states. */
	game_state_menu_init(&game->state_menu);
	game_state_over_init(&game->state_over);
	game_state_win_init(&game->state_win);


	/* Create animated sprite batcher. */
	if(game->batcher == NULL) {
		game->batcher = animatedsprites_create();
	} else {
		animatedsprites_clear(game->batcher);
	}
	if(game->ui == NULL) {
		game->ui = animatedsprites_create();
	} else {
		animatedsprites_clear(game->batcher);
	}

	/* Create animations. */
	player_anims_init(&game->player.anims);
	animatedsprites_setanim(&game->anim_grass_1,		1, atlas_frame_index(&game->atlas, "grass_1"),		1, 150.0f);
	animatedsprites_setanim(&game->anim_grass_2,		1, atlas_frame_index(&game->atlas, "grass_2"),		1, 150.0f);
	animatedsprites_setanim(&game->anim_grass_3,		1, atlas_frame_index(&game->atlas, "grass_3"),		1, 150.0f);
	animatedsprites_setanim(&game->anim_grass_stone,	1, atlas_frame_index(&game->atlas, "grass_stone"),	1, 150.0f);
	animatedsprites_setanim(&game->anim_grass_sand_1,	1, atlas_frame_index(&game->atlas, "grass_sand_1"),	1, 150.0f);
	animatedsprites_setanim(&game->anim_grass_sand_2,	1, atlas_frame_index(&game->atlas, "grass_sand_2"),	1, 150.0f);

	/* Create monster. */
	monster_init(&game->monster);

	/* Create sprite. */
	player_init(&game->player);

	/* Create tiles. */
	map_init();
	tiles_init(&game->tiles, (struct anim **) game->tiles_data, TILE_SIZE, VIEW_WIDTH, VIEW_HEIGHT, TILES_X, TILES_Y);

	camera_set(game->monster.base_pos, 1);
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
