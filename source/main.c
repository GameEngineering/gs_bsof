/*================================================================ 
	* Copyright: 2020 John Jackson
	* File: main.c 
	All Rights Reserved

    Game: 'Binding Of Star Fox'

    Flecs: 
        - Introduce this for some better entity and systems management/organization

    Design: 
        Star Fox: 
            - Rail shooting  
            - 3D
            - Ships
            - Laser fire
            - Bombs
        Binding of Isaac: 
            - Randomly generated 'dungeons' with sub-rooms
            - Items pickups and unlocks
            - Base stats that increase with items
            - Unlockable characters (ships)

    Runs: 
        - Builds on "meta"
        - Seeded 

    Rooms:
        - Procedural, but arranged from pre-designed sets in "pools"
        - Room flow (if rail shooter): 
            * If room contains no mobs:
                * Prompted with arrows on direction you can turn (to face other rooms)
            * Else: 
                * Rail shooter, fight mobs until all gone
        - Room flow (if open):
            * If room contains mobs:
                * If hit boundaries of rooms, then ship will do the star fox 64 "range mode" about-face
            * Else: 
                Portals open for doors into adjacent rooms

        - Go into "dungeon", so flying around large rooms with walls? Or are there artificial walls instead with 
            open, visible areas?  
        - Editor to design room templates
            - Editor camera (simple fp-fly camera)

    Room Template:
        - What is this? What does this look like?
        - A room template has placement for various objects, like mobs/obstacles/items/props
        - A scene of placed entities, basically, either specifically plalced items/mobs or generators that 
            use random generation to determine what they can/cannot place

    Enemies:
        - Number? Types?
        - Bandit
        - Turret

    Bosses: 
        - Single boss? Two bosses?

    Items: 
        - 

    Floors: 
        - Levels in StarFox, can unlock various paths, just like BOI

    Base Stats:
        Health:     The amount of damage the character can take before dying.
        Speed:      How quickly the character moves.
        Fire Rate:  The character's rate of fire. The lower the number, the faster tears are fired.
        Damage:     The amount of damage that each of the character's tears deals to enemies.
        Range:      How long the character's shots will travel before landing.   (not sure about this one...)
        Shot Speed: How quickly the character's tears travel.
        Luck:       Affects a variety of chance-based effects.

    TODO:   
        - Physics meshes for poly collisions
        - Behaviors for 3 different basic enemy types
        - Boss
        - UI
        - Various obstacles/props
        - Construct more room templates
        - Make it look nice...This is priority right now
        - Room navigation

=================================================================*/


#define GS_IMPL
#include <gs/gs.h>

#define GS_IMMEDIATE_DRAW_IMPL
#include <gs/util/gs_idraw.h>

#define GS_GUI_IMPL
#include <gs/util/gs_gui.h>

#define GS_PHYSICS_IMPL
#include <gs/util/gs_physics.h> 

#define GS_GFXT_IMPL
#include <gs/util/gs_gfxt.h>

#define GS_AI_IMPL
#include <gs/util/gs_ai.h>

#include "flecs/flecs.h" 

// Defines
#define BSF_SEED_MAX_LEN    (8 + 1)
#define BSF_ROOM_MAX_COLS   9
#define BSF_ROOM_MAX_ROWS   8
#define BSF_ROOM_MAX        (BSF_ROOM_MAX_ROWS * BSF_ROOM_MAX_COLS)
#define BSF_ROOM_BOUND_X    25.f
#define BSF_ROOM_BOUND_Y    25.f
#define BSF_ROOM_BOUND_Z    100.f 

// Forward decls.
struct bsf_t;

// Typedefs
typedef ecs_world_t bsf_ecs_t;
typedef ecs_entity_t bsf_entity_t;

//=== BSF Graphics ===//

typedef struct
{
    gs_camera_t cam;
    float shake_time;
    float pitch;
    float speed;
} bsf_camera_t;

//=== BSF Camera ===// 

GS_API_DECL void bsf_camera_init(struct bsf_t* bsf, bsf_camera_t* camera);
GS_API_DECL void bsf_camera_update(struct bsf_t* bsf, bsf_camera_t* camera);
GS_API_DECL void bsf_camera_shake(struct bsf_t* bsf, bsf_camera_t* camera, float amt); 
GS_API_DECL void bsf_dbg_reload_ss(struct bsf_t* bsf);

typedef enum
{
    BSF_SHAPE_BOX = 0x00,
    BSF_SHAPE_SPHERE, 
    BSF_SHAPE_RECT,
    BSF_SHAPE_CONE,
    BSF_SHAPE_CROSS,
    BSF_SHAPE_CYLINDER,
    BSF_SHAPE_ROOM
} bsf_shape_type;

typedef gs_gfxt_material_t bsf_material_instance_t; 

typedef struct
{
    gs_gfxt_material_t* material;   // Material instance of parent material
    gs_gfxt_mesh_t* mesh;		    // Handle to gfxt mesh
    gs_mat4 model;				    // Model matrix to be uploaded to GPU
} bsf_renderable_t;

typedef bsf_renderable_t bsf_renderable_desc_t;

typedef struct
{
	gs_slot_array(bsf_renderable_t) renderables;         // Collection of renderables for a graphics scene
    bsf_camera_t camera;
} bsf_graphics_scene_t;

GS_API_DECL uint32_t bsf_graphics_scene_renderable_create(bsf_graphics_scene_t* scene, const bsf_renderable_desc_t* desc); 
GS_API_DECL void bsf_graphics_scene_renderable_destroy(bsf_graphics_scene_t* scene, uint32_t hndl);
GS_API_DECL void bsf_graphics_render(struct bsf_t* bsf);

//=== BSF Components ===// 

typedef struct
{
    gs_vqs xform;
} bsf_component_transform_t; 

typedef struct
{
	uint32_t hndl;		            // Handle to a renderable in bsf graphics scene
} bsf_component_renderable_t;

GS_API_DECL void bsf_component_renderable_dtor(ecs_world_t* world, ecs_entity_t comp, const ecs_entity_t* ent, void* ptr, size_t sz, int32_t count, void* ctx);
GS_API_DECL void bsf_renderable_system(ecs_iter_t* it);

enum 
{
    BSF_RENDERABLE_IMMEDIATE_BLEND_ENABLE     = (1 << 0), 
    BSF_RENDERABLE_IMMEDIATE_DEPTH_ENABLE     = (1 << 1), 
    BSF_RENDERABLE_IMMEDIATE_FACE_CULL_ENABLE = (1 << 2) 
};

typedef struct
{
    bsf_shape_type shape;
    gs_gfxt_material_t* material;
    gs_mat4 model; 
    gs_color_t color;
    gs_gfxt_texture_t* texture;
    uint32_t opt;
} bsf_component_renderable_immediate_t;

GS_API_DECL void bsf_renderable_immediate_system(ecs_iter_t* it);

enum {
    BSF_COLLIDER_AABB = 0x00,
    BSF_COLLIDER_CYLINDER, 
    BSF_COLLIDER_SPHERE,
    BSF_COLLIDER_CONE
};

typedef struct
{
    int16_t type;
    union {
        gs_aabb_t aabb;
        gs_sphere_t sphere;
        gs_cone_t cone;
        gs_cylinder_t cylinder;
    } shape;
	gs_vqs xform;
} bsf_physics_collider_t;

typedef struct
{
    gs_vec3 velocity;
    gs_vec3 angular_velocity;
    float speed;
    bsf_physics_collider_t collider;
} bsf_component_physics_t;

GS_API_DECL void bsf_physics_debug_draw_system(ecs_iter_t* it);
GS_API_DECL gs_contact_info_t bsf_component_physics_collide(const bsf_component_physics_t* c0, const gs_vqs* xform0, const bsf_component_physics_t* c1, 
        const gs_vqs* xform1);

typedef struct
{
    int16_t hit;
    float health;
    float hit_timer;
} bsf_component_health_t;

typedef struct
{
    gs_vqs xform;
    gs_vec3 angular_velocity;
} bsf_component_camera_track_t;

enum 
{
    BSF_PROJECTILE_OPT_HOMING = (1 << 0), 
    BSF_PROJECTILE_OPT_HOMING_BOMB = (1 << 1)
};

typedef struct
{
    float health;    
    float speed;            
    float fire_rate;
    float damage;
    float range;
    float shot_speed;
    float luck;
    float shot_size;
    uint32_t shot_count;
    uint32_t projectile_opt;
} bsf_component_character_stats_t;

typedef struct
{
    int16_t active;
    float time;
    int16_t direction;
    int16_t vel_dir;
    float avz;
    float max_time;
} bsf_component_barrel_roll_t;

typedef struct
{
    float time;
    float max;
} bsf_component_timer_t; 

typedef enum 
{
    BSF_PROJECTILE_BULLET,
    BSF_PROJECTILE_BOMB
} bsf_projectile_type;

typedef enum
{
    BSF_OWNER_PLAYER = 0x00,
    BSF_OWNER_ENEMY
} bsf_owner_type;

typedef struct
{
    bsf_owner_type owner;
    bsf_projectile_type type;
} bsf_component_projectile_t;

typedef struct 
{
    bsf_owner_type owner;
} bsf_component_explosion_t;

GS_API_DECL ecs_entity_t bsf_explosion_create(struct bsf_t* bsf, ecs_world_t* world, gs_vqs* xform, bsf_owner_type owner);
GS_API_DECL void bsf_explosion_system(ecs_iter_t* it); 

typedef struct
{
    int16_t firing;
    float time;
} bsf_component_gun_t; 

typedef enum 
{
    BSF_MOB_BANDIT = 0x00,
    BSF_MOB_TURRET,
    BSF_MOB_BOSS,
    BSF_MOB_COUNT
} bsf_mob_type;

typedef struct 
{
    bsf_mob_type type;
} bsf_component_mob_t; 

GS_API_DECL ecs_entity_t bsf_mob_create(struct bsf_t* bsf, ecs_world_t* world, gs_vqs* xform, bsf_mob_type type);
GS_API_DECL void bsf_mob_system(ecs_iter_t* it); 

typedef enum
{
    BSF_CONSUMABLE_HEALTH = 0x00,
    BSF_CONSUMABLE_BOMB,
    BSF_CONSUMABLE_COUNT 
} bsf_consumable_type;

typedef enum
{
    BSF_OBSTACLE_BUILDING = 0x00,
    BSF_OBSTACLE_COUNT
} bsf_obstacle_type;

typedef struct
{
    bsf_obstacle_type type;
} bsf_component_obstacle_t;

GS_API_DECL ecs_entity_t bsf_obstacle_create(struct bsf_t* bsf, gs_vqs* xform, bsf_obstacle_type type);
GS_API_DECL void bsf_obstacle_system(ecs_iter_t* it); 
GS_API_DECL void bsf_obstacle_destroy(ecs_world_t* world, ecs_entity_t obstacle);

// Three item types for now, one to increase all stats, one to give double shots, one to give larger bullets
typedef enum
{
    BSF_ITEM_SAD_ONION = 0x00,      // Fire rate increase,
    BSF_ITEM_INNER_EYE,             // Fire three shots, fire rate decrease significantly
    BSF_ITEM_SPOON_BENDER,          // Homing shots,
    BSF_ITEM_MAGIC_MUSHROOM,		// All stats up
	BSF_ITEM_POLYPHEMUS,			// Shot size increase masive, Single shot
	BSF_ITEM_COUNT
} bsf_item_type;

GS_API_DECL void bsf_item_remove_from_pool(struct bsf_t* bsf, bsf_item_type type);

typedef struct
{
    bsf_consumable_type type;
    gs_vqs origin;
    float time_scale[3];
} bsf_component_consumable_t;

GS_API_DECL ecs_entity_t bsf_consumable_create(struct bsf_t* bsf, gs_vqs* xform, bsf_consumable_type type);
GS_API_DECL void bsf_consumable_system(ecs_iter_t* it);
GS_API_DECL void bsf_consumable_destroy(ecs_world_t* world, ecs_entity_t item);

typedef struct
{
	float hit_timer;
	b32 hit;
    bsf_item_type type;
    gs_vqs origin;
    float time_scale[3];
    uint32_t hit_count;
} bsf_component_item_chest_t;

GS_API_DECL ecs_entity_t bsf_item_chest_create(struct bsf_t* bsf, gs_vqs* xform, bsf_item_type type);
GS_API_DECL void bsf_item_chest_system(ecs_iter_t* it);
GS_API_DECL void bsf_item_chest_destroy(ecs_world_t* world, ecs_entity_t item_chest);

typedef struct
{
    bsf_item_type type;
} bsf_component_item_t;

GS_API_DECL ecs_entity_t bsf_item_create(struct bsf_t* bsf, ecs_world_t* world, gs_vqs* xform, bsf_item_type type);
GS_API_DECL void bsf_item_system(ecs_iter_t* it);
GS_API_DECL void bsf_item_destroy(ecs_world_t* world, ecs_entity_t item);
GS_API_DECL void bsf_item_bt(gs_ai_bt_t* bt); 

typedef struct 
{
    int16_t bombs;
    gs_dyn_array(bsf_item_type) items;
} bsf_component_inventory_t; 

typedef struct
{
    gs_ai_bt_t bt;
    gs_vec3 target;
    float wait;
} bsf_component_ai_t; 

GS_API_DECL void bsf_component_ai_dtor(ecs_world_t* world, ecs_entity_t comp, const ecs_entity_t* ent, void* ptr, size_t sz, int32_t count, void* ctx);

typedef struct
{
    bsf_component_gun_t* gc;
    bsf_component_transform_t* tc;
    bsf_component_physics_t* pc;
    bsf_component_health_t* hc;
    bsf_component_transform_t* ptc; // Player transform component
    bsf_component_ai_t* ac;
    bsf_component_mob_t* mc;
	bsf_component_item_t* ic;
    ecs_world_t* world;
    ecs_entity_t ent;
} bsf_ai_data_t;

GS_API_DECL void bsf_ai_task_move_to_target_location(struct gs_ai_bt_t* bt, struct gs_ai_bt_node_t* node);

ECS_COMPONENT_DECLARE(bsf_component_renderable_t);
ECS_COMPONENT_DECLARE(bsf_component_renderable_immediate_t);
ECS_COMPONENT_DECLARE(bsf_component_transform_t);
ECS_COMPONENT_DECLARE(bsf_component_physics_t);
ECS_COMPONENT_DECLARE(bsf_component_projectile_t);
ECS_COMPONENT_DECLARE(bsf_component_character_stats_t);
ECS_COMPONENT_DECLARE(bsf_component_timer_t);
ECS_COMPONENT_DECLARE(bsf_component_mob_t);
ECS_COMPONENT_DECLARE(bsf_component_gun_t);
ECS_COMPONENT_DECLARE(bsf_component_health_t);
ECS_COMPONENT_DECLARE(bsf_component_consumable_t);
ECS_COMPONENT_DECLARE(bsf_component_camera_track_t);
ECS_COMPONENT_DECLARE(bsf_component_barrel_roll_t); 
ECS_COMPONENT_DECLARE(bsf_component_explosion_t); 
ECS_COMPONENT_DECLARE(bsf_component_inventory_t);
ECS_COMPONENT_DECLARE(bsf_component_ai_t);
ECS_COMPONENT_DECLARE(bsf_component_obstacle_t);
ECS_COMPONENT_DECLARE(bsf_component_item_chest_t);
ECS_COMPONENT_DECLARE(bsf_component_item_t);

//=== BSF Entities ===// 

GS_API_DECL void bsf_entities_init(struct bsf_t* bsf);

GS_API_DECL void bsf_projectile_create(struct bsf_t* bsf, ecs_world_t* world,
        bsf_projectile_type type, bsf_owner_type owner, const gs_vqs* xform, gs_vec3 velocity);  // Create single projectile
GS_API_DECL void bsf_projectile_system(ecs_iter_t* it);                                          // System for updating projectiles

/*
    Base Stats:
        Health:     The amount of damage the character can take before dying.
        Speed:      How quickly the character moves.
        Fire Rate:  The character's rate of fire. The lower the number, the faster tears are fired.
        Damage:     The amount of damage that each of the character's tears deals to enemies.
        Range:      How long the character's shots will travel before landing.   (not sure about this one...)
        Shot Speed: How quickly the character's tears travel.
        Luck:       Affects a variety of chance-based effects.

    Characters: 
        ArWing
        Slave
        ...Tank?
*/

enum 
{
    BSF_MOVEMENT_FREE_RANGE = 0x00,
    BSF_MOVEMENT_RAIL
};

GS_API_DECL void bsf_player_init(struct bsf_t* bsf);
GS_API_DECL void bsf_player_system(ecs_iter_t* iter);
GS_API_DECL void bsf_player_damage(struct bsf_t* bsf, ecs_world_t* world, float damage); 
GS_API_DECL void bsf_player_consumable_pickup(struct bsf_t* bsf, ecs_world_t* world, bsf_consumable_type type);
GS_API_DECL void bsf_player_item_pickup(struct bsf_t* bsf, ecs_world_t* world, bsf_item_type type);

//=== BSF State ===//

typedef enum {
    BSF_STATE_TITLE = 0x00,
    BSF_STATE_FILE_SELECT,
    BSF_STATE_MAIN_MENU, 
    BSF_STATE_OPTIONS,
    BSF_STATE_GAME_SETUP,
    BSF_STATE_START,            // For initialization of a run
    BSF_STATE_END,              // For deinitialization of a run
    BSF_STATE_PLAY,             // For continuing play of existing run (from pause menu)
    BSF_STATE_PAUSE,            // Pause menu
    BSF_STATE_GAME_OVER,        // Game over screen
    BSF_STATE_EDITOR_START,     // Editor init
    BSF_STATE_EDITOR,           // Editor screen
    BSF_STATE_QUIT,             // Quit the game
    BSF_STATE_TEST              // Test room
} bsf_state; 

//=== BSF Menus ===//

GS_API_DECL void bsf_menu_update(struct bsf_t* bsf);

//=== BSF Editor ===//

GS_API_DECL void bsf_editor_start(struct bsf_t* bsf);
GS_API_DECL void bsf_editor(struct bsf_t* bsf);
GS_API_DECL void bsf_editor_camera_update(struct bsf_t* bsf, bsf_camera_t* camera);

//=== BSF Game ===// 

GS_API_DECL void bsf_game_start(struct bsf_t* bsf);
GS_API_DECL void bsf_game_end(struct bsf_t* bsf);
GS_API_DECL void bsf_game_update(struct bsf_t* bsf);

//=== BSF Audio ===//
GS_API_DECL void bsf_play_sound(struct bsf_t* bsf, const char* key, float volume);
GS_API_DECL void bsf_play_music(struct bsf_t* bsf, const char* key);

//=== BSF Test ===//

GS_API_DECL void bsf_test(struct bsf_t* bsf);

//=== BSF Room ===// 

#define BSF_ROOM_WIDTH  100.f
#define BSF_ROOM_HEIGHT 100.f
#define BSF_ROOM_DEPTH  100.f

typedef enum {
    BSF_ROOM_DEFAULT = 0x00,
	BSF_ROOM_START, 
    BSF_ROOM_ITEM, 
    BSF_ROOM_SECRET, 
    BSF_ROOM_SHOP,
    BSF_ROOM_BOSS
} bsf_room_type;

typedef struct
{
    bsf_room_type type;
    int16_t distance;                   // "Walking" distance from starting cell, used for dead end sorts
    gs_dyn_array(ecs_entity_t) mobs;    // Mobs for this room
    gs_dyn_array(ecs_entity_t) obstacles;    // Mobs for this room
    gs_dyn_array(ecs_entity_t) consumables;    // Consumable for this room
    gs_dyn_array(ecs_entity_t) items;    // Consumable for this room
    b32 cleared;                          // Whether or not this room is clear
    int16_t movement_type;
    int16_t cell;
} bsf_room_t; 

GS_API_DECL void bsf_room_load(struct bsf_t* bsf, uint32_t cell);

//=== BSF Room Template ===//

typedef enum 
{
    BSF_ROOM_BRUSH_INVALID =  0x00,
    BSF_ROOM_BRUSH_MOB,
    BSF_ROOM_BRUSH_CONSUMABLE,
    BSF_ROOM_BRUSH_PROP,
    BSF_ROOM_BRUSH_OBSTACLE,
    BSF_ROOM_BRUSH_BANDIT,
    BSF_ROOM_BRUSH_TURRET,
} bsf_room_brush_type;

typedef struct
{
    gs_vqs xform;
    bsf_room_brush_type type;
} bsf_room_brush_t;

typedef struct 
{
    gs_slot_array(bsf_room_brush_t) brushes;
    char path[256];
} bsf_room_template_t;

//=== BSF Assets ===// 

typedef struct 
{
    gs_handle(gs_graphics_vertex_buffer_t) vbo;
    gs_handle(gs_graphics_index_buffer_t) ibo;
    gs_gfxt_material_t* material;
} bsf_model_t; 

typedef struct {
    const char* asset_dir;
    gs_hash_table(uint64_t, gs_gfxt_pipeline_t)   pipelines;
    gs_hash_table(uint64_t, gs_gfxt_texture_t)    textures;
    gs_hash_table(uint64_t, gs_gfxt_material_t)   materials;
    gs_hash_table(uint64_t, gs_gfxt_mesh_t)       meshes;
    gs_hash_table(uint64_t, gs_asset_font_t)      fonts;
    gs_hash_table(uint64_t, gs_gui_style_sheet_t) style_sheets;
    gs_hash_table(uint64_t, gs_gfxt_texture_t)    cubemaps;
    gs_hash_table(uint64_t, bsf_model_t)          models;
    gs_hash_table(uint64_t, bsf_room_template_t)  room_templates;
    gs_hash_table(uint64_t, gs_asset_audio_t)     sounds;
} bsf_assets_t;

GS_API_DECL void bsf_assets_init(struct bsf_t* bsf, bsf_assets_t* assets);

//=== BSF App ===// 

typedef struct bsf_t
{
    struct {
        gs_command_buffer_t cb;
        gs_immediate_draw_t gsi;
        gs_gui_context_t gui;
    } gs;                       // All gunslinger related contexts and data 

    bsf_assets_t assets;        // Asset manager 
	bsf_graphics_scene_t scene; // Should the scene hold onto the active camera?  
    bsf_state state;            // Current state of application

    struct {
        char seed[BSF_SEED_MAX_LEN];     // Current seed for run 
        bool32 is_playing;               // Whether or not game is currently active
        int16_t room_ids[BSF_ROOM_MAX];  // Id lookup grid into rooms data
        uint16_t level;                  // Current level
        gs_slot_array(bsf_room_t) rooms; // Rooms for current level
        uint32_t cell;                   // Current room cell (starts at (4, 4))
        uint16_t boss;
        uint16_t item;
        gs_mt_rand_t rand;
        float time_scale;
        b32 just_cleared_room;
        b32 just_cleared_level;
        b32 complete;
        float clear_timer;
        gs_dyn_array(bsf_item_type) item_pool;
    } run;

    struct {
        ecs_entity_t player;    // Main player
        ecs_entity_t boss;      // Boss
        ecs_world_t* world;     // Main flecs entity world
    } entities;

    int16_t dbg;

    // Music audio handle
    gs_handle(gs_audio_instance_t) music;

} bsf_t; 

//=== App Init ===// 

void bsf_init()
{ 
    bsf_t* bsf = gs_user_data(bsf_t);

    // Init all gunslinger related contexts and data
	bsf->gs.cb = gs_command_buffer_new();
	bsf->gs.gsi = gs_immediate_draw_new(gs_platform_main_window());
    gs_gui_init(&bsf->gs.gui, gs_platform_main_window()); 

    // Initialize game state to title
    bsf->state = BSF_STATE_TITLE;

    // Initialize all asset data
    bsf_assets_init(bsf, &bsf->assets);

    // Start playing title music
    bsf->music.id = UINT32_MAX;
    bsf_play_music(bsf, "audio.music_title");
}

void bsf_update()
{
    bsf_t* bsf = gs_user_data(bsf_t); 

    gs_gui_begin(&bsf->gs.gui, gs_platform_framebuffer_sizev(gs_platform_main_window())); 

    if (gs_platform_key_pressed(GS_KEYCODE_P))
    {
        bsf_dbg_reload_ss(bsf);
    }

    switch (bsf->state)
    {
        case BSF_STATE_TITLE:
        case BSF_STATE_MAIN_MENU:
        case BSF_STATE_OPTIONS:
        case BSF_STATE_PAUSE:
        case BSF_STATE_GAME_SETUP:
        {
            gs_platform_lock_mouse(gs_platform_main_window(), false);
            bsf_menu_update(bsf);
        } break; 

        case BSF_STATE_START:
        {
            gs_platform_lock_mouse(gs_platform_main_window(), false);
            bsf_game_start(bsf);
        } break;

        case BSF_STATE_PLAY:
        {
            bsf_game_update(bsf);
        } break; 

        case BSF_STATE_END:
        { 
            gs_platform_lock_mouse(gs_platform_main_window(), false);
            bsf_game_end(bsf);
        } break;

        case BSF_STATE_EDITOR_START:
        {
            gs_platform_lock_mouse(gs_platform_main_window(), false);
            bsf_editor_start(bsf);
        } break; 

        case BSF_STATE_EDITOR:
        {
            bsf_editor(bsf);
        } break; 

        case BSF_STATE_GAME_OVER:
        {
            // Just end the game for now...
            bsf_game_end(bsf);
            bsf_play_music(bsf, "audio.music_title");
        } break;

        case BSF_STATE_QUIT:
        {
            gs_platform_lock_mouse(gs_platform_main_window(), false);
            gs_quit();
        } break;

        case BSF_STATE_TEST:
        {
            bsf_test(bsf);
        } break;
    } 

    gs_gui_end(&bsf->gs.gui);

    bsf_graphics_render(bsf);
}

void bsf_shutdown()
{
    bsf_t* bsf = gs_user_data(bsf_t);
    gs_immediate_draw_free(&bsf->gs.gsi);
    gs_command_buffer_free(&bsf->gs.cb);
    gs_gui_free(&bsf->gs.gui);
}

gs_app_desc_t gs_main(int32_t argc, char** argv)
{
	return (gs_app_desc_t) {
        .user_data = gs_malloc_init(bsf_t),
		.window_width = 1200,
		.window_height = 700,
        .window_title = "Binding of Star Fox",
		.init = bsf_init,
		.update = bsf_update,
        .shutdown = bsf_shutdown
	};
}

//=== BSF Assets ===//

GS_API_DECL void bsf_assets_init(bsf_t* bsf, bsf_assets_t* assets)
{
    assets->asset_dir = gs_platform_dir_exists("./assets") ? "./assets" : "../assets";

    // Room templates
    struct {const char* key; const char* path;} room_templates[] = {
        {.key = "rt.r0", .path = "room_templates/r0.rt"},
        {.key = "rt.r1", .path = "room_templates/r1.rt"},
        {.key = "rt.r2", .path = "room_templates/r2.rt"},
        {.key = "rt.r3", .path = "room_templates/r3.rt"},
        {.key = "rt.r4", .path = "room_templates/r4.rt"},
        {.key = "rt.r5", .path = "room_templates/r5.rt"},
        {.key = "rt.r6", .path = "room_templates/r6.rt"},
        {.key = "rt.r7", .path = "room_templates/r7.rt"},
        {.key = "rt.r8", .path = "room_templates/r8.rt"},
        {.key = "rt.r9", .path = "room_templates/r9.rt"},
        {NULL}
    };

    for (uint32_t i = 0; room_templates[i].key; ++i)
    {
        bsf_room_template_t rt = {0};
        memcpy(rt.path, room_templates[i].path, 256);

        gs_snprintfc(TMP, 256, "%s/%s", assets->asset_dir, room_templates[i].path); 
        if (gs_platform_file_exists(TMP))
        {
            gs_byte_buffer_t buffer = gs_byte_buffer_new();
            gs_byte_buffer_read_from_file(&buffer, TMP); 
            gs_byte_buffer_readc(&buffer, uint16_t, ct); 
            for (uint32_t i = 0; i < ct; ++i)
            {
                gs_byte_buffer_readc(&buffer, bsf_room_brush_t, br);
                gs_slot_array_insert(rt.brushes, br);
            }
            gs_byte_buffer_free(&buffer);
        }

        gs_hash_table_insert(assets->room_templates, gs_hash_str64(room_templates[i].key), rt);
    }

    struct {const char* key; const char* path;} pipelines[] = {
        {.key = "pip.simple", .path = "pipelines/simple.sf"},
        {.key = "pip.hit", .path = "pipelines/hit.sf"},
        {.key = "pip.color", .path = "pipelines/color.sf"},
        {.key = "pip.gsi", .path = "pipelines/gsi.sf"},
        {.key = "pip.skybox", .path = "pipelines/skybox.sf"},
        {NULL}
    };

    for (uint32_t i = 0; pipelines[i].key; ++i)
    {
        gs_snprintfc(TMP, 256, "%s/%s", assets->asset_dir, pipelines[i].path);
        gs_hash_table_insert(assets->pipelines, gs_hash_str64(pipelines[i].key), gs_gfxt_pipeline_load_from_file(TMP));
    }

    struct {const char* key; const char* path; const char* pip;} meshes[] = {
        {.key = "mesh.ship", .path = "meshes/ship.gltf", .pip = "pip.simple"}, 
        {.key = "mesh.slave", .path = "meshes/slave.gltf", .pip = "pip.simple"}, 
        {.key = "mesh.arwing", .path = "meshes/arwing.gltf", .pip = "pip.simple"}, 
        {.key = "mesh.arwing64", .path = "meshes/arwing64.gltf", .pip = "pip.simple"}, 
        {.key = "mesh.bandit", .path = "meshes/bandit.gltf", .pip = "pip.simple"}, 
        {.key = "mesh.turret", .path = "meshes/turret.gltf", .pip = "pip.simple"}, 
        {.key = "mesh.laser_player", .path = "meshes/laser_player.gltf", .pip = "pip.color"}, 
        {.key = "mesh.brain", .path = "meshes/brain.gltf", .pip = "pip.simple"}, 
        {NULL} 
    };

    for (uint32_t i = 0; meshes[i].key; ++i)
    {
        gs_snprintfc(TMP, 256, "%s/%s", assets->asset_dir, meshes[i].path);
        gs_gfxt_pipeline_t* pip = gs_hash_table_getp(assets->pipelines, gs_hash_str64(meshes[i].pip));
        gs_hash_table_insert(assets->meshes, gs_hash_str64(meshes[i].key), gs_gfxt_mesh_load_from_file(TMP, &(gs_gfxt_mesh_import_options_t){
            .layout = pip->mesh_layout,
            .size = gs_dyn_array_size(pip->mesh_layout) * sizeof(gs_gfxt_mesh_layout_t),
            .index_buffer_element_size = pip->desc.raster.index_buffer_element_size
        }));
    }

    gs_graphics_texture_desc_t desc = (gs_graphics_texture_desc_t) {
        .format = GS_GRAPHICS_TEXTURE_FORMAT_RGBA8,
        .min_filter = GS_GRAPHICS_TEXTURE_FILTER_LINEAR,
        .mag_filter = GS_GRAPHICS_TEXTURE_FILTER_LINEAR,
        .wrap_s = GS_GRAPHICS_TEXTURE_WRAP_CLAMP_TO_EDGE,
        .wrap_t = GS_GRAPHICS_TEXTURE_WRAP_CLAMP_TO_EDGE
    };

    struct {const char* key; const char* path; gs_graphics_texture_desc_t desc;} textures[] = {
        {.key = "tex.slave", .path = "textures/slave.png", .desc = desc}, 
        {.key = "tex.arwing", .path = "textures/arwing.png", .desc = desc}, 
        {.key = "tex.vignette", .path = "textures/vignette.png", .desc = desc}, 
        {.key = "tex.title_bg", .path = "textures/title_bg.png", .desc = (gs_graphics_texture_desc_t){
            .format = GS_GRAPHICS_TEXTURE_FORMAT_RGBA8,
            .min_filter = GS_GRAPHICS_TEXTURE_FILTER_NEAREST,
            .mag_filter = GS_GRAPHICS_TEXTURE_FILTER_NEAREST
        }},
        {.key = "tex.icon_inner_eye", .path = "textures/icon_inner_eye.png", .desc = (gs_graphics_texture_desc_t){
            .format = GS_GRAPHICS_TEXTURE_FORMAT_RGBA8,
            .min_filter = GS_GRAPHICS_TEXTURE_FILTER_NEAREST,
            .mag_filter = GS_GRAPHICS_TEXTURE_FILTER_NEAREST
        }},
        {.key = "tex.icon_polyphemus", .path = "textures/icon_polyphemus.png", .desc = (gs_graphics_texture_desc_t){
            .format = GS_GRAPHICS_TEXTURE_FORMAT_RGBA8,
            .min_filter = GS_GRAPHICS_TEXTURE_FILTER_NEAREST,
            .mag_filter = GS_GRAPHICS_TEXTURE_FILTER_NEAREST
        }},
        {.key = "tex.icon_spoon_bender", .path = "textures/icon_spoon_bender.png", .desc = (gs_graphics_texture_desc_t){
            .format = GS_GRAPHICS_TEXTURE_FORMAT_RGBA8,
            .min_filter = GS_GRAPHICS_TEXTURE_FILTER_NEAREST,
            .mag_filter = GS_GRAPHICS_TEXTURE_FILTER_NEAREST
        }},
        {.key = "tex.icon_sad_onion", .path = "textures/icon_sad_onion.png", .desc = (gs_graphics_texture_desc_t){
            .format = GS_GRAPHICS_TEXTURE_FORMAT_RGBA8,
            .min_filter = GS_GRAPHICS_TEXTURE_FILTER_NEAREST,
            .mag_filter = GS_GRAPHICS_TEXTURE_FILTER_NEAREST
        }},
        {.key = "tex.icon_magic_mushroom", .path = "textures/icon_magic_mushroom.png", .desc = (gs_graphics_texture_desc_t){
            .format = GS_GRAPHICS_TEXTURE_FORMAT_RGBA8,
            .min_filter = GS_GRAPHICS_TEXTURE_FILTER_NEAREST,
            .mag_filter = GS_GRAPHICS_TEXTURE_FILTER_NEAREST
        }},
        {NULL}
    };

    for (uint32_t i = 0; textures[i].key; ++i)
    { 
        gs_snprintfc(TMP, 256, "%s/%s", assets->asset_dir, textures[i].path);
        gs_hash_table_insert(assets->textures, gs_hash_str64(textures[i].key), gs_gfxt_texture_load_from_file(TMP, &textures[i].desc, false, false));
    }

    // Add in default texture
    gs_hash_table_insert(assets->textures, gs_hash_str64("tex.default"), gs_gfxt_texture_generate_default());

    // Sounds
    struct {const char* key; const char* path;} sounds[] = {
        {"audio.laser", "sounds/arwing_laser.mp3"},
        {"audio.laser2", "sounds/laser2.wav"},
        {"audio.bang", "sounds/bang.mp3"},
        {"audio.explosion", "sounds/explosion.mp3"},
        {"audio.explosion_boss", "sounds/explosion_boss.mp3"},
        {"audio.health_pickup", "sounds/pickup.mp3"},
        {"audio.bomb_pickup", "sounds/bomb_pickup.mp3"},
        {"audio.menu_select", "sounds/menu_select.mp3"},
        {"audio.start", "sounds/start.mp3"},
        {"audio.pause", "sounds/pause.mp3"},
        {"audio.arwing_hit", "sounds/arwing_hit.mp3"},
        {"audio.hit_no_damage", "sounds/hit_no_damage.mp3"},
        {"audio.music_boss", "sounds/music_boss.mp3"},
        {"audio.music_title", "sounds/music_title.mp3"},
        {"audio.music_level", "sounds/music_level.mp3"},
        {"audio.music_level_complete", "sounds/music_level_complete.mp3"},
        {NULL}
    };

    for (uint32_t i = 0; sounds[i].key; ++i)
    {
        gs_snprintfc(TMP, 256, "%s/%s", assets->asset_dir, sounds[i].path);
        gs_asset_audio_t snd = {0};
        gs_asset_audio_load_from_file(TMP, &snd);
        gs_hash_table_insert(assets->sounds, gs_hash_str64(sounds[i].key), snd);
    }

    struct {const char* key; const char* paths[6];} cmaps[] = {
        {
            .key = "cmap.skybox",
            .paths = {
                "textures/sky_back.jpg", 
                "textures/sky_back.jpg", 
                "textures/sky_top.jpg", 
                "textures/sky_bottom.jpg", 
                "textures/sky_back.jpg", 
                "textures/sky_back.jpg"
            }
        },
        {NULL}
    };

    for (uint32_t i = 0; cmaps[i].key; ++i) 
    {
        gs_graphics_texture_desc_t desc = { 
            .type = GS_GRAPHICS_TEXTURE_CUBEMAP, 
            .min_filter = GS_GRAPHICS_TEXTURE_FILTER_NEAREST,
            .mag_filter = GS_GRAPHICS_TEXTURE_FILTER_NEAREST,
            .wrap_s = GS_GRAPHICS_TEXTURE_WRAP_CLAMP_TO_EDGE,
            .wrap_t = GS_GRAPHICS_TEXTURE_WRAP_CLAMP_TO_EDGE,
            .wrap_r = GS_GRAPHICS_TEXTURE_WRAP_CLAMP_TO_EDGE
        };
        uint32_t w = 0, h = 0, ncmps = 0;
        for (uint32_t c = 0; c < 6; ++c)
        {
            uint32_t nc = 0;
            gs_snprintfc(PATH, 256, "%s/%s", assets->asset_dir, cmaps[i].paths[c]);
            gs_util_load_texture_data_from_file(PATH, &w, &h, &nc, &desc.data[c], false);
            gs_assert(desc.data[c]);
            if (c == 0) {
                desc.height = h;
                desc.width = w; 
                ncmps = nc;
                desc.format = ncmps == 3 ? GS_GRAPHICS_TEXTURE_FORMAT_RGB8 : GS_GRAPHICS_TEXTURE_FORMAT_RGBA8;
            } else {
                gs_assert(w == desc.width); 
                gs_assert(h == desc.height);
                gs_assert(nc == ncmps);
            }
        } 
        // Create cubemap texture
        gs_gfxt_texture_t cmap = gs_graphics_texture_create(&desc); 
        gs_hash_table_insert(assets->cubemaps, gs_hash_str64(cmaps[i].key), cmap);
    } 

    struct {const char* key; const char* pip;} materials[] = {
        {.key = "mat.simple", .pip = "pip.simple"},
        {.key = "mat.color", .pip = "pip.color"},
        {.key = "mat.hit", .pip = "pip.hit"},
        {.key = "mat.gsi", .pip = "pip.gsi"},
        {.key = "mat.skybox", .pip = "pip.skybox"},
        {NULL}
    };

    for (uint32_t i = 0; materials[i].key; ++i)
    {
        gs_gfxt_pipeline_t* pip = gs_hash_table_getp(assets->pipelines, gs_hash_str64(materials[i].pip));
        gs_hash_table_insert(assets->materials, gs_hash_str64(materials[i].key), gs_gfxt_material_create(&(gs_gfxt_material_desc_t){.pip_func.hndl = pip}));
    } 

    struct {const char* key; const char* mat;} material_instances[] = {
        {.key = "mat.ship_arwing", .mat = "mat.simple"},
        {.key = "mat.slave", .mat = "mat.simple"},
        {.key = "mat.bandit", .mat = "mat.simple"},
        {.key = "mat.turret", .mat = "mat.simple"},
        {.key = "mat.laser_player", .mat = "mat.color"},
        {.key = "mat.laser_enemy", .mat = "mat.color"},
        {.key = "mat.sky_sphere", .mat = "mat.color"},
        {.key = "mat.brain", .mat = "mat.simple"},
        {NULL}
    }; 

    for (uint32_t i = 0; material_instances[i].key; ++i)
    {
        gs_gfxt_material_t* mat = gs_hash_table_getp(assets->materials, gs_hash_str64(material_instances[i].mat));
        gs_gfxt_material_t inst = gs_gfxt_material_deep_copy(mat);
        gs_hash_table_insert(assets->materials, gs_hash_str64(material_instances[i].key), inst);
    } 
    
    struct {const char* key; const char* path; int32_t pt;} fonts[] = {
        {.key = "font.arwing_h0", .path = "fonts/arwing.ttf", .pt = 90},
        {.key = "font.arwing_h1", .path = "fonts/arwing.ttf", .pt = 32},
        {.key = "font.arwing_h2", .path = "fonts/arwing.ttf", .pt = 24},
        {.key = "font.arwing_p", .path = "fonts/arwing.ttf", .pt = 16},
        {.key = "font.upheaval_h0", .path = "fonts/upheaval.ttf", .pt = 48},
        {.key = "font.upheaval_h1", .path = "fonts/upheaval.ttf", .pt = 32},
        {.key = "font.upheaval_h2", .path = "fonts/upheaval.ttf", .pt = 24},
        {.key = "font.upheaval_p", .path = "fonts/upheaval.ttf", .pt = 16},
        {NULL}
    }; 

    for (uint32_t i = 0; fonts[i].key; ++i) 
    {
        gs_snprintfc(TMP, 256, "%s/%s", assets->asset_dir, fonts[i].path);
        gs_asset_font_t* font = gs_malloc_init(gs_asset_font_t); 
        gs_asset_font_load_from_file(TMP, font, fonts[i].pt);
        gs_hash_table_insert(bsf->gs.gui.font_stash, gs_hash_str64(fonts[i].key), font);
    } 

    // Skybox
    {
        // Vertex data for skybox
        float v_data[] = {
            -1.0f,  1.0f,  1.0f, 
            -1.0f, -1.0f,  1.0f, 
             1.0f, -1.0f,  1.0f,
             1.0f,  1.0f,  1.0f, 
            -1.0f,  1.0f, -1.0f, 
            -1.0f, -1.0f, -1.0f, 
             1.0f, -1.0f, -1.0f, 
             1.0f,  1.0f, -1.0f 
        };

        uint16_t i_data[] = {
           0, 1, 2, 3,
           3, 2, 6, 7,
           7, 6, 5, 4,
           4, 5, 1, 0,
           0, 3, 7, 4, // T
           1, 2, 6, 5  // B
        };

        bsf_model_t skybox = {0}; 

        // Skybox vbo/ibo
        skybox.vbo = gs_graphics_vertex_buffer_create(&(gs_graphics_vertex_buffer_desc_t){
            .data = v_data,
            .size = sizeof(v_data)
        });

        skybox.ibo = gs_graphics_index_buffer_create(&(gs_graphics_index_buffer_desc_t){
            .data = i_data, 
            .size = sizeof(i_data)
        });

        skybox.material = gs_hash_table_getp(assets->materials, gs_hash_str64("mat.skybox"));

        gs_hash_table_insert(assets->models, gs_hash_str64("models.skybox"), skybox);
    } 

    struct {const char* key; const char* path;} style_sheets[] = {
        {.key = "ss.title", .path = "style_sheets/bsf_title.ss"},
        {NULL}
    }; 

    for (uint32_t i = 0; style_sheets[i].key; ++i)
    {
        gs_snprintfc(TMP, 256, "%s/%s", assets->asset_dir, style_sheets[i].path);
        gs_gui_style_sheet_t ss = gs_gui_style_sheet_load_from_file(&bsf->gs.gui, TMP);
        gs_hash_table_insert(assets->style_sheets, gs_hash_str64(style_sheets[i].key), ss);
    }
}

GS_API_DECL void bsf_dbg_reload_ss(struct bsf_t* bsf)
{
    gs_gui_style_sheet_t* ss = gs_hash_table_getp(bsf->assets.style_sheets, gs_hash_str64("ss.title"));
    gs_gui_style_sheet_destroy(&bsf->gs.gui, ss);
    gs_snprintfc(TMP, 256, "%s/%s", bsf->assets.asset_dir, "style_sheets/bsf_title.ss");
    *ss = gs_gui_style_sheet_load_from_file(&bsf->gs.gui, TMP); 
}

//=== BSF Graphics ===//

GS_API_DECL uint32_t bsf_graphics_scene_renderable_create(bsf_graphics_scene_t* scene, const bsf_renderable_desc_t* desc)
{
    return gs_slot_array_insert(scene->renderables, *desc);
}

GS_API_DECL void bsf_graphics_scene_renderable_destroy(bsf_graphics_scene_t* scene, uint32_t hndl)
{
    if (gs_slot_array_handle_valid(scene->renderables, hndl))
    {
        gs_slot_array_erase(scene->renderables, hndl);
    }
}

GS_API_DECL void bsf_graphics_render(struct bsf_t* bsf)
{
    gs_command_buffer_t* cb = &bsf->gs.cb;
    gs_immediate_draw_t* gsi = &bsf->gs.gsi;
    gs_gui_context_t* gui = &bsf->gs.gui; 
	gs_vec2 fbs = gs_platform_framebuffer_sizev(gs_platform_main_window()); 

    // Render pass
    gs_graphics_renderpass_begin(cb, (gs_renderpass){0}); 
    { 
        gs_graphics_clear_desc_t clear = {.actions = &(gs_graphics_clear_action_t){.color = {0.05f, 0.05, 0.05, 1.f}}}; 
        gs_graphics_clear(cb, &clear);
        gs_graphics_set_viewport(cb, 0, 0, (uint32_t)fbs.x, (uint32_t)fbs.y);

        switch (bsf->state)
        {
            case BSF_STATE_PLAY:
            {
                // Need to eventually sort all renderables by pipelines to reduce binds

                const gs_mat4 vp = gs_camera_get_view_projection(&bsf->scene.camera.cam, fbs.x, fbs.y); 

                // Main render pass for scene 
                for (
                        gs_slot_array_iter it = gs_slot_array_iter_new(bsf->scene.renderables);
                        gs_slot_array_iter_valid(bsf->scene.renderables, it);
                        gs_slot_array_iter_advance(bsf->scene.renderables, it)
                )
                {
                    bsf_renderable_t* rend = gs_slot_array_iter_getp(bsf->scene.renderables, it); 
                    gs_gfxt_material_t* mat = rend->material;
                    gs_gfxt_mesh_t* mesh = rend->mesh; 
                    gs_mat4 mvp = gs_mat4_mul(vp, rend->model);

                    gs_gfxt_material_set_uniform(mat, "u_mvp", &mvp);
                    gs_gfxt_material_bind(cb, mat);
                    gs_gfxt_material_bind_uniforms(cb, mat);
                    gs_gfxt_mesh_draw(cb, mesh); 
                } 

                // Render skybox
                bsf_model_t* skybox = gs_hash_table_getp(bsf->assets.models, gs_hash_str64("models.skybox"));
				gs_gfxt_texture_t* cmap = gs_hash_table_getp(bsf->assets.cubemaps, gs_hash_str64("cmap.skybox"));
                gs_assert(skybox);
				gs_assert(cmap);
                gs_mat4 model = gs_mat4_scalev(gs_v3s(2000.f));
                gs_mat4 mvp = gs_mat4_mul(vp, model);
				gs_gfxt_material_set_uniform(skybox->material, "u_tex", cmap);
                gs_gfxt_material_set_uniform(skybox->material, "u_mvp", &mvp);
                gs_gfxt_material_bind(cb, skybox->material);
                gs_gfxt_material_bind_uniforms(cb, skybox->material);
                gs_graphics_bind_desc_t binds = {
                    .vertex_buffers = {.desc = &(gs_graphics_bind_vertex_buffer_desc_t){.buffer = skybox->vbo}},
                    .index_buffers = {.desc = &(gs_graphics_bind_index_buffer_desc_t){.buffer = skybox->ibo}}
                };
                gs_graphics_apply_bindings(cb, &binds);
                gs_graphics_draw(cb, &(gs_graphics_draw_desc_t){.start = 0, .count = 36});
            } break;
        }

        // Render all gsi
        gsi_renderpass_submit_ex(gsi, cb, NULL);

        // Render all gui
        gs_gui_render(gui, cb);
    } 
    gs_graphics_renderpass_end(cb);

	// Submit command buffer for GPU
	gs_graphics_command_buffer_submit(cb);
}

//=== BSF Entities ===// 

GS_API_DECL void bsf_entities_init(struct bsf_t* bsf)
{
    bsf->entities.world = ecs_init();

#define ECS_REGISTER_COMP(T)\
    do {\
        ecs_id(T) = ecs_component_init(bsf->entities.world, &(ecs_component_desc_t) {\
            .entity.name = gs_to_str(T),\
            .size = sizeof(T),\
            .alignment = ECS_ALIGNOF(T)\
        });\
    } while (0)

    // Register all components
    ecs_component_desc_t desc = {0};

    ECS_REGISTER_COMP(bsf_component_renderable_t); 
    ecs_set_component_actions_w_id(bsf->entities.world, ecs_id(bsf_component_renderable_t), &(EcsComponentLifecycle) {
        .dtor = bsf_component_renderable_dtor
    });

    ECS_REGISTER_COMP(bsf_component_transform_t);
    ECS_REGISTER_COMP(bsf_component_physics_t); 
    ECS_REGISTER_COMP(bsf_component_timer_t); 
    ECS_REGISTER_COMP(bsf_component_projectile_t); 
    ECS_REGISTER_COMP(bsf_component_mob_t); 
    ECS_REGISTER_COMP(bsf_component_renderable_immediate_t); 
    ECS_REGISTER_COMP(bsf_component_health_t); 
    ECS_REGISTER_COMP(bsf_component_consumable_t); 
    ECS_REGISTER_COMP(bsf_component_gun_t); 
    ECS_REGISTER_COMP(bsf_component_character_stats_t);
    ECS_REGISTER_COMP(bsf_component_camera_track_t);
    ECS_REGISTER_COMP(bsf_component_barrel_roll_t);
    ECS_REGISTER_COMP(bsf_component_explosion_t);
    ECS_REGISTER_COMP(bsf_component_inventory_t);
    ECS_REGISTER_COMP(bsf_component_obstacle_t);
    ECS_REGISTER_COMP(bsf_component_item_chest_t);
    ECS_REGISTER_COMP(bsf_component_item_t);

    ECS_REGISTER_COMP(bsf_component_ai_t); 
    ecs_set_component_actions_w_id(bsf->entities.world, ecs_id(bsf_component_ai_t), &(EcsComponentLifecycle) {
        .dtor = bsf_component_ai_dtor
    });

    // Register all systems 

    ECS_SYSTEM(
        bsf->entities.world, 
        bsf_player_system, 
        EcsOnUpdate,
        bsf_component_renderable_t, 
        bsf_component_transform_t, 
        bsf_component_physics_t, 
        bsf_component_character_stats_t, 
        bsf_component_health_t,
        bsf_component_gun_t,
        bsf_component_camera_track_t,
        bsf_component_barrel_roll_t,
        bsf_component_inventory_t
    );

    ECS_SYSTEM(
        bsf->entities.world, 
        bsf_projectile_system, 
        EcsOnUpdate,
        bsf_component_renderable_t, bsf_component_transform_t, bsf_component_physics_t, bsf_component_timer_t, bsf_component_projectile_t
    );

    ECS_SYSTEM(
        bsf->entities.world, 
        bsf_renderable_system, 
        EcsOnUpdate,
        bsf_component_renderable_t, bsf_component_transform_t
    );

    ECS_SYSTEM(
        bsf->entities.world, 
        bsf_physics_debug_draw_system,
        EcsOnUpdate,
        bsf_component_physics_t, bsf_component_transform_t
    ); 

    ECS_SYSTEM(
        bsf->entities.world, 
        bsf_renderable_immediate_system, 
        EcsOnUpdate,
        bsf_component_renderable_immediate_t, bsf_component_transform_t
    );

    ECS_SYSTEM(
        bsf->entities.world, 
        bsf_mob_system,
        EcsOnUpdate,
        bsf_component_renderable_t, 
        bsf_component_transform_t,
        bsf_component_physics_t, 
        bsf_component_health_t, 
        bsf_component_mob_t,
        bsf_component_gun_t,
        bsf_component_ai_t
    ); 

    ECS_SYSTEM(
        bsf->entities.world, 
        bsf_consumable_system, 
        EcsOnUpdate,
        bsf_component_transform_t, 
        bsf_component_physics_t, 
        bsf_component_consumable_t
    ); 

    ECS_SYSTEM(
        bsf->entities.world, 
        bsf_explosion_system,
        EcsOnUpdate,
        bsf_component_explosion_t,
        bsf_component_physics_t,
        bsf_component_transform_t, 
        bsf_component_timer_t 
    );

    ECS_SYSTEM(
        bsf->entities.world, 
        bsf_obstacle_system,
        EcsOnUpdate,
        bsf_component_transform_t, 
        bsf_component_physics_t,
        bsf_component_obstacle_t
    );

    ECS_SYSTEM(
        bsf->entities.world, 
        bsf_item_chest_system, 
        EcsOnUpdate, 
        bsf_component_transform_t,
        bsf_component_physics_t,
        bsf_component_item_chest_t,
		bsf_component_renderable_immediate_t 
    );

    ECS_SYSTEM(
        bsf->entities.world, 
        bsf_item_system,
        EcsOnUpdate, 
        bsf_component_transform_t,
        bsf_component_physics_t, 
        bsf_component_item_t,
		bsf_component_ai_t 
    );
}

//=== BSF Components ===// 

GS_API_DECL void bsf_component_renderable_dtor(ecs_world_t* world, ecs_entity_t comp, const ecs_entity_t* ent, void* ptr, size_t sz, int32_t count, void* ctx)
{
    bsf_t* bsf = gs_user_data(bsf_t);
    bsf_component_renderable_t* rend = ecs_get(world, *ent, bsf_component_renderable_t);
    bsf_graphics_scene_renderable_destroy(&bsf->scene, rend->hndl);
} 

//=== BSF AI ===// 

GS_API_DECL void bsf_component_ai_dtor( ecs_world_t* world, ecs_entity_t comp, const ecs_entity_t* ent, void* ptr, size_t sz, int32_t count, void* ctx )
{
    gs_println("DESTROY AI");
    bsf_t* bsf = gs_user_data(bsf_t);
    bsf_component_ai_t* ai = ecs_get(world, *ent, bsf_component_ai_t);
	gs_ai_bt_free(&ai->bt);
}

//=== BSF Physics ===// 

GS_API_DECL gs_contact_info_t bsf_component_physics_collide(const bsf_component_physics_t* c0, 
        const gs_vqs* xform0, const bsf_component_physics_t* c1, const gs_vqs* xform1)
{ 
    gs_contact_info_t res = {0}; 
    gs_vqs xf0 = gs_vqs_absolute_transform(&c0->collider.xform, xform0);
    gs_vqs xf1 = gs_vqs_absolute_transform(&c1->collider.xform, xform1); 

    switch (c0->collider.type)
    {
        case BSF_COLLIDER_AABB: 
        {
            gs_aabb_t* s0 = &c0->collider.shape.aabb;

            switch (c1->collider.type) {
                case BSF_COLLIDER_AABB:     gs_aabb_vs_aabb(s0, &xf0, &c1->collider.shape.aabb, &xf1, &res); break;
                case BSF_COLLIDER_SPHERE:   gs_aabb_vs_sphere(s0, &xf0, &c1->collider.shape.sphere, &xf1, &res); break;
                case BSF_COLLIDER_CONE:     gs_aabb_vs_cone(s0, &xf0, &c1->collider.shape.cone, &xf1, &res); break;
                case BSF_COLLIDER_CYLINDER: gs_aabb_vs_cylinder(s0, &xf0, &c1->collider.shape.cylinder, &xf1, &res); break;
            }
        } break;

        case BSF_COLLIDER_SPHERE:
        {
            gs_sphere_t* s0 = &c0->collider.shape.sphere;

            switch (c1->collider.type) {
                case BSF_COLLIDER_AABB:     gs_sphere_vs_aabb(s0, &xf0, &c1->collider.shape.aabb, &xf1, &res); break;
                case BSF_COLLIDER_SPHERE:   gs_sphere_vs_sphere(s0, &xf0, &c1->collider.shape.sphere, &xf1, &res); break;
                case BSF_COLLIDER_CONE:     gs_sphere_vs_cone(s0, &xf0, &c1->collider.shape.cone, &xf1, &res); break;
                case BSF_COLLIDER_CYLINDER: gs_sphere_vs_cylinder(s0, &xf0, &c1->collider.shape.cylinder, &xf1, &res); break;
            }
        } break;

        case BSF_COLLIDER_CONE:
        {
            gs_cone_t* s0 = &c0->collider.shape.cone;

            switch (c1->collider.type) {
                case BSF_COLLIDER_AABB:     gs_cone_vs_aabb(s0, &xf0, &c1->collider.shape.aabb, &xf1, &res); break;
                case BSF_COLLIDER_SPHERE:   gs_cone_vs_sphere(s0, &xf0, &c1->collider.shape.sphere, &xf1, &res); break;
                case BSF_COLLIDER_CONE:     gs_cone_vs_cone(s0, &xf0, &c1->collider.shape.cone, &xf1, &res); break;
                case BSF_COLLIDER_CYLINDER: gs_cone_vs_cylinder(s0, &xf0, &c1->collider.shape.cylinder, &xf1, &res); break;
            }
        } break;

        case BSF_COLLIDER_CYLINDER:
        {
            gs_cylinder_t* s0 = &c0->collider.shape.cylinder;

            switch (c1->collider.type) {
                case BSF_COLLIDER_AABB:     gs_cylinder_vs_aabb(s0, &xf0, &c1->collider.shape.aabb, &xf1, &res); break;
                case BSF_COLLIDER_SPHERE:   gs_cylinder_vs_sphere(s0, &xf0, &c1->collider.shape.sphere, &xf1, &res); break;
                case BSF_COLLIDER_CONE:     gs_cylinder_vs_cone(s0, &xf0, &c1->collider.shape.cone, &xf1, &res); break;
                case BSF_COLLIDER_CYLINDER: gs_cylinder_vs_cylinder(s0, &xf0, &c1->collider.shape.cylinder, &xf1, &res); break;
            }
        } break;
    }

    return res;
}

//=== BSF Exposion ===// 

GS_API_DECL ecs_entity_t bsf_explosion_create(struct bsf_t* bsf, ecs_world_t* world, gs_vqs* xform, bsf_owner_type owner)
{ 
    ecs_entity_t b = ecs_new(world, 0); 

    ecs_set(world, b, bsf_component_transform_t, {.xform = *xform});

    ecs_set(world, b, bsf_component_physics_t, {
        .velocity = gs_v3s(0.f), 
        .speed = 0.f,
        .collider = {
            .xform = gs_vqs_default(),
            .type = BSF_COLLIDER_SPHERE,
            .shape.sphere = {
                .r = 0.5f,
                .c = gs_v3s(0.f)
            }
        }
    }); 

    bsf_camera_shake(bsf, &bsf->scene.camera, 0.3f);

    // Play sound
    bsf_play_sound(bsf, "audio.explosion", 0.1f);

    ecs_set(world, b, bsf_component_timer_t, {.max = 1.f}); 
    ecs_set(world, b, bsf_component_explosion_t, {.owner = owner});

    return b;
}

GS_API_DECL void bsf_explosion_system(ecs_iter_t* it)
{
    bsf_t* bsf = gs_user_data(bsf_t); 
    const float t = gs_platform_elapsed_time();
    const float dt = gs_platform_delta_time(); 
    bsf_component_explosion_t* eca = ecs_term(it, bsf_component_explosion_t, 1);
    bsf_component_physics_t* pca = ecs_term(it, bsf_component_physics_t, 2);
    bsf_component_transform_t* tca = ecs_term(it, bsf_component_transform_t, 3); 
    bsf_component_timer_t* kca = ecs_term(it, bsf_component_timer_t, 4); 
    const bsf_room_t* room = gs_slot_array_getp(bsf->run.rooms, bsf->run.room_ids[bsf->run.cell]);

    if (bsf->dbg) return; 

    for (uint32_t i = 0; i < it->count; ++i)
    {
        ecs_entity_t e = it->entities[i];
        bsf_component_explosion_t* ec = &eca[i];
        bsf_component_physics_t* pc = &pca[i];
        bsf_component_transform_t* tc = &tca[i];
        bsf_component_timer_t* kc = &kca[i]; 

        // Update timer
        kc->time += dt;

        tc->xform.scale = gs_vec3_add(tc->xform.scale, gs_v3s(0.1f));

        if (kc->time >= kc->max) 
        {
            ecs_delete(it->world, e);
        } 

        switch (ec->owner)
        {
            case BSF_OWNER_PLAYER:
            {
				for (uint32_t m = 0; m < gs_dyn_array_size(room->mobs); ++m)
				{
					ecs_entity_t mob = room->mobs[m]; 
					bsf_component_transform_t* mtc = ecs_get(bsf->entities.world, mob, bsf_component_transform_t);
					bsf_component_physics_t* mpc = ecs_get(bsf->entities.world, mob, bsf_component_physics_t); 
					if (!mtc || !mpc) continue;
					gs_contact_info_t res = bsf_component_physics_collide(pc, &tc->xform, mpc, &mtc->xform); 

					if (res.hit)
					{ 
						bsf_component_health_t* h = ecs_get(bsf->entities.world, mob, bsf_component_health_t);
						h->hit = true; 
						h->health -= 1.f;
						h->hit_timer = 0.f;
						break;
					} 
				}
            } break;
        }
    }
}

//=== BSF Systems ===// 

GS_API_DECL void bsf_physics_debug_draw_system(ecs_iter_t* it)
{
    bsf_t* bsf = gs_user_data(bsf_t); 
    gs_immediate_draw_t* gsi = &bsf->gs.gsi;
    const float t = gs_platform_elapsed_time();
    const float dt = gs_platform_delta_time(); 
    const gs_vec2 fbs = gs_platform_framebuffer_sizev(gs_platform_main_window());
    bsf_component_physics_t* pca = ecs_term(it, bsf_component_physics_t, 1);
    bsf_component_transform_t* tca = ecs_term(it, bsf_component_transform_t, 2);

	gsi_defaults(gsi);
	gsi_depth_enabled(gsi, true);
	gsi_blend_enabled(gsi, true);
    gsi_camera(gsi, &bsf->scene.camera.cam, (uint32_t)fbs.x, (uint32_t)fbs.y);
    for (uint32_t i = 0; i < it->count; ++i)
    {
        ecs_entity_t e = it->entities[i];
        bsf_component_physics_t* pc = &pca[i];
        bsf_component_transform_t* tc = &tca[i];

        gsi_push_matrix(gsi, GSI_MATRIX_MODELVIEW);
        {
			gs_vqs xform = gs_vqs_absolute_transform(&pc->collider.xform, &tc->xform);
            gsi_mul_matrix(gsi, gs_vqs_to_mat4(&xform));
            switch (pc->collider.type)
            {
                default:
                case BSF_COLLIDER_AABB:
                {
                    gs_aabb_t* s = &pc->collider.shape.aabb;
                    gs_vec3 hd = gs_vec3_scale(gs_vec3_sub(s->max, s->min), 0.5f);
                    gs_vec3 c = gs_vec3_add(s->min, hd);
                    gsi_box(gsi, c.x, c.y, c.z, hd.x, hd.y, hd.z, 255, 255, 255, 255, GS_GRAPHICS_PRIMITIVE_LINES);
                } break;

                case BSF_COLLIDER_SPHERE:
                {
                    gs_sphere_t* s = &pc->collider.shape.sphere;
                    gsi_sphere(gsi, s->c.x, s->c.y, s->c.z, s->r, 255, 255, 255, 255, GS_GRAPHICS_PRIMITIVE_LINES); 
                } break; 

                case BSF_COLLIDER_CYLINDER:
                {
                    gs_cylinder_t* s = &pc->collider.shape.cylinder;
                    gsi_cylinder(gsi, s->base.x, s->base.y, s->base.z, s->r, s->r, s->height, 16, 255, 255, 255, 255, GS_GRAPHICS_PRIMITIVE_LINES); 
                } break; 

                case BSF_COLLIDER_CONE:
                { 
                    gs_cone_t* s = &pc->collider.shape.cone;
                    gsi_cone(gsi, s->base.x, s->base.y, s->base.z, s->r, s->height, 16, 255, 255, 255, 255, GS_GRAPHICS_PRIMITIVE_LINES);
                } break;
            }
        }
        gsi_pop_matrix(gsi);
    }
} 

GS_API_DECL void bsf_renderable_immediate_system(ecs_iter_t* it)
{
    bsf_t* bsf = gs_user_data(bsf_t); 
    gs_immediate_draw_t* gsi = &bsf->gs.gsi;
    const float t = gs_platform_elapsed_time();
    const float dt = gs_platform_delta_time(); 
    const gs_platform_input_t* input = &gs_subsystem(platform)->input;
    const gs_platform_gamepad_t* gp = &input->gamepads[0];
    const gs_vec2 fbs = gs_platform_framebuffer_sizev(gs_platform_main_window());
    bsf_component_renderable_immediate_t* rca = ecs_term(it, bsf_component_renderable_immediate_t, 1);
    bsf_component_transform_t* tca = ecs_term(it, bsf_component_transform_t, 2);

    gsi_camera3D(gsi, (uint32_t)fbs.x, (uint32_t)fbs.y);
    for (uint32_t i = 0; i < it->count; ++i)
    { 
		bsf_component_renderable_immediate_t* rc = &rca[i];
		bsf_component_transform_t* tc = &tca[i];

        if (rc->shape == BSF_SHAPE_ROOM) continue;
                
        rc->model = gs_vqs_to_mat4(&tc->xform); 
        const gs_color_t* col = &rc->color;
        gs_gfxt_pipeline_t* pip = gs_gfxt_material_get_pipeline(rc->material);
        gs_mat4 mvp = {0}; 

        if (rc->opt & BSF_RENDERABLE_IMMEDIATE_BLEND_ENABLE)     gsi_blend_enabled(gsi, true);
        if (rc->opt & BSF_RENDERABLE_IMMEDIATE_DEPTH_ENABLE)     gsi_depth_enabled(gsi, true);
        if (rc->opt & BSF_RENDERABLE_IMMEDIATE_FACE_CULL_ENABLE) gsi_face_cull_enabled(gsi, true);

        gsi_push_matrix(gsi, GSI_MATRIX_MODELVIEW);
        {
            gsi_vattr_list_mesh(gsi, pip->mesh_layout, gs_dyn_array_size(pip->mesh_layout) * sizeof(gs_gfxt_mesh_layout_t)); 
            gsi_load_matrix(gsi, rc->model);

            switch (rc->shape)
            {
                case BSF_SHAPE_RECT:
                {
                    gsi_rectvd(gsi, gs_v2s(-0.5f), gs_v2s(0.5f), gs_v2s(0.f), gs_v2s(1.f), *col, pip->desc.raster.primitive);
                } break;

                case BSF_SHAPE_SPHERE:
                {
                    gsi_sphere(gsi, 0.f, 0.f, 0.f, 0.5, col->r, col->g, col->b, col->a, pip->desc.raster.primitive); 
                } break;

                case BSF_SHAPE_BOX:
                {
                    gsi_box(gsi, 0.f, 0.f, 0.f, 0.5f, 0.5f, 0.5f, col->r, col->g, col->b, col->a, pip->desc.raster.primitive);
                } break;

                case BSF_SHAPE_CONE:
                {
                    gsi_cone(gsi, 0.f, 0.f, 0.f, 0.5f, 1.f, 16, col->r, col->g, col->b, col->a, pip->desc.raster.primitive);
                } break;

                case BSF_SHAPE_CYLINDER:
                {
                    gsi_cylinder(gsi, 0.f, 0.f, 0.f, 0.5f, 0.5f, 1.f, 16, col->r, col->g, col->b, col->a, pip->desc.raster.primitive);
                } break;

                case BSF_SHAPE_CROSS:
                {
                    gs_aabb_t aabb = {
                        .min = gs_v3s(-0.5f),
                        .max = gs_v3s(0.5f)
                    };
                    gs_vec3 hd = gs_vec3_scale(gs_vec3_sub(aabb.max, aabb.min), 0.5f);
                    gs_vec3 c = gs_vec3_add(aabb.min, hd);
                    gsi_box(gsi, c.x, c.y, c.z, hd.x * 0.2f, hd.y, hd.z * 0.2f, col->r, col->g, col->b, col->a, pip->desc.raster.primitive);
                    gsi_box(gsi, c.x, c.y, c.z, hd.x, hd.y * 0.2f, hd.z * 0.2f, col->r, col->g, col->b, col->a, pip->desc.raster.primitive);

                } break;
            }

            mvp = gsi_get_mvp_matrix(gsi); 
        }
        gs_gfxt_texture_t tex = rc->texture ? *rc->texture : GSI()->tex_default;
        gsi_pop_matrix_ex(gsi, false);
		gs_mat4 vp = gs_camera_get_view_projection(&bsf->scene.camera.cam, fbs.x, fbs.y);
		mvp = gs_mat4_mul(vp, rc->model);
        gs_gfxt_material_set_uniform(rc->material, "u_mvp", &mvp);
        gs_gfxt_material_set_uniform(rc->material, "u_tex", &tex);
        gs_gfxt_material_bind(&gsi->commands, rc->material);
        gs_gfxt_material_bind_uniforms(&gsi->commands, rc->material);
        gsi_flush(gsi); 
    } 

    // World 
    gsi_defaults(gsi);
    gsi_blend_enabled(gsi, true);
    gsi_depth_enabled(gsi, true);
    gsi_camera(gsi, &bsf->scene.camera, (uint32_t)fbs.x, (uint32_t)fbs.y);
    bsf_room_t* room = gs_slot_array_getp(bsf->run.rooms, bsf->run.room_ids[bsf->run.cell]);
    switch (room->movement_type)
    {
        case BSF_MOVEMENT_FREE_RANGE:
        {
            const float sz = 2000;
            const float sx = 2000;
            const float sy = 300; 
            const float step = 20.f;
            gs_color_t c0 = gs_color(6, 59, 0, 255); 
            gs_color_t c1 = gs_color(14, 255, 0, 255); 
            for (float r = -sz; r <= sz; r += step)
            {
                const float ct = gs_map_range(-sz, sz, 0.f, 1.f, r);
                gs_color_t color = gs_color(
                    (uint8_t)(255.f * gs_interp_smoothstep((float)c0.r / 255.f, (float)c1.r / 255.f, ct)),
                    (uint8_t)(255.f * gs_interp_smoothstep((float)c0.g / 255.f, (float)c1.g / 255.f, ct)), 
                    (uint8_t)(255.f * gs_interp_smoothstep((float)c0.b / 255.f, (float)c1.b / 255.f, ct)),
                    (uint8_t)(255.f * gs_interp_smoothstep((float)c0.a / 255.f, (float)c1.a / 255.f, ct))
                );

                gs_vec3 s = gs_v3(-sx, 0.f, r);
                gsi_rect3Dv(gsi, s, gs_v3(2 * sx, 0.f, r + step), gs_v2s(0.f), gs_v2s(1.f), color, GS_GRAPHICS_PRIMITIVE_TRIANGLES);
            }
        } break;

        case BSF_MOVEMENT_RAIL:
        {
            // All the ground
            const float sz = 150;
            const float sx = 500;
            const float sy = 100;
            const float step = 20.f;
            const float speed_mod = gp->axes[GS_PLATFORM_JOYSTICK_AXIS_RTRIGGER] >= 0.4f || gs_platform_key_down(GS_KEYCODE_LEFT_SHIFT) ? 3.f : 1.f;
            const float zoff = fmod(t * 0.02f * speed_mod, step);
            gs_color_t c0 = gs_color(6, 59, 0, 255); 
            gs_color_t c1 = gs_color(14, 255, 0, 255); 
            for (float r = -sz * 0.5f; r <= sz; r += step)
            { 
                for (float c = -80; c < 80; c += step)
                {
                    // Draw columns
                    gsi_box(gsi, c, 0.f, r + zoff, 0.1f, 0.1f, 0.1f, 255, 255, 255, 255, GS_GRAPHICS_PRIMITIVE_TRIANGLES);
                }
            }

            for (float r = -sz; r <= sz; r += step)
            {
                const float ct = gs_map_range(-sz, sz, 0.f, 1.f, r);
                gs_color_t color = gs_color(
                    (uint8_t)(255.f * gs_interp_smoothstep((float)c0.r / 255.f, (float)c1.r / 255.f, ct)),
                    (uint8_t)(255.f * gs_interp_smoothstep((float)c0.g / 255.f, (float)c1.g / 255.f, ct)), 
                    (uint8_t)(255.f * gs_interp_smoothstep((float)c0.b / 255.f, (float)c1.b / 255.f, ct)),
                    (uint8_t)(255.f * gs_interp_smoothstep((float)c0.a / 255.f, (float)c1.a / 255.f, ct))
                );

                gs_vec3 s = gs_v3(-sx, 0.f, r);
                gsi_rect3Dv(gsi, s, gs_v3(2 * sx, 0.f, r + step), gs_v2s(0.f), gs_v2s(1.f), color, GS_GRAPHICS_PRIMITIVE_TRIANGLES);
            }

            // Debug bounding area
            gs_vec3 kb = {BSF_ROOM_BOUND_X, BSF_ROOM_BOUND_Y, BSF_ROOM_BOUND_Z}; 
            /*
            gsi_box(gsi, 0.f, kb.y / 2.f, 0.f, kb.x + 1.f, kb.y / 2.f, kb.z + 1.f, 0, 255, 0, 255, GS_GRAPHICS_PRIMITIVE_LINES);
            */

            // Drawing the tiny, moving guide boxes
            for (float r = -sz; r <= step; r += step)
            { 
                for (float c = -sx; c < sx; c += step)
                {
                    // Draw columns
                    float y = gs_perlin2(sx, sz);
                    gsi_box(gsi, c, y * 0.5f, sz + zoff, 1.f, y * 0.5f, 1.f, 0, 255, 0, 255, GS_GRAPHICS_PRIMITIVE_TRIANGLES);
                }
            }

            // Do random boxes based on seed value (for obstacles) 
            gs_mt_rand_t rand = gs_rand_seed(gs_hash_str64("mountains"));
            for (uint32_t i = 0; i < 100; ++i) 
            {
                float rx = gs_rand_gen_range(&rand, -500, 500.f); 
                float rsx = gs_rand_gen_range(&rand, 1.f, 50.f); 
                float ry = gs_rand_gen_range(&rand, 1.f, kb.y); 
                float rz = 1.f; 
                gsi_box(gsi, rx, ry * 0.5f, -kb.z, rsx, ry * 0.5f, rz, 20, 100, 255, 255, GS_GRAPHICS_PRIMITIVE_TRIANGLES);
            }

        } break;
    }

}

GS_API_DECL void bsf_renderable_system(ecs_iter_t* it)
{
    bsf_t* bsf = gs_user_data(bsf_t); 
    const float t = gs_platform_elapsed_time();
    const float dt = gs_platform_delta_time(); 
    bsf_component_renderable_t* rc = ecs_term(it, bsf_component_renderable_t, 1);
    bsf_component_transform_t* tc = ecs_term(it, bsf_component_transform_t, 2);

    for (uint32_t i = 0; i < it->count; ++i)
    {
        // Update renderable based on transform (can do this in a separate system for just renderable and transform pairs)
        if (gs_slot_array_handle_valid(bsf->scene.renderables, rc[i].hndl)) {
            bsf_renderable_t* rend = gs_slot_array_getp(bsf->scene.renderables, rc[i].hndl);
            rend->model = gs_vqs_to_mat4(&tc[i].xform); 
        }
    }
} 

//=== BSF Camera ===// 

GS_API_DECL void bsf_camera_init(struct bsf_t* bsf, bsf_camera_t* camera)
{
    camera->cam = gs_camera_perspective();
    camera->shake_time = 0.f;
    camera->pitch = 0.f;
    camera->cam.fov = 60.f;
}

GS_API_DECL void bsf_camera_update(struct bsf_t* bsf, bsf_camera_t* camera) 
{
    const float dt = gs_platform_delta_time() * bsf->run.time_scale;
    bsf_component_camera_track_t* ptc = ecs_get(bsf->entities.world, bsf->entities.player, bsf_component_camera_track_t);
	gs_camera_t* c = &camera->cam; 
    const bsf_room_t* room = gs_slot_array_getp(bsf->run.rooms, bsf->run.room_ids[bsf->run.cell]);
    gs_vec3 so = gs_v3s(0.f);

    // Do camera shake
    if (camera->shake_time > 0.f)
    {
        // Shake offset
        so = gs_vec3_scale(gs_vec3_norm(gs_v3(
            gs_rand_gen_range(&bsf->run.rand, -1.f, 1.f),
            gs_rand_gen_range(&bsf->run.rand, -1.f, 1.f),
            0.f
        )), 0.1f * camera->shake_time);
        camera->shake_time -= dt;
    }

	// Update camera position to follow target
    switch (room->movement_type)
    {
        case BSF_MOVEMENT_FREE_RANGE:
        { 
            gs_vec3 backward = gs_quat_rotate(ptc->xform.rotation, gs_vec3_scale(GS_ZAXIS, 0.2f));
            gs_vec3 up = gs_quat_rotate(ptc->xform.rotation, gs_vec3_scale(GS_YAXIS, 0.2f));
            gs_vec3 target = gs_vec3_add(ptc->xform.position, gs_vec3_add(backward, up));
            gs_vec3 v = gs_vec3_sub(target, c->transform.position);

            gs_vec3 np = gs_vec3_add(c->transform.position, v); 

            const float plerp = 0.025f;
            const float zlerp = 0.05f;
            const float rlerp = 0.05f;

            c->transform.rotation = gs_quat_slerp(c->transform.rotation, ptc->xform.rotation, rlerp);

            // Lerp position to new position
            c->transform.position = gs_v3(
                gs_interp_linear(c->transform.position.x, np.x, zlerp),
                gs_interp_linear(c->transform.position.y, np.y, zlerp),
                gs_interp_linear(c->transform.position.z, np.z, zlerp)
            ); 

        } break;

        case BSF_MOVEMENT_RAIL:
        { 
            bsf_component_transform_t* ptc = ecs_get(bsf->entities.world, bsf->entities.player, bsf_component_transform_t);

            // Update camera position to follow target
            gs_vec3 backward = gs_quat_rotate(ptc->xform.rotation, GS_ZAXIS);
            gs_vec3 target = gs_vec3_add(ptc->xform.position, gs_v3(0.f, 0.f, 3.f));
            gs_vec3 v = gs_vec3_sub(target, c->transform.position); 

            gs_vec3 np = gs_vec3_add(c->transform.position, v);

            const float plerp = 0.08f;
            const float mult = 2.f;

            // Lerp position to new position
            c->transform.position = gs_v3(
                gs_interp_linear(c->transform.position.x, v.x * mult, plerp),
                gs_interp_linear(c->transform.position.y, 4.f + v.y * mult, plerp),
                gs_interp_linear(c->transform.position.z, np.z, plerp)
            ); 
            
            c->transform.rotation = gs_quat_angle_axis(gs_deg2rad(target.x * 1.4f), GS_ZAXIS);
        } break;
    } 

    // Camera shake
    c->transform.position = gs_vec3_add(c->transform.position, gs_quat_rotate(c->transform.rotation, so));
}

GS_API_DECL void bsf_camera_shake(struct bsf_t* bsf, bsf_camera_t* camera, float amt)
{
    camera->shake_time = gs_clamp(camera->shake_time + amt, 0.f, 1.f);
}

//=== BSF Obstacle ===//

GS_API_DECL ecs_entity_t bsf_obstacle_create(struct bsf_t* bsf, gs_vqs* xform, bsf_obstacle_type type)
{
    gs_gfxt_material_t* mat = gs_hash_table_getp(bsf->assets.materials, gs_hash_str64("mat.gsi"));
    ecs_entity_t e = ecs_new(bsf->entities.world, 0); 

    ecs_set(bsf->entities.world, e, bsf_component_transform_t, {.xform = *xform});
	ecs_set(bsf->entities.world, e, bsf_component_obstacle_t, {.type = type});

    switch (type)
    {
        case BSF_OBSTACLE_BUILDING:
        { 
            ecs_set(bsf->entities.world, e, bsf_component_renderable_immediate_t, {
                .shape = BSF_SHAPE_BOX,
                .model = gs_vqs_to_mat4(xform),
				.material = mat,
                .color = GS_COLOR_WHITE
            });

	        ecs_set(bsf->entities.world, e, bsf_component_physics_t, {
				.collider = {
					.type = BSF_COLLIDER_AABB,
					.xform = gs_vqs_default(),
					.shape.aabb = (gs_aabb_t) {
						.min = gs_v3s(-0.5f),
						.max = gs_v3s(0.5f)
					}
				}
            });
        } break;
    }
} 

GS_API_DECL void bsf_obstacle_system(ecs_iter_t* it)
{
	bsf_t* bsf = gs_user_data(bsf_t); 
	bsf_room_t* room = gs_slot_array_getp(bsf->run.rooms, bsf->run.room_ids[bsf->run.cell]);
    const gs_vec2 fbs = gs_platform_framebuffer_sizev(gs_platform_main_window());
    const gs_platform_input_t* input = gs_platform_input();
    const gs_platform_gamepad_t* gp = &input->gamepads[0];
    const float t = gs_platform_elapsed_time();
    const float dt = gs_platform_delta_time() * bsf->run.time_scale; 
    bsf_component_transform_t* tca = ecs_term(it, bsf_component_transform_t, 1);
    bsf_component_physics_t* pca = ecs_term(it, bsf_component_physics_t, 2); 
    bsf_component_obstacle_t* oca = ecs_term(it, bsf_component_obstacle_t, 3); 

    bsf_component_transform_t* ptc = ecs_get(bsf->entities.world, bsf->entities.player, bsf_component_transform_t);
    bsf_component_physics_t* ppc = ecs_get(bsf->entities.world, bsf->entities.player, bsf_component_physics_t); 

    float speed_mod = gp->axes[GS_PLATFORM_JOYSTICK_AXIS_RTRIGGER] >= 0.4f || gs_platform_key_down(GS_KEYCODE_LEFT_SHIFT) ? 35.f : 20.f;
    if (room->cleared) speed_mod = 50.f;

    if (bsf->dbg) return; 

    for (uint32_t i = 0; i < it->count; ++i)
    {
        ecs_entity_t ent = it->entities[i];
        bsf_component_transform_t* tc = &tca[i];
        bsf_component_physics_t* pc = &pca[i]; 
        bsf_component_obstacle_t* oc = &oca[i]; 

        // Move obstacle forward over time towards player over time, snap back to start
        gs_vec3* pos = &tc->xform.translation; 

        switch (room->movement_type)
        {
            case BSF_MOVEMENT_RAIL:
            { 
                pos->z += dt * speed_mod;

                if (room->cleared && pos->z > BSF_ROOM_BOUND_Z) { 
                    bsf_obstacle_destroy(it->world, ent);
                } 
                else if (pos->z > BSF_ROOM_BOUND_Z) {
                    pos->z = -BSF_ROOM_BOUND_Z;
                }
            } break;

            case BSF_MOVEMENT_FREE_RANGE:
            {
                if (room->cleared) { 
                    bsf_obstacle_destroy(it->world, ent);
                } 
            } break;
        }
        
    }
} 

//=== BSF Item ===//

GS_API_DECL void bsf_item_remove_from_pool(struct bsf_t* bsf, bsf_item_type type)
{
    for (uint32_t i = 0; i < gs_dyn_array_size(bsf->run.item_pool); ++i)
    {
        if (bsf->run.item_pool[i] == type) return;
    }
    gs_dyn_array_push(bsf->run.item_pool, type);
} 

GS_API_DECL ecs_entity_t bsf_item_create(struct bsf_t* bsf, ecs_world_t* world, gs_vqs* xform, bsf_item_type type) 
{
    // Use a simple texture material with the assigned texture
    gs_gfxt_material_t* mat = gs_hash_table_getp(bsf->assets.materials, gs_hash_str64("mat.gsi"));
    ecs_entity_t e = ecs_new(world, 0); 
    gs_gfxt_texture_t* tex = NULL;

    switch (type)
    {
        case BSF_ITEM_SAD_ONION: {
            tex = gs_hash_table_getp(bsf->assets.textures, gs_hash_str64("tex.icon_sad_onion"));
        } break;

        case BSF_ITEM_INNER_EYE: {
            tex = gs_hash_table_getp(bsf->assets.textures, gs_hash_str64("tex.icon_inner_eye"));
        } break;

        case BSF_ITEM_SPOON_BENDER: {
            tex = gs_hash_table_getp(bsf->assets.textures, gs_hash_str64("tex.icon_spoon_bender"));
        } break;

        case BSF_ITEM_MAGIC_MUSHROOM: {
            tex = gs_hash_table_getp(bsf->assets.textures, gs_hash_str64("tex.icon_magic_mushroom"));
        } break;

        case BSF_ITEM_POLYPHEMUS: { 
            tex = gs_hash_table_getp(bsf->assets.textures, gs_hash_str64("tex.icon_polyphemus"));
        } break;
    }

	ecs_set(world, e, bsf_component_ai_t, {
		.target = xform->translation
	}); 

	ecs_set(world, e, bsf_component_renderable_immediate_t, {
		.shape = BSF_SHAPE_RECT,
		.model = gs_vqs_to_mat4(xform),
		.material = mat,
		.color = GS_COLOR_WHITE,
		.texture = tex
    });

    ecs_set(world, e, bsf_component_physics_t, {
        .collider = {
            .type = BSF_COLLIDER_AABB,
			.xform = (gs_vqs) {
				.translation = gs_v3(-0.25f, -0.25f, 0.f),
				.rotation = gs_quat_default(),
				.scale = gs_v3(0.5f, 0.5f, 0.2f)
			},
            .shape.aabb = (gs_aabb_t) {
                .min = gs_v3s(-0.5f),
                .max = gs_v3s(0.5f)
            }
        }
    }); 

    ecs_set(world, e, bsf_component_item_t, {
        .type = type
    });

    ecs_set(world, e, bsf_component_transform_t, {.xform = *xform});

    return e;
} 

void bsf_ai_task_float_up(struct gs_ai_bt_t* bt, struct gs_ai_bt_node_t* node) 
{ 
	bsf_t* bsf = gs_user_data(bsf_t); 
	bsf_room_t* room = gs_slot_array_getp(bsf->run.rooms, bsf->run.room_ids[bsf->run.cell]);
    const gs_vec2 fbs = gs_platform_framebuffer_sizev(gs_platform_main_window());
    const float t = gs_platform_elapsed_time();
    const float dt = gs_platform_delta_time() * bsf->run.time_scale; 
	bsf_ai_data_t* data = (bsf_ai_data_t*)bt->ctx.user_data;
	
	if (data->ac->wait < 2.f)
	{ 
		data->ac->wait += dt;
        data->tc->xform.translation.y += dt;
		data->tc->xform.rotation = gs_quat_angle_axis(t * 0.001f, GS_YAXIS);
		node->state = GS_AI_BT_STATE_RUNNING;
	}
	else
	{
		node->state = GS_AI_BT_STATE_SUCCESS;
	}
} 

void bsf_ai_task_player_give_item(struct gs_ai_bt_t* bt, struct gs_ai_bt_node_t* node) 
{
	bsf_t* bsf = gs_user_data(bsf_t); 
	bsf_ai_data_t* data = (bsf_ai_data_t*)bt->ctx.user_data; 
    bsf_player_item_pickup(bsf, data->world, data->ic->type);
    node->state = GS_AI_BT_STATE_SUCCESS; 
}

void bsf_ai_task_item_destroy(struct gs_ai_bt_t* bt, struct gs_ai_bt_node_t* node) 
{
	bsf_t* bsf = gs_user_data(bsf_t); 
	bsf_ai_data_t* data = (bsf_ai_data_t*)bt->ctx.user_data; 
    bsf_item_destroy(data->world, data->ent);
    node->state = GS_AI_BT_STATE_SUCCESS; 
}

GS_API_DECL void bsf_item_bt(gs_ai_bt_t* bt)
{
    gsai_bt(bt, { 

		gsai_sequence(bt, {

			// Float up and rotate over time
			gsai_leaf(bt, bsf_ai_task_float_up);

			// Move toward player
			gsai_leaf(bt, bsf_ai_task_move_to_target_location);

			// Give player item
			gsai_leaf(bt, bsf_ai_task_player_give_item);

            // Cleanup item
            gsai_leaf(bt, bsf_ai_task_item_destroy);
		});
    });
}

GS_API_DECL void bsf_item_system(ecs_iter_t* it)
{
	bsf_t* bsf = gs_user_data(bsf_t); 
	bsf_room_t* room = gs_slot_array_getp(bsf->run.rooms, bsf->run.room_ids[bsf->run.cell]);
    const gs_vec2 fbs = gs_platform_framebuffer_sizev(gs_platform_main_window());
    const float t = gs_platform_elapsed_time();
    const float dt = gs_platform_delta_time() * bsf->run.time_scale; 
    const gs_platform_input_t* input = gs_platform_input();
    const gs_platform_gamepad_t* gp = &input->gamepads[0];
    bsf_component_transform_t* tca = ecs_term(it, bsf_component_transform_t, 1);
    bsf_component_physics_t* pca = ecs_term(it, bsf_component_physics_t, 2); 
    bsf_component_item_t* ica = ecs_term(it, bsf_component_item_t, 3); 
    bsf_component_ai_t* aica = ecs_term(it, bsf_component_ai_t, 4); 
    bsf_component_transform_t* ptc = ecs_get(bsf->entities.world, bsf->entities.player, bsf_component_transform_t);
    bsf_component_physics_t* ppc = ecs_get(bsf->entities.world, bsf->entities.player, bsf_component_physics_t); 

    if (bsf->dbg) return; 

    for (uint32_t i = 0; i < it->count; ++i)
    {
        ecs_entity_t ent = it->entities[i];
        bsf_component_transform_t* tc = &tca[i];
        bsf_component_physics_t* pc = &pca[i]; 
        bsf_component_item_t* ic = &ica[i];
		bsf_component_ai_t* ai = &aica[i];
		
		bsf_ai_data_t data = {
			.tc = tc,
			.pc = pc,
			.ac = ai,
			.ptc = ptc,
			.ent = ent,
			.ic = ic,
			.world = it->world
		};
		ai->target = ptc->xform.translation;
		ai->bt.ctx.user_data = &data;

		// Go up, then move towards the player. I want a behavior system for this.
		bsf_item_bt(&ai->bt);
    }
}

GS_API_DECL void bsf_item_destroy(ecs_world_t* world, ecs_entity_t ent)
{
    bsf_t* bsf = gs_user_data(bsf_t);
	bsf_room_t* room = gs_slot_array_getp(bsf->run.rooms, bsf->run.room_ids[bsf->run.cell]);

    // Have to iterate over the dynamic array of mobs and remove... 
    for (uint32_t i = 0; i < gs_dyn_array_size(room->items); ++i)
    {
        if (ent == room->items[i])
        { 
            room->items[i] = gs_dyn_array_back(room->items);
            gs_dyn_array_pop(room->items);
            break;
        }
    }

    ecs_delete(world, ent);
}

//=== BSF Item Chest ===//

GS_API_DECL ecs_entity_t bsf_item_chest_create(struct bsf_t* bsf, gs_vqs* xform, bsf_item_type type)
{
    gs_gfxt_material_t* mat = gs_hash_table_getp(bsf->assets.materials, gs_hash_str64("mat.gsi"));
    ecs_entity_t e = ecs_new(bsf->entities.world, 0); 

    ecs_set(bsf->entities.world, e, bsf_component_renderable_immediate_t, {
        .shape = BSF_SHAPE_BOX,
        .model = gs_vqs_to_mat4(xform),
        .material = mat,
        .color = GS_COLOR_ORANGE
    });

    ecs_set(bsf->entities.world, e, bsf_component_physics_t, {
        .collider = {
            .type = BSF_COLLIDER_AABB,
            .xform = gs_vqs_default(),
            .shape.aabb = (gs_aabb_t) {
                .min = gs_v3s(-0.5f),
                .max = gs_v3s(0.5f)
            }
        }
    }); 

    ecs_set(bsf->entities.world, e, bsf_component_item_chest_t, {
        .type = type, 
        .origin = (gs_vqs) {
            .translation = xform->translation,
            .rotation = xform->rotation, 
            .scale = gs_v3s(0.2f)
        },
        .time_scale = {
            gs_rand_gen_range(&bsf->run.rand, 0.001f, 0.005f),
            gs_rand_gen_range(&bsf->run.rand, 0.001f, 0.005f),
            gs_rand_gen_range(&bsf->run.rand, 0.001f, 0.005f)
        }
    });

    ecs_set(bsf->entities.world, e, bsf_component_transform_t, {.xform = *xform});

    return e;
}

GS_API_DECL void bsf_item_chest_system(ecs_iter_t* it)
{
	bsf_t* bsf = gs_user_data(bsf_t); 
	bsf_room_t* room = gs_slot_array_getp(bsf->run.rooms, bsf->run.room_ids[bsf->run.cell]);
    const gs_vec2 fbs = gs_platform_framebuffer_sizev(gs_platform_main_window());
    const float t = gs_platform_elapsed_time();
    const float dt = gs_platform_delta_time() * bsf->run.time_scale; 
    const gs_platform_input_t* input = gs_platform_input();
    const gs_platform_gamepad_t* gp = &input->gamepads[0];
    bsf_component_transform_t* tca = ecs_term(it, bsf_component_transform_t, 1);
    bsf_component_physics_t* pca = ecs_term(it, bsf_component_physics_t, 2); 
    bsf_component_item_chest_t* ica = ecs_term(it, bsf_component_item_chest_t, 3); 
	bsf_component_renderable_immediate_t* rca = ecs_term(it, bsf_component_renderable_immediate_t, 4);
    bsf_component_transform_t* ptc = ecs_get(bsf->entities.world, bsf->entities.player, bsf_component_transform_t);
    bsf_component_physics_t* ppc = ecs_get(bsf->entities.world, bsf->entities.player, bsf_component_physics_t); 

    if (bsf->dbg) return; 

    for (uint32_t i = 0; i < it->count; ++i)
    {
        ecs_entity_t ent = it->entities[i];
        bsf_component_transform_t* tc = &tca[i];
        bsf_component_physics_t* pc = &pca[i]; 
        bsf_component_item_chest_t* ic = &ica[i];
		bsf_component_renderable_immediate_t* rc = &rca[i];

        gs_vqs offset = (gs_vqs){
            .translation = gs_v3(0.f, sin(t * ic->time_scale[0]) * 0.1f, 0.f),
            .rotation = gs_quat_angle_axis(t * ic->time_scale[1], GS_YAXIS),
            .scale = gs_v3s(gs_map_range(0.f, 1.f, 4.f, 6.f, (sin(dt * ic->time_scale[2]) * 0.5f + 0.5f)))
        }; 

        // Move original translation up over time towards player
        gs_vec3* pos = &ic->origin.translation; 

        // Spin and hover over time 
        tc->xform = (gs_vqs) {
            .translation = gs_vec3_add(ic->origin.translation, offset.translation),
            .rotation = gs_quat_mul(ic->origin.rotation, offset.rotation),
            .scale = gs_vec3_add(ic->origin.scale, offset.scale)
        }; 

		if (ic->hit)
		{
			ic->hit = false; 
			ic->hit_count++;
			if (ic->hit_count > 10) {
				bsf_explosion_create(bsf, it->world, &tc->xform, BSF_OWNER_PLAYER); 
                ecs_entity_t e = bsf_item_create(bsf, it->world, &tc->xform, ic->type);
				gs_dyn_array_push(room->items, e);
				bsf_item_chest_destroy(it->world, ent);
			}
		} 

		// Grab the renderable immediate component, then set the color based on it being hit
		gs_color_t c0 = GS_COLOR_ORANGE; 
		gs_color_t c1 = GS_COLOR_RED; 
		const float ct = gs_map_range(0, 10, 0.f, 1.f, (float)ic->hit_count);
		gs_color_t color = gs_color(
			(uint8_t)(255.f * gs_interp_smoothstep((float)c0.r / 255.f, (float)c1.r / 255.f, ct)),
			(uint8_t)(255.f * gs_interp_smoothstep((float)c0.g / 255.f, (float)c1.g / 255.f, ct)), 
			(uint8_t)(255.f * gs_interp_smoothstep((float)c0.b / 255.f, (float)c1.b / 255.f, ct)),
			(uint8_t)(255.f * gs_interp_smoothstep((float)c0.a / 255.f, (float)c1.a / 255.f, ct))
		); 

		rc->color = color; 
    } 
}

GS_API_DECL void bsf_item_chest_destroy(ecs_world_t* world, ecs_entity_t ent)
{
    bsf_t* bsf = gs_user_data(bsf_t);
	bsf_room_t* room = gs_slot_array_getp(bsf->run.rooms, bsf->run.room_ids[bsf->run.cell]);

    // Have to iterate over the dynamic array of mobs and remove... 
    for (uint32_t i = 0; i < gs_dyn_array_size(room->items); ++i)
    {
        if (ent == room->items[i])
        { 
            room->items[i] = gs_dyn_array_back(room->items);
            gs_dyn_array_pop(room->items);
            break;
        }
    }

    ecs_delete(world, ent);
}

//=== BSF Consumable ===//

GS_API_DECL void bsf_consumable_destroy(ecs_world_t* world, ecs_entity_t ent)
{
    bsf_t* bsf = gs_user_data(bsf_t);
	bsf_room_t* room = gs_slot_array_getp(bsf->run.rooms, bsf->run.room_ids[bsf->run.cell]);

    // Have to iterate over the dynamic array of mobs and remove... 
    for (uint32_t i = 0; i < gs_dyn_array_size(room->consumables); ++i)
    {
        if (ent == room->consumables[i])
        { 
            room->consumables[i] = gs_dyn_array_back(room->consumables);
            gs_dyn_array_pop(room->consumables);
            break;
        }
    }

    ecs_delete(world, ent);
}

GS_API_DECL ecs_entity_t bsf_consumable_create(struct bsf_t* bsf, gs_vqs* xform, bsf_consumable_type type)
{
    gs_gfxt_material_t* mat = gs_hash_table_getp(bsf->assets.materials, gs_hash_str64("mat.gsi"));
    ecs_entity_t e = ecs_new(bsf->entities.world, 0); 

    switch (type)
    {
        case BSF_CONSUMABLE_HEALTH:
        {
            ecs_set(bsf->entities.world, e, bsf_component_renderable_immediate_t, {
                .shape = BSF_SHAPE_CROSS,
                .model = gs_vqs_to_mat4(xform),
				.material = mat,
                .color = GS_COLOR_RED
            });

	        ecs_set(bsf->entities.world, e, bsf_component_physics_t, {
				.collider = {
					.type = BSF_COLLIDER_AABB,
					.xform = gs_vqs_default(),
					.shape.aabb = (gs_aabb_t) {
						.min = gs_v3s(-0.5f),
						.max = gs_v3s(0.5f)
					}
				}
            });

        } break;

        case BSF_CONSUMABLE_BOMB:
        {
            ecs_set(bsf->entities.world, e, bsf_component_renderable_immediate_t, {
                .shape = BSF_SHAPE_SPHERE,
                .model = gs_vqs_to_mat4(xform),
				.material = mat,
                .color = gs_color(10, 140, 225, 255)
            });

	        ecs_set(bsf->entities.world, e, bsf_component_physics_t, {
				.collider = {
					.type = BSF_COLLIDER_SPHERE,
					.xform = gs_vqs_default(),
					.shape.sphere = (gs_sphere_t) {
						.c = gs_v3s(0.f),
						.r = 0.5f
					}
				}
            });
        } break;
    } 

    ecs_set(bsf->entities.world, e, bsf_component_consumable_t, {
        .type = type, 
        .origin = (gs_vqs) {
            .translation = xform->translation,
            .rotation = xform->rotation, 
            .scale = gs_v3s(0.2f)
        },
        .time_scale = {
            gs_rand_gen_range(&bsf->run.rand, 0.001f, 0.005f),
            gs_rand_gen_range(&bsf->run.rand, 0.001f, 0.005f),
            gs_rand_gen_range(&bsf->run.rand, 0.001f, 0.005f)
        }
    });
    ecs_set(bsf->entities.world, e, bsf_component_transform_t, {.xform = *xform});

    return e;
}

GS_API_DECL void bsf_consumable_system(ecs_iter_t* it)
{
	bsf_t* bsf = gs_user_data(bsf_t); 
	bsf_room_t* room = gs_slot_array_getp(bsf->run.rooms, bsf->run.room_ids[bsf->run.cell]);
    const gs_vec2 fbs = gs_platform_framebuffer_sizev(gs_platform_main_window());
    const float t = gs_platform_elapsed_time();
    const float dt = gs_platform_delta_time() * bsf->run.time_scale; 
    const gs_platform_input_t* input = gs_platform_input();
    const gs_platform_gamepad_t* gp = &input->gamepads[0];
    bsf_component_transform_t* tca = ecs_term(it, bsf_component_transform_t, 1);
    bsf_component_physics_t* pca = ecs_term(it, bsf_component_physics_t, 2); 
    bsf_component_consumable_t* ica = ecs_term(it, bsf_component_consumable_t, 3);
    bsf_component_transform_t* ptc = ecs_get(bsf->entities.world, bsf->entities.player, bsf_component_transform_t);
    bsf_component_physics_t* ppc = ecs_get(bsf->entities.world, bsf->entities.player, bsf_component_physics_t);

    float speed_mod = gp->axes[GS_PLATFORM_JOYSTICK_AXIS_RTRIGGER] >= 0.4f || gs_platform_key_down(GS_KEYCODE_LEFT_SHIFT) ? 35.f : 20.f;
    if (room->cleared) speed_mod = 35.f;

    if (bsf->dbg) return; 

    for (uint32_t i = 0; i < it->count; ++i)
    {
        ecs_entity_t ent = it->entities[i];
        bsf_component_transform_t* tc = &tca[i];
        bsf_component_physics_t* pc = &pca[i]; 
        bsf_component_consumable_t* ic = &ica[i]; 

        gs_vqs offset = (gs_vqs){
            .translation = gs_v3(0.f, sin(t * ic->time_scale[0]) * 0.1f, 0.f),
            .rotation = gs_quat_angle_axis(t * ic->time_scale[1], GS_YAXIS),
            .scale = gs_v3s(gs_map_range(0.f, 1.f, 0.7f, 1.f, (sin(dt * ic->time_scale[2]) * 0.5f + 0.5f)))
        }; 

        // Move original translation up over time towards player
        gs_vec3* pos = &ic->origin.translation; 
        
        pos->z += dt * speed_mod;

        if (room->cleared && pos->z > BSF_ROOM_BOUND_Z) { 
            bsf_consumable_destroy(it->world, ent);
        } 
        else if (pos->z > BSF_ROOM_BOUND_Z) {
            pos->z = -BSF_ROOM_BOUND_Z;
        }

        // Spin and hover over time 
        tc->xform = (gs_vqs) {
            .translation = gs_vec3_add(ic->origin.translation, offset.translation),
            .rotation = gs_quat_mul(ic->origin.rotation, offset.rotation),
            .scale = gs_vec3_add(ic->origin.scale, offset.scale)
        }; 

        // Check for collision against player
        gs_contact_info_t res = bsf_component_physics_collide(pc, &tc->xform, ppc, &ptc->xform); 

        if (res.hit)
        {
            // Player pickup
            bsf_player_consumable_pickup(bsf, it->world, ic->type);

            // Destroy consumable
            bsf_consumable_destroy(it->world, ent); 
        } 
    }
}

//=== BSF Mob ===//

GS_API_DECL ecs_entity_t bsf_mob_create(struct bsf_t* bsf, ecs_world_t* world, gs_vqs* xform, bsf_mob_type type)
{
    ecs_entity_t e = ecs_new(world, 0); 

    switch (type)
    {
        case BSF_MOB_BOSS:
        {
            gs_gfxt_material_t* mat = gs_hash_table_getp(bsf->assets.materials, gs_hash_str64("mat.brain"));
            gs_gfxt_mesh_t* mesh = gs_hash_table_getp(bsf->assets.meshes, gs_hash_str64("mesh.brain")); 
            gs_gfxt_texture_t* tex = gs_hash_table_getp(bsf->assets.textures, gs_hash_str64("tex.default"));
            gs_gfxt_material_set_uniform(mat, "u_color", &(gs_vec3){1.f, 1.f, 1.f}); 
            gs_gfxt_material_set_uniform(mat, "u_tex", tex); 

            xform->scale = gs_v3s(1.f); 
            ecs_set(world, e, bsf_component_transform_t, {.xform = *xform});

            ecs_set(world, e, bsf_component_renderable_t, { 
                .hndl = bsf_graphics_scene_renderable_create(&bsf->scene, &(bsf_renderable_desc_t){
                    .material = mat,
                    .mesh = mesh,
                    .model = gs_vqs_to_mat4(xform)
                })
            }); 

	        ecs_set(world, e, bsf_component_physics_t, {
				.collider = {
					.type = BSF_COLLIDER_AABB,
					.xform = (gs_vqs){
                        .translation = gs_v3(0.f, 3.f, -2.5f),
                        .rotation = gs_quat_default(),
                        .scale = gs_v3(10.f, 10.f, 10.f)
                    },
					.shape.aabb = (gs_aabb_t) {
						.min = gs_v3s(-0.5f),
						.max = gs_v3s(0.5f)
					}
				}
            });

            ecs_set(world, e, bsf_component_ai_t, {
                .target = xform->translation
            }); 

            ecs_set(world, e, bsf_component_health_t, {.health = 100.f});

        } break;

        case BSF_MOB_TURRET:
        {
            gs_gfxt_material_t* mat = gs_hash_table_getp(bsf->assets.materials, gs_hash_str64("mat.turret"));
            gs_gfxt_mesh_t* mesh = gs_hash_table_getp(bsf->assets.meshes, gs_hash_str64("mesh.turret")); 
            gs_gfxt_texture_t* tex = gs_hash_table_getp(bsf->assets.textures, gs_hash_str64("tex.arwing"));
            gs_gfxt_material_set_uniform(mat, "u_color", &(gs_vec3){0.5f, 0.8f, 0.2f}); 
            gs_gfxt_material_set_uniform(mat, "u_tex", tex); 

            xform->scale = gs_v3s(0.1f);
            ecs_set(world, e, bsf_component_transform_t, {.xform = *xform});

            ecs_set(world, e, bsf_component_renderable_t, { 
                .hndl = bsf_graphics_scene_renderable_create(&bsf->scene, &(bsf_renderable_desc_t){
                    .material = mat,
                    .mesh = mesh,
                    .model = gs_vqs_to_mat4(xform)
                })
            }); 

	        ecs_set(world, e, bsf_component_physics_t, {
				.collider = {
					.type = BSF_COLLIDER_AABB,
					.xform = (gs_vqs){
                        .translation = gs_v3(0.f, 3.f, -2.5f),
                        .rotation = gs_quat_default(),
                        .scale = gs_v3(10.f, 10.f, 10.f)
                    },
					.shape.aabb = (gs_aabb_t) {
						.min = gs_v3s(-0.5f),
						.max = gs_v3s(0.5f)
					}
				}
            });

            ecs_set(world, e, bsf_component_ai_t, {
                .target = xform->translation
            }); 

            ecs_set(world, e, bsf_component_health_t, {.health = 1.f});

        } break;

        case BSF_MOB_BANDIT: 
        { 
            gs_gfxt_material_t* mat = gs_hash_table_getp(bsf->assets.materials, gs_hash_str64("mat.bandit"));
            gs_gfxt_mesh_t* mesh = gs_hash_table_getp(bsf->assets.meshes, gs_hash_str64("mesh.bandit")); 
            gs_gfxt_texture_t* tex = gs_hash_table_getp(bsf->assets.textures, gs_hash_str64("tex.arwing"));
            gs_gfxt_material_set_uniform(mat, "u_color", &(gs_vec3){1.f, 0.4f, 0.1f}); 
            gs_gfxt_material_set_uniform(mat, "u_tex", tex); 

            xform->scale = gs_v3s(0.3f);
            xform->rotation = gs_quat_angle_axis(gs_deg2rad(180.f), GS_YAXIS);
            ecs_set(world, e, bsf_component_transform_t, {.xform = *xform});

            ecs_set(world, e, bsf_component_renderable_t, { 
                .hndl = bsf_graphics_scene_renderable_create(&bsf->scene, &(bsf_renderable_desc_t){
                    .material = mat,
                    .mesh = mesh,
                    .model = gs_vqs_to_mat4(xform)
                })
            }); 

	        ecs_set(world, e, bsf_component_physics_t, {
				.collider = {
					.type = BSF_COLLIDER_AABB,
					.xform = (gs_vqs){
                        .translation = gs_v3(0.f, 2.5f, 0.f),
                        .rotation = gs_quat_default(),
                        .scale = gs_v3(5.f, 5.f, 5.f)
                    },
					.shape.aabb = (gs_aabb_t) {
						.min = gs_v3s(-0.5f),
						.max = gs_v3s(0.5f)
					}
				}
            });

            ecs_set(world, e, bsf_component_ai_t, {
                .target = xform->translation
            }); 

            ecs_set(world, e, bsf_component_health_t, {.health = 1.f});

        } break; 
    }
 
    ecs_set(world, e, bsf_component_mob_t, {.type = type});
    ecs_set(world, e, bsf_component_gun_t, {0}); 

    return e;
} 

void bsf_obstacle_destroy(ecs_world_t* world, ecs_entity_t obstacle) 
{
    bsf_t* bsf = gs_user_data(bsf_t);
	bsf_room_t* room = gs_slot_array_getp(bsf->run.rooms, bsf->run.room_ids[bsf->run.cell]);

    // Have to iterate over the dynamic array of mobs and remove... 
    for (uint32_t i = 0; i < gs_dyn_array_size(room->obstacles); ++i)
    {
        if (obstacle == room->obstacles[i])
        { 
            room->obstacles[i] = gs_dyn_array_back(room->obstacles);
            gs_dyn_array_pop(room->obstacles);
            break;
        }
    }

    ecs_delete(world, obstacle);
}

void bsf_mob_destroy(ecs_world_t* world, ecs_entity_t mob)
{
    bsf_t* bsf = gs_user_data(bsf_t);
	bsf_room_t* room = gs_slot_array_getp(bsf->run.rooms, bsf->run.room_ids[bsf->run.cell]);

    // Have to iterate over the dynamic array of mobs and remove... 
    for (uint32_t i = 0; i < gs_dyn_array_size(room->mobs); ++i)
    {
        if (mob == room->mobs[i])
        { 
            room->mobs[i] = gs_dyn_array_back(room->mobs);
            gs_dyn_array_pop(room->mobs);
            break;
        }
    }

    // Chance to slow down time scale
    if (gs_rand_gen_long(&bsf->run.rand) % 3) bsf->run.time_scale = 0.5f;

    if (bsf->entities.boss == mob) bsf->entities.boss = UINT_MAX;

    ecs_delete(world, mob);
}

void bsf_ai_task_find_rand_target_location(struct gs_ai_bt_t* bt, struct gs_ai_bt_node_t* node)
{ 
	bsf_t* bsf = gs_user_data(bsf_t);
    const float dt = gs_platform_time()->delta; 
    bsf_ai_data_t* data = (bsf_ai_data_t*)bt->ctx.user_data; 
    float dist = gs_vec3_dist(data->tc->xform.translation, data->ac->target);
	bsf_room_t* room = gs_slot_array_getp(bsf->run.rooms, bsf->run.room_ids[bsf->run.cell]);
    gs_vec3 target = data->ac->target;
    const bsf_component_mob_t* mc = data->mc;
    if (dist < 1.f)
    {
        switch (room->movement_type)
        {
            case BSF_MOVEMENT_RAIL:
            {
                switch (mc->type)
                {
                    default:
                    {
                        target = gs_v3(
                            gs_rand_gen_range(&bsf->run.rand, -10.f, 10.f),
                            gs_rand_gen_range(&bsf->run.rand, 1.f, 10.f),
                            gs_rand_gen_range(&bsf->run.rand, -4.f, 10.f)
                        );
                    } break;

                    case BSF_MOB_BOSS:
                    {
                        target = gs_v3(
                            gs_rand_gen_range(&bsf->run.rand, -10.f, 10.f),
                            gs_rand_gen_range(&bsf->run.rand, 1.f, 10.f),
                            gs_rand_gen_range(&bsf->run.rand, -10.f, 4.f)
                        );
                    } break;
                }
            } break;

            case BSF_MOVEMENT_FREE_RANGE: 
            {
                target = gs_v3(
                    gs_rand_gen_range(&bsf->run.rand, -BSF_ROOM_BOUND_X, BSF_ROOM_BOUND_X),
                    gs_rand_gen_range(&bsf->run.rand, 1.f, BSF_ROOM_BOUND_Y),
                    gs_rand_gen_range(&bsf->run.rand, -BSF_ROOM_BOUND_Z, BSF_ROOM_BOUND_Z)
                );
            } break;
        }
        data->ac->target = target;
		data->ac->wait = 0.f;
    }
    node->state = GS_AI_BT_STATE_SUCCESS;
}

GS_API_DECL void bsf_ai_task_move_to_target_location(struct gs_ai_bt_t* bt, struct gs_ai_bt_node_t* node)
{
	bsf_t* bsf = gs_user_data(bsf_t);
    const float dt = gs_platform_time()->delta * bsf->run.time_scale; 
    bsf_ai_data_t* data = (bsf_ai_data_t*)bt->ctx.user_data; 
    float dist = gs_vec3_dist(data->tc->xform.translation, data->ac->target);
    float speed = dist * dt * 50.f;
    const gs_vec2 fbs = gs_platform_framebuffer_sizev(gs_platform_main_window());

    // Face player while moving 
    gs_vec3 diff = gs_vec3_sub(data->ptc->xform.translation, data->tc->xform.translation);
    diff.y = 0.f;
    gs_vec3 dir = gs_vec3_norm(diff);
    gs_vec3 forward = gs_quat_rotate(data->tc->xform.rotation, gs_vec3_scale(GS_ZAXIS, 1.f)); 
    forward.y = 0.f;
    forward = gs_vec3_norm(forward);
    gs_quat rot = gs_quat_from_to_rotation(forward, dir);
    // data->tc->xform.rotation = rot; //gs_quat_slerp(data->tc->xform.rotation, rot, 0.8f);
    data->tc->xform.rotation = gs_quat_slerp(data->tc->xform.rotation, rot, 0.8f);

    // Debug draw line
    gsi_defaults(&bsf->gs.gsi);
    gsi_depth_enabled(&bsf->gs.gsi, true);
    gsi_camera(&bsf->gs.gsi, &bsf->scene.camera.cam, (u32)fbs.x, (u32)fbs.y);
    gsi_line3Dv(&bsf->gs.gsi, data->tc->xform.translation, gs_vec3_add(data->tc->xform.translation, gs_vec3_scale(dir, 10.f)), GS_COLOR_RED); 
    gsi_line3Dv(&bsf->gs.gsi, data->tc->xform.translation, gs_vec3_add(data->tc->xform.translation, gs_vec3_scale(forward, 5.f)), GS_COLOR_BLUE);

    float scl = (data->mc && data->mc->type == BSF_MOB_BOSS) ? 0.075f : 
				 data->ic ? 0.3f : 
				 0.1f;

    if (dist > 1.f) {
        gs_vec3 vel = gs_vec3_scale(gs_vec3_norm(gs_vec3_sub(data->ac->target, data->tc->xform.translation)), speed);
        gs_vec3 np = gs_vec3_add(data->tc->xform.translation, vel);
        data->tc->xform.translation = gs_v3(
            gs_interp_smoothstep(data->tc->xform.translation.x, np.x, scl),
            gs_interp_smoothstep(data->tc->xform.translation.y, np.y, scl),
            gs_interp_smoothstep(data->tc->xform.translation.z, np.z, scl)
        );
		node->state = GS_AI_BT_STATE_RUNNING;
    }
    else {
        node->state = GS_AI_BT_STATE_SUCCESS;
    }
} 

void bsf_ai_task_spawn_bandit(struct gs_ai_bt_t* bt, struct gs_ai_bt_node_t* node)
{
	bsf_t* bsf = gs_user_data(bsf_t);
    const float dt = gs_platform_time()->delta; 
    const gs_vec2 fbs = gs_platform_framebuffer_sizev(gs_platform_main_window());
    bsf_ai_data_t* data = (bsf_ai_data_t*)bt->ctx.user_data; 
	bsf_room_t* room = gs_slot_array_getp(bsf->run.rooms, bsf->run.room_ids[bsf->run.cell]);

    // Get random direction vector
    gs_vec3 dir = gs_vec3_norm(gs_v3(
        gs_rand_gen(&bsf->run.rand),
        gs_rand_gen(&bsf->run.rand),
        gs_rand_gen(&bsf->run.rand)
    )); 

    float rad = gs_rand_gen_range(&bsf->run.rand, 5.f, 50.f); 

    bsf_mob_type type = BSF_MOB_BANDIT;
    gs_vqs xform = gs_vqs_default();
    xform.translation = gs_vec3_add(data->tc->xform.translation, gs_vec3_scale(dir, rad));
    ecs_entity_t e = bsf_mob_create(bsf, data->world, &xform, type);
    gs_dyn_array_push(room->mobs, e); 

    node->state = GS_AI_BT_STATE_SUCCESS;
}

void bsf_ai_task_shoot_at_player(struct gs_ai_bt_t* bt, struct gs_ai_bt_node_t* node)
{
	bsf_t* bsf = gs_user_data(bsf_t);
    const float dt = gs_platform_time()->delta; 
    const gs_vec2 fbs = gs_platform_framebuffer_sizev(gs_platform_main_window());
    bsf_ai_data_t* data = (bsf_ai_data_t*)bt->ctx.user_data; 

    const gs_vec3 diff = gs_vec3_sub(data->ptc->xform.translation, data->tc->xform.translation);
    float dist2 = gs_vec3_len2(diff);

    const gs_vec3 spread = gs_vec3_norm(gs_v3(
        gs_rand_gen(&bsf->run.rand),
        gs_rand_gen(&bsf->run.rand),
        gs_rand_gen(&bsf->run.rand)
    ));
    const gs_vec3 dir = gs_vec3_norm(gs_vec3_add(diff, gs_vec3_scale(spread, dist2 * 0.00005f)));
    const gs_vec3 forward = gs_vec3_norm(gs_quat_rotate(data->tc->xform.rotation, gs_vec3_scale(GS_ZAXIS, 1.f))); 
    const gs_vec3 up = gs_vec3_norm(gs_quat_rotate(data->tc->xform.rotation, GS_YAXIS)); 
    gs_quat rotation = gs_quat_from_to_rotation(forward, dir); 

    // Debug draw line
    gsi_defaults(&bsf->gs.gsi);
    gsi_depth_enabled(&bsf->gs.gsi, true);
    gsi_camera(&bsf->gs.gsi, &bsf->scene.camera.cam, (u32)fbs.x, (u32)fbs.y);
    gsi_line3Dv(&bsf->gs.gsi, data->tc->xform.translation, gs_vec3_add(data->tc->xform.translation, gs_vec3_scale(dir, 10.f)), GS_COLOR_RED); 
    gsi_line3Dv(&bsf->gs.gsi, data->tc->xform.translation, gs_vec3_add(data->tc->xform.translation, gs_vec3_scale(forward, 5.f)), GS_COLOR_BLUE);

    // Fire
    bool chance = data->mc->type == BSF_MOB_BOSS ? 1 : gs_rand_gen_long(&bsf->run.rand) % 3 == 0;
    bool fire_rate = data->mc->type == BSF_MOB_BOSS ? 5.f : 10.f;
    if (data->gc->time >= fire_rate && chance)
    { 
        data->gc->time = 0.f;

        // Fire projectile using forward of player transform
        gs_vec3 forward = dir; 

        switch (data->mc->type)
        {
            case BSF_MOB_TURRET:
            {
                gs_vqs xform = (gs_vqs){
                    .translation = data->tc->xform.translation, 
                    .rotation = rotation,// rotation,   // Need rotation from forward to player
                    .scale = gs_v3s(0.5f)
                };

                // Play sound 
                bsf_play_sound(bsf, "audio.laser2", 0.1f);

                float speed = 5.f; 
                bsf_projectile_create(bsf, data->world, BSF_PROJECTILE_BULLET, BSF_OWNER_ENEMY, &xform, gs_vec3_scale(forward, speed));
                node->state = GS_AI_BT_STATE_SUCCESS;
            } break;

            case BSF_MOB_BANDIT:
            {
                gs_vqs xform = (gs_vqs){
                    .translation = data->tc->xform.translation, 
                    .rotation = rotation,// rotation,   // Need rotation from forward to player
                    .scale = gs_v3s(0.5f)
                };

                float speed = 5.f; 
                bsf_projectile_create(bsf, data->world, BSF_PROJECTILE_BULLET, BSF_OWNER_ENEMY, &xform, gs_vec3_scale(forward, speed));

                // Play sound 
                bsf_play_sound(bsf, "audio.laser2", 0.1f);

                if (gs_rand_gen_long(&bsf->run.rand) % 2) node->state = GS_AI_BT_STATE_SUCCESS;
            } break;

            case BSF_MOB_BOSS:
            { 
                gs_vqs xform = (gs_vqs){
                    .translation = data->tc->xform.translation, 
                    .rotation = rotation,// rotation,   // Need rotation from forward to player
                    .scale = gs_v3s(1.f)
                };

                float speed = 10.f; 
                bsf_projectile_create(bsf, data->world, BSF_PROJECTILE_BULLET, BSF_OWNER_ENEMY, &xform, gs_vec3_scale(forward, speed));

                // Play sound 
                bsf_play_sound(bsf, "audio.laser2", 0.1f);

                if (gs_rand_gen_long(&bsf->run.rand) % 33 == 0) node->state = GS_AI_BT_STATE_SUCCESS;
            } break;
        }
    }
    else
    {
        node->state = GS_AI_BT_STATE_RUNNING; 

        switch (data->mc->type)
        {
            case BSF_MOB_TURRET: data->gc->time += dt * gs_rand_gen_range(&bsf->run.rand, 0.1f, 1.f); break;
            case BSF_MOB_BANDIT: data->gc->time += dt * gs_rand_gen_range(&bsf->run.rand, 0.1f, 10.f); break;
            case BSF_MOB_BOSS: data->gc->time += dt * gs_rand_gen_range(&bsf->run.rand, 3.f, 10.f); break;
        }
    }
}

void bsf_ai_task_random_impulse(struct gs_ai_bt_t* bt, struct gs_ai_bt_node_t* node)
{ 
    const float dt = gs_platform_time()->delta; 
    bsf_ai_data_t* data = (bsf_ai_data_t*)bt->ctx.user_data; 
    bsf_t* bsf = gs_user_data(bsf_t);
    gs_vec3 impulse = gs_v3(
        gs_rand_gen_range(&bsf->run.rand, -1.f, 1.f),
        0.f, 
        -1.f
    );
    gs_vec3 angular_impulse = gs_v3(
        gs_rand_gen_range(&bsf->run.rand, -1.f, 1.f),
        gs_rand_gen_range(&bsf->run.rand, -1.f, 1.f), 
        gs_rand_gen_range(&bsf->run.rand, -1.f, 1.f)
    );
    data->pc->velocity = gs_vec3_scale(gs_vec3_norm(impulse), gs_rand_gen_range(&bsf->run.rand, 0.1f, 0.3f));
    data->pc->angular_velocity = gs_vec3_scale(gs_vec3_norm(angular_impulse), 10.f);
    node->state = GS_AI_BT_STATE_SUCCESS;
}

void bsf_ai_task_fall_to_death(struct gs_ai_bt_t* bt, struct gs_ai_bt_node_t* node)
{ 
    bsf_t* bsf = gs_user_data(bsf_t);
    const float t = gs_platform_time()->elapsed;
    const float dt = gs_platform_time()->delta * bsf->run.time_scale; 
    bsf_ai_data_t* data = (bsf_ai_data_t*)bt->ctx.user_data; 

    // Continue to fall until y = 0.f, then diE!
    gs_quat* rot = &data->tc->xform.rotation;
    gs_vec3* pos = &data->tc->xform.translation;
    gs_vec3* vel = &data->pc->velocity;
    gs_vec3* av = &data->pc->angular_velocity;

    if (pos->y >= 0.f)
    { 
        vel->y -= dt * 0.2f;
        gs_vec3 head = gs_vec3_add(*pos, *vel);
        gs_vec3 dir = gs_vec3_norm(gs_vec3_sub(head, *pos));
        *pos = gs_vec3_add(*pos, *vel);

        // Add angular velocity to rotation
        gs_quat angular = gs_quat_mul_list(3, 
            gs_quat_angle_axis(av->x + t * 0.002f * bsf->run.time_scale, GS_XAXIS), 
            gs_quat_angle_axis(av->y + t * 0.008f * bsf->run.time_scale, GS_YAXIS), 
            gs_quat_angle_axis(av->z + t * 0.005f * bsf->run.time_scale, GS_ZAXIS)
        );

        gs_vec3 fwd = gs_vec3_norm(gs_quat_rotate(*rot, GS_ZAXIS));
        gs_quat nrot = gs_quat_mul(angular, gs_quat_from_to_rotation(fwd, dir));
        *rot = gs_quat_slerp(*rot, angular, 0.1f); 

        node->state = GS_AI_BT_STATE_RUNNING;
    }
    else
    { 
        node->state = GS_AI_BT_STATE_SUCCESS;
        switch (data->mc->type)
        {
            default: bsf_explosion_create(bsf, data->world, &data->tc->xform, BSF_OWNER_PLAYER); break;
            case BSF_MOB_BOSS: 
            {
                gs_vqs xform = data->tc->xform;
                xform.scale = gs_v3s(50.f);
                bsf_explosion_create(bsf, data->world, &xform, BSF_OWNER_PLAYER);
                bsf_play_sound(bsf, "audio.explosion_boss", 0.5f);
            } break; 
        }
        bsf_mob_destroy(data->world, data->ent); 
    } 
}

static void bsf_bandit_bt(struct gs_ai_bt_t* bt)
{
    /* 
        // Bandit will fly to random location within cone of player 
        // then turn to face player
        // then attack for a few seconds
        // then repeat
    */ 
    const float dt = gs_platform_time()->delta; 
    bsf_ai_data_t* data = (bsf_ai_data_t*)bt->ctx.user_data;

    // If dying, then simulate falling to the ground... that can be a generic shared behavior though.
    
    gsai_bt(bt, { 
        gsai_repeater(bt, { 
            gsai_selector(bt, {

                // Death sequence 
                gsai_condition(bt, (data->hc->health <= 0.f), {
                    gsai_sequence(bt, {
                        gsai_leaf(bt, bsf_ai_task_random_impulse);
                        gsai_leaf(bt, bsf_ai_task_fall_to_death);
                    });
                });

                // Move/shoot sequence
                gsai_condition(bt, (data->hc->health > 0.f), {
                    gsai_sequence(bt, {
                        gsai_leaf(bt, bsf_ai_task_find_rand_target_location);
                        gsai_leaf(bt, bsf_ai_task_move_to_target_location);
                        gsai_wait(bt, &data->ac->wait, dt, 0.3f);
                        gsai_leaf(bt, bsf_ai_task_shoot_at_player);
                        gsai_wait(bt, &data->ac->wait, dt, 0.2f);
                    });
                });
            }); 
        }); 
    });
}

static void bsf_turret_bt(struct gs_ai_bt_t* bt)
{ 
    /* 
        // Turret will move on track towards player 
        // Will turn to face player
        // then attack for a few seconds
        // then repeat
    */ 

    const float dt = gs_platform_time()->delta; 
    bsf_ai_data_t* data = (bsf_ai_data_t*)bt->ctx.user_data;
    gsai_bt(bt, { 
        gsai_repeater(bt, { 
            gsai_selector(bt, {

                // Death sequence 
                gsai_condition(bt, (data->hc->health <= 0.f), 
                {
                    gsai_sequence(bt, {
                        gsai_leaf(bt, bsf_ai_task_fall_to_death);
                    }); 
                });


                // Move/shoot sequence
                gsai_condition(bt, (data->hc->health > 0.f), 
                {
                    gsai_sequence(bt, {
                        gsai_leaf(bt, bsf_ai_task_shoot_at_player);
                        gsai_wait(bt, &data->ac->wait, dt, 5.f);
                    }); 
                }); 

            });
        });
    });
}

static void bsf_boss_bt(struct gs_ai_bt_t* bt)
{
    /* 
        // Bandit will fly to random location within cone of player 
        // then turn to face player
        // then attack for a few seconds
        // then repeat
    */ 
    const float dt = gs_platform_time()->delta; 
    bsf_ai_data_t* data = (bsf_ai_data_t*)bt->ctx.user_data; 
	bsf_t* bsf = gs_user_data(bsf_t);
    
    gsai_bt(bt, { 
        gsai_repeater(bt, { 
            gsai_selector(bt, {

                // Death sequence 
                gsai_condition(bt, (data->hc->health <= 0.f), {
                    gsai_sequence(bt, {
                        gsai_leaf(bt, bsf_ai_task_random_impulse);
                        gsai_leaf(bt, bsf_ai_task_fall_to_death);
                    });
                });

                // Move/shoot sequence
                gsai_condition(bt, (data->hc->health > 0.f), { 
                    gsai_sequence(bt, {

                        gsai_leaf(bt, bsf_ai_task_find_rand_target_location);
                        gsai_leaf(bt, bsf_ai_task_move_to_target_location);
                        gsai_wait(bt, &data->ac->wait, dt, 0.3f);

                        // Either spawn a bandit or shoot the player
                        gsai_selector(bt, {

                            // If pass random check, then spawn bandit
                            gsai_condition(bt, gs_rand_gen_long(&bsf->run.rand) % 3 == 0, {
                                gsai_leaf(bt, bsf_ai_task_spawn_bandit);
                            });

                            // Otherwise we shoot player
                            gsai_leaf(bt, bsf_ai_task_shoot_at_player);
                        });

                        gsai_wait(bt, &data->ac->wait, dt, 0.2f);

                    }); 
                });
            }); 
        }); 
    });
}

GS_API_DECL void bsf_mob_system(ecs_iter_t* it)
{
	bsf_t* bsf = gs_user_data(bsf_t); 
    gs_immediate_draw_t* gsi = &bsf->gs.gsi;
	bsf_room_t* room = gs_slot_array_getp(bsf->run.rooms, bsf->run.room_ids[bsf->run.cell]);
    const gs_vec2 fbs = gs_platform_framebuffer_sizev(gs_platform_main_window());
    const float t = gs_platform_elapsed_time();
    const float dt = gs_platform_delta_time() * bsf->run.time_scale; 
    bsf_component_renderable_t* rca = ecs_term(it, bsf_component_renderable_t, 1);
    bsf_component_transform_t* tca = ecs_term(it, bsf_component_transform_t, 2);
    bsf_component_physics_t* pca = ecs_term(it, bsf_component_physics_t, 3); 
    bsf_component_health_t* hca = ecs_term(it, bsf_component_health_t, 4);
    bsf_component_mob_t* mca = ecs_term(it, bsf_component_mob_t, 5);
    bsf_component_gun_t* bca = ecs_term(it, bsf_component_gun_t, 6); 
    bsf_component_ai_t* aic = ecs_term(it, bsf_component_ai_t, 7);
    bsf_component_transform_t* ptc = ecs_get(bsf->entities.world, bsf->entities.player, bsf_component_transform_t);

    if (bsf->dbg) return; 

    for (uint32_t i = 0; i < it->count; ++i)
    {
        ecs_entity_t ent = it->entities[i];
        bsf_component_renderable_t* rc = &rca[i];
        bsf_component_transform_t* tc = &tca[i];
        bsf_component_physics_t* pc = &pca[i]; 
        bsf_component_health_t* hc = &hca[i];
        bsf_component_mob_t* mc = &mca[i];
        bsf_component_gun_t* gc = &bca[i];
        bsf_component_ai_t* ai = &aic[i];
        const gs_vec3* ppos = &ptc->xform.translation;

        bsf_ai_data_t ai_data = (bsf_ai_data_t){
            .gc = gc,
            .tc = tc,
            .pc = pc,
            .hc = hc,
            .mc = mc,
            .ptc = ptc, 
            .ac = ai,
            .world = it->world,
            .ent = ent
        };
        ai->bt.ctx.user_data = &ai_data;

        switch (mc->type)
        {
            default: {
            } break;

            case BSF_MOB_TURRET:
            {
                bsf_turret_bt(&ai->bt);

                switch (room->movement_type)
                {
                    case BSF_MOVEMENT_RAIL:
                    {
                        // Move turret over time
                        bsf_ai_data_t* data = &ai_data; 
                        bsf_t* bsf = gs_user_data(bsf_t);
                        const gs_vec2 fbs = gs_platform_framebuffer_sizev(gs_platform_main_window());
                        const gs_platform_input_t* input = &gs_subsystem(platform)->input;
                        const gs_platform_gamepad_t* gp = &input->gamepads[0];
                        const float speed_mod = gp->axes[GS_PLATFORM_JOYSTICK_AXIS_RTRIGGER] >= 0.4f || gs_platform_key_down(GS_KEYCODE_LEFT_SHIFT) ? 35.f : 20.f;

                        gs_vec3* pos = &data->tc->xform.translation;
                        pos->z += dt * speed_mod;

                        if (pos->z > BSF_ROOM_BOUND_Z) {
                            pos->z = -BSF_ROOM_BOUND_Z;
                        } 
                    } break;
                }

            } break;

            case BSF_MOB_BANDIT:
            { 
                bsf_bandit_bt(&ai->bt);
            } break;

            case BSF_MOB_BOSS:
            {
                bsf_boss_bt(&ai->bt);
            } break;
        }

        // Do collision hit
        if (hc->hit)
        {
            gs_gfxt_material_t* hit_mat = gs_hash_table_getp(bsf->assets.materials, gs_hash_str64("mat.hit"));
            gs_gfxt_material_set_uniform(hit_mat, "u_color", &(gs_vec3){1.f, 1.f, 1.f}); 
            bsf_renderable_t* rend = gs_slot_array_getp(bsf->scene.renderables, rc->hndl);
            rend->material = hit_mat;
            gs_gfxt_material_t* mat = NULL;

            if (hc->hit_timer >= 0.1f) {
                switch (mc->type)
                {
                    case BSF_MOB_BANDIT: mat = gs_hash_table_getp(bsf->assets.materials, gs_hash_str64("mat.bandit")); break;
                    case BSF_MOB_TURRET: mat = gs_hash_table_getp(bsf->assets.materials, gs_hash_str64("mat.turret")); break;
                    case BSF_MOB_BOSS:   mat = gs_hash_table_getp(bsf->assets.materials, gs_hash_str64("mat.brain")); break;
                }
                rend->material = mat;
                hc->hit = false;
				hc->hit_timer = 0.f;
            } 

            hc->hit_timer += dt;
        }
    }
}

//=== BSF Projectile ===//

GS_API_DECL void bsf_projectile_create(struct bsf_t* bsf, ecs_world_t* world, bsf_projectile_type type, bsf_owner_type owner, const gs_vqs* xform, gs_vec3 velocity)
{
	gs_gfxt_material_t* mat = NULL; 
    gs_gfxt_mesh_t* mesh = gs_hash_table_getp(bsf->assets.meshes, gs_hash_str64("mesh.laser_player"));

	switch ( owner )
	{
		case BSF_OWNER_PLAYER:
		{ 
			mat = gs_hash_table_getp(bsf->assets.materials, gs_hash_str64("mat.laser_player"));
			gs_gfxt_material_set_uniform(mat, "u_color", &(gs_vec3){0.f, 1.f, 0.f}); 
		} break;

		case BSF_OWNER_ENEMY:
		{
			mat = gs_hash_table_getp(bsf->assets.materials, gs_hash_str64("mat.laser_enemy"));
			gs_gfxt_material_set_uniform(mat, "u_color", &(gs_vec3){1.f, 0.f, 0.f}); 
		} break;
	}

    const float speed = gs_vec3_len(velocity);

    ecs_entity_t b = ecs_new(world, 0); 

    ecs_set(world, b, bsf_component_transform_t, {.xform = *xform});

    switch (type)
    {
        case BSF_PROJECTILE_BULLET:
        {
            ecs_set(world, b, bsf_component_renderable_t, {
                .hndl = bsf_graphics_scene_renderable_create(&bsf->scene, &(bsf_renderable_desc_t){
                    .material = mat,
                    .mesh = mesh,
                    .model = gs_vqs_to_mat4(xform)
                })
            });

            ecs_set(world, b, bsf_component_physics_t, {
                .velocity = velocity, 
                .speed = speed,
                .collider = {
                    .xform = (gs_vqs) { 
                        .translation = gs_v3(0.f, 0.f, 0.4f),
                        .rotation = gs_quat_angle_axis(gs_deg2rad(90.f), GS_XAXIS),
                        .scale = gs_v3s(1.f)
                    },
                    .type = BSF_COLLIDER_CYLINDER,
                    .shape.cylinder = {
                        .r = 0.2f,
                        .base = gs_v3s(0.f),
                        .height = 1.f
                    }
                }
            });

            ecs_set(world, b, bsf_component_timer_t, {.max = 15.f}); 

        } break;

        case BSF_PROJECTILE_BOMB:
        {
            ecs_set(world, b, bsf_component_physics_t, {
                .velocity = velocity, 
                .speed = speed,
                .collider = {
                    .xform = gs_vqs_default(),
                    .type = BSF_COLLIDER_SPHERE,
                    .shape.sphere = {
                        .r = 0.5f,
                        .c = gs_v3s(0.f)
                    }
                }
            }); 

            ecs_set(world, b, bsf_component_renderable_t, {.hndl = UINT_MAX}); 

            ecs_set(world, b, bsf_component_timer_t, {.max = 2.f});
        } break;
    }

    ecs_set(world, b, bsf_component_projectile_t, {.type = type, .owner = owner});
}

GS_API_DECL void bsf_projectile_system(ecs_iter_t* it)
{
    bsf_t* bsf = gs_user_data(bsf_t); 
    const float t = gs_platform_elapsed_time();
    const float dt = gs_platform_delta_time() * bsf->run.time_scale;
    const gs_vec2 fbs = gs_platform_framebuffer_sizev(gs_platform_main_window());
    bsf_component_renderable_t* rca = ecs_term(it, bsf_component_renderable_t, 1);
    bsf_component_transform_t* tca = ecs_term(it, bsf_component_transform_t, 2);
    bsf_component_physics_t* pca = ecs_term(it, bsf_component_physics_t, 3); 
    bsf_component_timer_t* kca = ecs_term(it, bsf_component_timer_t, 4);
    bsf_component_projectile_t* bca = ecs_term(it, bsf_component_projectile_t, 5);
    bsf_room_t* room = gs_slot_array_getp(bsf->run.rooms, bsf->run.room_ids[bsf->run.cell]); 

    if (bsf->dbg) return;

    for (uint32_t i = 0; i < it->count; ++i)
    {
		bsf_component_renderable_t* rc = &rca[i];
		bsf_component_transform_t* tc = &tca[i];
		bsf_component_physics_t* pc = &pca[i];
		bsf_component_timer_t* kc = &kca[i];
		bsf_component_projectile_t* bc = &bca[i];

        switch (bc->owner)
        {
            case BSF_OWNER_PLAYER:
            {
                bsf_component_character_stats_t* psc = ecs_get(it->world, bsf->entities.player, bsf_component_character_stats_t);

                if (
                    (bc->type == BSF_PROJECTILE_BULLET && psc->projectile_opt & BSF_PROJECTILE_OPT_HOMING) || 
                    (bc->type == BSF_PROJECTILE_BOMB && psc->projectile_opt & BSF_PROJECTILE_OPT_HOMING_BOMB)
                )
                {
                    float dist = FLT_MAX;
                    gs_vec3 vel = pc->velocity;
                    for (uint32_t m = 0; m < gs_dyn_array_size(room->mobs); ++m)
                    {
                        bsf_component_transform_t* mtc = ecs_get(it->world, room->mobs[m], bsf_component_transform_t);
                        float d2 = gs_vec3_dist2(mtc->xform.translation, tc->xform.translation);
                        if (d2 < dist)
                        { 
                            dist = d2;
                            vel = gs_vec3_sub(mtc->xform.translation, tc->xform.translation);
                        }
                    }
                    
                    pc->velocity = gs_vec3_norm(vel);

                    gsi_defaults(&bsf->gs.gsi);
                    gsi_depth_enabled(&bsf->gs.gsi, true);
                    gsi_camera(&bsf->gs.gsi, &bsf->scene.camera.cam, (u32)fbs.x, (u32)fbs.y);
                    gsi_line3Dv(&bsf->gs.gsi, tc->xform.translation, gs_vec3_add(tc->xform.translation, gs_vec3_scale(pc->velocity, 5.f)), GS_COLOR_BLUE);
                } 

            } break;
        } 

        ecs_entity_t projectile = it->entities[i];
		gs_vqs* xform = &tc->xform;
        gs_vec3* trans = &xform->position; 
        const gs_vec3* vel = &pc->velocity; 
        const float speed = pc->speed * dt;

        // Update position based on velocity and dt
        *trans = gs_vec3_add(*trans, gs_vec3_scale(*vel, speed)); 

        // Update timer
        kc->time += dt;
        if (kc->time >= kc->max || trans->y <= 0.f) 
        {
            switch (bc->type)
            {
                case BSF_PROJECTILE_BULLET:
                {
                    ecs_delete(it->world, projectile);
                    bsf_play_sound(bsf, "audio.hit_no_damage", 0.1f);
                } break;

                case BSF_PROJECTILE_BOMB:
                {
                    // Do bomb explosion
                    gs_vqs xform = tc->xform;
                    xform.scale = gs_v3s(5.f);
                    bsf_explosion_create(bsf, it->world, &xform, bc->owner);
                    ecs_delete(it->world, projectile);
				} break;
            }
        } 

		switch (bc->type)
		{
			case BSF_PROJECTILE_BULLET:
			{
				switch (bc->owner)
				{ 
					case BSF_OWNER_PLAYER:
					{ 
						for (uint32_t m = 0; m < gs_dyn_array_size(room->mobs); ++m)
						{
							ecs_entity_t mob = room->mobs[m]; 
							bsf_component_transform_t* tform = ecs_get(bsf->entities.world, mob, bsf_component_transform_t);
							bsf_component_physics_t* phys = ecs_get(bsf->entities.world, mob, bsf_component_physics_t); 

							gs_contact_info_t res = bsf_component_physics_collide(pc, &tc->xform, phys, 
									&tform->xform); 

							if (res.hit)
							{ 
								bsf_component_health_t* h = ecs_get(bsf->entities.world, mob, bsf_component_health_t);
								h->hit = true; 
								h->health -= 1.f;
								h->hit_timer = 0.f;
                                bsf_camera_shake(bsf, &bsf->scene.camera, 0.1f);
                                bsf_play_sound(bsf, "audio.bang", gs_rand_gen_range(&bsf->run.rand, 0.01f, 0.03f));
								ecs_delete(it->world, projectile);
								break;
							} 
						}

						// Check against any items in scene
						for (uint32_t ie = 0; ie < gs_dyn_array_size(room->items); ++ie)
						{ 
							ecs_entity_t item = room->items[ie];
							bsf_component_transform_t* tform = ecs_get(bsf->entities.world, item, bsf_component_transform_t);
							bsf_component_physics_t* phys = ecs_get(bsf->entities.world, item, bsf_component_physics_t); 
							gs_contact_info_t res = bsf_component_physics_collide(pc, &tc->xform, phys, &tform->xform);

							if (res.hit)
							{ 
								bsf_component_item_chest_t* cp = ecs_get(bsf->entities.world, item, bsf_component_item_chest_t);
								if (cp)
								{
									cp->hit = true;
									cp->hit_timer = 0.f;
									bsf_camera_shake(bsf, &bsf->scene.camera, 0.1f);
									bsf_play_sound(bsf, "audio.bang", gs_rand_gen_range(&bsf->run.rand, 0.01f, 0.03f));
									ecs_delete(it->world, projectile);
									break; 
								}
							}
						}

					} break;

					case BSF_OWNER_ENEMY:
					{
						// Check collision against player
						bsf_component_transform_t* ptc = ecs_get(bsf->entities.world, bsf->entities.player, bsf_component_transform_t);
						bsf_component_physics_t* ppc = ecs_get(bsf->entities.world, bsf->entities.player, bsf_component_physics_t); 
                        bsf_component_barrel_roll_t* pbc = ecs_get(bsf->entities.world, bsf->entities.player, bsf_component_barrel_roll_t);
						gs_contact_info_t res = bsf_component_physics_collide(pc, &tc->xform, ppc, &ptc->xform); 

						if (res.hit)
						{ 
                            // Check if barrel rolling
                            if (pbc->active)
                            {
                                // Reflect based on hit normal? Or just random direction vector
                                gs_vec3 off = gs_vec3_norm(gs_v3(
                                    gs_rand_gen(&bsf->run.rand),
                                    gs_rand_gen(&bsf->run.rand),
                                    gs_rand_gen(&bsf->run.rand)
                                ));
                                gs_vec3 dir = gs_vec3_norm(gs_vec3_add(gs_vec3_scale(gs_vec3_norm(pc->velocity), -1.f), gs_vec3_scale(off, 0.1f)));

                                // Change the orientation to match new velocity
                                gs_vec3 forward = gs_vec3_norm(pc->velocity);
                                tc->xform.rotation = gs_quat_from_to_rotation(gs_vec3_scale(GS_ZAXIS, -1.f), dir);

                                // Change owner of projectile to player
                                bc->owner = BSF_OWNER_PLAYER;

                                // Change velocity based on hit normal, scale by previous magnitude for speed
                                pc->velocity = gs_vec3_scale(dir, gs_vec3_len(pc->velocity));
                            }
                            else
                            {
                                // Shake camera
                                bsf_camera_shake(bsf, &bsf->scene.camera, 1.f);

                                // Hurt player
                                bsf_player_damage(bsf, it->world, 0.5f);

                                // Delete bullet
                                ecs_delete(it->world, projectile);
                            }

							break;
						} 

					} break;
				}
			} break;

			case BSF_PROJECTILE_BOMB:
			{
				for (uint32_t m = 0; m < gs_dyn_array_size(room->mobs); ++m)
				{
					ecs_entity_t mob = room->mobs[m]; 
					bsf_component_transform_t* tform = ecs_get(bsf->entities.world, mob, bsf_component_transform_t);
					bsf_component_physics_t* phys = ecs_get(bsf->entities.world, mob, bsf_component_physics_t); 

					gs_contact_info_t res = bsf_component_physics_collide(pc, &tc->xform, phys, 
							&tform->xform); 

					if (res.hit)
					{ 
						bsf_component_health_t* h = ecs_get(bsf->entities.world, mob, bsf_component_health_t);
						h->hit = true; 
						h->health -= 0.5f;
						h->hit_timer = 0.f;
						bsf_explosion_create(bsf, it->world, &tc->xform, bc->owner);
						ecs_delete(it->world, projectile);
						break;
					} 
				}
			} break;
		}
    } 
} 

//=== BSF Player ===//

GS_API_DECL void bsf_player_init(struct bsf_t* bsf)
{ 
    // This should all be initialized based on what the run context actually is
    gs_gfxt_material_t* mat = gs_hash_table_getp(bsf->assets.materials, gs_hash_str64("mat.ship_arwing"));
    gs_gfxt_mesh_t* mesh = gs_hash_table_getp(bsf->assets.meshes, gs_hash_str64("mesh.arwing"));
    gs_gfxt_texture_t* tex = gs_hash_table_getp(bsf->assets.textures, gs_hash_str64("tex.arwing"));
    gs_gfxt_material_t* gsi_mat = gs_hash_table_getp(bsf->assets.materials, gs_hash_str64("mat.gsi"));
    gs_gfxt_material_set_uniform(mat, "u_tex", tex); 
    gs_gfxt_material_set_uniform(mat, "u_color", &(gs_vec3){1.f, 1.f, 1.f}); 

    bsf->entities.player = ecs_new(bsf->entities.world, 0); 
    ecs_entity_t p = bsf->entities.player;

    ecs_set(bsf->entities.world, p, bsf_component_transform_t, {
        .xform = (gs_vqs){
            .translation = gs_v3(0.f, 5.f, 0.f),
            .rotation = gs_quat_default(),
            .scale = gs_v3s(0.08f)
        } 
    }); 

    ecs_set(bsf->entities.world, p, bsf_component_inventory_t, {
        .bombs = 1  
    });

    ecs_set(bsf->entities.world, p, bsf_component_camera_track_t, {0}); 
    ecs_set(bsf->entities.world, p, bsf_component_barrel_roll_t, {0});

    ecs_set(bsf->entities.world, p, bsf_component_character_stats_t, {
        .health = 3.f,
        .speed = 5.f,
        .fire_rate = 1.3f,
        .shot_speed = 1.2f,
        .shot_count = 1,
        .shot_size = 2.f,
    }); 

    ecs_set(bsf->entities.world, p, bsf_component_gun_t, {0});
    
    ecs_set(bsf->entities.world, p, bsf_component_renderable_t, { 
        .hndl = bsf_graphics_scene_renderable_create(&bsf->scene, &(bsf_renderable_desc_t){
            .material = mat,
            .mesh = mesh,
            .model = gs_mat4_identity()
        })
    }); 

    ecs_set(bsf->entities.world, p, bsf_component_physics_t, {
        .velocity = gs_v3(0.f, 0.f, -0.01f), 
        .speed = 3,
        .collider = {
            .xform = (gs_vqs) {
                .translation = gs_v3(0.f, 0.f, 2.f),
                .rotation = gs_quat_default(),
                .scale = gs_v3(10.f, 3.f, 5.f)
            },
            .type = BSF_COLLIDER_AABB,
            .shape = {
                .aabb = {
                    .min = gs_v3s(-0.5f),
                    .max = gs_v3s(0.5f)
                } 
            }
        }
    });

    ecs_set(bsf->entities.world, p, bsf_component_health_t, {.health = 3.f});
}

GS_API_DECL void bsf_player_system(ecs_iter_t* it)
{ 
    bsf_t* bsf = gs_user_data(bsf_t); 
    const float t = gs_platform_elapsed_time();
    const float dt = gs_platform_delta_time() * bsf->run.time_scale; 
    const gs_platform_input_t* input = &gs_subsystem(platform)->input;
    const gs_platform_gamepad_t* gp = &input->gamepads[0];
    bsf_component_renderable_t* rca = ecs_term(it, bsf_component_renderable_t, 1);
    bsf_component_transform_t* tca = ecs_term(it, bsf_component_transform_t, 2);
    bsf_component_physics_t* pca = ecs_term(it, bsf_component_physics_t, 3); 
    bsf_component_character_stats_t* psca = ecs_term(it, bsf_component_character_stats_t, 4);
    bsf_component_health_t* hca = ecs_term(it, bsf_component_health_t, 5);
    bsf_component_gun_t* gca = ecs_term(it, bsf_component_gun_t, 6);
    bsf_component_camera_track_t* cca = ecs_term(it, bsf_component_camera_track_t, 7);
    bsf_component_barrel_roll_t* bca = ecs_term(it, bsf_component_barrel_roll_t, 8);
    bsf_component_inventory_t* ica = ecs_term(it, bsf_component_inventory_t, 9);

    const float gp_thresh = 0.2f;

    if (bsf->dbg) return;

    const bsf_room_t* room = gs_slot_array_getp(bsf->run.rooms, bsf->run.room_ids[bsf->run.cell]);

	float mod = 1.f;
    float mod_lr = 0.f;


    if (gs_platform_key_down(GS_KEYCODE_LEFT_SHIFT)) {
        mod = 2.f;
    }
    else if (gs_platform_key_down(GS_KEYCODE_LEFT_CONTROL)) {
        mod = 0.1f;
    }

    if (gp->present)
    {
        if (gp->axes[GS_PLATFORM_JOYSTICK_AXIS_LTRIGGER] >= 0.4f) mod = 0.1f; 
        else if (gp->axes[GS_PLATFORM_JOYSTICK_AXIS_RTRIGGER] >= 0.4f) mod = 2.f;
    }

    for (uint32_t i = 0; i < it->count; ++i)
    { 
        bsf_component_renderable_t* rc = &rca[i];
        bsf_component_transform_t* tc = &tca[i];
        bsf_component_physics_t* pc = &pca[i]; 
        bsf_component_character_stats_t* psc = &psca[i];
        bsf_component_health_t* hc = &hca[i];
        bsf_component_gun_t* gc = &gca[i];
        bsf_component_camera_track_t* cc = &cca[i];
        bsf_component_barrel_roll_t* bc = &bca[i];
        bsf_component_inventory_t* ic = &ica[i];

        pc->speed = mod * psc->speed * dt;

        // Change physics shape based on whether or not barrel roll is active
        if (bc->active)
        {
            pc->collider = (bsf_physics_collider_t) {
                .type = BSF_COLLIDER_SPHERE,
                .xform = (gs_vqs) {
                    .translation = gs_v3s(0.f),
                    .rotation = gs_quat_default(), 
                    .scale = gs_v3s(10.f)
                },
                .shape.sphere = (gs_sphere_t){
                    .r = 0.5f,
                    .c = gs_v3s(0.f) 
                }
            }; 
        }
        else
        {
            pc->collider = (bsf_physics_collider_t){
                .type = BSF_COLLIDER_AABB,
                .xform = (gs_vqs) {
                    .translation = gs_v3(0.f, 0.f, 2.f),
                    .rotation = gs_quat_default(),
                    .scale = gs_v3(10.f, 3.f, 5.f)
                },
                .shape.aabb = (gs_aabb_t){
                    .min = gs_v3s(-0.5f),
                    .max = gs_v3s(0.5f)
                }
            };
        }

        // Update player position based on input
        const gs_vec2 xb = gs_v2(-10.f, 10.f);
        const gs_vec2 yb = gs_v2(0.f, 9.f); 
        gs_vec3* ps = &tc->xform.position;
        gs_vec3 v = gs_v3s(0.f);

        if (gs_platform_key_down(GS_KEYCODE_W) || gs_platform_key_down(GS_KEYCODE_UP))    {v.y = ps->y > yb.x ? v.y - 1.f : 0.f;}
        if (gs_platform_key_down(GS_KEYCODE_S) || gs_platform_key_down(GS_KEYCODE_DOWN))  {v.y = ps->y < yb.y ? v.y + 1.f : 0.f;}
        if (gs_platform_key_down(GS_KEYCODE_A) || gs_platform_key_down(GS_KEYCODE_LEFT))  {v.x = ps->x > xb.x ? v.x - 1.f : 0.f;}
        if (gs_platform_key_down(GS_KEYCODE_D) || gs_platform_key_down(GS_KEYCODE_RIGHT)) {v.x = ps->x < xb.y ? v.x + 1.f : 0.f;} 

        if (fabsf(gp->axes[1]) > gp_thresh) v.y = gp->axes[1];
        if (fabsf(gp->axes[0]) > gp_thresh) v.x = gp->axes[0];

        if (bc->active)
        {
            mod_lr = bc->vel_dir * dt;
        }

        switch (room->movement_type)
        {
            case BSF_MOVEMENT_FREE_RANGE:
            { 
                // Normalize velocity then scale by player speed 
                v = gp->present ? v : gs_vec3_norm(v);
                v = gs_vec3_scale(v, pc->speed * mod * 150.f * dt);

                // Forward impulse
                gs_vec3 forward = gs_quat_rotate(tc->xform.rotation, gs_vec3_scale(GS_ZAXIS, -1.f)); 

                pc->velocity = gs_vec3_add(
                        gs_vec3_scale(gs_vec3_norm(forward), pc->speed),
                        gs_vec3_scale(gs_vec3_norm(gs_quat_rotate(cc->xform.rotation, GS_XAXIS)), mod_lr * 0.1f)
                );
                gs_vec3 pnp = gs_vec3_add(tc->xform.position, pc->velocity);
                tc->xform.position = gs_v3(
                    gs_interp_linear(tc->xform.position.x, pnp.x, 0.98f),
                    gs_interp_linear(tc->xform.position.y, pnp.y, 0.98f),
                    gs_interp_linear(tc->xform.position.z, pnp.z, 0.98f)
                ); 

                float brz = 0.f;

                // Calculate angular velocity
                gs_vec3 av = gs_v3s(0.f);
                if (gs_platform_key_down(GS_KEYCODE_W)) av.y -= 1.f;
                if (gs_platform_key_down(GS_KEYCODE_S)) av.y += 1.f;
                if (gs_platform_key_down(GS_KEYCODE_A)) {av.x += 1.f; av.z += 0.2f;}
                if (gs_platform_key_down(GS_KEYCODE_D)) {av.x -= 1.f; av.z -= 0.2f;}
                if (gs_platform_key_down(GS_KEYCODE_Q)) av.z += 1.f;
                if (gs_platform_key_down(GS_KEYCODE_E)) av.z -= 1.f;

                av = gs_vec3_scale(gs_vec3_norm(av), 80.f * dt); 

                const float max_rotx = 90.f;
                const float max_roty = 90.f;
                const float max_rotz = 90.f;
                const float cmax_rotz = 45.f;

                bool double_click_right = false;
                bool double_click_left = false;

                if (gp->present)
                {
                    static bool count[2] = {0};
                    static float press_time[2] = {1.f, 1.f}; 
                    static bool was_pressed[2] = {0};

                    if (fabsf(gp->axes[1]) > gp_thresh) av.y = gp->axes[1];
                    if (fabsf(gp->axes[0]) > gp_thresh) {av.x = -gp->axes[0]; av.z = -gp->axes[0] * 0.2f;}

                    if (!bc->active && gp->buttons[GS_PLATFORM_GAMEPAD_BUTTON_RBUMPER]) {

                        if (!was_pressed[1]) { 
                            if (press_time[1] < 0.5f) double_click_right = true;
                            press_time[1] = 0.f;
                        } 
                        av.z -= 1.f; 
                    }
                    if (!bc->active && gp->buttons[GS_PLATFORM_GAMEPAD_BUTTON_LBUMPER]) {

                        if (!was_pressed[0]) { 
                            if (press_time[0] < 0.5f) double_click_left = true;
                            press_time[0] = 0.f;
                        } 
                        av.z += 1.f; 
                    } 

                    press_time[0] = gs_min(press_time[0] + dt, 1.f); 
                    press_time[1] = gs_min(press_time[1] + dt, 1.f);

                    av = gs_vec3_scale(av, 80.f * dt);

                    was_pressed[0] = gp->buttons[GS_PLATFORM_GAMEPAD_BUTTON_LBUMPER];
                    was_pressed[1] = gp->buttons[GS_PLATFORM_GAMEPAD_BUTTON_RBUMPER];
                } 

                if (double_click_right && !bc->active)
                {
                    bc->active = true;
                    bc->time = 0.f;
                    bc->direction = -1;
                    bc->vel_dir = 1;
                    bc->avz = pc->angular_velocity.z;
                    bc->max_time = 0.5f;
                }
                else if (double_click_left && !bc->active)
                {
                    bc->active = true;
                    bc->time = 0.f;
                    bc->direction = 1;
                    bc->vel_dir = -1;
                    bc->avz = pc->angular_velocity.z;
                    bc->max_time = 0.5f;
                } 

                // Barrel roll
                if (gs_platform_key_pressed(GS_KEYCODE_B) && !bc->active)
                {
                    bc->active = true;
                    bc->time = 0.f;
                    bc->direction = av.z >= 0.f ? 1 : -1;
                    bc->vel_dir = av.z < 0.f ? 1 : av.z > 0.f ? -1 : 0;
                    bc->avz = pc->angular_velocity.z;
                    bc->max_time = 0.5f;
                }
                if (bc->active) 
                {
                    bc->time += dt;
                    const uint16_t rotations = 3;
                    float v = gs_map_range(0.f, bc->max_time, 0.f, 1.f, bc->time);
                    v = gs_interp_deceleration(0.f, 1.f, v);
                    brz = (v * (float)rotations * 360.f) * bc->direction;
                    brz = fmodf(brz + bc->avz, 360.f);
                }
                if (bc->time > bc->max_time && bc->active)
                {
                    bc->active = false;
                    pc->angular_velocity.z = bc->avz;
                }

                // Set camera track angular velocity
                float coffset = cc->angular_velocity.z * -2.5f * dt;
                float cavz = bc->active ? coffset : (av.z * 10.f) + coffset;
                gs_vec3* cav = &cc->angular_velocity;
                gs_vec3 cnav = gs_vec3_add(cc->angular_velocity, gs_v3(av.x, av.y, cavz));
                *cav = cnav;

                // Use angular velocity to update camera track component
                cc->xform = (gs_vqs){
                    .translation = tc->xform.translation, 
                    .rotation = gs_quat_mul_list(3,
                        gs_quat_angle_axis(gs_deg2rad(cav->x), GS_YAXIS),
                        gs_quat_angle_axis(gs_deg2rad(cav->y), GS_XAXIS),
                        gs_quat_angle_axis(gs_deg2rad(gs_clamp(cav->z, -cmax_rotz, cmax_rotz)), GS_ZAXIS)
                    ),
                    .scale = tc->xform.scale
                }; 

                // Add negated angular velocity to this
                av.z = bc->active ? av.z * 50.f : av.z * 10.f;
                av.z = av.z + pc->angular_velocity.z * -2.5f * dt;

                gs_vec3* pav = &pc->angular_velocity;
                gs_vec3 pnav = gs_vec3_add(pc->angular_velocity, av);
                *pav = gs_v3(
                    pnav.x, 
                    pnav.y, 
                    bc->active ? brz : gs_interp_linear(pc->angular_velocity.z, gs_clamp(pnav.z, -max_rotz, max_rotz), 0.5f)
                ); 

                // Do rotation (need barrel roll rotation)
                tc->xform.rotation = gs_quat_mul_list(3,
                    gs_quat_angle_axis(gs_deg2rad(pav->x), GS_YAXIS),
                    gs_quat_angle_axis(gs_deg2rad(pav->y), GS_XAXIS),
                    gs_quat_angle_axis(gs_deg2rad(pav->z), GS_ZAXIS)
                ); 

            } break;

            case BSF_MOVEMENT_RAIL:
            { 
                // Normalize velocity then scale by player speed
                v = gp->present ? v : gs_vec3_norm(v);
                v = gs_vec3_scale(v, pc->speed * mod * 150.f * dt);

                const float lerp = (ps->y >= yb.y && v.y < 0 || ps->y <= yb.x && v.y > 0 || ps->x >= xb.y && v.x < 0 || ps->x <= xb.x && v.x > 0) ? 0.2f : 0.1f;

                // Add to velocity
                pc->velocity = gs_v3(
                    gs_interp_smoothstep(pc->velocity.x, v.x, lerp),
                    gs_interp_smoothstep(pc->velocity.y, v.y, lerp),
                    gs_interp_smoothstep(pc->velocity.z, v.z, lerp)
                );

                gs_vec3 pnp = gs_vec3_add(tc->xform.position, pc->velocity);
                pnp = gs_v3(
                    gs_clamp(pnp.x, xb.x, xb.y),
                    gs_clamp(pnp.y, yb.x, yb.y),
                    pnp.z
                );
                tc->xform.position = pnp;

                // Move slowly back to origin
                float nz = -tc->xform.position.z;
                tc->xform.position.z = gs_interp_linear(tc->xform.position.z, 25.f, 0.3f); 

                gs_vec3 av = gs_v3s(0.f);
                bool br = false;
                if (gs_platform_key_down(GS_KEYCODE_W) && ps->y > yb.x) av.y -= 1.f;
                if (gs_platform_key_down(GS_KEYCODE_S) && ps->y < yb.y) av.y += 1.f;
                if (gs_platform_key_down(GS_KEYCODE_A) && ps->x > xb.x) {av.x += 1.f; av.z += 0.1f;}
                if (gs_platform_key_down(GS_KEYCODE_D) && ps->x < xb.y) {av.x -= 1.f; av.z -= 0.1f;}
                if (gs_platform_key_down(GS_KEYCODE_Q)) av.z += 1.f;
                if (gs_platform_key_down(GS_KEYCODE_E)) av.z -= 1.f;
                if (gs_platform_key_down(GS_KEYCODE_B)) {br = true; av.z += 1.f;}
                av = gs_vec3_scale(gs_vec3_norm(av), 180.f * dt);

                bool double_click_right = false;
                bool double_click_left = false;

                if (gp->present)
                {
                    static bool count[2] = {0};
                    static float press_time[2] = {1.f, 1.f}; 
                    static bool was_pressed[2] = {0};

                    if (fabsf(gp->axes[1]) > gp_thresh && ps->y > yb.x && ps->y < yb.y) av.y = gp->axes[1];
                    if (fabsf(gp->axes[0]) > gp_thresh && ps->x > xb.x && ps->x < xb.y) {av.x = -gp->axes[0]; av.z = -gp->axes[0] * 0.2f;}

                    if (!bc->active && gp->buttons[GS_PLATFORM_GAMEPAD_BUTTON_RBUMPER]) {

                        if (!was_pressed[1]) { 
                            if (press_time[1] < 0.5f) double_click_right = true;
                            press_time[1] = 0.f;
                        } 

                        av.z -= 1.f; 
                    }
                    if (!bc->active && gp->buttons[GS_PLATFORM_GAMEPAD_BUTTON_LBUMPER]) {

                        if (!was_pressed[0]) { 
                            if (press_time[0] < 0.5f) double_click_left = true;
                            press_time[0] = 0.f;
                        } 

                        av.z += 1.f; 
                    } 

                    press_time[0] = gs_min(press_time[0] + dt, 1.f); 
                    press_time[1] = gs_min(press_time[1] + dt, 1.f);

                    av = gs_vec3_scale(av, 180.f * dt);

                    was_pressed[0] = gp->buttons[GS_PLATFORM_GAMEPAD_BUTTON_LBUMPER];
                    was_pressed[1] = gp->buttons[GS_PLATFORM_GAMEPAD_BUTTON_RBUMPER];
                }

                float brz = 0.f;

                if (double_click_right && !bc->active)
                {
                    bc->active = true;
                    bc->time = 0.f;
                    bc->direction = -1;
                    bc->vel_dir = 1;
                    bc->avz = pc->angular_velocity.z;
                    bc->max_time = 0.5f;
                }
                else if (double_click_left && !bc->active)
                {
                    bc->active = true;
                    bc->time = 0.f;
                    bc->direction = 1;
                    bc->vel_dir = -1;
                    bc->avz = pc->angular_velocity.z;
                    bc->max_time = 0.5f;
                } 

                // Barrel roll
                if (gs_platform_key_pressed(GS_KEYCODE_B) && !bc->active)
                {
                    bc->active = true;
                    bc->time = 0.f;
                    bc->direction = av.z >= 0.f ? 1 : -1;
                    bc->vel_dir = av.z < 0.f ? 1 : av.z > 0.f ? -1 : 0;
                    bc->avz = pc->angular_velocity.z;
                    bc->max_time = 0.5f;
                }
                if (bc->active) 
                {
                    bc->time += dt;
                    const uint16_t rotations = 3;
                    float v = gs_map_range(0.f, bc->max_time, 0.f, 1.f, bc->time);
                    v = gs_interp_deceleration(0.f, 1.f, v);
                    brz = (v * (float)rotations * 360.f) * bc->direction;
                    brz = fmodf(brz + bc->avz, 360.f);
                }
                if (bc->time > bc->max_time && bc->active)
                {
                    bc->active = false;
                    pc->angular_velocity.z = bc->avz;
                }

                const float max_rotx = 30.f;
                const float max_roty = 60.f;
                const float max_rotz = 90.f;

                // Add negated angular velocity to this
                av.z = bc->active ? av.z * 50.f : av.z * 10.f; 
                gs_vec3 nav = gs_vec3_scale(pc->angular_velocity, -2.5f * dt);

                const float plerp = 0.45f;
                gs_vec3* pav = &pc->angular_velocity;
                gs_vec3 npav = pc->angular_velocity;
                pav->x = gs_clamp(gs_interp_smoothstep(pav->x, pav->x + av.x, plerp), -max_rotx, max_rotx);
                pav->y = gs_clamp(gs_interp_smoothstep(pav->y, pav->y + av.y, plerp), -max_roty, max_roty);
                pav->z = bc->active ? brz : gs_interp_smoothstep(pav->z, gs_clamp(pav->z + av.z, -max_rotz, max_rotz), plerp);
                pav->x = gs_interp_smoothstep(pav->x, pav->x + nav.x, 0.85f);
                pav->y = gs_interp_smoothstep(pav->y, pav->y + nav.y, 0.85f);
                pav->z = gs_interp_smoothstep(pav->z, pav->z + nav.z, 0.85f);

                // Do rotation (need barrel roll rotation)
                tc->xform.rotation = gs_quat_mul_list(3,
                    gs_quat_angle_axis(gs_deg2rad(pav->x), GS_YAXIS),
                    gs_quat_angle_axis(gs_deg2rad(pav->y), GS_XAXIS),
                    gs_quat_angle_axis(gs_deg2rad(pav->z), GS_ZAXIS)
                ); 

            } break;
        } 

        // Update renderable
        bsf_renderable_t* rend = gs_slot_array_getp(bsf->scene.renderables, rc->hndl);
        rend->model = gs_vqs_to_mat4(&tc->xform);

        for (uint32_t m = 0; m < gs_dyn_array_size(room->mobs); ++m)
        {
            ecs_entity_t mob = room->mobs[m]; 
            bsf_component_transform_t* mtc = ecs_get(bsf->entities.world, mob, bsf_component_transform_t);
            bsf_component_physics_t* mpc = ecs_get(bsf->entities.world, mob, bsf_component_physics_t); 

            gs_contact_info_t res = bsf_component_physics_collide(pc, &tc->xform, mpc, &mtc->xform); 

            if (res.hit)
            { 
                bsf_component_health_t* h = ecs_get(bsf->entities.world, mob, bsf_component_health_t);
                h->hit = true; 
                h->health -= 1.f;
                h->hit_timer = 0.f;
                break;
            } 
        } 

        {
            if ((gp->buttons[GS_PLATFORM_GAMEPAD_BUTTON_A] || gs_platform_mouse_down(GS_MOUSE_LBUTTON)) && !gc->firing) {
                gc->firing = true;
            }
            if (!gp->buttons[GS_PLATFORM_GAMEPAD_BUTTON_A] && !gs_platform_mouse_down(GS_MOUSE_LBUTTON)) {
                gc->firing = false;
            } 
            if (gc->firing) {
                gc->time += psc->fire_rate * dt * 100.f;
            } 
        }

        // Fire
        if (gc->firing && gc->time >= 30.f)
        { 
            gc->time = 0.f;

            // Total shot size based on pc_count
            float xsize = 0.5f; 
			float xstep = xsize / (float)psc->shot_count;

            for (uint32_t s = 0; s < psc->shot_count; ++s)
            {
                float xoff = psc->shot_count > 1 ? gs_map_range(0.f, (float)psc->shot_count, -xsize, xsize, (float)s) + xstep : 0.f;

                // Fire projectile using forward of player transform
                gs_vec3 cam_forward = gs_vec3_scale(gs_vec3_norm(gs_camera_forward(&bsf->scene.camera.cam)), 100.f);
                gs_vec3 vel_dir = gs_vec3_norm(gs_vec3_sub(cam_forward, tc->xform.translation));
                gs_vec3 forward = gs_vec3_norm(gs_quat_rotate(tc->xform.rotation, gs_vec3_scale(GS_ZAXIS, -1.f))); 
                gs_vec3 right = gs_vec3_norm(gs_quat_rotate(tc->xform.rotation, GS_XAXIS)); 
                gs_vec3 up = gs_vec3_norm(gs_quat_rotate(tc->xform.rotation, gs_vec3_scale(GS_YAXIS, 1.f))); 
                gs_vec3 trans = gs_vec3_add(tc->xform.translation, gs_vec3_add(gs_vec3_scale(forward, 0.1f), gs_vec3_scale(right, xoff)));
                gs_vqs xform = (gs_vqs){
                    .translation = trans, 
                    .rotation = tc->xform.rotation, 
                    .scale = gs_v3s(0.2f * psc->shot_size)
                };

                float speed = psc->shot_speed * 5.f;
                if (psc->projectile_opt & BSF_PROJECTILE_OPT_HOMING) speed *= 2.f;
                bsf_projectile_create(bsf, it->world, BSF_PROJECTILE_BULLET, BSF_OWNER_PLAYER, &xform, gs_vec3_scale(forward, speed));
            }

            // Play sound 
            bsf_play_sound(bsf, "audio.laser", 0.5f);

            bsf_camera_shake(bsf, &bsf->scene.camera, 0.05f);
        }

        if (ic->bombs && (gs_platform_mouse_pressed(GS_MOUSE_RBUTTON) || gp->buttons[GS_PLATFORM_GAMEPAD_BUTTON_B]))
        {
            // Fire projectile using forward of player transform
            gs_vec3 cam_forward = gs_vec3_scale(gs_vec3_norm(gs_camera_forward(&bsf->scene.camera.cam)), 100.f);
            gs_vec3 vel_dir = gs_vec3_norm(gs_vec3_sub(cam_forward, tc->xform.translation));
            gs_vec3 forward = gs_vec3_norm(gs_quat_rotate(tc->xform.rotation, gs_vec3_scale(GS_ZAXIS, -1.f))); 
            gs_vec3 up = gs_vec3_norm(gs_quat_rotate(tc->xform.rotation, gs_vec3_scale(GS_YAXIS, 1.f))); 
            gs_vec3 trans = gs_vec3_add(tc->xform.translation, gs_vec3_scale(forward, 0.1f));
            gs_vqs xform = (gs_vqs){
                .translation = trans, 
                .rotation = tc->xform.rotation, 
                .scale = gs_v3s(0.2f)
            };

            float speed = psc->shot_speed * 5.f;
            if (psc->projectile_opt & BSF_PROJECTILE_OPT_HOMING_BOMB) speed *= 2.f;
            bsf_projectile_create(bsf, it->world, BSF_PROJECTILE_BOMB, BSF_OWNER_PLAYER, &xform, gs_vec3_scale(forward, speed));
            ic->bombs = gs_max(ic->bombs - 1, 0);
        }
    } 
}

GS_API_DECL void bsf_player_damage(struct bsf_t* bsf, ecs_world_t* world, float damage)
{
    bsf_component_health_t* hc = ecs_get(world, bsf->entities.player, bsf_component_health_t);
    hc->health -= damage; 
    bsf_play_sound(bsf, "audio.arwing_hit", 0.3f);

    if (hc->health <= 0.f) 
    {
        bsf->state = BSF_STATE_GAME_OVER;
    } 
}

GS_API_DECL void bsf_player_consumable_pickup(struct bsf_t* bsf, ecs_world_t* world, bsf_consumable_type type)
{
    switch (type)
    {
        case BSF_CONSUMABLE_HEALTH:
        {
            bsf_component_health_t* hc = ecs_get(world, bsf->entities.player, bsf_component_health_t);
			bsf_component_character_stats_t* sc = ecs_get(world, bsf->entities.player, bsf_component_character_stats_t);
            hc->health = gs_min(hc->health + 0.5f, sc->health);
            bsf_play_sound(bsf, "audio.health_pickup", 0.5f);
        } break;

        case BSF_CONSUMABLE_BOMB:
        {
            bsf_component_inventory_t* ic = ecs_get(world, bsf->entities.player, bsf_component_inventory_t);
            ic->bombs++; 
            bsf_play_sound(bsf, "audio.bomb_pickup", 0.5f);
        } break;
    } 
} 

GS_API_DECL void bsf_player_item_pickup(struct bsf_t* bsf, ecs_world_t* world, bsf_item_type type)
{
    // Seach through current items, if not already picked up, grab
    bsf_component_inventory_t* ic = ecs_get(world, bsf->entities.player, bsf_component_inventory_t);
    bsf_component_character_stats_t* sc = ecs_get(world, bsf->entities.player, bsf_component_character_stats_t);
    bsf_component_health_t* hc = ecs_get(world, bsf->entities.player, bsf_component_health_t); 
    bsf_component_transform_t* tc = ecs_get(world, bsf->entities.player, bsf_component_transform_t); 

    for (uint32_t i = 0; i < gs_dyn_array_size(ic->items); ++i)
    {
        if (ic->items[i] == type)
        {
            gs_println("Already has item!");
            return;
        }
    }

    gs_dyn_array_push(ic->items, type);

    switch (type)
    {
		case BSF_ITEM_POLYPHEMUS:
		{ 
            // Fire rate increases by 0.7
            sc->shot_speed *= 0.5f;

            // Single shot
            sc->shot_count = 1;

			// Shot size increased
			sc->shot_size *= 4.f;

			gs_println("Picked up POLYPHEMUS");
		} break;

        case BSF_ITEM_SAD_ONION:       
        {
            // Fire rate increases by 0.7
            sc->fire_rate += 0.7f;

            gs_println("Picked up SAD ONION");

        } break;

        case BSF_ITEM_INNER_EYE:
        {
            // Fire decrease by 0.8
            sc->fire_rate = gs_max(0.1f, sc->fire_rate - 0.8f);

            // Triple shot
            sc->shot_count = gs_max(3,  sc->shot_count);

            gs_println("Picked up INNER EYE");

        } break;

        case BSF_ITEM_SPOON_BENDER:
        {
            sc->projectile_opt |= BSF_PROJECTILE_OPT_HOMING;

            gs_println("Picked up SPOON BENDER");
        } break;

        case BSF_ITEM_MAGIC_MUSHROOM: 
        {
            sc->health += 1.f;
            sc->damage *= 1.5f;
            sc->speed += 0.3f;
            hc->health = sc->health;
            tc->xform.scale = gs_vec3_scale(tc->xform.scale, 1.5f);
            gs_println("Picked up MAGIC MUSHROOM");
        } break;
    }

    bsf_play_sound(bsf, "audio.health_pickup", 0.5f);

    // Take the item out of the available item pool 
    bsf_item_remove_from_pool(bsf, type);
} 

//=== BSF Menus ===//

#define BSF_GUI_ELEMENT_CENTERED(...)\
    do {\
        gs_gui_layout_next(gui);\
        __VA_ARGS__;\
    } while (0);


static void bsf_gui_menu_panel_row_begin(struct bsf_t* bsf, gs_gui_layout_t* layout, gs_vec2 size, gs_vec2 offset, float mp)
{
    // Anchor to current layout
    gs_gui_rect_t lrect = layout->body;
    gs_gui_rect_t anchor = gs_gui_layout_anchor(&lrect, size.x, size.y, offset.x, offset.y, GS_GUI_LAYOUT_ANCHOR_CENTER);
    gs_gui_layout_set_next(&bsf->gs.gui, anchor, 0);
    gs_gui_panel_begin(&bsf->gs.gui, "##panel");
    {
        float m = lrect.w * mp; 
        float w = (lrect.w * 0.5f) - m;
        gs_gui_layout_row(&bsf->gs.gui, 2, (int[]){m, -m}, 0);
    } 
}

static void bsf_gui_menu_panel_row_end(struct bsf_t* bsf)
{
    gs_gui_panel_end(&bsf->gs.gui);
}

static void bsf_game_setup_menu(struct bsf_t* bsf, const gs_gui_rect_t* parent)
{ 
    gs_gui_context_t* gui = &bsf->gs.gui;
    gs_gui_container_t* cnt = gs_gui_get_current_container(gui); 
    gs_gui_layout_t* layout = gs_gui_get_layout(gui);

    if (gs_platform_key_pressed(GS_KEYCODE_ESC))
    {
        bsf->state = BSF_STATE_MAIN_MENU;
    } 

    bsf_gui_menu_panel_row_begin(bsf, layout, gs_v2(layout->body.w, layout->body.h * 0.7f), gs_v2(0.f, 0.f), 0.3f);
    {
        layout = gs_gui_get_layout(gui);

        BSF_GUI_ELEMENT_CENTERED({
            if (gs_gui_button_ex(gui, "Play", &(gs_gui_selector_desc_t){.classes = {"top_panel_item"}}, 0x00)) {
                bsf->state = BSF_STATE_START;
                bsf->run.is_playing = true;
                bsf_play_sound(bsf, "audio.menu_select", 0.5f);
            } 
        }); 

        float w = 150.f;
        gs_gui_layout_row_ex(gui, 2, (int[]){w, w}, 0, GS_GUI_JUSTIFY_CENTER);
        gs_gui_label_ex(gui, "Seed : ", &(gs_gui_selector_desc_t){.classes = {"seed"}}, 0x00); 
        gs_gui_textbox_ex(gui, bsf->run.seed, BSF_SEED_MAX_LEN, NULL, 0x00);
    } 
    bsf_gui_menu_panel_row_end(bsf);

}

static void bsf_options_menu(struct bsf_t* bsf, const gs_gui_rect_t* parent)
{ 
    gs_gui_context_t* gui = &bsf->gs.gui;
    gs_gui_container_t* cnt = gs_gui_get_current_container(gui); 
    gs_gui_layout_t* layout  = gs_gui_get_layout(gui);

    if (gs_platform_key_pressed(GS_KEYCODE_ESC))
    {
        if (bsf->run.is_playing) bsf->state = BSF_STATE_PAUSE;
        else                     bsf->state = BSF_STATE_MAIN_MENU;
    }

    bsf_gui_menu_panel_row_begin(bsf, layout, gs_v2(layout->body.w * 0.9f, layout->body.h * 0.7f), gs_v2(0.f, 0.f), 0.3f);
    {
        BSF_GUI_ELEMENT_CENTERED({
            if (gs_gui_button_ex(gui, "Back", &(gs_gui_selector_desc_t){.classes ={"top_panel_item"}}, 0x00)) {
                if (bsf->run.is_playing) bsf->state = BSF_STATE_PAUSE;
                else                     bsf->state = BSF_STATE_MAIN_MENU;
                bsf_play_sound(bsf, "audio.menu_select", 0.5f);
            } 
        });
    }
    bsf_gui_menu_panel_row_end(bsf);

}

static void bsf_main_menu(struct bsf_t* bsf, const gs_gui_rect_t* parent)
{
    gs_gui_context_t* gui = &bsf->gs.gui;
    gs_gui_container_t* cnt = gs_gui_get_current_container(gui); 
    gs_gui_layout_t* layout = gs_gui_get_layout(gui);

    if (gs_platform_key_pressed(GS_KEYCODE_ESC))
    {
        bsf->state = BSF_STATE_TITLE;
    }

    bsf_gui_menu_panel_row_begin(bsf, layout, gs_v2(layout->body.w * 0.9f, layout->body.h * 0.9f), gs_v2(0.f, 0.f), 0.3f);
    {
        // This will go to a "character select screen"
        BSF_GUI_ELEMENT_CENTERED({ 
            if (gs_gui_button_ex(gui, "New Run", &(gs_gui_selector_desc_t){.classes ={"top_panel_item"}}, 0x00))
            {
                bsf->state = BSF_STATE_GAME_SETUP; 

                // Initial seed
                char str[32] = {0};
                gs_uuid_t uuid = gs_platform_uuid_generate();
                gs_platform_uuid_to_string(str, &uuid);
                memcpy(bsf->run.seed, str, BSF_SEED_MAX_LEN - 1);
                bsf_play_sound(bsf, "audio.menu_select", 0.5f);
            } 
        });

        BSF_GUI_ELEMENT_CENTERED({
            if (gs_gui_button(gui, "Editor"))
            {
                bsf->state = BSF_STATE_EDITOR_START;
                bsf_play_sound(bsf, "audio.menu_select", 0.5f);
            }
        });

        BSF_GUI_ELEMENT_CENTERED({ 
            if (gs_gui_button(gui, "Options"))
            {
                bsf->state = BSF_STATE_OPTIONS;
                bsf_play_sound(bsf, "audio.menu_select", 0.5f);
            }
        });

        /*
        BSF_GUI_ELEMENT_CENTERED({
            if (gs_gui_button(gui, "Test Suite"))
            {
                bsf->state = BSF_STATE_TEST;
            }
        });
        */

        BSF_GUI_ELEMENT_CENTERED({
            if (gs_gui_button(gui, "Quit"))
            {
                bsf->state = BSF_STATE_QUIT; 
                bsf_play_sound(bsf, "audio.menu_select", 0.5f);
            }
        });
    }
    bsf_gui_menu_panel_row_end(bsf);

}

static void bsf_title_screen(struct bsf_t* bsf, const gs_gui_rect_t* parent)
{
    gs_gui_context_t* gui = &bsf->gs.gui;
    gs_platform_input_t* input = &gs_subsystem(platform)->input; 
	const float t = gs_platform_elapsed_time();
    const gs_vec2 fbs = gs_platform_framebuffer_sizev(gs_platform_main_window()); 
	gs_gui_container_t* cnt = gs_gui_get_current_container(gui); 
    gs_gui_layout_t* layout = gs_gui_get_layout(gui);

    // Simulate movement
    int32_t y = (int32_t)gs_map_range(0.f, 1.f, 1.f, 6.f, (sin(t * 0.001f) * 0.5f + 0.5f));
    gs_gui_layout_row(gui, 1, (int[]) {-1}, y); gs_gui_layout_next(gui);

    bsf_gui_menu_panel_row_begin(bsf, layout, gs_v2(layout->body.w * 0.9f, layout->body.h * 0.9f), gs_v2(0.f, y), 0.01f);
    {
        BSF_GUI_ELEMENT_CENTERED({
            gs_gui_label_ex(gui, "The Binding of", &(gs_gui_selector_desc_t){.id = "title_bind", .classes = {"margin_title"}}, 0x00);
        });

        BSF_GUI_ELEMENT_CENTERED({
            gs_gui_label_ex(gui, "Starfox", &(gs_gui_selector_desc_t){.id = "title_sf", .classes ={"hover_red"}}, 0x00);
        });
    } 
    bsf_gui_menu_panel_row_end(bsf);

    gs_gui_rect_t anchor = gs_gui_layout_anchor(parent, parent->w, 50, 0, -50, GS_GUI_LAYOUT_ANCHOR_BOTTOMLEFT);
    gs_gui_layout_set_next(gui, anchor, 0);
    gs_gui_panel_begin_ex(gui, "##start", NULL, GS_GUI_OPT_NOSCROLL);
    {
        gs_gui_layout_row(gui, 1, (int[]){-1}, 0);
        if (gs_gui_label_ex(gui, "Press Start", &(gs_gui_selector_desc_t){.classes={"start_lbl", "hover_red"}}, 0x00) || 
            input->gamepads[0].buttons[GS_PLATFORM_GAMEPAD_BUTTON_START] || 
            input->gamepads[0].buttons[GS_PLATFORM_GAMEPAD_BUTTON_A]) {
            bsf->state = BSF_STATE_MAIN_MENU;
            bsf_play_sound(bsf, "audio.start", 0.5f);
        }
    }
    gs_gui_panel_end(gui); 

    if (
        gs_platform_key_pressed(GS_KEYCODE_ESC)
    )
    {
        gs_quit();
    } 

    if (
        gs_platform_key_pressed(GS_KEYCODE_ENTER) || 
        gs_platform_key_pressed(GS_KEYCODE_SPACE)
    )
    {
        bsf->state = BSF_STATE_MAIN_MENU;
    }
} 

GS_API_DECL void bsf_menu_update(struct bsf_t* bsf)
{
    gs_command_buffer_t* cb = &bsf->gs.cb;
    gs_gui_context_t* gui = &bsf->gs.gui;
	const float t = gs_platform_elapsed_time();
    const gs_vec2 fbs = gs_platform_framebuffer_sizev(gs_platform_main_window()); 

    // Assets
    const gs_gui_style_sheet_t* ss = gs_hash_table_getp(bsf->assets.style_sheets, gs_hash_str64("ss.title")); 
    const gs_gfxt_texture_t* title_bg = gs_hash_table_getp(bsf->assets.textures, gs_hash_str64("tex.title_bg"));
    const gs_gfxt_texture_t* vignette = gs_hash_table_getp(bsf->assets.textures, gs_hash_str64("tex.vignette"));

    // Reset style sheet to default at top of frame
    gs_gui_set_style_sheet(gui, NULL);

    int32_t opt = GS_GUI_OPT_FULLSCREEN |
                  GS_GUI_OPT_NOTITLE |
                  GS_GUI_OPT_NOBORDER |
                  GS_GUI_OPT_NOMOVE |
                  GS_GUI_OPT_NOSCROLL |
                  GS_GUI_OPT_NORESIZE;

    switch (bsf->state) 
    {
        case BSF_STATE_MAIN_MENU:
        case BSF_STATE_OPTIONS:
        case BSF_STATE_GAME_SETUP:
        case BSF_STATE_TITLE:
        {
            gs_gui_rect_t anchor = {0};

            // Bind style sheet
            gs_gui_set_style_sheet(gui, ss); 

            gs_vec2 ws = {800.f, 500.f}; 
            gs_gui_window_begin_ex(gui, "##title", gs_gui_rect((fbs.x - ws.x) * 0.5f, (fbs.y - ws.y) * 0.5f, ws.x, ws.y), NULL, NULL, opt); 
            {
                gs_gui_container_t* cnt = gs_gui_get_current_container(gui); 

                // Do background image
                {
                    anchor = gs_gui_layout_anchor(&cnt->body, 3000, 3000, 0, 210, GS_GUI_LAYOUT_ANCHOR_CENTER);
                    gs_gui_layout_set_next(gui, anchor, 0);
                    gs_gui_panel_begin(gui, "##image");
                        gs_gui_layout_row(gui, 1, (int[]){-1}, -1);
                        gs_gui_image_ex(gui, *title_bg, gs_v2s(0.f), gs_v2s(1.f), 
                            &(gs_gui_selector_desc_t){.classes = {"title_bg"}}, GS_GUI_OPT_NOINTERACT);
                    gs_gui_panel_end(gui);
                }

                // Do vignette overlay
                {
                    const int32_t off = 30;
                    anchor = gs_gui_layout_anchor(&cnt->rect, cnt->rect.w + off * 2 + 10.f, 
                            cnt->rect.h + off * 2 + 10.f, -off, -off, GS_GUI_LAYOUT_ANCHOR_CENTER);
                    gs_gui_layout_set_next(gui, anchor, 0);
                    gs_gui_panel_begin(gui, "##vignette");
                        gs_gui_layout_row(gui, 1, (int[]){-1}, -1);
                        gs_gui_image_ex(gui, *vignette, gs_v2s(0.f), gs_v2s(1.f), 
                            &(gs_gui_selector_desc_t){.classes = {"title_vin"}}, GS_GUI_OPT_NOINTERACT);
                    gs_gui_panel_end(gui);
                }

                gs_gui_rect_t anchor = gs_gui_layout_anchor(&cnt->body, cnt->body.w * 0.8f, 300, 0, 0, GS_GUI_LAYOUT_ANCHOR_CENTER);
                gs_gui_layout_set_next(gui, anchor, 0);
                gs_gui_panel_begin_ex(gui, "##title_screen", &(gs_gui_selector_desc_t){.classes = {"bsfpanel"}}, GS_GUI_OPT_NOSCROLL);
                {
                    switch (bsf->state)
                    {
                        default:                                                       break;
                        case BSF_STATE_TITLE:       bsf_title_screen(bsf, &anchor);    break;
                        case BSF_STATE_MAIN_MENU:   bsf_main_menu(bsf, &anchor);       break;
                        case BSF_STATE_OPTIONS:     bsf_options_menu(bsf, &anchor);    break;
                        case BSF_STATE_GAME_SETUP:  bsf_game_setup_menu(bsf, &anchor); break;
                    }
                }
                gs_gui_panel_end(gui);

            } 

            gs_gui_window_end(gui);

        } break; 

        case BSF_STATE_PAUSE:
        {
            gs_gui_window_begin_ex(gui, "##pause", gs_gui_rect(0, 0, 100, 100), NULL, NULL, opt); 

			gs_gui_container_t* cnt = gs_gui_get_current_container(gui); 

            if (gs_platform_key_pressed(GS_KEYCODE_ESC)) 
            {
                bsf->state = BSF_STATE_PLAY;
            }

            if (gs_gui_button(gui, "Resume"))
            {
                bsf->state = BSF_STATE_PLAY;
                bsf_play_sound(bsf, "audio.start", 0.5f);
            } 

            if (gs_gui_button(gui, "Options"))
            {
                bsf->state = BSF_STATE_OPTIONS;
            } 

            if (gs_gui_button(gui, "Exit"))
            {
                bsf->state = BSF_STATE_END;
                bsf_play_music(bsf, "audio.music_title");
            }

            gs_gui_window_end(gui);

        } break;
    }
} 

//=== BSF Game ===//

static uint32_t bsf_game_room_get_cell(uint32_t row, uint32_t col)
{
	return row * BSF_ROOM_MAX_COLS + col;
} 

static void bsf_game_room_get_row_col(struct bsf_t* bsf, uint32_t cell, uint32_t* row, uint32_t* col)
{
    *row = (int32_t)((float)cell / (float)BSF_ROOM_MAX_COLS);
    *col = cell - *row * BSF_ROOM_MAX_COLS;
}

static bool bsf_game_room_cell_valid(struct bsf_t* bsf, uint32_t cell)
{
    uint32_t row = 0, col = 0;
    bsf_game_room_get_row_col(bsf, cell, &row, &col);
    return (row > 0 && row < BSF_ROOM_MAX_ROWS - 1 && col > 0 && col < BSF_ROOM_MAX_COLS - 1);
}

static bool bsf_game_room_id_filled(struct bsf_t* bsf, uint32_t cell)
{
	if (!bsf_game_room_cell_valid(bsf, cell)) return false;
    int16_t room_id = bsf->run.room_ids[cell]; 
    return (room_id != -1);
}

static uint32_t bsf_game_room_start_cell()
{
	return bsf_game_room_get_cell(4, 4);
} 

static uint32_t bsf_game_room_num_neighbors(struct bsf_t* bsf, uint32_t cell)
{
    uint32_t num_filled_neighbors = 0;
    const uint32_t l = cell - 1;
    const uint32_t r = cell + 1; 
    const uint32_t t = cell - BSF_ROOM_MAX_COLS; 
    const uint32_t b = cell + BSF_ROOM_MAX_COLS; 

    if (bsf_game_room_id_filled(bsf, l)) num_filled_neighbors++;
    if (bsf_game_room_id_filled(bsf, r)) num_filled_neighbors++;
    if (bsf_game_room_id_filled(bsf, t)) num_filled_neighbors++;
    if (bsf_game_room_id_filled(bsf, b)) num_filled_neighbors++;

    return num_filled_neighbors;
}

static bool bsf_game_level_visit_cell(struct bsf_t* bsf, uint32_t cell, uint32_t max_rooms)
{ 
    uint32_t num_rooms = gs_slot_array_size(bsf->run.rooms);

    // Out of bounds
    if (!bsf_game_room_cell_valid(bsf, cell))
    {
        return false;
    }

    // Determine if number of rooms met
    if (num_rooms >= max_rooms)
    {
        return false;
    }

    // Determine if already occupied
    if (bsf_game_room_id_filled(bsf, cell))
    {
        return false;
    }

    // Determine if already has more than one filled neighbor cell
    uint32_t num_filled_neighbors = bsf_game_room_num_neighbors(bsf, cell); 

    if (num_filled_neighbors > 1)
    {
        return false;
    }

    // Determine if 50% chance to quit
    bool rnd = (gs_rand_gen_long(&bsf->run.rand) % 2) == 0;
    if (rnd)
    {
        return false;
    }

    // Otherwise add room
    return true;
}

static void bsf_game_insert_dead_end(struct bsf_t* bsf, gs_dyn_array(uint16_t)* dead_ends, uint32_t cell)
{
	if (!bsf_game_room_cell_valid(bsf, cell)) return;
	if (cell == bsf_game_room_start_cell()) return;

	for (uint32_t i = 0; i < gs_dyn_array_size(*dead_ends); ++i)
	{
		if ((*dead_ends)[i] == cell) return;
	}

	gs_dyn_array_push(*dead_ends, cell);
}

static bsf_room_t* bsf_game_room_getp(struct bsf_t* bsf, uint32_t cell)
{
    if (!bsf_game_room_cell_valid(bsf, cell)) return NULL;
    return gs_slot_array_getp(bsf->run.rooms, bsf->run.room_ids[cell]);
}

static bool bsf_game_room_cell_next_to(struct bsf_t* bsf, uint32_t cell, bsf_room_type type)
{
    const uint32_t l = cell - 1;
    const uint32_t r = cell + 1; 
    const uint32_t t = cell - BSF_ROOM_MAX_COLS; 
    const uint32_t b = cell + BSF_ROOM_MAX_COLS; 
    bsf_room_t* room = NULL;

    room = bsf_game_room_getp(bsf, l);
    if (room && room->type == type) return true;

    room = bsf_game_room_getp(bsf, r);
    if (room && room->type == type) return true;

    room = bsf_game_room_getp(bsf, t);
    if (room && room->type == type) return true;

    room = bsf_game_room_getp(bsf, b);
    if (room && room->type == type) return true;

    return false;
    
}

static void bsf_game_level_queue(struct bsf_t* bsf, gs_dyn_array(uint16_t)* queue, gs_dyn_array(uint16_t)* dead_ends, uint32_t max_rooms)
{ 
    for (uint32_t i = 0; i < gs_dyn_array_size(*queue); ++i)
    {
        bool neighbor = false;
        uint32_t cell = (*queue)[i];
        bsf_room_t* rp = bsf_game_room_getp(bsf, cell);
        uint32_t distance = rp ? rp->distance : 0;

        // Look at all cardinal directions for each cell
        const uint32_t l = cell - 1;
        const uint32_t r = cell + 1; 
        const uint32_t t = cell - BSF_ROOM_MAX_COLS; 
        const uint32_t b = cell + BSF_ROOM_MAX_COLS; 

        if (bsf_game_level_visit_cell(bsf, l, max_rooms)) 
        {
            bsf->run.room_ids[l] = gs_slot_array_insert(bsf->run.rooms, (bsf_room_t){.distance = distance + 1});
            gs_dyn_array_push(*queue, l); 
            neighbor = true;
        }

        if (bsf_game_level_visit_cell(bsf, r, max_rooms)) 
        {
            bsf->run.room_ids[r] = gs_slot_array_insert(bsf->run.rooms, (bsf_room_t){.distance = distance + 1});
            gs_dyn_array_push(*queue, r); 
            neighbor = true;
        }

        if (bsf_game_level_visit_cell(bsf, t, max_rooms)) 
        {
            bsf->run.room_ids[t] = gs_slot_array_insert(bsf->run.rooms, (bsf_room_t){.distance = distance + 1});
            gs_dyn_array_push(*queue, t); 
            neighbor = true;
        }

        if (bsf_game_level_visit_cell(bsf, b, max_rooms)) 
        {
            bsf->run.room_ids[b] = gs_slot_array_insert(bsf->run.rooms, (bsf_room_t){.distance = distance + 1});
            gs_dyn_array_push(*queue, b); 
            neighbor = true;
        } 

        // Dead end
        if (!neighbor)
        {
			bsf_game_insert_dead_end(bsf, dead_ends, cell);
        } 
    }
}

static int32_t bsf_game_room_compare(void* cp0, void* cp1)
{
    bsf_t* bsf = gs_user_data(bsf_t);

    // Determine manhattan distance of cells and sort based on distance from start cell
    uint16_t c0 = *((uint16_t*)(cp0));
    uint16_t c1 = *((uint16_t*)(cp1));

    bsf_room_t* r0 = bsf_game_room_getp(bsf, c0);
    bsf_room_t* r1 = bsf_game_room_getp(bsf, c1);

    return (r0->distance > r1->distance);
}

GS_API_DECL void bsf_room_load(struct bsf_t* bsf, uint32_t cell)
{
    // Unload previous room cell of items, mobs, obstacles
    for (
        gs_slot_array_iter it = gs_slot_array_iter_new(bsf->run.rooms); 
        gs_slot_array_iter_valid(bsf->run.rooms, it);
        gs_slot_array_iter_advance(bsf->run.rooms, it)
    )
    {
        bsf_room_t* room = gs_slot_array_iter_getp(bsf->run.rooms, it); 
        for (uint32_t i = 0; i < gs_dyn_array_size(room->mobs); ++i) {
            ecs_delete(bsf->entities.world, room->mobs[i]);
        }

        for (uint32_t i = 0; i < gs_dyn_array_size(room->obstacles); ++i) {
            ecs_delete(bsf->entities.world, room->obstacles[i]);
        }

        for (uint32_t i = 0; i < gs_dyn_array_size(room->consumables); ++i) {
            ecs_delete(bsf->entities.world, room->consumables[i]);
        }

        for (uint32_t i = 0; i < gs_dyn_array_size(room->items); ++i) {
            ecs_delete(bsf->entities.world, room->items[i]);
        }

        gs_dyn_array_clear(room->mobs);
        gs_dyn_array_clear(room->obstacles);
        gs_dyn_array_clear(room->consumables);
        gs_dyn_array_clear(room->items);
    }

    gs_println("Loading room: %zu", cell);

    // Set new cell
    bsf->run.cell = cell;

    // Get new room to load
    bsf_room_t* room = gs_slot_array_iter_getp(bsf->run.rooms, bsf->run.room_ids[bsf->run.cell]);
    ecs_world_t* world = bsf->entities.world;

    // Do not load room if already cleared
    if (room->cleared) {
        return; 
    }

    room->movement_type = BSF_MOVEMENT_RAIL;

    switch (room->type)
    { 
        default: break; 
        case BSF_ROOM_START: break;
        case BSF_ROOM_SECRET: break;
        case BSF_ROOM_SHOP: break;

        // Place item "chest" to be shot open by player
        case BSF_ROOM_ITEM: 
        {
            // Generate new seeded rand for items
            gs_mt_rand_t item_rand = gs_rand_seed(bsf->run.rand.mt[32] + bsf->run.cell); 

			uint32_t cnt = 3; // gs_rand_gen_range_long( &item_rand, 1, 3 );

            float xsize = 13.f; 
			float xstep = xsize / (float)cnt;

			for (uint32_t c = 0; c < cnt; ++c)
			{
				float xoff = cnt > 1 ? gs_map_range(0.f, (float)cnt, -xsize, xsize, (float)c) + xstep : 0.f;

				// Random item chest to be placed in front of player
				gs_vqs xform = gs_vqs_default(); 
				xform.translation = gs_v3(xoff, 4.f, -3.f);
				xform.scale = gs_v3s(5.f);
				bsf_item_type type = 0x00; 
				bool found = true;
				while (found) 
				{ 
					type = (bsf_item_type)gs_rand_gen_range_long(&item_rand, 0, BSF_ITEM_COUNT - 1); 
					bool inner_found = false; 
					for (uint32_t i = 0; i < gs_dyn_array_size(bsf->run.item_pool); ++i) {
						if (bsf->run.item_pool[i] == type) {
							inner_found = true; 
							break;
						}
					} 
					found = inner_found;
				}
				// Have to verify that this item isn't taken yet...
				ecs_entity_t e = bsf_item_chest_create(bsf, &xform, type);
				gs_dyn_array_push(room->items, e);

				// Add to item pool
				gs_dyn_array_push(bsf->run.item_pool, type); 
			}

        } break;

        case BSF_ROOM_BOSS: 
        {
            // Load up the only boss we got, hoss
            bsf_mob_type type = BSF_MOB_BOSS;
            gs_vqs xform = gs_vqs_default();
            xform.translation.z = 10.f;
            ecs_entity_t e = bsf_mob_create(bsf, world, &xform, type);
            gs_dyn_array_push(room->mobs, e);
            bsf->entities.boss = e;

            // Play boss music
            bsf_play_music(bsf, "audio.music_boss");
        }break; 

        case BSF_ROOM_DEFAULT:
        { 
            // Generate new seeded rand for items
            gs_mt_rand_t item_rand = gs_rand_seed(bsf->run.rand.mt[32] + bsf->run.cell); 

			uint32_t rs = gs_hash_table_size(bsf->assets.room_templates);
			uint16_t v = (uint16_t)gs_rand_gen_long(&item_rand) % rs; 
			gs_snprintfc(HASH_BUF, 256, "rt.r%zu", v);
			gs_println("Loading room template: %s, rs: %zu", HASH_BUF, rs);
			uint64_t hash = gs_hash_str64(HASH_BUF);

			bsf_room_template_t* rt = gs_hash_table_getp(bsf->assets.room_templates, hash);
			gs_assert(rt); 

			for (
				gs_slot_array_iter it = gs_slot_array_iter_new(rt->brushes);
				gs_slot_array_iter_valid(rt->brushes, it);
				gs_slot_array_iter_advance(rt->brushes, it)
			)
			{ 
				bsf_room_brush_t* brush = gs_slot_array_iter_getp(rt->brushes, it);
				switch (brush->type)
				{
                    case BSF_ROOM_BRUSH_MOB: 
                    {
                        // If it's a turret, then it stays on the floor and moves with the level
                        uint64_t v = gs_rand_gen_long(&item_rand);
                        bsf_mob_type type = v % (uint64_t)BSF_MOB_BOSS;
                        ecs_entity_t e = bsf_mob_create(bsf, world, &brush->xform, type);
                        gs_dyn_array_push(room->mobs, e);
                    } break;

                    case BSF_ROOM_BRUSH_BANDIT: 
                    {
                        bsf_mob_type type = BSF_MOB_BANDIT;
                        ecs_entity_t e = bsf_mob_create(bsf, world, &brush->xform, type);
                        gs_dyn_array_push(room->mobs, e);
                    } break;

                    case BSF_ROOM_BRUSH_TURRET: 
                    {
                        // If it's a turret, then it stays on the floor and moves with the level
                        bsf_mob_type type = BSF_MOB_TURRET;
                        ecs_entity_t e = bsf_mob_create(bsf, world, &brush->xform, type);
                        gs_dyn_array_push(room->mobs, e);
                    } break;

                    case BSF_ROOM_BRUSH_CONSUMABLE:
                    {
                        uint64_t v = gs_rand_gen_long(&item_rand);
                        bsf_consumable_type type = v % (uint64_t)BSF_CONSUMABLE_COUNT;
                        ecs_entity_t e = bsf_consumable_create(bsf, &brush->xform, type);
                        gs_dyn_array_push(room->consumables, e);
                    } break;

                    case BSF_ROOM_BRUSH_OBSTACLE:
                    {
                        uint64_t v = gs_rand_gen_long(&item_rand);
                        bsf_obstacle_type type = v % (uint64_t)BSF_OBSTACLE_COUNT; 
						gs_vqs xform = brush->xform;

                        // Get random scale 
                        xform.scale = gs_v3( 
                            gs_rand_gen_range(&item_rand, 1.f, 10.f), 
                            gs_rand_gen_range(&item_rand, 1.f, 25.f), 
                            gs_rand_gen_range(&item_rand, 1.f, 10.f)
                        );
                        xform.translation.z -= 1.5f * BSF_ROOM_BOUND_Z;

                        ecs_entity_t e = bsf_obstacle_create(bsf, &xform, type);
                        gs_dyn_array_push(room->obstacles, e);
                    } break; 
				}
			} 
        } break; 
    }

}

static void bsf_game_level_gen(struct bsf_t* bsf)
{
    // How is the world structured? A series of "rooms" that the player can travel in a level, with one boss per level
    // Reference: https://www.boristhebrave.com/2020/09/12/dungeon-generation-in-binding-of-isaac/
    /*
        Grid: 
            *********   // Boundaries are ignored (x = 0, x = MAX_COLS - 1, y = 0, y = MAX_ROWS - 1)
            *--X--XX*
            *-XXX-X-*
            *--XXXX-*
            *********

        Number of rooms in level: random(2) + 5 + level * 2.6

        The game then places the starting room, cell (4, 4), on a queue. 
        It then loops over the queue. For each cell in the queue, it loops over the 4 cardinal directions and does the following:
            * Determine the neighbour cell by adding +1/-1/+MAX_COL/-MAX_COL/ to the current cell.
            * If the neighbour cell is already occupied, give up
            * If the neighbour cell itself has more than one filled neighbour, give up.
            * If we already have enough rooms, give up
            * Random 50% chance, give up
            * Otherwise, mark the neighbour cell as having a room in it, and add it to the queue.

        If a cell doesn't add a neighbor, mark it as "dead end" and store for later

        Special Rooms: 
            - Boss room is placed by reading last item from 'end rooms list'. Guaranteed to be farthest from start.
            - Next secret room is placed.
            - Then others.
    */ 

    // Local variables
    const uint16_t num_rooms = (uint16_t)(gs_rand_gen_range(&bsf->run.rand, 1.0, 2.0) + 5.f + (float)bsf->run.level * 2.6f); 
    const uint16_t start_room = bsf_game_room_start_cell();
    gs_dyn_array(uint16_t) queue = NULL;
    gs_dyn_array(uint16_t) dead_ends = NULL;
	const uint32_t iter_amount = 100;
	uint32_t it = 0;
    bsf_room_t* room = NULL;
    uint32_t item_idx = 0, shop_idx = 0;
    bool found = false;

    // Reset previous room/level data
    memset(bsf->run.room_ids, -1, sizeof(int16_t) * (BSF_ROOM_MAX));
    
    // Iterate through room data and clear mobs
    for (
        gs_slot_array_iter it = gs_slot_array_iter_new(bsf->run.rooms);
        gs_slot_array_iter_valid(bsf->run.rooms, it);
        gs_slot_array_iter_advance(bsf->run.rooms, it)
    )
    {
        bsf_room_t* room = gs_slot_array_iter_getp(bsf->run.rooms, it); 
        for (uint32_t i = 0; i < gs_dyn_array_size(room->mobs); ++i) {
            ecs_delete(bsf->entities.world, room->mobs[i]);
        }
        for (uint32_t i = 0; i < gs_dyn_array_size(room->items); ++i) {
            ecs_delete(bsf->entities.world, room->items[i]);
        }
        for (uint32_t i = 0; i < gs_dyn_array_size(room->obstacles); ++i) {
            ecs_delete(bsf->entities.world, room->obstacles[i]);
        }
        for (uint32_t i = 0; i < gs_dyn_array_size(room->consumables); ++i) {
            ecs_delete(bsf->entities.world, room->consumables[i]);
        }
        gs_dyn_array_clear(room->mobs);
        gs_dyn_array_clear(room->items);
        gs_dyn_array_clear(room->obstacles);
        gs_dyn_array_clear(room->consumables);
        room->cleared = false;
    } 

    gs_dyn_array_clear(bsf->run.item_pool);
    gs_slot_array_clear(bsf->run.rooms);
    gs_slot_array_reserve(bsf->run.rooms, num_rooms); 

    // Add starting room to room data and id map
    bsf->run.room_ids[start_room] = (int16_t)gs_slot_array_insert(bsf->run.rooms, (bsf_room_t){.type = BSF_ROOM_START});

    // Room data generation queue
	while (it < iter_amount && gs_slot_array_size(bsf->run.rooms) < num_rooms) 
	{
		gs_dyn_array_clear(queue);
		gs_dyn_array_push(queue, start_room);

        // Push all dead ends as well
        for (uint32_t d = 0; d < gs_dyn_array_size(dead_ends); ++d) {
            gs_dyn_array_push(queue, dead_ends[d]);
        }

		bsf_game_level_queue(bsf, &queue, &dead_ends, num_rooms);
		it++;
	} 

	// Prune all dead ends that need to be removed (no longer dead ends)
	for (uint32_t d = 0; d < gs_dyn_array_size(dead_ends);) 
	{ 
		// Determine if already has more than one filled neighbor cell
		uint32_t cell = dead_ends[d];
		uint32_t num_filled_neighbors = 0;
		const uint32_t l = cell - 1;
		const uint32_t r = cell + 1; 
		const uint32_t t = cell - BSF_ROOM_MAX_COLS; 
		const uint32_t b = cell + BSF_ROOM_MAX_COLS; 

        num_filled_neighbors = bsf_game_room_num_neighbors(bsf, cell);
		if (num_filled_neighbors > 1)
		{ 
			// Swap and pop with back
			dead_ends[d] = dead_ends[gs_dyn_array_size(dead_ends) - 1];
			gs_dyn_array_pop(dead_ends);
		} 
		else
		{ 
			++d;
		} 
	}

    // Sort dead ends by manhattan distance from start
    qsort(dead_ends, gs_dyn_array_size(dead_ends), sizeof(uint16_t), bsf_game_room_compare); 

	// Assign boss room (use manhattan distance to determine farthest room from start)
    uint16_t boss = (uint16_t)gs_dyn_array_back(dead_ends);
    bsf->run.boss = boss;
	room = gs_slot_array_getp(bsf->run.rooms, bsf->run.room_ids[boss]);
	room->type = BSF_ROOM_BOSS;

    // Pop off end 
    gs_dyn_array_pop(dead_ends);

    // Place item room
    item_idx = gs_rand_gen_range_long(&bsf->run.rand, 0, gs_dyn_array_size(dead_ends) - 1);   
    if (!gs_dyn_array_empty(dead_ends))
    {
        room = gs_slot_array_getp(bsf->run.rooms, bsf->run.room_ids[dead_ends[item_idx]]);
        room->type = BSF_ROOM_ITEM;
        bsf->run.item = (uint16_t)dead_ends[item_idx];
        dead_ends[item_idx] = gs_dyn_array_back(dead_ends);
        gs_dyn_array_pop(dead_ends);
    }

    // Place secret room
    uint32_t sr_neighbor_count = 3;
    while (!found)
    {
        for (uint32_t r = 0; r < BSF_ROOM_MAX_ROWS; ++r)
            for (uint32_t c = 0; c < BSF_ROOM_MAX_COLS; ++c)
        { 
            if (found) break;
            uint32_t cell = bsf_game_room_get_cell(r, c);
            uint32_t num_filled_neighbors = bsf_game_room_num_neighbors(bsf, cell);
            bool next_to_dead_end = false;
            int16_t id = bsf->run.room_ids[cell];
            if (id == -1 && num_filled_neighbors >= sr_neighbor_count)
            {
                // Check neighbors (make sure not next to boss room)
                if (
                    !bsf_game_room_cell_next_to(bsf, cell, BSF_ROOM_BOSS) &&
                    !bsf_game_room_cell_next_to(bsf, cell, BSF_ROOM_ITEM)
                )
                {
                    // Insert new room
                    bsf_room_t room = (bsf_room_t){.type = BSF_ROOM_SECRET, .cell = (uint16_t)cell};
                    bsf->run.room_ids[cell] = gs_slot_array_insert(bsf->run.rooms, room);
                    found = true; 
                    break;
                } 
            }
        }
        sr_neighbor_count--;
    } 

    // Place shop
    shop_idx = gs_rand_gen_range_long(&bsf->run.rand, 0, gs_dyn_array_size(dead_ends) - 1);   
    if (!gs_dyn_array_empty(dead_ends))
    {
        room = gs_slot_array_getp(bsf->run.rooms, bsf->run.room_ids[dead_ends[shop_idx]]);
        room->type = BSF_ROOM_SHOP;
        dead_ends[shop_idx] = gs_dyn_array_back(dead_ends);
        gs_dyn_array_pop(dead_ends);
    } 

    // For each room, generate mobs (this will be based on templates) 
    bsf_room_load(bsf, start_room);

    // For each room, set room cell
    for (uint32_t i = 0; i < BSF_ROOM_MAX; ++i)
    { 
        uint16_t cell = (uint16_t)i;
        int16_t id = bsf->run.room_ids[cell];
        if (id != -1) 
        {
            bsf_room_t* room = gs_slot_array_getp(bsf->run.rooms, id);
            room->cell = cell;
        }
    } 

    gs_dyn_array_free(queue);
    gs_dyn_array_free(dead_ends);
} 

void bsf_game_add_room_entity(struct bsf_t* bsf)
{ 
    // Add entity for room
    ecs_entity_t e = ecs_new(bsf->entities.world, 0); 

    ecs_set(bsf->entities.world, e, bsf_component_renderable_immediate_t, {
        .shape = BSF_SHAPE_ROOM,
        .model = gs_mat4_identity(),
        .color = GS_COLOR_WHITE
    });

    ecs_set(bsf->entities.world, e, bsf_component_transform_t, {
        .xform = gs_vqs_default()
    });
}

GS_API_DECL void bsf_game_start(struct bsf_t* bsf) 
{ 
	// Initialize entities
    bsf_entities_init(bsf);

    // Initialize camera
    bsf_camera_init(bsf, &bsf->scene.camera);

    // Initialize player
    bsf_player_init(bsf);

    // Initialize run 
    bsf->run.rand = gs_rand_seed(gs_hash_str64(bsf->run.seed)); 
    bsf->run.level = 1;
    bsf->run.cell = bsf_game_room_get_cell(4, 4);
    bsf->run.time_scale = 1.f;
    bsf->run.just_cleared_room = false;
    bsf->run.just_cleared_level = false;
    bsf->run.complete = false;
    bsf->run.clear_timer = 0.f;

    // Initialize world based on seed
    bsf_game_level_gen(bsf);

    // Add room entity
    bsf_game_add_room_entity(bsf);

    gs_platform_lock_mouse(gs_platform_main_window(), true);

    // Set state to playing game
    bsf->state = BSF_STATE_PLAY;

    // Start level music
    bsf_play_music(bsf, "audio.music_level");
}

GS_API_DECL void bsf_game_end(struct bsf_t* bsf)
{ 
	// Destroy entity world
	ecs_fini(bsf->entities.world);

    bsf->run.is_playing = false;
    bsf->state = BSF_STATE_MAIN_MENU;
}

GS_API_DECL void bsf_play_music(struct bsf_t* bsf, const char* key)
{
    uint64_t hash = gs_hash_str64(key);
    gs_asset_audio_t* src = gs_hash_table_getp(bsf->assets.sounds, hash);
    gs_assert(src);
    gs_audio_stop(bsf->music);
    if (!gs_handle_is_valid(bsf->music)) {
        gs_println("CREATING INSTANCE DATA");
        bsf->music = gs_audio_instance_create(&(gs_audio_instance_decl_t) {
            .src = src->hndl, 
            .volume = 0.7f, 
            .loop = true, 
            .persistent = false
        });
    }
    else
    {
        gs_println("SETTING INSTANCE DATA");
        gs_audio_set_instance_data(bsf->music, (gs_audio_instance_decl_t) {
            .src = src->hndl, 
            .volume = 0.7f, 
            .loop = true,
            .persistent = false,
            .sample_position = 0
        });
    }
    gs_audio_play(bsf->music);
}

GS_API_DECL void bsf_play_sound(struct bsf_t* bsf, const char* key, float volume)
{
    uint64_t hash = gs_hash_str64(key);
    gs_asset_audio_t* src = gs_hash_table_getp(bsf->assets.sounds, hash);
    gs_assert(src);
    gs_audio_play_source(src->hndl, volume);
}

typedef struct
{
    int16_t type;
} bsf_gui_cb_data_t;

void gui_cb(gs_gui_context_t* ctx, gs_gui_customcommand_t* cmd)
{
    // Push a draw directly into ctx gsi here 
    // NOTE(john): Already in a render pass, so cannot start a new one 

    bsf_t* bsf = gs_user_data(bsf_t); 
    gs_immediate_draw_t* gsi = &ctx->gsi;
    const float t = gs_platform_elapsed_time();
    const gs_vec2 fbs = gs_platform_framebuffer_sizev(gs_platform_main_window());
    gs_gfxt_material_t* mat = gs_hash_table_getp(bsf->assets.materials, gs_hash_str64("mat.gsi"));
    gs_gfxt_pipeline_t* pip = gs_gfxt_material_get_pipeline(mat); 
    gs_gui_id id = gs_gui_get_id_hash(ctx, "#ctrl", 5, cmd->hash); 
    gs_color_t col = cmd->hover == id ? GS_COLOR_RED : GS_COLOR_WHITE;

    gsi_camera3D(gsi, (uint32_t)cmd->viewport.w, (uint32_t)cmd->viewport.h);
    gsi_vattr_list_mesh(gsi, pip->mesh_layout, gs_dyn_array_size(pip->mesh_layout) * sizeof(gs_gfxt_mesh_layout_t)); 
    gsi_push_matrix(gsi, GSI_MATRIX_MODELVIEW);
    gsi_translatev(gsi, gs_v3(1.f, 0.f, -1.f));
    gsi_rotatev(gsi, t * 0.001f, GS_YAXIS);
    gsi_box(gsi, 0.f, 0.f, 0.f, 0.5f, 0.5f, 0.5f, col.r, col.g, col.b, col.a, pip->desc.raster.primitive);
    gs_mat4 mvp = gsi_get_mvp_matrix(gsi); 
    gsi_pop_matrix_ex(gsi, false); 
    gs_graphics_set_viewport(&gsi->commands, cmd->viewport.x, 
            fbs.y - cmd->viewport.h - cmd->viewport.y, cmd->viewport.w, cmd->viewport.h); 
    gs_gfxt_material_set_uniform(mat, "u_mvp", &mvp);
    gs_gfxt_material_bind(&gsi->commands, mat);
    gs_gfxt_material_bind_uniforms(&gsi->commands, mat);
    gsi_flush(gsi);
}

GS_API_DECL void bsf_game_update(struct bsf_t* bsf)
{ 
    gs_command_buffer_t* cb = &bsf->gs.cb;
    gs_immediate_draw_t* gsi = &bsf->gs.gsi;
    gs_gui_context_t* gui = &bsf->gs.gui;
	gs_vec2 fbs = gs_platform_framebuffer_sizev(gs_platform_main_window()); 
	const float t = gs_platform_elapsed_time() * 0.001f;
    const float dt = gs_platform_delta_time();
    const float frame = gs_platform_time()->frame;
    bsf_room_t* room = gs_slot_array_getp(bsf->run.rooms, bsf->run.room_ids[bsf->run.cell]);

    if ((bsf->dbg && gs_platform_mouse_down(GS_MOUSE_RBUTTON)) || !bsf->dbg)
    {
        // Lock mouse at start by default
        gs_platform_lock_mouse(gs_platform_main_window(), true);
        if (bsf->dbg) {
            bsf_editor_camera_update(bsf, &bsf->scene.camera);
        }
    }
    else
    {
        // Lock mouse at start by default
        gs_platform_lock_mouse(gs_platform_main_window(), false);
    } 

    if (gs_platform_key_pressed(GS_KEYCODE_ESC)) {
        bsf->state = BSF_STATE_PAUSE;
        bsf_play_sound(bsf, "audio.pause", 0.5f);
        return;
    } 

    if (gs_platform_key_pressed(GS_KEYCODE_F1)) {
        bsf->dbg = !bsf->dbg;
        if (bsf->dbg) {
            bsf_camera_init(bsf, &bsf->scene.camera);
        }
    } 

    if (bsf->dbg)
    {
        if (gs_platform_mouse_down(GS_MOUSE_RBUTTON)) {
            gs_platform_lock_mouse(gs_platform_main_window(), true);
            bsf_editor_camera_update(bsf, &bsf->scene.camera);
        }
    }
    else
    { 
        // Update camera
        bsf_camera_update(bsf, &bsf->scene.camera);
    } 

    if (bsf->run.time_scale < 1.f)
    {
        bsf->run.time_scale = gs_interp_smoothstep(bsf->run.time_scale, 1.f, 0.1f);
    }

    // Update entity world
    ecs_progress(bsf->entities.world, 0);

    // If all mobs cleared from room, then clear it
    const float time_max = 1.f;
    if (
		!bsf->run.complete && 
		gs_dyn_array_empty(room->mobs) && 
		gs_dyn_array_empty(room->items)
	)
    {
        room->cleared = true;

        if (!bsf->run.just_cleared_room)
        {
            bsf->run.just_cleared_room = true;
            bsf->run.clear_timer = time_max;
        } 

        // If all obstacles are cleared as well, then move on
        if (
            gs_dyn_array_empty(room->obstacles) && 
            gs_dyn_array_empty(room->consumables) && 
            bsf->run.just_cleared_room && bsf->run.clear_timer <= 0.f
        )
        {
            // Iterate through rooms, find first "non-cleared" one 
            bool loaded = false;

            bsf_room_t* item_room = gs_slot_array_getp(bsf->run.rooms, bsf->run.room_ids[bsf->run.item]);

            // Roll to load the item room
            uint32_t roll = gs_rand_gen_range_long(&bsf->run.rand, 0, gs_slot_array_size(bsf->run.rooms));
            if (roll % 5 == 0 && !item_room->cleared)
            {
                gs_println("Rolled item room...");

                // Load item room
                loaded = true;
                bsf_room_load(bsf, bsf->run.item);
            }
            else
            {
                for (
                        gs_slot_array_iter it = gs_slot_array_iter_new(bsf->run.rooms);
                        gs_slot_array_iter_valid(bsf->run.rooms, it);
                        gs_slot_array_iter_advance(bsf->run.rooms, it)
                    )
                {
                    bsf_room_t* room = gs_slot_array_iter_getp(bsf->run.rooms, it);
                    if (!room->cleared && room->type != BSF_ROOM_BOSS)
                    {
                        bsf_room_load(bsf, room->cell);
                        loaded = true;
                        break;
                    }
                }
            }


            // Boss time!
            if (!loaded)
            {
                // Check to see if boss room is cleared or not
                bsf_room_t* br = gs_slot_array_getp(bsf->run.rooms, bsf->run.room_ids[bsf->run.boss]);
                if (!br->cleared) {
                    bsf_room_load(bsf, bsf->run.boss);
                }
                else {
                    bsf->run.just_cleared_level = true;
                }
            }
        }
    }
    else
    {
        bsf->run.just_cleared_room = false;
        bsf->run.clear_timer = 0.f;
    } 

    if (bsf->run.just_cleared_level)
    {
        bsf->run.just_cleared_level = false;
        bsf->run.complete = true;
        bsf_play_music(bsf, "audio.music_level_complete");
    }

    // If not in a boss room, we need to have obstacles that scroll by...depending on the room type

    // UI 
	int32_t opt = GS_GUI_OPT_NOBRINGTOFRONT |
				  GS_GUI_OPT_NOINTERACT | 
				  GS_GUI_OPT_NOFRAME | 
				  GS_GUI_OPT_NOTITLE | 
				  GS_GUI_OPT_NORESIZE | 
				  GS_GUI_OPT_NOMOVE |
                  GS_GUI_OPT_FULLSCREEN | 
                  GS_GUI_OPT_FORCESETRECT |
				  GS_GUI_OPT_NODOCK;

    gs_gui_window_begin_ex(gui, "#ui_canvas", gs_gui_rect(0, 0, 0, 0), NULL, NULL, opt);
    { 
        gs_gui_container_t* cnt = gs_gui_get_current_container(gui);

        // Iventory
        {
            bsf_component_inventory_t* ic = ecs_get(bsf->entities.world, bsf->entities.player, bsf_component_inventory_t);
            bsf_component_health_t* hc = ecs_get(bsf->entities.world, bsf->entities.player, bsf_component_health_t);

            char TMP[256] = {0};

            gs_gui_rect_t anchor = gs_gui_layout_anchor(&cnt->body, 350, 30, 10, 0, GS_GUI_LAYOUT_ANCHOR_TOPLEFT);
            gs_gui_layout_set_next(gui, anchor, 0); 
            gs_gui_layout_row(gui, 1, (int16_t[]){-1}, 0); 
            gs_gui_label(gui, "bombs: %zu, health: %.2f, ts: %.2f, fps: %.2f", ic->bombs, hc->health, bsf->run.time_scale, frame); 
        }

        // Boss health
        {
            if (room->type == BSF_ROOM_BOSS && bsf->entities.boss != UINT_MAX)
            {
                gs_gui_rect_t anchor = gs_gui_layout_anchor(&cnt->body, 32, cnt->body.h * 0.8f, 10, 50, GS_GUI_LAYOUT_ANCHOR_TOPLEFT);
                gs_gui_layout_set_next(gui, anchor, 0);
                bsf_component_health_t* bhc = ecs_get(bsf->entities.world, bsf->entities.boss, bsf_component_health_t); 
                gs_color_t c0 = GS_COLOR_RED; 
                gs_color_t c1 = GS_COLOR_GREEN; 
                const float ct = gs_map_range(0.f, 100.f, 0.f, 1.f, bhc->health);
                gs_color_t color = gs_color(
                    (uint8_t)(255.f * gs_interp_smoothstep((float)c0.r / 255.f, (float)c1.r / 255.f, ct)),
                    (uint8_t)(255.f * gs_interp_smoothstep((float)c0.g / 255.f, (float)c1.g / 255.f, ct)), 
                    (uint8_t)(255.f * gs_interp_smoothstep((float)c0.b / 255.f, (float)c1.b / 255.f, ct)),
                    (uint8_t)(255.f * gs_interp_smoothstep((float)c0.a / 255.f, (float)c1.a / 255.f, ct))
                );

                gs_gui_draw_rect(gui, anchor, GS_COLOR_BLACK);
                float y = gs_map_range(100.f, 0.f, anchor.y, anchor.y + anchor.h, bhc->health); 
                float diff = y - anchor.y;
                anchor.y = y;
                anchor.h -= diff; 
                gs_gui_draw_rect(gui, anchor, color);
            }
        }

        // Clear message
        if (bsf->run.clear_timer > 0.f)
        {
            float ct = gs_map_range(0.f, time_max, 0.f, 1.f, gs_max(bsf->run.clear_timer - 3.f, 0.f));
            float yoff = gs_interp_smoothstep(-30, 30, 1.f - ct);
            gs_gui_rect_t anchor = gs_gui_layout_anchor(&cnt->body, 200, 30, 10, yoff, GS_GUI_LAYOUT_ANCHOR_TOPCENTER);
            gs_gui_layout_set_next(gui, anchor, 0); 
            gs_gui_layout_row(gui, 1, (int16_t[]){-1}, 0);
            gs_gui_label(gui, "ROOM CLEARED");
            bsf->run.clear_timer -= dt;

            // On clearing a room, need to select next in list
        }

        // Aim reticles
        struct {float scl; float rscl; int16_t w; int16_t can;} scls[] = {
            {-5.f, 35.f, 2, 1},
            {-20.f, 20.f, 1, 1},
            {-25.f, 15.f, 1, 2},
            {0, 0, 0}
        };

        for (uint32_t i = 0; scls[i].can; ++i)
        {
            bsf_component_transform_t* tc = ecs_get(bsf->entities.world, bsf->entities.player, bsf_component_transform_t);
            gs_vec3 pp = tc->xform.translation;
            gs_vec3 fv = gs_vec3_norm(gs_quat_rotate(tc->xform.rotation, GS_ZAXIS));
            gs_vec3 pos = gs_vec3_add(pp, gs_vec3_scale(fv, scls[i].scl));

            gs_vec3 dir = gs_vec3_norm(gs_vec3_sub(pp, bsf->scene.camera.cam.transform.translation));
            gs_vec3 fwd = gs_vec3_norm(gs_camera_forward(&bsf->scene.camera.cam));

            if (gs_vec3_dot(dir, fwd) >= 0.f)
            {
                gs_vec3 coords = gs_camera_world_to_screen(&bsf->scene.camera.cam, pos, fbs.x, fbs.y); 

                const float sz = scls[i].rscl;
                const float w = scls[i].w;

                switch (scls[i].can)
                {
                    case 1: 
                    {
                        gs_gui_draw_box(gui, gs_gui_rect(coords.x - sz * 0.5f, coords.y - sz * 0.5f, sz, sz), (int16_t[]){w, w, w, w}, GS_COLOR_GREEN);
                    } break;

                    case 2:
                    {
                        float lw = sz * 0.1f;
                        gs_gui_draw_rect(gui, gs_gui_rect(coords.x - lw * 0.5f, coords.y - sz * 0.5f, lw, sz), GS_COLOR_GREEN); // UD
                        gs_gui_draw_rect(gui, gs_gui_rect(coords.x - sz * 0.5f, coords.y, sz, lw), GS_COLOR_GREEN); // LR
                    } break;
                }
            } 
        } 

        // Bullet overlays
        ecs_query_t* q = ecs_query_init(bsf->entities.world, &(ecs_query_desc_t){
            .filter.terms = {
                {ecs_id(bsf_component_projectile_t)}
            }
        });

        ecs_iter_t it = ecs_query_iter(bsf->entities.world, q);
        while (ecs_query_next(&it)) 
        {
            bsf_component_projectile_t* bca = ecs_term(&it, bsf_component_projectile_t, 1);

            for (uint32_t i = 0; i < it.count; ++i)
            {
                bsf_component_projectile_t* bc = &bca[i];
                ecs_entity_t b = it.entities[i];

                switch (bc->owner)
                { 
                    case BSF_OWNER_ENEMY:
                    {
                        const bsf_component_transform_t* tc = ecs_get(bsf->entities.world, b, bsf_component_transform_t);
                        const bsf_component_physics_t* pc = ecs_get(bsf->entities.world, b, bsf_component_physics_t);
                        gs_vec3 bp = tc->xform.translation; 
                        gs_vec3 vel = pc->velocity;

                        gs_vec3 dir = gs_vec3_norm(gs_vec3_sub(bp, bsf->scene.camera.cam.transform.translation));
                        gs_vec3 fwd = gs_vec3_norm(gs_camera_forward(&bsf->scene.camera.cam));

                        if (gs_vec3_dot(dir, fwd) >= 0.f && gs_vec3_dot(vel, fwd) < 0.f)
                        { 
                            gs_vec3 coords = gs_camera_world_to_screen(&bsf->scene.camera.cam, bp, fbs.x, fbs.y); 

                            const float sz = 20.f;
                            const float w = 2.f;

                            gs_gui_draw_box(gui, gs_gui_rect(coords.x - sz * 0.5f, coords.y - sz * 0.5f, sz, sz), (int16_t[]){w, w, w, w}, GS_COLOR_RED);
                        }

                    } break;
                }
            }
        }
        ecs_query_fini(q);
    }
    gs_gui_window_end(gui);

    if (bsf->dbg)
    { 
        static int32_t gop = GS_GUI_GIZMO_TRANSLATE; 
        if (gs_platform_key_pressed(GS_KEYCODE_T)) gop = GS_GUI_GIZMO_TRANSLATE;
        if (gs_platform_key_pressed(GS_KEYCODE_Y)) gop = GS_GUI_GIZMO_SCALE;
        if (gs_platform_key_pressed(GS_KEYCODE_R)) gop = GS_GUI_GIZMO_ROTATE;

        static int32_t mode = GS_GUI_TRANSFORM_WORLD;
        if (gs_platform_key_pressed(GS_KEYCODE_P)) mode = 
            mode == GS_GUI_TRANSFORM_LOCAL ? GS_GUI_TRANSFORM_WORLD : 
                GS_GUI_TRANSFORM_LOCAL; 

        gs_gui_set_style_sheet(gui, NULL);

        // Update debug gui
        gs_gui_window_begin(gui, "#dbg_window", gs_gui_rect(100, 100, 350, 200));
        {
            char TMP[256] = {0};

            gs_gui_layout_t* layout = gs_gui_get_layout(gui); 

            // Set layout here for whatever you want to render into?
            gs_gui_layout_row(gui, 1, (int[]){250}, 50);
            {
                gs_gui_rect_t dr = gs_gui_layout_next(gui); 
                int16_t border[] = {1, 1, 1, 1};
                gs_gui_id id = gs_gui_get_id(gui, "#ctrl", 5);
                gs_gui_update_control(gui, id, dr, 0x00);
                gs_color_t col = gui->hover == id ? GS_COLOR_ORANGE : GS_COLOR_WHITE;
                gs_gui_draw_box(gui, gs_gui_expand_rect(dr, border), border, col);
                gs_gui_draw_custom(gui, dr, gui_cb, NULL, 0);
            }

            gs_platform_input_t* input = &gs_subsystem(platform)->input;
            if (gs_gui_treenode_begin(gui, "#controller")) 
            {
                gs_platform_gamepad_t* gp = &input->gamepads[0];
                if (gp->present)
                {
                    char TMP[256] = {0};

                    for (uint32_t i = 0; i < GS_PLATFORM_GAMEPAD_BUTTON_COUNT; ++i) {
                        gs_snprintf(TMP, sizeof(TMP), "btn(%zu): %s", i, gp->buttons[i] ? "pressed" : "released");
                        gs_gui_label(gui, TMP);
                    }

                    for (uint32_t i = 0; i < GS_PLATFORM_JOYSTICK_AXIS_COUNT; ++i) {
                        gs_snprintf(TMP, sizeof(TMP), "axes(%zu): %.2f", i, gp->axes[i]);
                        gs_gui_label(gui, TMP);
                    }
                } 

                gs_gui_treenode_end(gui);
            }

            gs_gui_layout_row(gui, 1, (int[]){-1}, 0);
            gs_snprintf(TMP, 64, "seed: %s", bsf->run.seed);
            gs_gui_label(gui, TMP); 

            #define GUI_LABEL(STR, ...)\
                do {\
                    gs_snprintf(TMP, 256, STR, ##__VA_ARGS__);\
                    gs_gui_label(gui, TMP);\
                } while (0) 

            GUI_LABEL("frame: %.2f", gs_subsystem(platform)->time.frame); 

            // Need automatic gui debug dumping
            if (gs_gui_treenode_begin(gui, "#run"))
            {
                gs_gui_layout_t* layout = gs_gui_get_layout(gui);
                bsf_room_t* room = gs_slot_array_getp(bsf->run.rooms, bsf->run.room_ids[bsf->run.cell]);

                if (gs_gui_button(gui, "bandit"))
                {
                    bsf_mob_type type = BSF_MOB_BANDIT;
                    gs_vqs xform = gs_vqs_default();
                    xform.translation = gs_v3(
                        gs_rand_gen_range(&bsf->run.rand, -20.f, 20.f),
                        gs_rand_gen_range(&bsf->run.rand, -20.f, 20.f),
                        gs_rand_gen_range(&bsf->run.rand, -20.f, 20.f)
                    );
                    ecs_entity_t e = bsf_mob_create(bsf, bsf->entities.world, &xform, type);
                    gs_dyn_array_push(room->mobs, e);
                }

                GUI_LABEL("level: %zu", bsf->run.level); 
                GUI_LABEL("num_rooms: %zu", gs_slot_array_size(bsf->run.rooms)); 
                GUI_LABEL("num_mobs: %zu", (u32)gs_dyn_array_size(room->mobs));
                GUI_LABEL("num_renderables: %zu", gs_slot_array_size(bsf->scene.renderables));
                GUI_LABEL("active cell: %zu", bsf->run.cell);
                GUI_LABEL("room cell: %zu", room->cell);

                gs_gui_layout_row(gui, 2, (int[]){150, -1}, 0); 

                gs_gui_label(gui, "time_scale: ");
                gs_gui_number(gui, &bsf->run.time_scale, 0.1f); 

                gs_gui_layout_row(gui, 1, (int[]){-1}, 0);
                if (gs_gui_button(gui, "gen")) 
                {
                    bsf_game_level_gen(bsf);
                }

                // Debug draw room
                uint32_t w = layout->body.w / BSF_ROOM_MAX_COLS * 0.8f;
                uint32_t h = layout->body.h * 0.1f;
                gs_gui_layout_row(gui, BSF_ROOM_MAX_COLS, (int[]){w, w, w, w, w, w, w, w, w}, h);
                for (uint32_t r = 0; r < BSF_ROOM_MAX_ROWS; ++r)
                    for (uint32_t c = 0; c < BSF_ROOM_MAX_COLS; ++c)
                {
                    struct {uint32_t k0; uint32_t k1;} hash = {.k0 = r, .k1 = c};
                    gs_gui_id id = gs_gui_get_id(gui, &hash, sizeof(hash));
                    gs_gui_rect_t rect = gs_gui_layout_next(gui);
                    gs_gui_update_control(gui, id, rect, 0x00);
                    const uint32_t cell = r * BSF_ROOM_MAX_COLS + c;
                    gs_color_t clr = gs_color(50, 50, 50, 255); 
                    bool filled = bsf_game_room_id_filled(bsf, cell);
                    if (filled)
                    { 
                        bsf_room_t* room = gs_slot_array_getp(bsf->run.rooms, bsf->run.room_ids[cell]);
                        switch (room->type)
                        { 
                            case BSF_ROOM_DEFAULT:  clr = GS_COLOR_WHITE; break;
                            case BSF_ROOM_START:    clr = GS_COLOR_GREEN; break; 
                            case BSF_ROOM_BOSS:     clr = GS_COLOR_RED; break;   
                            case BSF_ROOM_ITEM:     clr = GS_COLOR_YELLOW; break;   
                            case BSF_ROOM_SHOP:     clr = GS_COLOR_ORANGE; break;   
                            case BSF_ROOM_SECRET:   clr = GS_COLOR_PURPLE; break;   
                        }
                    }
                    gs_gui_draw_box(gui, rect, (int16_t[]){1, 1, 1, 1}, clr);
                    if (gui->hover == id && filled)
                    {
                        gs_gui_draw_box(gui, rect, (int16_t[]){3, 3, 3, 3}, GS_COLOR_YELLOW);
                        
                        if (gs_platform_mouse_pressed(GS_MOUSE_LBUTTON))
                        { 
                            // Load cell
                            bsf_room_load(bsf, cell);
                        } 
                    }
                }

                gs_gui_treenode_end(gui);
            } 

            if (gs_gui_treenode_begin(gui, "#player"))
            {
                bsf_component_character_stats_t* psc = ecs_get(bsf->entities.world, bsf->entities.player, bsf_component_character_stats_t);
                gs_gui_layout_row(gui, 1, (int[]){-1}, 0); 

                gs_gui_layout_row(gui, 2, (int[]){150, -1}, 0);

                GUI_LABEL("health: %zu", psc->health);
                GUI_LABEL("damage: %.2f", psc->damage);
                GUI_LABEL("range: %.2f", psc->damage);
                GUI_LABEL("luck: %.2f", psc->luck);

                gs_gui_label(gui, "pspeed: ");
                gs_gui_number(gui, &psc->speed, 0.1f); 

                gs_gui_label(gui, "fire_rate: " );
                gs_gui_number(gui, &psc->fire_rate, 0.1f); 

                gs_gui_label(gui, "shot_speed: " );
                gs_gui_number(gui, &psc->shot_speed, 0.1f); 

                gs_gui_label(gui, "shot_size: " );
                gs_gui_number(gui, &psc->shot_size, 0.1f); 

                gs_gui_treenode_end(gui);
            }
        }
        gs_gui_window_end(gui); 
    } 
}

//=== BSF Editor ===//

#define SENSITIVITY  0.2f
static gs_vqs gxform = {0};
static bsf_room_template_t room_template = {0}; 
static bsf_room_template_t* rt = NULL;
static uint32_t brush = 0;
static bool editor_init = false;
static rt_file[256] = {0};

GS_API_DECL void bsf_editor_start(struct bsf_t* bsf)
{
    gs_gui_context_t* gui = &bsf->gs.gui;

    bsf_camera_init(bsf, &bsf->scene.camera);

    bsf->state = BSF_STATE_EDITOR; 

    if (!rt)
    {
        rt = gs_hash_table_getp(bsf->assets.room_templates, gs_hash_str64("rt.r0")); 
    } 

    brush = UINT_MAX;

    if (!editor_init)
    {
        editor_init = true;
        gs_gui_dock_ex(gui, "Scene##editor", "Dockspace##editor", GS_GUI_SPLIT_BOTTOM, 1.f);
        gs_gui_dock_ex(gui, "Outliner##editor", "Scene##editor", GS_GUI_SPLIT_RIGHT, 0.3f); 
        gs_gui_dock_ex(gui, "Properties##editor", "Outliner##editor", GS_GUI_SPLIT_BOTTOM, 0.7f); 
        gs_gui_dock_ex(gui, "Debug##editor", "Properties##editor", GS_GUI_SPLIT_BOTTOM, 0.5f);
    }

    // Set default style sheet
    gs_gui_set_style_sheet(gui, NULL);
}

GS_API_DECL void bsf_editor_scene_cb(gs_gui_context_t* ctx, gs_gui_customcommand_t* cmd)
{
    bsf_t* bsf = gs_user_data(bsf_t); 
    gs_immediate_draw_t* gsi = &ctx->gsi;
    const float t = gs_platform_elapsed_time(); 
	const gs_gui_rect_t vp = cmd->viewport;
	const gs_vec2 fbs = gs_platform_framebuffer_sizev(gs_platform_main_window());
    // const bsf_room_template_t* rt = &room_template;

    // Draw room
	gsi_defaults(gsi);
    gsi_camera(gsi, &bsf->scene.camera.cam, (uint32_t)vp.w, (uint32_t)vp.h); 
    gs_graphics_set_viewport(&gsi->commands, vp.x, fbs.y - vp.h - vp.y, vp.w, vp.h); 
    gsi_depth_enabled(gsi, true);
    float sz = BSF_ROOM_DEPTH;
    float sx = BSF_ROOM_WIDTH;
    float sy = BSF_ROOM_HEIGHT;
    const gs_color_t room_col = gs_color_alpha(GS_COLOR_GREEN, 30);
    for (float r = -sz; r < sz; r += 1.f)
    {
        // Draw row
        gs_vec3 s = gs_v3(-sx, 0.f, r);
        gs_vec3 e = gs_v3(sx, 0.f, r);
        gsi_line3Dv(gsi, s, e, room_col);

        for (float c = -sx; c < sx; c += 1.f) {
            // Draw columns
            s = gs_v3(c, 0.f, -sz);
            e = gs_v3(c, 0.f, sz);
            gsi_line3Dv(gsi, s, e, room_col);
        }
    }
    gsi_box(gsi, 0.f, sy / 2.f, 0.f, sy + 1.f, sy / 2.f, sy + 1.f, 0, 255, 0, 255, GS_GRAPHICS_PRIMITIVE_LINES); 

    // Draw all brushes (probably just as billboards that can be selected)
    for (
        gs_slot_array_iter it = gs_slot_array_iter_new(rt->brushes);
        gs_slot_array_iter_valid(rt->brushes, it);
        gs_slot_array_iter_advance(rt->brushes, it)
    )
    {
        if (it == 0) continue;
        bsf_room_brush_t* br = gs_slot_array_iter_getp(rt->brushes, it);
        gsi_push_matrix(gsi, GSI_MATRIX_MODELVIEW);
        {
            gs_mat4 model = gs_vqs_to_mat4(&br->xform);
            gsi_mul_matrix(gsi, model); 

            gs_color_t col = GS_COLOR_WHITE;
            switch (br->type)
            {
                case BSF_ROOM_BRUSH_MOB: col   = GS_COLOR_RED; break;
                case BSF_ROOM_BRUSH_BANDIT: col   = GS_COLOR_RED; break;
                case BSF_ROOM_BRUSH_TURRET: col   = GS_COLOR_RED; break;
                case BSF_ROOM_BRUSH_PROP: col  = GS_COLOR_WHITE; break;
                case BSF_ROOM_BRUSH_CONSUMABLE: col  = GS_COLOR_ORANGE; break;
                case BSF_ROOM_BRUSH_OBSTACLE: col  = GS_COLOR_WHITE; break;
            }

            // Depending on type, do various shapes
            gsi_box(gsi, 0.f, 0.f, 0.f, 0.5f, 0.5f, 0.5, col.r, col.g, col.b, col.a, GS_GRAPHICS_PRIMITIVE_LINES);
        }
        gsi_pop_matrix(gsi);
    } 

    // Draw player position
    gsi_box(gsi, 0.f, 0.f, 0.f, 0.5f, 0.5f, 0.5, 0, 255, 0, 255, GS_GRAPHICS_PRIMITIVE_LINES);

    // Draw visible rail box
    // Debug bounding area
    gs_vec3 kb = {BSF_ROOM_BOUND_X, BSF_ROOM_BOUND_Y, BSF_ROOM_BOUND_Z}; 
    gsi_box(gsi, 0.f, kb.y / 2.f, 0.f, kb.x + 1.f, kb.y / 2.f, kb.z + 1.f, 0, 255, 0, 255, GS_GRAPHICS_PRIMITIVE_LINES);
}

GS_API_DECL int32_t gs_gui_combo(gs_gui_context_t* ctx, const char* label, int32_t* current_item, const char* items[], int32_t items_count, 
    int32_t popup_max_height_in_items)
{ 
    int32_t res = 0;
    int32_t opt = GS_GUI_OPT_NOMOVE | 
                  GS_GUI_OPT_NORESIZE | 
                  GS_GUI_OPT_NOTITLE | 
                  GS_GUI_OPT_FORCESETRECT; 

    if (gs_gui_button(ctx, label)) {
        gs_gui_popup_open(ctx, "#popup");
    }

    int32_t ct = popup_max_height_in_items > 0 ? popup_max_height_in_items : items_count;
    gs_gui_rect_t rect = ctx->last_rect;
    rect.y += rect.h;
    rect.h = (ct + 1) * ctx->style_sheet->styles[GS_GUI_ELEMENT_BUTTON][0x00].size[1];
    if (gs_gui_popup_begin_ex(ctx, "#popup", rect, NULL, opt))
    {
        gs_gui_container_t* cnt = gs_gui_get_current_container(ctx);
        gs_gui_layout_row(ctx, 1, (int[]){-1}, 0);
        for (int32_t i = 0; i < items_count; ++i) 
        {
            if (gs_gui_button(ctx, items[i])) {
                *current_item = i;
                res |= GS_GUI_RES_CHANGE;
                cnt->open = false;
            }
        }
        gs_gui_popup_end(ctx);
    }

    return res;
} 

GS_API_DECL void bsf_editor(struct bsf_t* bsf)
{
    gs_command_buffer_t* cb = &bsf->gs.cb;
    gs_immediate_draw_t* gsi = &bsf->gs.gsi;
    gs_gui_context_t* gui = &bsf->gs.gui;

	gs_vec2 fbs = gs_platform_framebuffer_sizev(gs_platform_main_window()); 
	const float t = gs_platform_elapsed_time() * 0.001f;

    if (gs_platform_mouse_down(GS_MOUSE_RBUTTON))
    {
        // Lock mouse at start by default
        gs_platform_lock_mouse(gs_platform_main_window(), true);
        bsf_editor_camera_update(bsf, &bsf->scene.camera);
    }
    else
    {
        // Lock mouse at start by default
        gs_platform_lock_mouse(gs_platform_main_window(), false);
    }

    static int32_t mode = GS_GUI_TRANSFORM_WORLD;
    if (gs_platform_key_pressed(GS_KEYCODE_P)) mode = 
        mode == GS_GUI_TRANSFORM_LOCAL ? GS_GUI_TRANSFORM_WORLD : 
            GS_GUI_TRANSFORM_LOCAL;

    static int32_t gop = GS_GUI_GIZMO_TRANSLATE; 
    if (gs_platform_key_pressed(GS_KEYCODE_T)) gop = GS_GUI_GIZMO_TRANSLATE;
    if (gs_platform_key_pressed(GS_KEYCODE_Y)) gop = GS_GUI_GIZMO_SCALE;
    if (gs_platform_key_pressed(GS_KEYCODE_R)) gop = GS_GUI_GIZMO_ROTATE;

    int32_t opt = 
        GS_GUI_OPT_NOCLIP | 
        GS_GUI_OPT_NOFRAME | 
        GS_GUI_OPT_FORCESETRECT | 
        GS_GUI_OPT_NOTITLE | 
        GS_GUI_OPT_DOCKSPACE | 
        GS_GUI_OPT_FULLSCREEN | 
        GS_GUI_OPT_NOMOVE | 
        GS_GUI_OPT_NOBRINGTOFRONT | 
        GS_GUI_OPT_NOFOCUS | 
        GS_GUI_OPT_NORESIZE;

    gs_gui_window_begin_ex(gui, "Dockspace##editor", gs_gui_rect(350, 40, 600, 500), NULL, NULL, opt);
    {
        // Empty dockspace...
    }
    gs_gui_window_end(gui);

	gs_gui_window_begin_ex(gui, "Outliner##editor", gs_gui_rect(350, 40, 300, 200), NULL, NULL, GS_GUI_OPT_NOSCROLLHORIZONTAL);
    {
        gs_gui_layout_row(gui, 1, (int[]){-1}, -1); 
        {
            gs_gui_layout_row(gui, 2, (int[]){10, -1}, 0); 
            bool quit = false;
            for (
                gs_slot_array_iter it = gs_slot_array_iter_new(rt->brushes);
                gs_slot_array_iter_valid(rt->brushes, it) && quit != true;
                gs_slot_array_iter_advance(rt->brushes, it)
            )
            {
                if (it == 0) continue;
                gs_snprintfc(TMP, 32, "%zu", it);
                bsf_room_brush_t* br = gs_slot_array_iter_getp(rt->brushes, it);
                gs_gui_push_id(gui, &it, sizeof(it));
                if (gs_gui_button(gui, "X")) {
                    gs_slot_array_erase(rt->brushes, it);
                    brush = 0;
                    quit = true;
                }
                gs_gui_pop_id(gui);
                if (!quit && gs_gui_button(gui, TMP)) {
                    brush = it;
                }
            }
        }
    }
	gs_gui_window_end(gui);

	gs_gui_window_begin_ex(gui, "Properties##editor", gs_gui_rect(0, 0, 300, 200), NULL, NULL, GS_GUI_OPT_NOSCROLLHORIZONTAL);
    {
        gs_gui_layout_row(gui, 1, (int[]){-1}, -1); 
        if (brush == UINT_MAX)
        {
            gs_gui_label(gui, "No brush selected...");
        }
        else
        {
            bsf_room_brush_t* br = gs_slot_array_getp(rt->brushes, brush);

            gs_gui_layout_t* layout = gs_gui_get_layout(gui);

            const char* types[] = {
                "Invalid", 
                "Mob", 
                "Consumable",
                "Prop",
                "Obstacle",
                "Bandit", 
                "Turret"
            };
            const ct = sizeof(types) / sizeof(const char*);
            gs_gui_layout_row(gui, 2, (int[]){100.f, -10}, 0); 
            gs_gui_label(gui, "Brush Type: ");
            gs_gui_combo(gui, types[br->type], (int32_t*)&br->type, types, ct, ct);

            gs_gui_layout_row(gui, 1, (int[]){-1}, 0); 
            if (gs_gui_header(gui, "Transform"))
            {
                layout = gs_gui_get_layout(gui);
                const float label_width = 100.f;
                float w = (layout->body.w - label_width) / 3.f - 10.f;
                gs_gui_layout_row(gui, 4, (int[]){label_width, w, w, w}, 50); 

                // Translation
                gs_gui_label(gui, "Translation: "); 
                gs_gui_number(gui, &br->xform.translation.x, 1.f); 
                gs_gui_number(gui, &br->xform.translation.y, 1.f); 
                gs_gui_number(gui, &br->xform.translation.z, 1.f); 

                // Scale
                gs_gui_label(gui, "Scale: "); 
                gs_gui_number(gui, &br->xform.scale.x, 1.f); 
                gs_gui_number(gui, &br->xform.scale.y, 1.f); 
                gs_gui_number(gui, &br->xform.scale.z, 1.f); 

                // Rotation
                w = (layout->body.w - label_width) / 4.f - 10.f;
                gs_gui_layout_row(gui, 5, (int[]){label_width, w, w, w, w}, 50); 
                gs_gui_label(gui, "Rotation: "); 
                gs_gui_number(gui, &br->xform.rotation.x, 1.f); 
                gs_gui_number(gui, &br->xform.rotation.y, 1.f); 
                gs_gui_number(gui, &br->xform.rotation.z, 1.f); 
                gs_gui_number(gui, &br->xform.rotation.w, 1.f); 

            } 
        }
    }
	gs_gui_window_end(gui);

	gs_gui_window_begin_ex(gui, "Debug##editor", gs_gui_rect(0, 0, 300.f, 200.f), NULL, NULL, GS_GUI_OPT_NOSCROLLHORIZONTAL);
    {
        gs_gui_layout_row(gui, 1, (int[]){-1}, 0); 

        static int32_t proj_type = 0x00;
        const char* items[] = {
            "orthographic", 
            "perspective"
        };
        const ct = sizeof(items) / sizeof(const char*);
        if (gs_gui_combo(gui, "Camera Projection", &proj_type, items, ct, ct))
        {
            switch (proj_type)
            {
                case 0: bsf->scene.camera.cam.proj_type = GS_PROJECTION_TYPE_ORTHOGRAPHIC; break;
                case 1: bsf->scene.camera.cam.proj_type = GS_PROJECTION_TYPE_PERSPECTIVE; break;
            }
        }

        // Duplicate brush
        if (brush != UINT32_MAX && gs_platform_key_down(GS_KEYCODE_LCTRL) && gs_platform_key_pressed(GS_KEYCODE_D))
        {
            bsf_room_brush_t* bp = gs_slot_array_getp(rt->brushes, brush);
            bsf_room_brush_t br = {0};
            br.type = bp->type;
            br.xform = bp->xform;
            brush = gs_slot_array_insert(rt->brushes, br);
        }

        if (gs_gui_button(gui, "Brush Create"))
        {
            bsf_room_brush_t br = {0}; 
            br.type = BSF_ROOM_BRUSH_MOB;
            br.xform = gs_vqs_default();
            br.xform.translation = gs_v3s(0.5f);
            brush = gs_slot_array_insert(rt->brushes, br);
        }

        // Ctx info
        if (gs_gui_header(gui, "GUI"))
        {
            char TMP[256] = {0};

            gs_snprintf(TMP, sizeof(TMP), "hover: %zu", gui->hover);
            gs_gui_label(gui, TMP);

            gs_snprintf(TMP, sizeof(TMP), "focus: %zu", gui->focus);
            gs_gui_label(gui, TMP);

            gs_snprintf(TMP, sizeof(TMP), "hover_root: %zu", gui->hover_root ? gui->hover_root->name : "NULL");
            gs_gui_label(gui, TMP);
        } 

        if (gs_gui_combo_begin_ex(gui, "##load_rt", rt->path, 5, NULL, 0x00))
        {
            gs_gui_layout_row(gui, 1, (int[]){-1}, 0); 
            for (
                gs_hash_table_iter it = gs_hash_table_iter_new(bsf->assets.room_templates); 
                gs_hash_table_iter_valid(bsf->assets.room_templates, it);
                gs_hash_table_iter_advance(bsf->assets.room_templates, it)
            )
            { 
                bsf_room_template_t* rtp = gs_hash_table_iter_getp(bsf->assets.room_templates, it);
                gs_snprintfc(TMP, 256, "%s##rt%zu", rtp->path, it);
                if (gs_gui_button(gui, TMP))
                {
                    rt = rtp;
					brush = UINT_MAX;
                    break;
                }
            } 

            gs_gui_combo_end(gui);
        } 

        // Save and sheeeiiiiit
        if (gs_gui_button(gui, "Save"))
        {
            gs_snprintfc(TMP, 256, "%s/%s", bsf->assets.asset_dir, "room_templates");
            if (!gs_platform_dir_exists(TMP)) {
                gs_platform_mkdir(TMP, 0x00);
            }

            gs_byte_buffer_t buffer = gs_byte_buffer_new();
            gs_byte_buffer_write(&buffer, uint16_t, (uint16_t)gs_slot_array_size(rt->brushes)); 
            for (
                gs_slot_array_iter it = gs_slot_array_iter_new(rt->brushes); 
                gs_slot_array_iter_valid(rt->brushes, it);
                gs_slot_array_iter_advance(rt->brushes, it)
            )
            { 
                bsf_room_brush_t* br = gs_slot_array_iter_getp(rt->brushes, it);
                gs_byte_buffer_write(&buffer, bsf_room_brush_t, *br);
            } 
            gs_snprintf(TMP, 256, "%s/%s", bsf->assets.asset_dir, rt->path);
            gs_byte_buffer_write_to_file(&buffer, TMP);
            gs_byte_buffer_free(&buffer);
        }

        if (gs_gui_button(gui, "Exit"))
        {
            bsf->state = BSF_STATE_MAIN_MENU;
            bsf_play_music(bsf, "audio.music_title");
        }
    }
	gs_gui_window_end(gui);

	gs_gui_window_begin_ex(gui, "Scene##editor", gs_gui_rect(0, 0, 500.f, 400.f), NULL, NULL, 0x00);
	{ 
        gs_gui_layout_t* layout = gs_gui_get_layout(gui);
        gs_gui_layout_row(gui, 1, (int[]){-1}, -1); 
        gs_gui_rect_t rect = gs_gui_layout_next(gui);
        gs_gui_layout_set_next(gui, rect, 0);
        gs_gui_container_t* cnt = gs_gui_get_current_container(gui);

        if (brush != UINT_MAX) {
            bsf_room_brush_t* bp = gs_slot_array_getp(rt->brushes, brush);
            float snap = (gop == GS_GUI_GIZMO_ROTATE) ? 15.f : 1.f;
            gs_gui_gizmo(gui, &bsf->scene.camera, &bp->xform, snap, gop, mode, GS_GUI_OPT_LEFTCLICKONLY);
        }

        // Scene
        gs_gui_draw_custom(gui, rect, bsf_editor_scene_cb, NULL, 0);

        if (gui->hover_root == gs_gui_get_current_container(gui))
        {
            gs_vec2 wheel = gs_platform_mouse_wheelv();
            bsf->scene.camera.cam.ortho_scale -= wheel.y;
        }

        if (gui->hover_root == cnt && gui->mouse_pressed && gui->mouse_down == GS_GUI_MOUSE_LEFT && !gui->focus)
        {
            brush = UINT_MAX;
        }

        // Select brush by clicking into scene or something...
        bool clicked = gui->mouse_pressed && gui->mouse_down == GS_GUI_MOUSE_LEFT; 

        // Transform mouse into viewport space
        gs_vec2 mc = gs_platform_mouse_positionv();
        mc = gs_vec2_sub(mc, gs_v2(rect.x, rect.y));

        // Project ray to world
        const float ray_len = 1000.f;
        gs_vec3 ms = gs_v3(mc.x, mc.y, 0.f); 
        gs_vec3 me = gs_v3(mc.x, mc.y, -ray_len);
        gs_vec3 ro = gs_camera_screen_to_world(&bsf->scene.camera, ms, 0, 0, (int32_t)rect.w, (int32_t)rect.h);
        gs_vec3 rd = gs_camera_screen_to_world(&bsf->scene.camera, me, 0, 0, (int32_t)rect.w, (int32_t)rect.h); 
        rd = gs_vec3_norm(gs_vec3_sub(ro, rd));
        gs_ray_t ray = gs_default_val();
        ray.p = ro;
        ray.d = rd;
        ray.len = ray_len;

        // Check for nan
        if (gs_vec3_nan(ray.p)) ray.p = gs_v3s(0.f);
        if (gs_vec3_nan(ray.d)) ray.d = gs_v3s(0.f);

        gs_aabb_t aabb = {
            .min = gs_v3s(-0.5f),
            .max = gs_v3s(0.5f)
        };

        float min_dist = FLT_MAX;
        for (
            gs_slot_array_iter it = gs_slot_array_iter_new(rt->brushes); 
            gs_slot_array_iter_valid(rt->brushes, it);
            gs_slot_array_iter_advance(rt->brushes, it)
        )
        {
            bsf_room_brush_t* br = gs_slot_array_iter_getp(rt->brushes, it);  
            gs_contact_info_t info = {0}; 
            gs_aabb_vs_ray(&aabb, &br->xform, &ray, NULL, &info);
            if (info.hit && clicked && !gui->hover && !gui->focus)
            {
                float dist = gs_vec3_dist(ray.p, br->xform.translation);
                if (dist < min_dist) {
                    brush = it;
                    min_dist = dist;
                }
            }
        }

        // Do overlay debug info
        {
            gs_gui_rect_t rect = gs_gui_layout_anchor(&cnt->body, 350, 32, 0, 0, GS_GUI_LAYOUT_ANCHOR_TOPLEFT);
            gs_gui_layout_set_next(gui, rect, 0);

            gs_gui_layout_row(gui, 1, (int[]){-1}, 0);
            gs_snprintfc(TMP, 512, "template: %s", rt->path);
            gs_gui_label(gui, TMP);
        }
        
	}
	gs_gui_window_end(gui);
}

GS_API_DECL void bsf_editor_camera_update(struct bsf_t* bsf, bsf_camera_t* bcamera)
{ 
    gs_platform_t* platform = gs_subsystem(platform);

    gs_vec2 dp = gs_vec2_scale(gs_platform_mouse_deltav(), SENSITIVITY);
    const float mod = gs_platform_key_down(GS_KEYCODE_LEFT_SHIFT) ? 2.f : 1.f; 
    float dt = platform->time.delta;
    float old_pitch = bcamera->pitch;
    gs_camera_t* camera = &bcamera->cam; 

    // Keep track of previous amount to clamp the camera's orientation
    bcamera->pitch = gs_clamp(old_pitch + dp.y, -90.f, 90.f);

    // Rotate camera
    gs_camera_offset_orientation(camera, -dp.x, old_pitch - bcamera->pitch);

    gs_vec3 vel = {0};
    switch (camera->proj_type)
    {
        case GS_PROJECTION_TYPE_ORTHOGRAPHIC:
        {
            if (gs_platform_key_down(GS_KEYCODE_W)) vel = gs_vec3_add(vel, gs_camera_up(camera));
            if (gs_platform_key_down(GS_KEYCODE_S)) vel = gs_vec3_add(vel, gs_vec3_scale(gs_camera_up(camera), -1.f));
            if (gs_platform_key_down(GS_KEYCODE_A)) vel = gs_vec3_add(vel, gs_camera_left(camera));
            if (gs_platform_key_down(GS_KEYCODE_D)) vel = gs_vec3_add(vel, gs_camera_right(camera)); 

            // Ortho scale
            gs_vec2 wheel = gs_platform_mouse_wheelv();
            camera->ortho_scale -= wheel.y;
        } break;

        case  GS_PROJECTION_TYPE_PERSPECTIVE:
        {
            if (gs_platform_key_down(GS_KEYCODE_W)) vel = gs_vec3_add(vel, gs_camera_forward(camera));
            if (gs_platform_key_down(GS_KEYCODE_S)) vel = gs_vec3_add(vel, gs_camera_backward(camera));
            if (gs_platform_key_down(GS_KEYCODE_A)) vel = gs_vec3_add(vel, gs_camera_left(camera));
            if (gs_platform_key_down(GS_KEYCODE_D)) vel = gs_vec3_add(vel, gs_camera_right(camera)); 
            gs_vec2 wheel = gs_platform_mouse_wheelv();
            bcamera->speed = gs_clamp(bcamera->speed + wheel.y, 0.01f, 50.f);
        } break;
    }

    camera->transform.position = gs_vec3_add(camera->transform.position, gs_vec3_scale(gs_vec3_norm(vel), dt * bcamera->speed * mod));
} 

//=== BSF Test ===//

// Want some test AI to run some sims with
// Make it so this simple AI will find a random location then wander to it, but if the camera(target) is 
// close enough, then the ai will chase. 
// If the target gets far enough or the AI's health gets lowered enough, it will then flee.
// Actions: 
//  * Move
//  * Chase
//  * Flee
//  * Heal
typedef struct
{
    float health;
    gs_vqs xform;
    gs_vqs target;
    gs_vqs player;
} bsf_test_ai_ctx_t;

enum
{
    BSF_TEST_AI_ACTION_MOVE = 0x00,
    BSF_TEST_AI_ACTION_FLEE, 
    BSF_TEST_AI_ACTION_CHASE,
    BSF_TEST_AI_ACTION_COUNT
};

#define BSF_TEST_WORLD_SIZE  50

static bsf_test_init = false;
static bsf_test_ai_ctx_t bsf_test_ai_ctx = {0}; 
static gs_ai_utility_action_desc_t s_actions[BSF_TEST_AI_ACTION_COUNT] = {0};

GS_API_DECL void bsf_test_scene_cb(gs_gui_context_t* ctx, gs_gui_customcommand_t* cmd) 
{
    bsf_t* bsf = gs_user_data(bsf_t); 
    gs_immediate_draw_t* gsi = &ctx->gsi;
    const float t = gs_platform_elapsed_time(); 
	const gs_gui_rect_t vp = cmd->viewport;
	const gs_vec2 fbs = gs_platform_framebuffer_sizev(gs_platform_main_window());

	gsi_defaults(gsi);
    gsi_camera(gsi, &bsf->scene.camera.cam, (uint32_t)vp.w, (uint32_t)vp.h); 
    gs_graphics_set_viewport(&gsi->commands, vp.x, fbs.y - vp.h - vp.y, vp.w, vp.h); 
    gsi_depth_enabled(gsi, true);

    gsi_push_matrix(gsi, GSI_MATRIX_MODELVIEW);
        gsi_mul_matrix(gsi, gs_vqs_to_mat4(&bsf_test_ai_ctx.xform));
        gsi_box(gsi, 0.f, 0.f, 0.f, 0.5f, 0.5f, 0.5f, 255, 255, 255, 255, GS_GRAPHICS_PRIMITIVE_LINES);
    gsi_pop_matrix(gsi);

    gsi_push_matrix(gsi, GSI_MATRIX_MODELVIEW);
        gsi_mul_matrix(gsi, gs_vqs_to_mat4(&bsf_test_ai_ctx.target));
        gsi_box(gsi, 0.f, 0.f, 0.f, 0.5f, 0.5f, 0.5f, 255, 255, 0, 255, GS_GRAPHICS_PRIMITIVE_LINES);
    gsi_pop_matrix(gsi);

    gsi_push_matrix(gsi, GSI_MATRIX_MODELVIEW);
        gsi_mul_matrix(gsi, gs_vqs_to_mat4(&bsf_test_ai_ctx.player));
        gsi_box(gsi, 0.f, 0.f, 0.f, 0.5f, 0.5f, 0.5f, 0, 255, 0, 255, GS_GRAPHICS_PRIMITIVE_LINES);
    gsi_pop_matrix(gsi);

    // Draw ground plane 
    gsi_rect3Dv(gsi, gs_v3(-BSF_TEST_WORLD_SIZE, 0.f, -BSF_TEST_WORLD_SIZE), 
            gs_v3(BSF_TEST_WORLD_SIZE, 0.f, BSF_TEST_WORLD_SIZE), gs_v2s(0.f), gs_v2s(1.f), gs_color(50, 50, 50, 255), GS_GRAPHICS_PRIMITIVE_TRIANGLES);
} 

void bsf_test_ai_move_action(struct bsf_t* bsf)
{
    // If dist to target within range, find new random target 
    float dist_to_target = gs_vec3_dist(bsf_test_ai_ctx.xform.translation, bsf_test_ai_ctx.target.translation); 
    if (dist_to_target <= 1.f)
    {
        gs_mt_rand_t rand = gs_rand_seed(time(NULL));
        bsf_test_ai_ctx.target.translation = gs_v3( 
            (float)gs_rand_gen_range(&rand, -BSF_TEST_WORLD_SIZE, BSF_TEST_WORLD_SIZE),
            0.5f,
            (float)gs_rand_gen_range(&rand, -BSF_TEST_WORLD_SIZE, BSF_TEST_WORLD_SIZE)
        );
    }

    // Move towards target
    gs_vec3 dir = gs_vec3_norm(gs_vec3_sub(bsf_test_ai_ctx.target.translation, bsf_test_ai_ctx.xform.translation));
    bsf_test_ai_ctx.xform.translation = gs_vec3_add(bsf_test_ai_ctx.xform.translation, gs_vec3_scale(dir, 0.01f));
}

void bsf_test_ai_flee_action(struct bsf_t* bsf)
{
    // Move quickly away from player
    gs_vec3 dir = gs_vec3_norm(gs_vec3_sub(bsf_test_ai_ctx.player.translation, bsf_test_ai_ctx.xform.translation));
    bsf_test_ai_ctx.xform.translation = gs_vec3_add(bsf_test_ai_ctx.xform.translation, gs_vec3_scale(dir, -0.08f));
}

void bsf_test_ai_chase_action(struct bsf_t* bsf)
{
    // If dist to target within range, find new random target 
    float dist_to_target = gs_vec3_dist(bsf_test_ai_ctx.xform.translation, bsf_test_ai_ctx.player.translation); 
    if (dist_to_target > 1.f)
    {
        // Move towards player
        gs_vec3 dir = gs_vec3_norm(gs_vec3_sub(bsf_test_ai_ctx.player.translation, bsf_test_ai_ctx.xform.translation));
        bsf_test_ai_ctx.xform.translation = gs_vec3_add(bsf_test_ai_ctx.xform.translation, gs_vec3_scale(dir, 0.05f));
    } 
}

void bsf_test_ai_heal_action(struct bsf_t* bsf)
{
    bsf_test_ai_ctx.health = gs_clamp(bsf_test_ai_ctx.health + 0.01f, 0.f, 100.f);
}

typedef void (* bsf_ai_action_func)(struct bsf_t*);

typedef struct
{
    const char* name;
    bsf_ai_action_func f;
    float score;
} bsf_ai_action_t; 

enum
{
    BSF_AI_ACTION_MOVE = 0x00,
    BSF_AI_ACTION_CHASE, 
    BSF_AI_ACTION_FLEE,
    BSF_AI_ACTION_HEAL,
    BSF_AI_ACTION_COUNT
};

int32_t bsf_ai_action_sort(const void* s0, const void* s1)
{
    return ((bsf_ai_action_t*)s0)->score < ((bsf_ai_action_t*)s1)->score;
} 

void bsf_test_ai_scene_update(struct bsf_t* bsf)
{
    bsf_ai_action_t actions[BSF_AI_ACTION_COUNT] = { 
        {.name = "move", .f = bsf_test_ai_move_action}, 
        {.name = "chase", .f = bsf_test_ai_chase_action}, 
        {.name = "flee", .f = bsf_test_ai_flee_action},
        {.name = "heal", .f = bsf_test_ai_heal_action}
    };

    float dist_to_player = gs_vec3_dist(bsf_test_ai_ctx.player.translation, bsf_test_ai_ctx.xform.translation);

    actions[BSF_AI_ACTION_MOVE].score = gs_ai_utility_action_evaluate(&(gs_ai_utility_action_desc_t){
       .considerations = (gs_ai_utility_consideration_desc_t[]){
            {
                .data = gs_vec3_dist(bsf_test_ai_ctx.player.translation, bsf_test_ai_ctx.xform.translation),
                .min = 0.f, 
                .max = 100.f,
                .curve = {
                    .func = gs_ai_curve_linearquad, 
                    .slope = 1.f,   // At 30 units distance
                    .exponent = 2.f,
                    .shift_x = -0.4f,
                    .shift_y = 0.f
                }
            },
            {
                .data = bsf_test_ai_ctx.health,
                .min = 0.f, 
                .max = 100.f,
                .curve = {
                    .func = gs_ai_curve_linearquad, 
                    .slope = 1.f,
                    .exponent = 8.f
                }
            }

       },
       .size = sizeof(gs_ai_utility_consideration_desc_t) * 2
    });

    actions[BSF_AI_ACTION_CHASE].score = gs_ai_utility_action_evaluate(&(gs_ai_utility_action_desc_t){
       .considerations = (gs_ai_utility_consideration_desc_t[]){
            {
                .data = dist_to_player,
                .min = 0.f, 
                .max = 100.f,
                .curve = {
                    .func = gs_ai_curve_linearquad, 
                    .slope = 1.f, 
                    .exponent = 2.f,
                    .shift_x = 1.01f
                }
            },
            {
                .data = bsf_test_ai_ctx.health,
                .min = 0.f, 
                .max = 100.f,
                .curve = {
                    .func = gs_ai_curve_linearquad, 
                    .slope = 1.f,
                    .exponent = 8.f
                }
            }
       },
       .size = sizeof(gs_ai_utility_consideration_desc_t) * 2
    }); 

    actions[BSF_AI_ACTION_FLEE].score = gs_ai_utility_action_evaluate(&(gs_ai_utility_action_desc_t){
       .considerations = (gs_ai_utility_consideration_desc_t[]){
            {
                .data = gs_vec3_dist(bsf_test_ai_ctx.player.translation, bsf_test_ai_ctx.xform.translation),
                .min = 0.f, 
                .max = 50.f,
                .curve = {
                    .func = gs_ai_curve_linearquad, 
                    .slope = 1.f,
                    .exponent = 2.f,
                    .shift_x = -1.1f
                }
            },
            {
                .data = bsf_test_ai_ctx.health,
                .min = 0.f, 
                .max = 100.f,
                .curve = {
                    .func = gs_ai_curve_logit, 
                    .slope = 0.4f,
                    .exponent = -2.5f,
                    .shift_x = -1.f,
                    .shift_y = -0.8f
                }
            }
       },
       .size = sizeof(gs_ai_utility_consideration_desc_t) * 2
    }); 

    actions[BSF_AI_ACTION_HEAL].score = gs_ai_utility_action_evaluate(&(gs_ai_utility_action_desc_t){
       .considerations = (gs_ai_utility_consideration_desc_t[]){
            {
                .data = gs_vec3_dist(bsf_test_ai_ctx.player.translation, bsf_test_ai_ctx.xform.translation),
                .min = 0.f, 
                .max = 100.f,
                .curve = {
                    .func = gs_ai_curve_linearquad, 
                    .slope = 1.f,
                    .exponent = 2.f,
                    .shift_x = 1.f
                }
            },
            {
                .data = bsf_test_ai_ctx.health,
                .min = 0.f, 
                .max = 100.f,
                .curve = {
                    .func = gs_ai_curve_linearquad, 
                    .slope = 1.f,
                    .exponent = 2.f,
                    .shift_x = 1.49f,
                    .shift_y = -0.1f
                }
            }
       },
       .size = sizeof(gs_ai_utility_consideration_desc_t) * 2
    }); 

    // Sort actions based on scores
    qsort(actions, BSF_AI_ACTION_COUNT, sizeof(bsf_ai_action_t), bsf_ai_action_sort); 

    // Call primary action
    actions[0].f(bsf);

    gs_timed_action(60, {
        gs_println("dtp: %.2f, %s: %.2f, %s: %.2f, %s: %.2f, %s: %.2f", dist_to_player, 
            actions[BSF_AI_ACTION_MOVE].name, actions[BSF_AI_ACTION_MOVE].score, 
            actions[BSF_AI_ACTION_CHASE].name, actions[BSF_AI_ACTION_CHASE].score, 
            actions[BSF_AI_ACTION_FLEE].name, actions[BSF_AI_ACTION_FLEE].score,
            actions[BSF_AI_ACTION_HEAL].name, actions[BSF_AI_ACTION_HEAL].score 
        );
    });
}


void bsf_bt_leaf_health_check(struct gs_ai_bt_t* ctx, struct gs_ai_bt_node_t* node)
{ 
    bsf_test_ai_ctx_t* data = (bsf_test_ai_ctx_t*)ctx->ctx.user_data;
    if (data->health <= 50) node->state = GS_AI_BT_STATE_SUCCESS; 
    else                    node->state = GS_AI_BT_STATE_FAILURE;
}

void bsf_bt_leaf_player_distance_check(struct gs_ai_bt_t* ctx, struct gs_ai_bt_node_t* node)
{
    bsf_test_ai_ctx_t* data = (bsf_test_ai_ctx_t*)ctx->ctx.user_data;
    float dist = gs_vec3_dist(data->xform.translation, data->player.translation);
    if (dist <= 40) node->state = GS_AI_BT_STATE_SUCCESS;
    else            node->state = GS_AI_BT_STATE_FAILURE;
}

void bsf_bt_leaf_player_distance_inv_check(struct gs_ai_bt_t* ctx, struct gs_ai_bt_node_t* node)
{
    bsf_test_ai_ctx_t* data = (bsf_test_ai_ctx_t*)ctx->ctx.user_data;
    float dist = gs_vec3_dist(data->xform.translation, data->player.translation);
    if (dist > 40) node->state = GS_AI_BT_STATE_SUCCESS;
    else           node->state = GS_AI_BT_STATE_FAILURE;
}

void bsf_bt_leaf_move_away_from_player(struct gs_ai_bt_t* ctx, struct gs_ai_bt_node_t* node)
{
    bsf_test_ai_ctx_t* data = (bsf_test_ai_ctx_t*)ctx->ctx.user_data;
    gs_vec3 dir = gs_vec3_norm(gs_vec3_sub(data->xform.translation, data->player.translation));
    data->xform.translation = gs_vec3_add(data->xform.translation, gs_vec3_scale(dir, 0.05f));
    float dist_to_target = gs_vec3_dist(data->xform.translation, data->player.translation); 
	if (dist_to_target <= 1.f) node->state = GS_AI_BT_STATE_SUCCESS;
    else					   node->state = GS_AI_BT_STATE_RUNNING;
} 

void bsf_bt_leaf_move_towards_player(struct gs_ai_bt_t* ctx, struct gs_ai_bt_node_t* node)
{
    bsf_test_ai_ctx_t* data = (bsf_test_ai_ctx_t*)ctx->ctx.user_data;
    gs_vec3 dir = gs_vec3_norm(gs_vec3_sub(data->xform.translation, data->player.translation));
    data->xform.translation = gs_vec3_add(data->xform.translation, gs_vec3_scale(dir, -0.03f));
    float dist_to_target = gs_vec3_dist(data->xform.translation, data->player.translation); 
	if (dist_to_target <= 1.f) node->state = GS_AI_BT_STATE_SUCCESS;
    else					   node->state = GS_AI_BT_STATE_RUNNING;
} 

void bsf_bt_leaf_find_target(struct gs_ai_bt_t* ctx, struct gs_ai_bt_node_t* node)
{
    bsf_test_ai_ctx_t* data = (bsf_test_ai_ctx_t*)ctx->ctx.user_data; 
    float dist_to_target = gs_vec3_dist(data->xform.translation, data->target.translation); 
    if (dist_to_target <= 1.f) {
        gs_mt_rand_t rand = gs_rand_seed(time(NULL));
        data->target.translation = gs_v3( 
            (float)gs_rand_gen_range(&rand, -BSF_TEST_WORLD_SIZE, BSF_TEST_WORLD_SIZE),
            0.5f,
            (float)gs_rand_gen_range(&rand, -BSF_TEST_WORLD_SIZE, BSF_TEST_WORLD_SIZE)
        );
    } 
	node->state = GS_AI_BT_STATE_SUCCESS;
}

void bsf_bt_leaf_move_towards_target(struct gs_ai_bt_t* ctx, struct gs_ai_bt_node_t* node)
{
	const float dt = gs_platform_time()->delta;
    bsf_test_ai_ctx_t* data = (bsf_test_ai_ctx_t*)ctx->ctx.user_data; 
    float dist_to_target = gs_vec3_dist(data->xform.translation, data->target.translation); 
    if (dist_to_target > 1.f) {
		gs_vec3 dir = gs_vec3_norm(gs_vec3_sub(data->target.translation, data->xform.translation));
		gs_vec3 np = gs_vec3_add(data->xform.translation, gs_vec3_scale(dir, 10.f * dt));
		data->xform.translation = gs_v3(
			gs_interp_linear(data->xform.translation.x, np.x, 0.4f),
			gs_interp_linear(data->xform.translation.y, np.y, 0.4f),
			gs_interp_linear(data->xform.translation.z, np.z, 0.4f) 
		);
		node->state = GS_AI_BT_STATE_RUNNING;
    } 
	else node->state = GS_AI_BT_STATE_SUCCESS;
} 

void simulate_bt(gs_ai_bt_t* ctx)
{
	static uint32_t frame = 0;
	frame++;
	gs_ai_bt_begin(ctx);
    {
        if (gs_ai_bt_repeater_begin(ctx, NULL)) 
        {
            if (gs_ai_bt_parallel_begin(ctx))
            {
                // Distance check
                if (gs_ai_bt_sequence_begin(ctx))
                {
                    gs_ai_bt_leaf(ctx, bsf_bt_leaf_player_distance_inv_check); // Need to invert this
                    gs_ai_bt_sequence_end(ctx);
                }

                // Select action
                if (gs_ai_bt_selector_begin(ctx))
                { 
                    // Flee
                    if (gs_ai_bt_sequence_begin(ctx))
                    {
                        gs_ai_bt_leaf(ctx, bsf_bt_leaf_health_check);
                        gs_ai_bt_leaf(ctx, bsf_bt_leaf_player_distance_check);
                        gs_ai_bt_leaf(ctx, bsf_bt_leaf_move_away_from_player);
                        gs_ai_bt_sequence_end(ctx);
                    }

                    // Chase
                    if (gs_ai_bt_sequence_begin(ctx))
                    {
                        gs_ai_bt_leaf(ctx, bsf_bt_leaf_player_distance_check);
                        gs_ai_bt_leaf(ctx, bsf_bt_leaf_move_towards_player);
                        gs_ai_bt_sequence_end(ctx);
                    }

                    // Move to Target
                    if (gs_ai_bt_sequence_begin(ctx))
                    {
                        gs_ai_bt_leaf(ctx, bsf_bt_leaf_find_target);
                        gs_ai_bt_leaf(ctx, bsf_bt_leaf_move_towards_target);
                        gs_ai_bt_sequence_end(ctx);
                    }

                    gs_ai_bt_selector_end(ctx);
                }
                gs_ai_bt_parallel_end(ctx);
            }
            gs_ai_bt_repeater_end(ctx);
        }
    }
    gs_ai_bt_end(ctx);
}

GS_API_DECL void bsf_test(struct bsf_t* bsf)
{ 
    gs_command_buffer_t* cb = &bsf->gs.cb;
    gs_immediate_draw_t* gsi = &bsf->gs.gsi;
    gs_gui_context_t* gui = &bsf->gs.gui;

	gs_vec2 fbs = gs_platform_framebuffer_sizev(gs_platform_main_window()); 
    const gs_platform_time_t* time = gs_platform_time();
	const float t = time->elapsed * 0.001f;
    const float dt = time->delta;
    const float frame = time->frame; 

    static gs_ai_bt_t bt = gs_default_val(); 

    // Init if needed
    if (!bsf_test_init)
    { 
        bsf_test_init = true;
        gs_gui_dock_ex(gui, "Scene", "Dockspace", GS_GUI_SPLIT_BOTTOM, 1.f);
        gs_gui_dock_ex(gui, "Options", "Scene", GS_GUI_SPLIT_RIGHT, 0.3f); 
		bsf_camera_init(bsf, &bsf->scene.camera);
        bsf->scene.camera.cam.transform.position = (gs_vec3){-0.59f, 2.24f, 2.31f};
        bsf->scene.camera.cam.transform.rotation = (gs_quat){-0.17f, -0.07f, -0.01f, 0.98f};
        bsf_test_ai_ctx = (bsf_test_ai_ctx_t){
            .health = 100.f,
            .xform = (gs_vqs){
                .translation = gs_v3(0.f, 0.5f, 0.f),
                .rotation = gs_quat_default(),
                .scale = gs_v3s(1.f)
            },
            .target = (gs_vqs){
                .translation = gs_v3(BSF_TEST_WORLD_SIZE * 0.2f, 0.5f, BSF_TEST_WORLD_SIZE * 0.3f),
                .rotation = gs_quat_default(), 
                .scale = gs_v3s(1.f)
            },
            .player = (gs_vqs) {
                .translation = gs_v3(BSF_TEST_WORLD_SIZE * 0.2f, 0.5f, -BSF_TEST_WORLD_SIZE * 0.8f),
                .rotation = gs_quat_default(), 
                .scale = gs_v3s(1.f)
            }
        }; 
    }


    if (gs_platform_mouse_down(GS_MOUSE_RBUTTON))
    {
        // Lock mouse at start by default
        gs_platform_lock_mouse(gs_platform_main_window(), true);
        bsf_editor_camera_update(bsf, &bsf->scene.camera);
    }
    else
    {
        // Lock mouse at start by default
        gs_platform_lock_mouse(gs_platform_main_window(), false);
    } 

    // Update ai
    // bsf_test_ai_scene_update(bsf); 

    bt.ctx.user_data = &bsf_test_ai_ctx;
	simulate_bt(&bt);

    // Push default style
    gs_gui_set_style_sheet(gui, NULL); 

    int32_t opt = 
        GS_GUI_OPT_NOCLIP | 
        GS_GUI_OPT_NOFRAME | 
        GS_GUI_OPT_FORCESETRECT | 
        GS_GUI_OPT_NOTITLE | 
        GS_GUI_OPT_DOCKSPACE | 
        GS_GUI_OPT_FULLSCREEN | 
        GS_GUI_OPT_NOMOVE | 
        GS_GUI_OPT_NOBRINGTOFRONT | 
        GS_GUI_OPT_NOFOCUS | 
        GS_GUI_OPT_NORESIZE;

    gs_gui_window_begin_ex(gui, "Dockspace", gs_gui_rect(350, 40, 600, 500), NULL, NULL, opt);
    {
        // Empty dockspace...
    }
    gs_gui_window_end(gui);

    // Want a group of actions that can be possible, with their utility values, and then be able to adjust 
    // these curves

    gs_gui_window_begin_ex(gui, "Options", gs_gui_rect(350, 40, 600, 500), NULL, NULL, GS_GUI_OPT_NOSCROLLHORIZONTAL);
    {
        if (gs_gui_button(gui, "Exit"))
        {
            bsf->state = BSF_STATE_MAIN_MENU;
            bsf_test_init = false; 
            bsf_play_music(bsf, "audio.music_title");
        } 

		// Curve parameters
		static struct {float m; float k; float c; float b;} params = {0};
        static uint32_t cf = 0; 

		// Curve selections 
        struct {const char* k; gs_ai_curve_func f;} curves[] = {
            {"Linear/Quad", gs_ai_curve_linearquad},
            {"Sin", gs_ai_curve_sin},
            {"Cos", gs_ai_curve_cos},
            {"Logistic", gs_ai_curve_logistic},
            {"Logit", gs_ai_curve_logit},
            {NULL}
        };

        // Try to draw a custom viewer for action response curves  
		gs_gui_layout_t* layout = gs_gui_get_layout(gui);
		float wh = gs_min(layout->body.w * 0.7f, layout->body.h * 0.7f);
        gs_gui_layout_row(gui, 1, (int[]){-1}, wh);
        gs_gui_panel_begin_ex(gui, "##curves", NULL, GS_GUI_OPT_NOSCROLL);
        { 
			layout = gs_gui_get_layout(gui);
            gs_gui_layout_row(gui, 1, (int[]){-1}, -1); 
            gs_gui_rect_t origin = gs_gui_layout_next(gui);

            // Graph
            uint32_t rows = 10, cols = 10;
			float margin = 20.f;
			float hm = margin * 0.5f;
            float rstep = (origin.h - margin) / (float)rows;
            float cstep = (origin.w - margin) / (float)cols;
            for (uint32_t r = 0; r <= rows; ++r)
            {
                gs_vec2 s = gs_v2(origin.x + hm, origin.y + hm + r * rstep);
                gs_vec2 e = gs_vec2_add(s, gs_v2(origin.w - margin, 0.f));
                gs_gui_draw_line(gui, s, e, GS_COLOR_WHITE);

                for (uint32_t c = 0; c <= cols; ++c)
                {
                    s = gs_v2(origin.x + hm + c * cstep, origin.y + hm);
                    e = gs_vec2_add(s, gs_v2(0.f, origin.h - margin));
                    gs_gui_draw_line(gui, s, e, GS_COLOR_WHITE);
                } 
            } 

            gs_gui_rect_t clip = gs_gui_rect(layout->body.x + hm, layout->body.y + hm, layout->body.w - margin, layout->body.h - margin);
            gs_gui_push_clip_rect(gui, clip);
            gs_gui_set_clip(gui, clip);

            float xstep = 0.005f;
			float w = layout->body.w - margin;
			float h = layout->body.h - margin;
            gs_vec2 o = gs_v2(origin.x + hm, origin.y + h + hm);
            for (float x = -2.f; x <= 2.f; x += xstep)
            {
				float x0 = x;
				float x1 = x + xstep;
                float y0 = curves[cf].f(params.m, params.k, params.c, params.b, x0);
                float y1 = curves[cf].f(params.m, params.k, params.c, params.b, x1); 
                gs_vec2 s = gs_v2(o.x + x * w, o.y - y0 * h);
                gs_vec2 e = gs_v2(o.x + (x + xstep) * w, o.y - y1 * h);
                gs_gui_draw_line(gui, s, e, GS_COLOR_RED);
            }

            gs_gui_pop_clip_rect(gui);
            gs_gui_set_clip(gui, gs_gui_unclipped_rect);
        }
        gs_gui_panel_end(gui); 

		layout = gs_gui_get_layout(gui);

		// Parameters: m, k, c, b
		struct {const char* k; float* v;} parameters[] = {
			{"m: ", &params.m}, 
			{"k: ", &params.k},
			{"c: ", &params.c},
			{"b: ", &params.b},
			{NULL}
		};
		int32_t lw = layout->body.w * 0.05f;
		int32_t sw = (int32_t)((layout->body.w / 4.f) - lw);
		gs_gui_layout_row(gui, 2 * 4, (int[]){lw, sw, lw, sw, lw, sw, lw, sw}, 0); 
		for (uint32_t p = 0; parameters[p].k; ++p)
		{
			gs_gui_label(gui, parameters[p].k);
			gs_gui_number(gui, parameters[p].v, 0.01f);
		}

        gs_gui_layout_row(gui, 1, (int[]){-1}, 0);
        if (gs_gui_combo_begin_ex(gui, "##curves", curves[cf].k, 5, NULL, 0x00))
        {
            gs_gui_layout_row(gui, 1, (int[]){-1}, 0); 
            for (uint32_t c = 0; curves[c].k; ++c)
            {
                if (gs_gui_button(gui, curves[c].k)) {
                    cf = c;
                } 
            }

            gs_gui_combo_end(gui);
        } 

		gs_gui_layout_row(gui, 2, (int[]){50, 150}, 0); 
        gs_gui_label(gui, "health: ");
        gs_gui_number(gui, &bsf_test_ai_ctx.health, 0.1f);

        static int32_t checked = 0;
        gs_gui_layout_row(gui, 1, (int[]){-1}, 0);
        gs_gui_checkbox(gui, "blah", &checked); 

        gs_gui_label(gui, "frame: %.3fms", time->frame);

    }
    gs_gui_window_end(gui);

    gs_gui_window_begin(gui, "Scene", gs_gui_rect(350, 40, 600, 500));
    {
        // Set layout for full windowed rect
        gs_gui_layout_row(gui, 1, (int[]){-1}, -1); 
        gs_gui_rect_t rect = gs_gui_layout_next(gui);
        gs_gui_layout_set_next(gui, rect, 0);

        // Scene
         gs_gui_draw_custom(gui, rect, bsf_test_scene_cb, NULL, 0);

        gs_gui_gizmo(gui, &bsf->scene.camera, &bsf_test_ai_ctx.player, 0.f, 0x00, 0x00, GS_GUI_OPT_LEFTCLICKONLY);
    }
    gs_gui_window_end(gui);
}
















































