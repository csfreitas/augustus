#include "custom_messages_localization.h"

#include "core/encoding.h"
#include "core/file.h"
#include "core/log.h"
#include "core/xml_parser.h"
#include "game/campaign.h"
#include "game/campaign/file.h"
#include "game/campaign/localization_file.h"
#include "scenario/custom_messages.h"
#include "scenario/property.h"

#include <stdlib.h>
#include <string.h>

#define CUSTOM_MESSAGE_LOCALIZATION_VERSION 1
#define CUSTOM_MESSAGE_MEDIA_LOCALIZATION_VERSION 1
#define XML_TOTAL_ELEMENTS 5
#define XML_MEDIA_TOTAL_ELEMENTS 4

typedef struct {
    uint8_t *title;
    uint8_t *subtitle;
    uint8_t *text;
    uint8_t *speech;
    uint8_t *background_music;
} localized_custom_message;

typedef enum {
    LOCALIZED_FIELD_TITLE,
    LOCALIZED_FIELD_SUBTITLE,
    LOCALIZED_FIELD_TEXT,
} localized_custom_message_field;

static struct {
    localized_custom_message *messages;
    uint8_t *seen_messages;
    int message_count;
    int current_message_id;
    int success;
    char active_language[FILE_NAME_MAX];
} data;

static int xml_start_localization(void);
static int xml_start_message(void);
static void xml_end_message(void);
static void xml_on_title(const char *text);
static void xml_on_subtitle(const char *text);
static void xml_on_text(const char *text);
static int xml_start_media_localization(void);
static int xml_start_media_message(void);
static void xml_end_media_message(void);
static int xml_start_speech(void);
static int xml_start_media_background_music(void);

static const xml_parser_element xml_elements[XML_TOTAL_ELEMENTS] = {
    { "localization", xml_start_localization },
    { "message", xml_start_message, xml_end_message, "localization" },
    { "title", 0, 0, "message", xml_on_title },
    { "subtitle", 0, 0, "message", xml_on_subtitle },
    { "text", 0, 0, "message", xml_on_text },
};

static const xml_parser_element media_xml_elements[XML_MEDIA_TOTAL_ELEMENTS] = {
    { "media_localization", xml_start_media_localization },
    { "message", xml_start_media_message, xml_end_media_message, "media_localization" },
    { "speech", xml_start_speech, 0, "message" },
    { "background_music", xml_start_media_background_music, 0, "message" },
};

void custom_messages_localization_clear(void)
{
    if (data.messages) {
        for (int i = 0; i <= data.message_count; i++) {
            free(data.messages[i].title);
            free(data.messages[i].subtitle);
            free(data.messages[i].text);
            free(data.messages[i].speech);
            free(data.messages[i].background_music);
        }
        free(data.messages);
    }
    free(data.seen_messages);
    memset(&data, 0, sizeof(data));
}

static void clear_localized_media(void)
{
    if (!data.messages) {
        return;
    }
    for (int i = 0; i <= data.message_count; i++) {
        free(data.messages[i].speech);
        free(data.messages[i].background_music);
        data.messages[i].speech = 0;
        data.messages[i].background_music = 0;
    }
}

static uint8_t *copy_encoded_text(const char *text)
{
    if (!text || !*text) {
        return 0;
    }
    size_t length = strlen(text) + 1;
    uint8_t *result = malloc(length);
    if (!result) {
        data.success = 0;
        return 0;
    }
    encoding_from_utf8(text, result, (int) length);
    return result;
}

static int xml_start_localization(void)
{
    int version = xml_parser_get_attribute_int("version");
    if (version != CUSTOM_MESSAGE_LOCALIZATION_VERSION) {
        log_error("Unsupported custom message localization version", 0, version);
        data.success = 0;
        return 0;
    }
    return 1;
}

static int xml_start_message(void)
{
    data.current_message_id = 0;
    if (!xml_parser_has_attribute("uid")) {
        log_error("Custom message localization entry has no uid", 0, 0);
        data.success = 0;
        return 0;
    }

    const char *uid = xml_parser_get_attribute_string("uid");
    if (!uid || strlen(uid) >= FILE_NAME_MAX) {
        log_error("Custom message localization uid is too long", 0, 0);
        data.success = 0;
        return 0;
    }
    uint8_t encoded_uid[FILE_NAME_MAX];
    encoding_from_utf8(uid, encoded_uid, FILE_NAME_MAX);
    int message_id = custom_messages_get_id_by_uid(encoded_uid);
    if (!message_id) {
        log_error("Unknown custom message uid in localization overlay", uid, 0);
        return 1;
    }
    if (message_id > data.message_count || data.seen_messages[message_id]) {
        log_error("Duplicate custom message uid in localization overlay", uid, 0);
        data.success = 0;
        return 0;
    }

    data.seen_messages[message_id] = 1;
    data.current_message_id = message_id;
    return 1;
}

static void xml_end_message(void)
{
    data.current_message_id = 0;
}

static void replace_text(uint8_t **destination, const char *text)
{
    if (!data.current_message_id) {
        return;
    }
    uint8_t *replacement = copy_encoded_text(text);
    if (!data.success) {
        return;
    }
    free(*destination);
    *destination = replacement;
}

static void xml_on_title(const char *text)
{
    if (data.current_message_id) {
        replace_text(&data.messages[data.current_message_id].title, text);
    }
}

static void xml_on_subtitle(const char *text)
{
    if (data.current_message_id) {
        replace_text(&data.messages[data.current_message_id].subtitle, text);
    }
}

static void xml_on_text(const char *text)
{
    if (data.current_message_id) {
        replace_text(&data.messages[data.current_message_id].text, text);
    }
}

static int xml_start_media_localization(void)
{
    int version = xml_parser_get_attribute_int("version");
    if (version != CUSTOM_MESSAGE_MEDIA_LOCALIZATION_VERSION) {
        log_error("Unsupported custom message media localization version", 0, version);
        data.success = 0;
        return 0;
    }
    return 1;
}

static int xml_start_media_message(void)
{
    data.current_message_id = 0;
    if (!xml_parser_has_attribute("uid")) {
        log_error("Custom message media localization entry has no uid", 0, 0);
        data.success = 0;
        return 0;
    }
    const char *uid = xml_parser_get_attribute_string("uid");
    if (!uid || strlen(uid) >= FILE_NAME_MAX) {
        log_error("Custom message media localization uid is too long", 0, 0);
        data.success = 0;
        return 0;
    }
    uint8_t encoded_uid[FILE_NAME_MAX];
    encoding_from_utf8(uid, encoded_uid, FILE_NAME_MAX);
    int message_id = custom_messages_get_id_by_uid(encoded_uid);
    if (!message_id) {
        log_error("Unknown custom message uid in media localization overlay", uid, 0);
        return 1;
    }
    if (message_id > data.message_count || data.seen_messages[message_id]) {
        log_error("Duplicate custom message uid in media localization overlay", uid, 0);
        data.success = 0;
        return 0;
    }
    data.seen_messages[message_id] = 1;
    data.current_message_id = message_id;
    return 1;
}

static void xml_end_media_message(void)
{
    data.current_message_id = 0;
}

static int is_safe_media_filename(const char *filename)
{
    if (!filename || !*filename) {
        return 0;
    }
    size_t length = strlen(filename);
    if (length >= FILE_NAME_MAX || filename[0] == ' ' || filename[0] == '.' ||
        filename[length - 1] == ' ' || filename[length - 1] == '.') {
        return 0;
    }
    for (const unsigned char *character = (const unsigned char *) filename; *character; character++) {
        if (*character < 0x20 || *character > 0x7e || strchr("<>:\"/\\|?*", *character)) {
            return 0;
        }
    }
    return 1;
}

static int replace_media_filename(uint8_t **destination, const char *description)
{
    if (!data.current_message_id || !xml_parser_has_attribute("filename")) {
        log_error(description, 0, 0);
        data.success = 0;
        return 0;
    }
    const char *filename = xml_parser_get_attribute_string("filename");
    if (!is_safe_media_filename(filename) || *destination) {
        log_error(description, filename, 0);
        data.success = 0;
        return 0;
    }
    *destination = copy_encoded_text(filename);
    return data.success;
}

static int xml_start_speech(void)
{
    if (!data.current_message_id) {
        return 1;
    }
    return replace_media_filename(&data.messages[data.current_message_id].speech,
        "Invalid or duplicate localized speech filename");
}

static int xml_start_media_background_music(void)
{
    if (!data.current_message_id) {
        return 1;
    }
    return replace_media_filename(&data.messages[data.current_message_id].background_music,
        "Invalid or duplicate localized background music filename");
}

static int parse_localization(const char *xml_text, size_t xml_size)
{
    data.message_count = custom_messages_count();
    data.messages = calloc((size_t) data.message_count + 1, sizeof(localized_custom_message));
    data.seen_messages = calloc((size_t) data.message_count + 1, sizeof(uint8_t));
    if (!data.messages || !data.seen_messages) {
        log_error("Unable to allocate custom message localization data", 0, 0);
        custom_messages_localization_clear();
        return 0;
    }

    data.success = 1;
    if (!xml_parser_init(xml_elements, XML_TOTAL_ELEMENTS, 1) ||
        !xml_parser_parse(xml_text, (unsigned int) xml_size, 1) || !data.success) {
        xml_parser_free();
        custom_messages_localization_clear();
        return 0;
    }
    xml_parser_free();
    free(data.seen_messages);
    data.seen_messages = 0;
    return 1;
}

static int parse_media_localization(const char *xml_text, size_t xml_size)
{
    data.seen_messages = calloc((size_t) data.message_count + 1, sizeof(uint8_t));
    if (!data.seen_messages) {
        return 0;
    }
    data.success = 1;
    int parsed = xml_parser_init(media_xml_elements, XML_MEDIA_TOTAL_ELEMENTS, 1) &&
        xml_parser_parse(xml_text, (unsigned int) xml_size, 1) && data.success;
    xml_parser_free();
    free(data.seen_messages);
    data.seen_messages = 0;
    if (!parsed) {
        clear_localized_media();
    }
    return parsed;
}

static char *load_media_localization_file(const char *language, const char *scenario_name,
    char *path, size_t path_length, size_t *xml_size)
{
    int path_size = snprintf(path, path_length, "localization/%s/media/%s.xml", language, scenario_name);
    if (path_size < 0 || (size_t) path_size >= path_length) {
        log_error("Custom message media localization path is too long", scenario_name, 0);
        return 0;
    }
    return campaign_file_load(path, xml_size);
}

int custom_messages_localization_load(void)
{
    custom_messages_localization_clear();
    if (!game_campaign_is_active() || !game_campaign_is_custom()) {
        return 0;
    }
    if (custom_messages_count() <= 0) {
        return 0;
    }

    const campaign_scenario *scenario = game_campaign_get_scenario(scenario_campaign_mission());
    if (!scenario || !scenario->path) {
        return 0;
    }

    char scenario_name[FILE_NAME_MAX];
    int scenario_name_size = snprintf(scenario_name, FILE_NAME_MAX, "%s", file_remove_path(scenario->path));
    if (scenario_name_size < 0 || scenario_name_size >= FILE_NAME_MAX) {
        log_error("Custom campaign scenario name is too long for localization", scenario->path, 0);
        return 0;
    }
    file_remove_extension(scenario_name);

    char path[FILE_NAME_MAX];
    int path_size = snprintf(path, FILE_NAME_MAX, "messages/%s.xml", scenario_name);
    if (path_size < 0 || path_size >= FILE_NAME_MAX) {
        log_error("Custom message localization path is too long", scenario_name, 0);
        return 0;
    }
    size_t xml_size = 0;
    char resolved_language[FILE_NAME_MAX];
    char *xml_text = campaign_localization_load_file(path, resolved_language, &xml_size);
    if (!xml_text) {
        return 0;
    }

    int result = parse_localization(xml_text, xml_size);
    free(xml_text);
    snprintf(path, FILE_NAME_MAX, "localization/%s/messages/%s.xml", resolved_language, scenario_name);
    if (result) {
        snprintf(data.active_language, FILE_NAME_MAX, "%s", resolved_language);
        log_info("Loaded custom message localization", path, 0);
        size_t media_xml_size = 0;
        char *media_xml_text = load_media_localization_file(data.active_language, scenario_name,
            path, FILE_NAME_MAX, &media_xml_size);
        if (media_xml_text) {
            if (parse_media_localization(media_xml_text, media_xml_size)) {
                log_info("Loaded custom message media localization", path, 0);
            } else {
                log_error("Unable to load custom message media localization", path, 0);
            }
            free(media_xml_text);
        }
    } else {
        log_error("Unable to load custom message localization", path, 0);
    }
    return result;
}

static const char *get_localized_media_path(uint8_t *filename)
{
    if (!filename || !*data.active_language) {
        return 0;
    }
    static char path[FILE_NAME_MAX];
    char filename_utf8[FILE_NAME_MAX];
    for (int i = 0; i <= encoding_system_uses_decomposed(); i++) {
        encoding_to_utf8(filename, filename_utf8, FILE_NAME_MAX, i);
        int path_size = snprintf(path, FILE_NAME_MAX, CAMPAIGNS_DIRECTORY
            "/localization/%s/audio/%s", data.active_language, filename_utf8);
        if (path_size < 0 || path_size >= FILE_NAME_MAX) {
            return 0;
        }
        if (game_campaign_has_file(path)) {
            return path;
        }
    }
    return 0;
}

static uint8_t *get_localized_field(int message_id, localized_custom_message_field field)
{
    if (!data.messages || message_id <= 0 || message_id > data.message_count) {
        return 0;
    }
    switch (field) {
        case LOCALIZED_FIELD_TITLE:
            return data.messages[message_id].title;
        case LOCALIZED_FIELD_SUBTITLE:
            return data.messages[message_id].subtitle;
        case LOCALIZED_FIELD_TEXT:
            return data.messages[message_id].text;
        default:
            return 0;
    }
}

uint8_t *custom_messages_localization_get_title(int message_id)
{
    return get_localized_field(message_id, LOCALIZED_FIELD_TITLE);
}

uint8_t *custom_messages_localization_get_subtitle(int message_id)
{
    return get_localized_field(message_id, LOCALIZED_FIELD_SUBTITLE);
}

uint8_t *custom_messages_localization_get_text(int message_id)
{
    return get_localized_field(message_id, LOCALIZED_FIELD_TEXT);
}

const char *custom_messages_localization_get_speech(int message_id)
{
    if (!data.messages || message_id <= 0 || message_id > data.message_count) {
        return 0;
    }
    return get_localized_media_path(data.messages[message_id].speech);
}

const char *custom_messages_localization_get_background_music(int message_id)
{
    if (!data.messages || message_id <= 0 || message_id > data.message_count) {
        return 0;
    }
    return get_localized_media_path(data.messages[message_id].background_music);
}
