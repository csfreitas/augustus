#include "localization.h"

#include "core/encoding.h"
#include "core/file.h"
#include "core/log.h"
#include "core/string.h"
#include "core/xml_parser.h"
#include "editor/editor.h"
#include "game/campaign.h"
#include "game/campaign/localization_file.h"
#include "scenario/property.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int object_id;
    uint8_t *source_name;
    uint8_t *localized_name;
    int name_seen;
} localized_city;

static struct {
    localized_city *cities;
    size_t city_count;
    int current_city;
    int success;
    int root_seen;
    int root_closed;
    int loaded;
    char campaign_path[FILE_NAME_MAX];
    int mission;
} data;

static int xml_start_root(void);
static void xml_end_root(void);
static int xml_start_city(void);
static void xml_end_city(void);
static int xml_start_city_name(void);
static void xml_on_city_name(const char *text);
static void unexpected_text(const char *text);

static const xml_parser_element xml_elements[] = {
    { "empire_localization", xml_start_root, xml_end_root, 0, unexpected_text },
    { "city", xml_start_city, xml_end_city, "empire_localization", unexpected_text },
    { "name", xml_start_city_name, 0, "city", xml_on_city_name },
};

static int fail(const char *reason, const char *value)
{
    log_error(reason, value, 0);
    data.success = 0;
    return 0;
}

static int is_empty(const char *text)
{
    return !text || strspn(text, " \t\r\n") == strlen(text);
}

static void unexpected_text(const char *text)
{
    if (!is_empty(text)) {
        fail("Unexpected text in empire localization", 0);
    }
}

static uint8_t *copy_encoded_text(const char *text)
{
    if (is_empty(text)) {
        return 0;
    }
    size_t length = strlen(text);
    if (length >= INT_MAX) {
        data.success = 0;
        return 0;
    }
    uint8_t *encoded = malloc(length + 1);
    if (!encoded) {
        data.success = 0;
        return 0;
    }
    encoding_from_utf8(text, encoded, (int) length + 1);
    return encoded;
}

static int xml_start_root(void)
{
    const char *version = xml_parser_get_attribute_string("version");
    if (data.root_seen || !version || strcmp(version, "1") != 0) {
        return fail("Invalid empire localization root or version", version);
    }
    data.root_seen = 1;
    return 1;
}

static void xml_end_root(void)
{
    data.root_closed = 1;
}

static int xml_start_city(void)
{
    data.current_city = -1;
    if (!data.success || !xml_parser_has_attribute("object-id") || !xml_parser_has_attribute("source")) {
        return fail("Empire localization city needs object-id and source", 0);
    }

    const char *object_id_text = xml_parser_get_attribute_string("object-id");
    const char *source = xml_parser_get_attribute_string("source");
    if (!object_id_text || !*object_id_text || is_empty(source) || strlen(source) >= FILE_NAME_MAX) {
        return fail("Invalid empire localization city", source);
    }
    int object_id = 0;
    for (const char *digit = object_id_text; *digit; digit++) {
        if (*digit < '0' || *digit > '9' || object_id > (INT_MAX - (*digit - '0')) / 10) {
            return fail("Invalid empire localization object-id", object_id_text);
        }
        object_id = object_id * 10 + (*digit - '0');
    }
    for (size_t i = 0; i < data.city_count; i++) {
        if (data.cities[i].object_id == object_id) {
            return fail("Duplicate empire localization object-id", source);
        }
    }

    if (data.city_count == SIZE_MAX / sizeof(localized_city) || data.city_count >= INT_MAX) {
        return fail("Too many empire localization cities", source);
    }
    localized_city *cities = realloc(data.cities, (data.city_count + 1) * sizeof(localized_city));
    if (!cities) {
        return fail("Unable to allocate empire localization city", source);
    }
    data.cities = cities;
    localized_city *city = &data.cities[data.city_count];
    memset(city, 0, sizeof(*city));
    city->object_id = object_id;
    city->source_name = copy_encoded_text(source);
    if (!city->source_name) {
        return fail("Invalid empire localization source name", source);
    }
    data.current_city = (int) data.city_count;
    data.city_count++;
    return 1;
}

static void xml_end_city(void)
{
    if (data.current_city >= 0 && !data.cities[data.current_city].localized_name) {
        fail("Empire localization city has no translated name", 0);
    }
    data.current_city = -1;
}

static int xml_start_city_name(void)
{
    if (data.current_city < 0 || !data.success) {
        return 0;
    }
    localized_city *city = &data.cities[data.current_city];
    if (city->name_seen) {
        return fail("Duplicate empire localization city name", 0);
    }
    city->name_seen = 1;
    return 1;
}

static void xml_on_city_name(const char *text)
{
    if (data.current_city < 0 || !data.success) {
        return;
    }
    localized_city *city = &data.cities[data.current_city];
    if (city->localized_name) {
        fail("Duplicate empire localization city name", 0);
        return;
    }
    city->localized_name = copy_encoded_text(text);
    if (!city->localized_name) {
        fail("Empire localization city has an empty translated name", 0);
    }
}

static int parse_localization(const char *xml_text, size_t xml_size)
{
    data.current_city = -1;
    data.success = 1;
    int parsed = xml_parser_init(xml_elements, sizeof(xml_elements) / sizeof(xml_elements[0]), 1) &&
        xml_parser_parse(xml_text, (unsigned int) xml_size, 1) && data.success;
    xml_parser_free();
    if (!parsed || !data.root_seen || !data.root_closed) {
        return 0;
    }
    return 1;
}

void empire_city_localization_clear(void)
{
    for (size_t i = 0; i < data.city_count; i++) {
        free(data.cities[i].source_name);
        free(data.cities[i].localized_name);
    }
    free(data.cities);
    memset(&data, 0, sizeof(data));
    data.current_city = -1;
}

int empire_city_localization_load(void)
{
    empire_city_localization_clear();
    if (editor_is_active() || !game_campaign_is_active() || !game_campaign_is_custom()) {
        return 0;
    }

    const campaign_scenario *camp_scenario = game_campaign_get_scenario(scenario_campaign_mission());
    const char *campaign_path = game_campaign_get_name();
    if (!camp_scenario || !camp_scenario->path || !campaign_path ||
        strlen(campaign_path) >= sizeof(data.campaign_path)) {
        return 0;
    }
    char scenario_name[FILE_NAME_MAX];
    int scenario_name_size = snprintf(scenario_name, FILE_NAME_MAX, "%s", file_remove_path(camp_scenario->path));
    if (scenario_name_size < 0 || scenario_name_size >= FILE_NAME_MAX) {
        log_error("Custom campaign scenario name is too long for empire localization", camp_scenario->path, 0);
        return 0;
    }
    file_remove_extension(scenario_name);

    char relative_path[FILE_NAME_MAX];
    int relative_path_size = snprintf(relative_path, FILE_NAME_MAX, "empire/%s.xml", scenario_name);
    if (relative_path_size < 0 || relative_path_size >= FILE_NAME_MAX) {
        log_error("Empire localization path is too long", scenario_name, 0);
        return 0;
    }
    char language[FILE_NAME_MAX] = { 0 };
    size_t xml_size = 0;
    char *xml_text = campaign_localization_load_file(relative_path, language, &xml_size);
    if (!xml_text) {
        return 0;
    }
    if (!xml_size || xml_size > INT_MAX / 2) {
        free(xml_text);
        log_error("Invalid empire localization file size", relative_path, 0);
        return 0;
    }

    int result = parse_localization(xml_text, xml_size);
    free(xml_text);
    if (!result) {
        empire_city_localization_clear();
        log_error("Unable to load empire localization", relative_path, 0);
        return 0;
    }
    snprintf(data.campaign_path, sizeof(data.campaign_path), "%s", campaign_path);
    data.mission = scenario_campaign_mission();
    data.loaded = 1;
    log_info("Loaded empire city localization", relative_path, 0);
    return 1;
}

const uint8_t *empire_city_localization_get_name(int object_id, const uint8_t *source_name)
{
    if (!data.loaded || !source_name || editor_is_active() ||
        !game_campaign_is_active() || !game_campaign_is_custom()) {
        return source_name;
    }
    const char *campaign_path = game_campaign_get_name();
    if (!campaign_path || strcmp(campaign_path, data.campaign_path) != 0 ||
        scenario_campaign_mission() != data.mission) {
        return source_name;
    }
    for (size_t i = 0; i < data.city_count; i++) {
        const localized_city *city = &data.cities[i];
        if (city->object_id == object_id && city->localized_name &&
            string_equals(city->source_name, source_name)) {
            return city->localized_name;
        }
    }
    return source_name;
}
