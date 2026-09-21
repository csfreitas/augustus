/* The optional probe reuses complete production decoder functions without
 * initializing graphics/audio or writing a player profile. */
#ifdef ARCH_SAVE_PROBE
#include "save_decoder.c"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "city/buildings.h"
#include "city/data_private.h"
#include "building/menu.h"
#include "game/tutorial.h"
#include "core/config.h"
#include "building/highway_station.h"
#include "core/calc.h"
#include "core/lang.h"
#include "core/image.h"
#include "assets/assets.h"
#include "scenario/allowed_building.h"
#include "scenario/property.h"

/* Unrelated rendering/building services must not be called by these checks. */
static void unexpected_service(void) { fputs("Unexpected game service in isolated test\n", stderr); abort(); }
building *building_get(unsigned int id) { (void) id; unexpected_service(); return 0; }
building *building_first_of_type(building_type type) { (void) type; unexpected_service(); return 0; }
int building_is_active(const building *b) { (void) b; unexpected_service(); return 0; }
int building_highway_station_is_functional(building *b) { (void) b; unexpected_service(); return 0; }
int calc_maximum_distance(int x1, int y1, int x2, int y2) { (void)x1;(void)y1;(void)x2;(void)y2;unexpected_service(); return 0; }
const uint8_t *lang_get_string(int group, int index) { (void)group;(void)index;unexpected_service(); return 0; }
int image_group(int group) { (void)group;unexpected_service(); return 0; }
int assets_get_image_id(const char *group, const char *name) { (void)group;(void)name;unexpected_service(); return 0; }
scenario_climate scenario_property_climate(void) { unexpected_service(); return CLIMATE_CENTRAL; }

static int failures;
struct city_data_t city_data;
static int checks;
static tutorial_build_buttons tutorial = TUT_BUILD_NORMAL;
static int resources_available = 1;

void log_info(const char *message, const char *detail, int value) { (void) message; (void) detail; (void) value; }
void log_error(const char *message, const char *detail, int value) { fprintf(stderr, "%s %s %d\n", message, detail ? detail : "", value); }
int empire_can_produce_resource_naturally(resource_type type) { (void) type; return resources_available; }
int empire_can_produce_resource_potentially(resource_type type) { (void) type; return resources_available; }
int empire_can_import_resource_potentially(resource_type type) { (void) type; return resources_available; }
int building_monument_has_required_resources_to_build(building_type type) { (void) type; return 1; }
int scenario_is_tutorial_2(void) { return tutorial == TUT2_BUILD_START; }
tutorial_build_buttons tutorial_get_build_buttons(void) { return tutorial; }

static void check(const char *name, int actual, int expected)
{
    checks++;
    if (actual != expected) { failures++; printf("FAIL %s: got %d, expected %d\n", name, actual, expected); }
    else { printf("PASS %s\n", name); }
}

static void check_menu(const char *name, int expected)
{
    building_menu_update();
    check(name, building_menu_is_enabled(BUILDING_TRIUMPHAL_ARCH), expected);
}

#ifdef ARCH_SAVE_PROBE
static int inspect_save(const char *path)
{
    FILE *fp = fopen(path, "rb");
    savegame_version_t version;
    resource_version_t resource_version;
    if (!fp || !get_savegame_versions(fp, &version, &resource_version)) { return 2; }
    if (version > SAVE_GAME_CURRENT_VERSION || resource_version > RESOURCE_CURRENT_VERSION) { fclose(fp); return 2; }
    resource_set_mapping(resource_version);
    init_savegame_data(version);
    int loaded = savegame_read_from_file(fp, version);
    fclose(fp);
    if (!loaded || !savegame_data.state.allowed_buildings) { return 2; }
    scenario_allowed_building_load_state(savegame_data.state.allowed_buildings);
    city_data_load_state(savegame_data.state.city_data, savegame_data.state.city_graph_order,
        savegame_data.state.city_entry_exit_xy, savegame_data.state.city_entry_exit_grid_offset, version);
    printf("SAVE %s version=%d arch_allowed=%d awarded=%d placed=%d victories=%d\n", path, version,
        scenario_allowed_building(BUILDING_TRIUMPHAL_ARCH), city_data.building.triumphal_arches_available,
        city_data.building.triumphal_arches_placed, city_data.distant_battle.won_count);
    check_menu("save reward is available in build menu", city_buildings_triumphal_arch_available());
    clear_savegame_pieces();
    return failures ? 1 : 0;
}
#endif

int main(int argc, char **argv)
{
#ifdef ARCH_SAVE_PROBE
    if (argc > 1) { return inspect_save(argv[1]); }
#else
    if (argc > 1) { fputs("Configure ARCH_SAVE_PROBE=ON to inspect local save copies.\n", stderr); return 2; }
    (void) argv;
#endif
    memset(&city_data, 0, sizeof(city_data));
    scenario_allowed_building_enable_all();
    check_menu("no award, allowed arch stays hidden", 0);
    scenario_allowed_building_set(BUILDING_TRIUMPHAL_ARCH, 0);
    check_menu("no award, forbidden arch stays hidden", 0);
    city_buildings_earn_triumphal_arch();
    check_menu("earned arch visible despite scenario flag", 1);
    check("reward does not rewrite scenario permission", scenario_allowed_building(BUILDING_TRIUMPHAL_ARCH), 0);
    scenario_allowed_building_set(BUILDING_SENATE, 0);
    building_menu_update();
    check("other building restrictions preserved", building_menu_is_enabled(BUILDING_SENATE), 0);
    city_buildings_build_triumphal_arch();
    check_menu("placed award exhausted", 0);
    city_buildings_remove_triumphal_arch();
    check_menu("demolition returns earned arch", 1);
    city_buildings_build_triumphal_arch();
    city_buildings_earn_triumphal_arch();
    check_menu("second victory awards another arch", 1);
    city_buildings_build_triumphal_arch();
    check_menu("both awards exhausted", 0);
    city_buildings_remove_triumphal_arch();
    scenario_allowed_building_set(BUILDING_TRIUMPHAL_ARCH, 1);
    check_menu("allowed earned arch still available", 1);
    tutorial = TUT1_BUILD_START;
    check_menu("tutorial restrictions unchanged", 0);
    tutorial = TUT_BUILD_NORMAL;
    resources_available = 0;
    check_menu("Rome supplied arch independent of trade", 1);
    printf("RESULT %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
