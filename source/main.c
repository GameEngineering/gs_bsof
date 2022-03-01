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

#include "flecs/flecs.h" 

// Defines
#define BSF_SEED_MAX_LEN    (8 + 1)
#define BSF_ROOM_MAX_COLS   9
#define BSF_ROOM_MAX_ROWS   8
#define BSF_ROOM_MAX        (BSF_ROOM_MAX_ROWS * BSF_ROOM_MAX_COLS)

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

typedef enum
{
    BSF_SHAPE_BOX = 0x00,
    BSF_SHAPE_SPHERE, 
    BSF_SHAPE_CONE,
    BSF_SHAPE_CROSS,
    BSF_SHAPE_CYLINDER
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

typedef struct
{
    bsf_shape_type shape;
    gs_gfxt_material_t* material;
    gs_mat4 model; 
    gs_color_t color;
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

typedef struct
{
    uint32_t health;    
    float speed;            
    float fire_rate;
    float damage;
    float range;
    float shot_speed;
    float luck;
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

typedef enum 
{
    BSF_MOB_BOX = 0x00,
    BSF_MOB_SPHERE, 
    BSF_MOB_CONE
} bsf_mob_type;

typedef struct 
{
    bsf_mob_type type;
} bsf_component_mob_t; 

GS_API_DECL ecs_entity_t bsf_mob_create(struct bsf_t* bsf, gs_vqs* xform, bsf_mob_type type);
GS_API_DECL void bsf_mob_system(ecs_iter_t* it); 

typedef enum
{
    BSF_ITEM_HEALTH = 0x00,
    BSF_ITEM_BOMB,
    BSF_ITEM_COUNT
} bsf_item_type; 

typedef struct
{
    bsf_item_type type;
    gs_vqs origin;
    float time_scale[3];
} bsf_component_item_t;

GS_API_DECL ecs_entity_t bsf_item_create(struct bsf_t* bsf, gs_vqs* xform, bsf_item_type type);
GS_API_DECL void bsf_item_system(ecs_iter_t* it);

typedef struct 
{
    int16_t bombs;
} bsf_component_inventory_t;

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
ECS_COMPONENT_DECLARE(bsf_component_item_t);
ECS_COMPONENT_DECLARE(bsf_component_camera_track_t);
ECS_COMPONENT_DECLARE(bsf_component_barrel_roll_t); 
ECS_COMPONENT_DECLARE(bsf_component_explosion_t);
ECS_COMPONENT_DECLARE(bsf_component_inventory_t);


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

typedef struct
{
    int16_t firing;
    float time;
} bsf_component_gun_t;

GS_API_DECL void bsf_player_init(struct bsf_t* bsf);
GS_API_DECL void bsf_player_system(ecs_iter_t* iter);
GS_API_DECL void bsf_player_damage(struct bsf_t* bsf, ecs_world_t* world, float damage); 
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
    BSF_STATE_QUIT              // Quit the game
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

//=== BSF Room ===// 

#define BSF_ROOM_WIDTH  30.f
#define BSF_ROOM_HEIGHT 50.f
#define BSF_ROOM_DEPTH  30.f

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
    b32 clear;                          // Whether or not this room is clear
    int16_t movement_type;
} bsf_room_t; 

GS_API_DECL void bsf_room_load(struct bsf_t* bsf, uint32_t cell);

//=== BSF Room Template ===//

typedef enum 
{
    BSF_ROOM_BRUSH_INVALID =  0x00,
    BSF_ROOM_BRUSH_MOB,
    BSF_ROOM_BRUSH_ITEM,
    BSF_ROOM_BRUSH_PROP,
} bsf_room_brush_type;

typedef struct
{
    gs_vqs xform;
    bsf_room_brush_type type;
} bsf_room_brush_t;

typedef struct 
{
    gs_slot_array(bsf_room_brush_t) brushes;
} bsf_room_template_t;

//=== BSF Assets ===// 

typedef struct {
    const char* asset_dir;
    gs_hash_table(uint64_t, gs_gfxt_pipeline_t)   pipelines;
    gs_hash_table(uint64_t, gs_gfxt_texture_t)    textures;
    gs_hash_table(uint64_t, gs_gfxt_material_t)   materials;
    gs_hash_table(uint64_t, gs_gfxt_mesh_t)       meshes;
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
        gs_mt_rand_t rand;
        float time_scale;
    } run;

    struct {
        ecs_entity_t player;    // Main player
        ecs_world_t* world;     // Main flecs entity world
    } entities;

    int16_t dbg;

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

    gs_hash_table(uint64_t, float) table = NULL;
    gs_hash_table_reserve(table, uint64_t, float, 81920);
}

void bsf_update()
{
    bsf_t* bsf = gs_user_data(bsf_t); 

    gs_gui_begin(&bsf->gs.gui);

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
        } break;

        case BSF_STATE_QUIT:
        {
            gs_platform_lock_mouse(gs_platform_main_window(), false);
            gs_quit();
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
		.init = bsf_init,
		.update = bsf_update,
        .shutdown = bsf_shutdown
	};
}

//=== BSF Assets ===//

GS_API_DECL void bsf_assets_init(bsf_t* bsf, bsf_assets_t* assets)
{
    assets->asset_dir = gs_platform_dir_exists("./assets") ? "./assets" : "../assets";

    struct {const char* key; const char* path;} pipelines[] = {
        {.key = "pip.simple", .path = "pipelines/simple.sf"},
        {.key = "pip.color", .path = "pipelines/color.sf"},
        {.key = "pip.gsi", .path = "pipelines/gsi.sf"},
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
        {.key = "mesh.laser_player", .path = "meshes/laser_player.gltf", .pip = "pip.color"}, 
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

    struct {const char* key; const char* path;} textures[] = {
        {.key = "tex.slave", .path = "textures/slave.png"}, 
        {.key = "tex.arwing", .path = "textures/arwing.png"}, 
        {NULL}
    };

    for (uint32_t i = 0; textures[i].key; ++i)
    { 
        gs_snprintfc(TMP, 256, "%s/%s", assets->asset_dir, textures[i].path);
        gs_hash_table_insert(assets->textures, gs_hash_str64(textures[i].key), gs_gfxt_texture_load_from_file(TMP, NULL, false, false));
    }

    struct {const char* key; const char* pip;} materials[] = {
        {.key = "mat.simple", .pip = "pip.simple"},
        {.key = "mat.color", .pip = "pip.color"},
        {.key = "mat.gsi", .pip = "pip.gsi"},
        {NULL}
    };

    for (uint32_t i = 0; materials[i].key; ++i)
    {
        gs_gfxt_pipeline_t* pip = gs_hash_table_getp(assets->pipelines, gs_hash_str64(materials[i].pip));
        gs_hash_table_insert(assets->materials, gs_hash_str64(materials[i].key), gs_gfxt_material_create(&(gs_gfxt_material_desc_t){.pip_func.hndl = pip}));
    } 

    struct {const char* key; const char* mat;} material_instances[] = {
        {.key = "mat.ship_arwing", .mat = "mat.simple"},
        {.key = "mat.laser_player", .mat = "mat.color"},
        {NULL}
    };

    for (uint32_t i = 0; material_instances[i].key; ++i)
    {
        gs_gfxt_material_t* mat = gs_hash_table_getp(assets->materials, gs_hash_str64(material_instances[i].mat));
        gs_gfxt_material_t inst = gs_gfxt_material_deep_copy(mat);
        gs_hash_table_insert(assets->materials, gs_hash_str64(material_instances[i].key), inst);
    } 
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
    gs_graphics_begin_render_pass(cb, (gs_renderpass){0}); 
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
            } break;
        }

        // Render all gsi
        gsi_render_pass_submit_ex(gsi, cb, NULL);

        // Render all gui
        gs_gui_render(gui, cb);
    } 
    gs_graphics_end_render_pass(cb);

	// Submit command buffer for GPU
	gs_graphics_submit_command_buffer(cb);
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
    ECS_REGISTER_COMP(bsf_component_item_t); 
    ECS_REGISTER_COMP(bsf_component_gun_t); 
    ECS_REGISTER_COMP(bsf_component_character_stats_t);
    ECS_REGISTER_COMP(bsf_component_camera_track_t);
    ECS_REGISTER_COMP(bsf_component_barrel_roll_t);
    ECS_REGISTER_COMP(bsf_component_explosion_t);
    ECS_REGISTER_COMP(bsf_component_inventory_t);

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
        bsf_component_renderable_immediate_t, 
        bsf_component_transform_t,
        bsf_component_physics_t, 
        bsf_component_health_t, 
        bsf_component_mob_t,
        bsf_component_gun_t
    ); 

    ECS_SYSTEM(
        bsf->entities.world, 
        bsf_item_system, 
        EcsOnUpdate,
        bsf_component_transform_t, 
        bsf_component_physics_t, 
        bsf_component_item_t
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
}

//=== BSF Components ===// 

GS_API_DECL void bsf_component_renderable_dtor(ecs_world_t* world, ecs_entity_t comp, const ecs_entity_t* ent, void* ptr, size_t sz, int32_t count, void* ctx)
{
    bsf_t* bsf = gs_user_data(bsf_t);
    bsf_component_renderable_t* rend = ecs_get(world, *ent, bsf_component_renderable_t);
    bsf_graphics_scene_renderable_destroy(&bsf->scene, rend->hndl);
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

    bsf_camera_shake(bsf, &bsf->scene.camera, 0.5f);

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
					gs_contact_info_t res = bsf_component_physics_collide(pc, &tc->xform, mpc, &mtc->xform); 

					if (res.hit)
					{ 
						bsf_component_health_t* h = ecs_get(bsf->entities.world, mob, bsf_component_health_t);
						h->hit = true; 
						h->health -= 0.4f;
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

    // Draw ground plane (need triangle mesh for this to be generated for world)
    float sz = BSF_ROOM_DEPTH;
    float sx = BSF_ROOM_WIDTH;
    float sy = BSF_ROOM_HEIGHT;
    for (float r = -sz; r < sz; r += 1.f)
    {
        // Draw row
        gs_vec3 s = gs_v3(-sx, 0.f, r);
        gs_vec3 e = gs_v3(sx, 0.f, r);
        gsi_line3Dv(gsi, s, e, gs_color_alpha(GS_COLOR_GREEN, 10));

        for (float c = -sx; c < sx; c += 1.f)
        {
            // Draw columns
            s = gs_v3(c, 0.f, -sz);
            e = gs_v3(c, 0.f, sz);
			gsi_line3Dv(gsi, s, e, gs_color_alpha(GS_COLOR_GREEN, 10));
        }
    }
    gsi_box(gsi, 0.f, sy / 2.f, 0.f, sy + 1.f, sy / 2.f, sy + 1.f, 0, 255, 0, 255, GS_GRAPHICS_PRIMITIVE_LINES);
} 

GS_API_DECL void bsf_renderable_immediate_system(ecs_iter_t* it)
{
    bsf_t* bsf = gs_user_data(bsf_t); 
    gs_immediate_draw_t* gsi = &bsf->gs.gsi;
    const float t = gs_platform_elapsed_time();
    const float dt = gs_platform_delta_time(); 
    const gs_vec2 fbs = gs_platform_framebuffer_sizev(gs_platform_main_window());
    bsf_component_renderable_immediate_t* rc = ecs_term(it, bsf_component_renderable_immediate_t, 1);
    bsf_component_transform_t* tc = ecs_term(it, bsf_component_transform_t, 2);

    gsi_camera3D(gsi, (uint32_t)fbs.x, (uint32_t)fbs.y);
    for (uint32_t i = 0; i < it->count; ++i)
    { 
        rc[i].model = gs_vqs_to_mat4(&tc[i].xform); 
        const gs_color_t* col = &rc[i].color;
        gs_gfxt_pipeline_t* pip = gs_gfxt_material_get_pipeline(rc[i].material);
        gs_mat4 mvp = {0};
        gsi_push_matrix(gsi, GSI_MATRIX_MODELVIEW);
        {
            gsi_vattr_list_mesh(gsi, pip->mesh_layout, gs_dyn_array_size(pip->mesh_layout) * sizeof(gs_gfxt_mesh_layout_t)); 
            gsi_load_matrix(gsi, rc[i].model);
            switch (rc[i].shape)
            {
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
        gsi_pop_matrix_ex(gsi, false); 
		gs_mat4 vp = gs_camera_get_view_projection(&bsf->scene.camera.cam, fbs.x, fbs.y);
		mvp = gs_mat4_mul(vp, rc[i].model);
        gs_gfxt_material_set_uniform(rc[i].material, "u_mvp", &mvp);
        gs_gfxt_material_bind(&gsi->commands, rc[i].material);
        gs_gfxt_material_bind_uniforms(&gsi->commands, rc[i].material);
        gsi_flush(gsi);
    }

    /*
    // Draw ground plane (need triangle mesh for this to be generated for world)
    float sz = 25.f;
    for (float r = -sz; r < sz; r += 1.f)
    {
        // Draw row
        gs_vec3 s = gs_v3(-sz, 0.f, r);
        gs_vec3 e = gs_v3(sz, 0.f, r);
        gsi_line3Dv(gsi, s, e, gs_color_alpha(GS_COLOR_GREEN, 10));

        for (float c = -sz; c < sz; c += 1.f)
        {
            // Draw columns
            s = gs_v3(c, 0.f, -sz);
            e = gs_v3(c, 0.f, sz);
			gsi_line3Dv(gsi, s, e, gs_color_alpha(GS_COLOR_GREEN, 10));
        }
    }
    gs_gfxt_pipeline_t* pip = gs_gfxt_material_get_pipeline(rc->material);
    gsi_box(gsi, 0.f, sz / 2.f, 0.f, sz + 1.f, sz / 2.f, sz + 1.f, 0, 255, 0, 255, pip->desc.raster.primitive);
    gs_mat4 vp = gs_camera_get_view_projection(&bsf->scene.camera, fbs.x, fbs.y);
    gs_gfxt_material_set_uniform(rc->material, "u_mvp", &vp);
    gs_gfxt_material_bind(&gsi->commands, rc->material);
    gs_gfxt_material_bind_uniforms(&gsi->commands, rc->material);
    gsi_flush(gsi);
    */
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
}

GS_API_DECL void bsf_camera_update(struct bsf_t* bsf, bsf_camera_t* camera) 
{
    const float dt = gs_platform_delta_time() * bsf->run.time_scale;
    const bsf_component_camera_track_t* ptc = ecs_get(bsf->entities.world, bsf->entities.player, bsf_component_camera_track_t);
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
            const float zlerp = 0.1f;
            const float rlerp = 0.05f;

            c->transform.rotation = gs_quat_slerp(c->transform.rotation, ptc->xform.rotation, rlerp);

            // Lerp position to new position
            c->transform.position = gs_v3(
                gs_interp_linear(c->transform.position.x, np.x, plerp),
                gs_interp_linear(c->transform.position.y, np.y, plerp),
                gs_interp_linear(c->transform.position.z, np.z, plerp)
            ); 
            c->transform.position = gs_vec3_add(c->transform.position, gs_quat_rotate(c->transform.rotation, so));

        } break;

        case BSF_MOVEMENT_RAIL:
        {
            // Update camera position to follow target
            gs_vec3 backward = gs_quat_rotate(ptc->xform.rotation, GS_ZAXIS);
            gs_vec3 target = gs_vec3_add(ptc->xform.position, gs_v3(0.f, 0.f, 1.f));
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
            
            c->transform.rotation = gs_quat_angle_axis(gs_deg2rad(target.x), GS_ZAXIS);
        } break;
    } 
}

GS_API_DECL void bsf_camera_shake(struct bsf_t* bsf, bsf_camera_t* camera, float amt)
{
    camera->shake_time += amt;
}

//=== BSF Item ===//

GS_API_DECL ecs_entity_t bsf_item_create(struct bsf_t* bsf, gs_vqs* xform, bsf_item_type type)
{
    gs_gfxt_material_t* mat = gs_hash_table_getp(bsf->assets.materials, gs_hash_str64("mat.gsi"));
    ecs_entity_t e = ecs_new(bsf->entities.world, 0); 

    switch (type)
    {
        case BSF_ITEM_HEALTH:
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

        case BSF_ITEM_BOMB:
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

    ecs_set(bsf->entities.world, e, bsf_component_item_t, {
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

GS_API_DECL void bsf_item_system(ecs_iter_t* it)
{
	bsf_t* bsf = gs_user_data(bsf_t); 
	bsf_room_t* room = gs_slot_array_getp(bsf->run.rooms, bsf->run.room_ids[bsf->run.cell]);
    const gs_vec2 fbs = gs_platform_framebuffer_sizev(gs_platform_main_window());
    const float t = gs_platform_elapsed_time();
    const float dt = gs_platform_delta_time() * bsf->run.time_scale; 
    bsf_component_transform_t* tca = ecs_term(it, bsf_component_transform_t, 1);
    bsf_component_physics_t* pca = ecs_term(it, bsf_component_physics_t, 2); 
    bsf_component_item_t* ica = ecs_term(it, bsf_component_item_t, 3);
    bsf_component_transform_t* ptc = ecs_get(bsf->entities.world, bsf->entities.player, bsf_component_transform_t);
    bsf_component_physics_t* ppc = ecs_get(bsf->entities.world, bsf->entities.player, bsf_component_physics_t);

    if (bsf->dbg) return; 

    for (uint32_t i = 0; i < it->count; ++i)
    {
        ecs_entity_t ent = it->entities[i];
        bsf_component_transform_t* tc = &tca[i];
        bsf_component_physics_t* pc = &pca[i]; 
        bsf_component_item_t* ic = &ica[i]; 

        gs_vqs offset = (gs_vqs){
            .translation = gs_v3(0.f, sin(dt * t * ic->time_scale[0]) * 0.1f, 0.f),
            .rotation = gs_quat_angle_axis(dt * t * ic->time_scale[1], GS_YAXIS),
            .scale = gs_v3s(gs_map_range(0.f, 1.f, 0.7f, 1.f, (sin(dt * t * ic->time_scale[2]) * 0.5f + 0.5f)))
        };

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
            bsf_player_item_pickup(bsf, it->world, ic->type);

            // Delete entity
            ecs_delete(it->world, ent);
        } 
    }
}

//=== BSF Mob ===//

GS_API_DECL ecs_entity_t bsf_mob_create(struct bsf_t* bsf, gs_vqs* xform, bsf_mob_type type)
{
    gs_gfxt_material_t* mat = gs_hash_table_getp(bsf->assets.materials, gs_hash_str64("mat.gsi"));

    ecs_entity_t e = ecs_new(bsf->entities.world, 0); 
 
    switch (type)
    {
        case BSF_MOB_BOX: 
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

        case BSF_MOB_SPHERE: 
        {
            ecs_set(bsf->entities.world, e, bsf_component_renderable_immediate_t, {
                .shape = BSF_SHAPE_SPHERE,
                .model = gs_vqs_to_mat4(xform),
				.material = mat,
                .color = GS_COLOR_WHITE
            });

	        ecs_set(bsf->entities.world, e, bsf_component_physics_t, {
				.collider = {
					.xform = gs_vqs_default(),
					.type = BSF_COLLIDER_SPHERE,
					.shape.sphere = (gs_sphere_t) {
						.c = gs_v3s(0.f),
						.r = 0.5f
					}
				}
            });
        } break;

        case BSF_MOB_CONE:
        {
            ecs_set(bsf->entities.world, e, bsf_component_renderable_immediate_t, {
                .shape = BSF_SHAPE_CONE,
                .model = gs_vqs_to_mat4(xform),
				.material = mat,
                .color = GS_COLOR_WHITE
            });

	        ecs_set(bsf->entities.world, e, bsf_component_physics_t, {
				.collider = {
					.xform = gs_vqs_default(),
					.type = BSF_COLLIDER_CONE,
					.shape.cone = (gs_cone_t) {
						.base = gs_v3s( 0.f ),
						.r = 0.5f,
						.height = 1.f
					}
				}
            });
        } break;
    }
 
    ecs_set(bsf->entities.world, e, bsf_component_health_t, {.health = 1.f});
    ecs_set(bsf->entities.world, e, bsf_component_transform_t, {.xform = *xform});
    ecs_set(bsf->entities.world, e, bsf_component_mob_t, {0});
    ecs_set(bsf->entities.world, e, bsf_component_gun_t, {0}); 

    return e;
} 

GS_API_DECL void bsf_mob_system(ecs_iter_t* it)
{
	bsf_t* bsf = gs_user_data(bsf_t); 
    gs_immediate_draw_t* gsi = &bsf->gs.gsi;
	bsf_room_t* room = gs_slot_array_getp(bsf->run.rooms, bsf->run.room_ids[bsf->run.cell]);
    const gs_vec2 fbs = gs_platform_framebuffer_sizev(gs_platform_main_window());
    const float t = gs_platform_elapsed_time();
    const float dt = gs_platform_delta_time(); 
    bsf_component_renderable_immediate_t* rca = ecs_term(it, bsf_component_renderable_immediate_t, 1);
    bsf_component_transform_t* tca = ecs_term(it, bsf_component_transform_t, 2);
    bsf_component_physics_t* pca = ecs_term(it, bsf_component_physics_t, 3); 
    bsf_component_health_t* hca = ecs_term(it, bsf_component_health_t, 4);
    bsf_component_mob_t* mca = ecs_term(it, bsf_component_mob_t, 5);
    bsf_component_gun_t* bca = ecs_term(it, bsf_component_gun_t, 6);
    bsf_component_transform_t* ptc = ecs_get(bsf->entities.world, bsf->entities.player, bsf_component_transform_t);

    if (bsf->dbg) return; 

    for (uint32_t i = 0; i < it->count; ++i)
    {
        ecs_entity_t ent = it->entities[i];
        bsf_component_renderable_immediate_t* rc = &rca[i];
        bsf_component_transform_t* tc = &tca[i];
        bsf_component_physics_t* pc = &pca[i]; 
        bsf_component_health_t* hc = &hca[i];
        bsf_component_mob_t* mc = &mca[i];
        bsf_component_gun_t* gc = &bca[i];
        const gs_vec3* ppos = &ptc->xform.translation;

        const gs_vec3 diff = gs_vec3_sub(*ppos, tc->xform.translation);
        float dist2 = gs_vec3_len2(diff);

        // Do targetting towards player
        if (dist2 <= 5000.f)
        {
            const gs_vec3 spread = gs_vec3_norm(gs_v3(
                gs_rand_gen(&bsf->run.rand),
                gs_rand_gen(&bsf->run.rand),
                gs_rand_gen(&bsf->run.rand)
            ));
            const gs_vec3 dir = gs_vec3_norm(gs_vec3_add(diff, gs_vec3_scale(spread, dist2 * 0.005f)));
            const gs_vec3 forward = gs_vec3_norm(gs_quat_rotate(tc->xform.rotation, gs_vec3_scale(GS_ZAXIS, -1.f))); 
            const gs_vec3 up = gs_vec3_norm(gs_quat_rotate(tc->xform.rotation, GS_YAXIS)); 
            gs_quat rotation = gs_quat_from_to_rotation(forward, dir); 

            // Debug draw line
            gsi_defaults(gsi);
            gsi_depth_enabled(gsi, true);
            gsi_camera(gsi, &bsf->scene.camera.cam, (u32)fbs.x, (u32)fbs.y);
            gsi_line3Dv(gsi, tc->xform.translation, gs_vec3_add(tc->xform.translation, gs_vec3_scale(dir, 10.f)), GS_COLOR_RED); 
            gsi_line3Dv(gsi, tc->xform.translation, gs_vec3_add(tc->xform.translation, gs_vec3_scale(forward, 5.f)), GS_COLOR_BLUE);

            // Fire
            if (gc->time >= 100.f && gs_rand_gen_long(&bsf->run.rand) % 3)
            { 
                gc->time = 0.f;

                // Fire projectile using forward of player transform
                gs_vec3 forward = dir; 
                gs_vqs xform = (gs_vqs){
                    .translation = tc->xform.translation, 
                    .rotation = rotation,// rotation,   // Need rotation from forward to player
                    .scale = gs_v3s(0.2f)
                };

                bsf_projectile_create(bsf, it->world, BSF_PROJECTILE_BULLET, BSF_OWNER_ENEMY, &xform, gs_vec3_scale(forward, 4.5f));
            }

            gc->time += 1.f * gs_rand_gen_range(&bsf->run.rand, 0.1f, 2.f);
        } 

        // Do collision hit
        if (hc->hit)
        {
            if (hc->hit_timer >= 0.1f) {
                hc->hit = false;
				hc->hit_timer = 0.f;
            }

            if (hc->health <= 0.f)
            {
				// Have to iterate over the dynamic array of mobs and remove... 
				for (uint32_t i = 0; i < gs_dyn_array_size(room->mobs); ++i)
				{
					if (ent == room->mobs[i])
					{ 
						room->mobs[i] = gs_dyn_array_back(room->mobs);
						gs_dyn_array_pop(room->mobs);
						break;
					}
				}

                ecs_delete(it->world, ent);
            } 

            rc->color = GS_COLOR_RED; 
            hc->hit_timer += dt;
        }
        else
        {
            rc->color = GS_COLOR_WHITE;
        }
    }
}

//=== BSF Projectile ===//

GS_API_DECL void bsf_projectile_create(struct bsf_t* bsf, ecs_world_t* world, bsf_projectile_type type, bsf_owner_type owner, const gs_vqs* xform, gs_vec3 velocity)
{
    gs_gfxt_material_t* mat = gs_hash_table_getp(bsf->assets.materials, gs_hash_str64("mat.laser_player"));
    gs_gfxt_mesh_t* mesh = gs_hash_table_getp(bsf->assets.meshes, gs_hash_str64("mesh.laser_player"));
    gs_gfxt_material_set_uniform(mat, "u_color", &(gs_vec3){0.f, 1.f, 0.f}); 

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

            ecs_set(world, b, bsf_component_timer_t, {.max = 10.f}); 

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

        ecs_entity_t projectile = it->entities[i];
		gs_vqs* xform = &tc->xform;
        gs_vec3* trans = &xform->position; 
        const gs_vec3* vel = &pc->velocity; 
        const float speed = pc->speed * dt;

        // Update position based on velocity and dt
        *trans = gs_vec3_add(*trans, gs_vec3_scale(*vel, speed)); 

        // Update timer
        kc->time += dt;
        if (kc->time >= kc->max) 
        {
            switch (bc->type)
            {
                case BSF_PROJECTILE_BULLET:
                {
                    ecs_delete(it->world, projectile);
                } break;

                case BSF_PROJECTILE_BOMB:
                {
                    // Do bomb explosion
                    bsf_explosion_create(bsf, it->world, &tc->xform, bc->owner);
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
								h->health -= 0.1f;
								h->hit_timer = 0.f;
                                bsf_camera_shake(bsf, &bsf->scene.camera, 0.01f);
								ecs_delete(it->world, projectile);
								break;
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

    bsf->entities.player = ecs_new(bsf->entities.world, 0); 
    ecs_entity_t p = bsf->entities.player;

    ecs_set(bsf->entities.world, p, bsf_component_transform_t, {
        .xform = (gs_vqs){
            .translation = gs_v3(0.f, 5.f, 0.f),
            .rotation = gs_quat_default(),
            .scale = gs_v3s(0.1f)
        } 
    }); 

    ecs_set(bsf->entities.world, p, bsf_component_inventory_t, {
        .bombs = 1  
    });

    ecs_set(bsf->entities.world, p, bsf_component_camera_track_t, {0}); 
    ecs_set(bsf->entities.world, p, bsf_component_barrel_roll_t, {0});

    ecs_set(bsf->entities.world, p, bsf_component_character_stats_t, {
        .speed = 3,
        .fire_rate = 2,
        .shot_speed = 1
    }); 

    ecs_set(bsf->entities.world, p, bsf_component_gun_t, {0});
    
    gs_gfxt_material_t* mat = gs_hash_table_getp(bsf->assets.materials, gs_hash_str64("mat.ship_arwing"));
    gs_gfxt_mesh_t* mesh = gs_hash_table_getp(bsf->assets.meshes, gs_hash_str64("mesh.arwing"));
    gs_gfxt_texture_t* tex = gs_hash_table_getp(bsf->assets.textures, gs_hash_str64("tex.arwing"));
    gs_gfxt_material_t* gsi_mat = gs_hash_table_getp(bsf->assets.materials, gs_hash_str64("mat.gsi"));
    gs_gfxt_material_set_uniform(mat, "u_tex", tex);

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
    bsf_component_renderable_t* rca = ecs_term(it, bsf_component_renderable_t, 1);
    bsf_component_transform_t* tca = ecs_term(it, bsf_component_transform_t, 2);
    bsf_component_physics_t* pca = ecs_term(it, bsf_component_physics_t, 3); 
    bsf_component_character_stats_t* psca = ecs_term(it, bsf_component_character_stats_t, 4);
    bsf_component_health_t* hca = ecs_term(it, bsf_component_health_t, 5);
    bsf_component_gun_t* gca = ecs_term(it, bsf_component_gun_t, 6);
    bsf_component_camera_track_t* cca = ecs_term(it, bsf_component_camera_track_t, 7);
    bsf_component_barrel_roll_t* bca = ecs_term(it, bsf_component_barrel_roll_t, 8);
    bsf_component_inventory_t* ica = ecs_term(it, bsf_component_inventory_t, 9);

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
        const gs_vec2 xb = gs_v2(-12.f, 12.f);
        const gs_vec2 yb = gs_v2(-4.f, 11.f);
        gs_vec3* ps = &tc->xform.position;
        gs_vec3 v = gs_v3s(0.f);

        if (gs_platform_key_down(GS_KEYCODE_W) || gs_platform_key_down(GS_KEYCODE_UP))    {v.y = ps->y > yb.x ? v.y - 1.f : 0.f;}
        if (gs_platform_key_down(GS_KEYCODE_S) || gs_platform_key_down(GS_KEYCODE_DOWN))  {v.y = ps->y < yb.y ? v.y + 1.f : 0.f;}
        if (gs_platform_key_down(GS_KEYCODE_A) || gs_platform_key_down(GS_KEYCODE_LEFT))  {v.x = ps->x > xb.x ? v.x - 1.f : 0.f;}
        if (gs_platform_key_down(GS_KEYCODE_D) || gs_platform_key_down(GS_KEYCODE_RIGHT)) {v.x = ps->x < xb.y ? v.x + 1.f : 0.f;} 

        if (bc->active)
        {
            mod_lr = bc->vel_dir * dt;
        }

        switch (room->movement_type)
        {
            case BSF_MOVEMENT_FREE_RANGE:
            { 
                // Normalize velocity then scale by player speed
                v = gs_vec3_scale(gs_vec3_norm(gs_quat_rotate(tc->xform.rotation, v)), pc->speed); 

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

                av = gs_vec3_scale(gs_vec3_norm(av), 80.2f * dt); 

                const float max_rotx = 90.f;
                const float max_roty = 90.f;
                const float max_rotz = 90.f;
                const float cmax_rotz = 45.f;

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

            } break;

            case BSF_MOVEMENT_RAIL:
            {
                // Normalize velocity then scale by player speed
                v = gs_vec3_scale(gs_vec3_norm(v), mod * 3.f * dt);

                const float lerp = 0.1f;

                // Add to velocity
                pc->velocity = gs_v3(
                    gs_interp_linear(pc->velocity.x, v.x, lerp),
                    gs_interp_linear(pc->velocity.y, v.y, lerp),
                    gs_interp_linear(pc->velocity.z, v.z, lerp)
                );

                tc->xform.position = gs_vec3_add(tc->xform.position, pc->velocity);

                // Move slowly back to origin
                float nz = -tc->xform.position.z;
                tc->xform.position.z = gs_interp_linear(tc->xform.position.z, 25.f, 0.3f);

                gs_vec3 av = gs_v3s(0.f);
                bool br = false;
                if (gs_platform_key_down(GS_KEYCODE_W)) av.y -= 1.f;
                if (gs_platform_key_down(GS_KEYCODE_S)) av.y += 1.f;
                if (gs_platform_key_down(GS_KEYCODE_A)) {av.x += 1.f; av.z += 0.05;}
                if (gs_platform_key_down(GS_KEYCODE_D)) {av.x -= 1.f; av.z -= 0.05f;}
                if (gs_platform_key_down(GS_KEYCODE_Q)) av.z += 1.f;
                if (gs_platform_key_down(GS_KEYCODE_E)) av.z -= 1.f;
                if (gs_platform_key_down(GS_KEYCODE_B)) {br = true; av.z += 1.f;}
                av = gs_vec3_scale(gs_vec3_norm(av), 2.0f);
                av.z = br ? av.z * 50.f : av.z * 10.f;
                // Add negated angular velocity to this
                av = gs_vec3_add(av, gs_vec3_scale(gs_vec3_neg(pc->angular_velocity), 3.5 * dt));

                pc->angular_velocity = gs_vec3_add(pc->angular_velocity, av);
                gs_vec3* pav = &pc->angular_velocity;
                const float max_rotx = 60.f;
                const float max_roty = 30.f;
                const float max_rotz = 90.f;
                pav->x = gs_clamp(pav->x, -max_rotx, max_rotx);
                pav->y = gs_clamp(pav->y, -max_roty, max_roty);
                pav->z = br ? pav->z : gs_clamp(pav->z, -max_rotz, max_rotz);

                // Do rotation (need barrel roll rotation)
                tc->xform.rotation = gs_quat_mul_list(3,
                    gs_quat_angle_axis(gs_deg2rad(pc->angular_velocity.x), GS_YAXIS),
                    gs_quat_angle_axis(gs_deg2rad(pc->angular_velocity.y), GS_XAXIS),
                    gs_quat_angle_axis(gs_deg2rad(pc->angular_velocity.z), GS_ZAXIS)
                );

            } break;
        } 

        // Update renderable
        bsf_renderable_t* rend = gs_slot_array_getp(bsf->scene.renderables, rc->hndl);
        rend->model = gs_vqs_to_mat4(&tc->xform);

        if (gs_platform_mouse_down(GS_MOUSE_LBUTTON) && !gc->firing) {
            gc->firing = true;
        }
        if (gs_platform_mouse_released(GS_MOUSE_LBUTTON)) {
            gc->firing = false;
        } 
        if (gc->firing) {
            gc->time += psc->fire_rate * dt * 100.f;
        } 

        // Fire
        if (gc->firing && gc->time >= 30.f)
        { 
            gc->time = 0.f;
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

            bsf_projectile_create(bsf, it->world, BSF_PROJECTILE_BULLET, BSF_OWNER_PLAYER, &xform, gs_vec3_scale(forward, psc->shot_speed * 5.f));
            bsf_camera_shake(bsf, &bsf->scene.camera, 0.05f);
        }

        if (ic->bombs && gs_platform_mouse_pressed(GS_MOUSE_RBUTTON))
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

            bsf_projectile_create(bsf, it->world, BSF_PROJECTILE_BOMB, BSF_OWNER_PLAYER, &xform, gs_vec3_scale(forward, psc->shot_speed * 5.f));
            ic->bombs = gs_max(ic->bombs - 1, 0);
        }
    } 
}

GS_API_DECL void bsf_player_damage(struct bsf_t* bsf, ecs_world_t* world, float damage)
{
    bsf_component_health_t* hc = ecs_get(world, bsf->entities.player, bsf_component_health_t);
    hc->health -= damage; 
    if (hc->health <= 0.f) 
    {
        bsf->state = BSF_STATE_GAME_OVER;
    }
}

GS_API_DECL void bsf_player_item_pickup(struct bsf_t* bsf, ecs_world_t* world, bsf_item_type type)
{
    switch (type)
    {
        case BSF_ITEM_HEALTH:
        {
            bsf_component_health_t* hc = ecs_get(world, bsf->entities.player, bsf_component_health_t);
            hc->health += 0.5f;
        } break;

        case BSF_ITEM_BOMB:
        {
            bsf_component_inventory_t* ic = ecs_get(world, bsf->entities.player, bsf_component_inventory_t);
            ic->bombs++;
        } break;
    }
} 

//=== BSF Menus ===//

GS_API_DECL void bsf_menu_update(struct bsf_t* bsf)
{
    gs_command_buffer_t* cb = &bsf->gs.cb;
    gs_gui_context_t* gui = &bsf->gs.gui;
    const gs_vec2 fbs = gs_platform_framebuffer_sizev(gs_platform_main_window()); 

    int32_t opt = GS_GUI_OPT_FULLSCREEN | 
                  GS_GUI_OPT_NOTITLE | 
                  GS_GUI_OPT_NOBORDER | 
                  GS_GUI_OPT_NOMOVE | 
                  GS_GUI_OPT_NORESIZE;

    gs_gui_begin_window_ex(gui, "#menu", gs_gui_rect(0, 0, 100, 100), NULL, opt);

    switch (bsf->state) 
    {
        case BSF_STATE_TITLE:
        {
            gs_gui_label(gui, "Binding of Star Fox");
            gs_gui_label(gui, "PRESS START");

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

        } break;

        case BSF_STATE_MAIN_MENU:
        {
            if (gs_platform_key_pressed(GS_KEYCODE_ESC))
            {
                bsf->state = BSF_STATE_TITLE;
            }

            // This will go to a "character select screen"
            if (gs_gui_button(gui, "Play"))
            {
                bsf->state = BSF_STATE_GAME_SETUP; 

                // Initial seed
                char str[32] = {0};
                gs_uuid_t uuid = gs_platform_uuid_generate();
                gs_platform_uuid_to_string(str, &uuid);
                memcpy(bsf->run.seed, str, BSF_SEED_MAX_LEN - 1);
            } 

            if (gs_gui_button(gui, "Editor"))
            {
                bsf->state = BSF_STATE_EDITOR_START;
            }

            if (gs_gui_button(gui, "Options"))
            {
                bsf->state = BSF_STATE_OPTIONS;
            }

            if (gs_gui_button(gui, "Quit"))
            {
                bsf->state = BSF_STATE_QUIT;
            }
        } break;

        case BSF_STATE_GAME_SETUP:
        {
            if (gs_platform_key_pressed(GS_KEYCODE_ESC))
            {
                bsf->state = BSF_STATE_MAIN_MENU;
            } 

            if (gs_gui_button(gui, "Play"))
            {
                bsf->state = BSF_STATE_START;
                bsf->run.is_playing = true;
            } 

            gs_gui_textbox(gui, bsf->run.seed, BSF_SEED_MAX_LEN);

        } break;

        case BSF_STATE_PAUSE:
        {
            if (gs_platform_key_pressed(GS_KEYCODE_ESC)) 
            {
                bsf->state = BSF_STATE_PLAY;
            }

            if (gs_gui_button(gui, "Resume"))
            {
                bsf->state = BSF_STATE_PLAY;
            } 

            if (gs_gui_button(gui, "Options"))
            {
                bsf->state = BSF_STATE_OPTIONS;
            } 

            if (gs_gui_button(gui, "Exit"))
            {
                bsf->state = BSF_STATE_END;
            }
        } break;

        case BSF_STATE_OPTIONS: 
        {
            if (gs_platform_key_pressed(GS_KEYCODE_ESC))
            {
                if (bsf->run.is_playing) bsf->state = BSF_STATE_PAUSE;
                else                     bsf->state = BSF_STATE_MAIN_MENU;
            }

            if (gs_gui_button(gui, "Back"))
            { 
                if (bsf->run.is_playing) bsf->state = BSF_STATE_PAUSE;
                else                     bsf->state = BSF_STATE_MAIN_MENU;
            } 
        } break;
    }
    gs_gui_end_window(gui);
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
    // Unload previous room cell of items and mobs
    bsf_room_t* room = gs_slot_array_iter_getp(bsf->run.rooms, bsf->run.cell); 
    for (uint32_t i = 0; i < gs_dyn_array_size(room->mobs); ++i) {
        ecs_delete(bsf->entities.world, room->mobs[i]);
    }
    gs_dyn_array_clear(room->mobs);

    // Set new cell
    bsf->run.cell = cell;

    // Get new room to load
    room = gs_slot_array_iter_getp(bsf->run.rooms, bsf->run.room_ids[bsf->run.cell]);
    room->movement_type = BSF_MOVEMENT_RAIL; 

    // Load a room template
    gs_snprintfc(RT_FILE, 256, "%s/room_templates/room.rt", bsf->assets.asset_dir);
    gs_byte_buffer_t buffer = gs_byte_buffer_new();
    gs_byte_buffer_read_from_file(&buffer, RT_FILE);

    gs_byte_buffer_readc(&buffer, uint16_t, ct);
    for (uint32_t i = 0; i <  ct; ++i)
    { 
        gs_byte_buffer_readc(&buffer, bsf_room_brush_t, brush);
        switch (brush.type)
        {
            case BSF_ROOM_BRUSH_MOB: 
            {
                bsf_shape_type type = BSF_SHAPE_BOX;
                ecs_entity_t e = bsf_mob_create(bsf, &brush.xform, type);
                gs_dyn_array_push(room->mobs, e);
            } break;

            case BSF_ROOM_BRUSH_ITEM:
            {
                bsf_item_type type = gs_rand_gen_range_long(&bsf->run.rand, 0, BSF_ITEM_COUNT - 1);
                ecs_entity_t e = bsf_item_create(bsf, &brush.xform, type);
            } break;
        } 
    } 

    gs_byte_buffer_free(&buffer);
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
        gs_dyn_array_clear(room->mobs);
    }

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
	room = gs_slot_array_getp(bsf->run.rooms, bsf->run.room_ids[gs_dyn_array_back(dead_ends)]);
	room->type = BSF_ROOM_BOSS; 

    // Pop off end 
    gs_dyn_array_pop(dead_ends);

    // Place item room
    item_idx = gs_rand_gen_range_long(&bsf->run.rand, 0, gs_dyn_array_size(dead_ends) - 1);   
    if (!gs_dyn_array_empty(dead_ends))
    {
        room = gs_slot_array_getp(bsf->run.rooms, bsf->run.room_ids[dead_ends[item_idx]]);
        room->type = BSF_ROOM_ITEM;
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
                    bsf->run.room_ids[cell] = gs_slot_array_insert(bsf->run.rooms, (bsf_room_t){.type = BSF_ROOM_SECRET});
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

    gs_dyn_array_free(queue);
    gs_dyn_array_free(dead_ends);
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

    // Initialize world based on seed
    bsf_game_level_gen(bsf);

    gs_platform_lock_mouse(gs_platform_main_window(), true);

    // Set state to playing game
    bsf->state = BSF_STATE_PLAY;
}

GS_API_DECL void bsf_game_end(struct bsf_t* bsf)
{ 
	// Destroy entity world
	ecs_fini(bsf->entities.world);

    bsf->run.is_playing = false;
    bsf->state = BSF_STATE_MAIN_MENU;
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
    gsi_rotatefv(gsi, t * 0.001f, GS_YAXIS);
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

    // Update entity world
    ecs_progress(bsf->entities.world, 0);

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

    gs_gui_begin_window_ex(gui, "#ui_canvas", gs_gui_rect(0, 0, 0, 0), NULL, opt);
    { 
        gs_gui_container_t* cnt = gs_gui_get_current_container(gui);

        // Iventory
        {
            bsf_component_inventory_t* ic = ecs_get(bsf->entities.world, bsf->entities.player, bsf_component_inventory_t);
            bsf_component_health_t* hc = ecs_get(bsf->entities.world, bsf->entities.player, bsf_component_health_t);

            char TMP[256] = {0};

            gs_gui_rect_t anchor = gs_gui_layout_anchor(&cnt->body, 200, 30, 10, 0, GS_GUI_LAYOUT_ANCHOR_TOPLEFT);
            gs_gui_layout_set_next(gui, anchor, 0); 
            gs_gui_layout_row(gui, 1, (int16_t[]){-1}, 0);

            gs_snprintf(TMP, 256, "bombs: %zu, health: %.2f", ic->bombs, hc->health);
            gs_gui_label(gui, TMP); // Need to make this take variable arguments
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
                        gs_vec3 bp = tc->xform.translation; 

                        gs_vec3 dir = gs_vec3_norm(gs_vec3_sub(bp, bsf->scene.camera.cam.transform.translation));
                        gs_vec3 fwd = gs_vec3_norm(gs_camera_forward(&bsf->scene.camera.cam));

                        if (gs_vec3_dot(dir, fwd) >= 0.f)
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
    }
    gs_gui_end_window(gui);

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

        // Update debug gui
        gs_gui_begin_window(gui, "#dbg", gs_gui_rect(100, 100, 350, 200));
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
            if (gs_gui_begin_treenode(gui, "#run"))
            {
                gs_gui_layout_t* layout = gs_gui_get_layout(gui);
                bsf_room_t* room = gs_slot_array_getp(bsf->run.rooms, bsf->run.room_ids[bsf->run.cell]);

                GUI_LABEL("level: %zu", bsf->run.level); 
                GUI_LABEL("num_rooms: %zu", gs_slot_array_size(bsf->run.rooms)); 
                GUI_LABEL("num_mobs: %zu", (u32)gs_dyn_array_size(room->mobs));
                GUI_LABEL("num_renderables: %zu", gs_slot_array_size(bsf->scene.renderables));
                GUI_LABEL("active cell: %zu", bsf->run.cell);

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

                gs_gui_end_treenode(gui);
            } 

            if (gs_gui_begin_treenode(gui, "#player"))
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

                gs_gui_end_treenode(gui);
            }
        }
        gs_gui_end_window(gui); 
    } 
}

//=== BSF Editor ===//

#define SENSITIVITY  0.2f
static gs_vqs gxform = {0};
static bsf_room_template_t room_template = {0}; 
static uint32_t brush = 0;
static bool editor_init = false;
static rt_file[256] = {0};

GS_API_DECL void bsf_editor_start(struct bsf_t* bsf)
{
    gs_gui_context_t* gui = &bsf->gs.gui;

    bsf_camera_init(bsf, &bsf->scene.camera);

    bsf->state = BSF_STATE_EDITOR; 

    gs_slot_array_clear(room_template.brushes); 

    // Try to load room template from file
    gs_snprintfc(TMP, 256, "%s/room_templates/room.rt", bsf->assets.asset_dir);
    memcpy(rt_file, TMP, 256);

    if (gs_platform_file_exists(rt_file))
    {
        gs_byte_buffer_t buffer = gs_byte_buffer_new();
        gs_byte_buffer_read_from_file(&buffer, rt_file); 
        gs_byte_buffer_readc(&buffer, uint16_t, ct); 
        for (uint32_t i = 0; i < ct; ++i)
        {
            gs_byte_buffer_readc(&buffer, bsf_room_brush_t, br);
            gs_slot_array_insert(room_template.brushes, br);
        }
        gs_byte_buffer_free(&buffer);
    }

    brush = UINT_MAX;

    if (!editor_init)
    {
        editor_init = true;
        gs_gui_dock_ex(gui, "Scene", "Dockspace", GS_GUI_SPLIT_BOTTOM, 1.f);
        gs_gui_dock_ex(gui, "Outliner", "Scene", GS_GUI_SPLIT_RIGHT, 0.3f); 
        gs_gui_dock_ex(gui, "Properties", "Outliner", GS_GUI_SPLIT_BOTTOM, 0.7f); 
        gs_gui_dock_ex(gui, "Debug", "Properties", GS_GUI_SPLIT_BOTTOM, 0.5f);
    }
}

GS_API_DECL void bsf_editor_scene_cb(gs_gui_context_t* ctx, gs_gui_customcommand_t* cmd)
{
    bsf_t* bsf = gs_user_data(bsf_t); 
    gs_immediate_draw_t* gsi = &ctx->gsi;
    const float t = gs_platform_elapsed_time(); 
	const gs_gui_rect_t vp = cmd->viewport;
	const gs_vec2 fbs = gs_platform_framebuffer_sizev(gs_platform_main_window());
    const bsf_room_template_t* rt = &room_template;

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
                case BSF_ROOM_BRUSH_PROP: col  = GS_COLOR_WHITE; break;
                case BSF_ROOM_BRUSH_ITEM: col  = GS_COLOR_ORANGE; break;
            }

            // Depending on type, do various shapes
            gsi_box(gsi, 0.f, 0.f, 0.f, 0.5f, 0.5f, 0.5, col.r, col.g, col.b, col.a, GS_GRAPHICS_PRIMITIVE_LINES);
        }
        gsi_pop_matrix(gsi);
    } 
}

GS_API_DECL int32_t gs_gui_selectable(gs_gui_context_t* ctx, const char* label, bool* selected)
{
    int32_t res = 0;
    int32_t opt = GS_GUI_OPT_NOMOVE | 
                  GS_GUI_OPT_NORESIZE | 
                  GS_GUI_OPT_NOTITLE | 
                  GS_GUI_OPT_FORCESETRECT;

    return res;
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
        gs_gui_open_popup(ctx, "#popup");
    }

    int32_t ct = popup_max_height_in_items > 0 ? popup_max_height_in_items : items_count;
    gs_gui_rect_t rect = ctx->last_rect;
    rect.y += rect.h;
    rect.h = (ct + 1) * ctx->style_sheet->styles[GS_GUI_ELEMENT_BUTTON][0x00].size[1];
    if (gs_gui_begin_popup_ex(ctx, "#popup", rect, opt))
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
        gs_gui_end_popup(ctx);
    }

    return res;
} 

GS_API_DECL void bsf_editor(struct bsf_t* bsf)
{
    gs_command_buffer_t* cb = &bsf->gs.cb;
    gs_immediate_draw_t* gsi = &bsf->gs.gsi;
    gs_gui_context_t* gui = &bsf->gs.gui;
    bsf_room_template_t* rt = &room_template; 

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

    gs_gui_begin_window_ex(gui, "Dockspace", gs_gui_rect(350, 40, 600, 500), NULL, opt);
    {
        // Empty dockspace...
    }
    gs_gui_end_window(gui);

	gs_gui_begin_window_ex(gui, "Outliner", gs_gui_rect(0, 0, 300.f, 200.f), NULL, 0x00);
    {
        gs_gui_layout_row(gui, 1, (int[]){-1}, -1); 
        gs_gui_begin_panel_ex(gui, "#panel", GS_GUI_OPT_NOTITLE);
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
        gs_gui_end_panel(gui);
    }
	gs_gui_end_window(gui);

	gs_gui_begin_window_ex(gui, "Properties", gs_gui_rect(0, 0, 300.f, 200.f), NULL, 0x00);
    {
        gs_gui_layout_row(gui, 1, (int[]){-1}, -1); 
        if (!brush)
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
                "Item",
                "Prop"
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
	gs_gui_end_window(gui);

	gs_gui_begin_window_ex(gui, "Debug", gs_gui_rect(0, 0, 300.f, 200.f), NULL, 0x00);
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
            gs_byte_buffer_write_to_file(&buffer, rt_file);
            gs_byte_buffer_free(&buffer);
        }

        if (gs_gui_button(gui, "Exit"))
        {
            bsf->state = BSF_STATE_MAIN_MENU;
        }
    }
	gs_gui_end_window(gui);

	gs_gui_begin_window_ex(gui, "Scene", gs_gui_rect(0, 0, 500.f, 400.f), NULL, 0x00);
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
            gs_snprintfc(TMP, 512, "template: %s", rt_file);
            gs_gui_label(gui, TMP);
        }
        
	}
	gs_gui_end_window(gui);
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

















































