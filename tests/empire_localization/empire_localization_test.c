/* Real empire/campaign/message overlays, campaign lifecycle, folder/ZIP loading and encoding.
 * Adapters below isolate OS roots, settings, progression and scenario execution.
 * No game assets, saves or profiles are required or modified. */
#include "core/config.h"
#include "core/encoding.h"
#include "core/file.h"
#include "core/lang.h"
#include "core/log.h"
#include "game/campaign.h"
#include "game/campaign/file.h"
#include "game/campaign/mission.h"
#include "game/campaign/localization.h"
#include "game/file.h"
#include "editor/editor.h"
#include "empire/localization.h"
#include "scenario/custom_messages.h"
#include "scenario/custom_messages_localization.h"
#include "scenario/property.h"
#include <limits.h>
#include "zip/zip.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define make_directory(path) _mkdir(path)
#else
#define make_directory(path) mkdir(path, 0700)
#endif

static const char *ui_language = "pt-BR";
static language_type detected_language = LANGUAGE_PORTUGUESE;
static int checks, failures, cases, archive_mode, rank, progression_reads, progression_writes, rank_updates, fixture_id;
static const char *case_name;
static char fixture_root[512];
static char run_root[64];
static struct zip_t *fixture_zip;
static int editor_active, scenario_id;
static uint8_t source_rome[] = "Rome";

int editor_is_active(void) { return editor_active; }
int scenario_campaign_mission(void) { return scenario_id; }
int custom_messages_count(void) { return 1; }
int custom_messages_get_id_by_uid(const uint8_t *uid) { return strcmp((const char *) uid, "briefing") == 0 ? 1 : 0; }
const char *config_get_string(config_string_key key) { (void) key; return ui_language; }
language_type locale_last_determined_language(void) { return detected_language; }
const uint8_t *lang_get_string(int group, int index) {
    (void) group; (void) index; return (const uint8_t *) "Original campaign text";
}
void log_info(const char *m, const char *s, int i) { (void) m; (void) s; (void) i; }
void log_error(const char *m, const char *s, int i) { (void) m; (void) s; (void) i; }
int city_emperor_rank(void) { return rank; }
void city_emperor_set_rank(int value) { rank = value; rank_updates++; }
int campaign_player_data_get_current_mission(const char *name) { (void) name; progression_reads++; return 1; }
void campaign_player_data_update_current_mission(const char *name, int value) {
    (void) name; (void) value; progression_writes++;
}
int campaign_original_setup(void) { return campaign_mission_init(); }
uint8_t *campaign_original_load_scenario(int id, size_t *size) { (void) id; *size = 0; return NULL; }
int game_file_start_scenario_from_buffer(uint8_t *data, int size, int save) {
    (void) data; (void) size; (void) save; return 0;
}
int game_file_io_read_scenario_info_from_buffer(buffer *buf, saved_game_info *info) {
    (void) buf; (void) info; return 0;
}
int game_file_io_read_saved_game_info_from_buffer(buffer *buf, saved_game_info *info) {
    (void) buf; (void) info; return 0;
}
FILE *file_open(const char *name, const char *mode) { return fopen(name, mode); }
int file_close(FILE *file) { return fclose(file); }
const char *dir_get_file_at_location(const char *name, int location) {
    (void) location;
    struct stat info;
    return stat(name, &info) == 0 ? name : NULL;
}
int file_exists(const char *name, int localizable) {
    (void) localizable; return dir_get_file_at_location(name, 0) != NULL;
}
int file_has_extension(const char *name, const char *ext) {
    const char *dot = strrchr(name, '.');
    return dot && strcmp(dot + 1, ext) == 0;
}
const char *file_remove_path(const char *name) {
    const char *result = name;
    for (; *name; name++) if (*name == '/' || *name == '\\') result = name + 1;
    return result;
}
void file_remove_extension(char *name) { char *dot = strrchr(name, '.'); if (dot) *dot = 0; }

static void check(int success, const char *expression, int line)
{
    checks++;
    if (!success) {
        failures++;
        printf("FAIL [%s/%s] %d: %s\n", archive_mode ? "ZIP" : "folder", case_name, line, expression);
    }
}
#define CHECK(expression) check(!!(expression), #expression, __LINE__)
static int equals(const uint8_t *actual, const char *expected)
{
    if (!actual) return expected == NULL;
    if (!expected) return 0;
    char converted[4096];
    encoding_to_utf8(actual, converted, sizeof(converted), 0);
    return strcmp(converted, expected) == 0;
}
static void add_file(const char *relative, const char *content)
{
    if (fixture_zip) {
        if (zip_entry_open(fixture_zip, relative) || zip_entry_write(fixture_zip, content, strlen(content))) exit(2);
        zip_entry_close(fixture_zip);
        return;
    }
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", fixture_root, relative);
    for (char *p = path; *p; p++) {
        if (*p != '/') continue;
        *p = 0; make_directory(path); *p = '/';
    }
    FILE *file = fopen(path, "wb");
    if (!file) exit(2);
    if (fwrite(content, 1, strlen(content), file) != strlen(content)) exit(2);
    fclose(file);
}
static void add_translation(const char *locale, const char *text)
{
    char path[256];
    snprintf(path, sizeof(path), "localization/%s/campaign.xml", locale);
    add_file(path, text);
}
static const char *settings =
    "<campaign version=\"1\"><description title=\"Canonical campaign\" author=\"Author\">Canonical description</description>"
    "<missions starting_rank=\"clerk\"><mission title=\"First mission\" next_rank=\"engineer\">"
    "<scenario file=\"One.mapx\" name=\"One\" description=\"One description\"/>"
    "<scenario file=\"Two.svx\" name=\"Two\" description=\"Two description\"/></mission>"
    "<mission title=\"Second mission\" file=\"Three.mapx\" name=\"Three\"/></missions></campaign>";
static const char *localized =
    "<campaign_localization version=\"1\" language=\"pt-BR\"><name>Campanha traduzida</name>"
    "<description>Descrição e ação</description><mission first-scenario=\"One\"><title>Missão inicial</title></mission>"
    "<scenario file=\"One\"><name>Primeiro</name><description>Descrição do primeiro</description></scenario>"
    "<scenario file=\"Two\"><name>Segundo</name></scenario></campaign_localization>";

static void create_fixture(const char *canonical)
{
    snprintf(fixture_root, sizeof(fixture_root), "%s/case_%03d%s", run_root, fixture_id++, archive_mode ? ".campaign" : "");
    if (archive_mode) {
        fixture_zip = zip_open(fixture_root, 6, 'w');
        if (!fixture_zip) exit(2);
    } else make_directory(fixture_root);
    add_file("settings.xml", canonical ? canonical : settings);
    add_file("scenario/One.mapx", "synthetic");
    add_file("scenario/Two.svx", "synthetic");
    add_file("scenario/Three.mapx", "synthetic");
}
static void begin_case(const char *name, const char *canonical)
{
    empire_city_localization_clear();
    custom_messages_localization_clear();
    game_campaign_clear();
    editor_active = scenario_id = 0;
    cases++; case_name = name;
    ui_language = "pt-BR"; detected_language = LANGUAGE_PORTUGUESE;
    encoding_determine(detected_language);
    rank = 7; progression_reads = 0; progression_writes = 0; rank_updates = 0;
    create_fixture(canonical);
}
static void finish_fixture(void)
{
    if (fixture_zip) { zip_close(fixture_zip); fixture_zip = NULL; }
}
static void load(void)
{
    finish_fixture();
    CHECK(game_campaign_load(fixture_root));
}
static void check_canonical(void)
{
    CHECK(equals(game_campaign_get_info()->name, "Canonical campaign"));
    CHECK(equals(game_campaign_get_info()->description, "Canonical description"));
    CHECK(equals(game_campaign_get_info()->author, "Author"));
    CHECK(equals(game_campaign_get_scenario(0)->name, "One"));
    CHECK(equals(campaign_mission_current(0)->title, "First mission"));
    CHECK(strcmp(game_campaign_get_scenario(0)->path, "***campaigns***/scenario/One.mapx") == 0);
    CHECK(strcmp(game_campaign_get_name(), fixture_root) == 0);
    CHECK(game_campaign_get_info()->current_mission == 1);
    CHECK(progression_writes == 0);
    CHECK(rank == 7); /* Display getters must not call resolve_rank. */
}

static const char *empire_xml =
    "<empire_localization version=\"1\"><city object-id=\"1\" source=\"Rome\"><name>Roma</name></city>"
    "<city object-id=\"2\" source=\"Carthage\"><name>Cartago</name></city>"
    "<city object-id=\"3\" source=\"Lugdúnum\"><name>Lião</name></city></empire_localization>";

static void add_empire(const char *locale, const char *scenario, const char *xml)
{
    char path[512];
    snprintf(path, sizeof(path), "localization/%s/empire/%s.xml", locale, scenario);
    add_file(path, xml);
}

static void expect_original(void)
{
    CHECK(empire_city_localization_get_name(1, source_rome) == source_rome);
}

static void test_names_and_lifecycle(void)
{
    begin_case("object identity, source guard and lifecycle", NULL);
    add_empire("pt-BR", "One", empire_xml);
    add_empire("pt-BR", "Two", "<empire_localization version=\"1\"><city object-id=\"1\" source=\"Rome\">"
        "<name>Segunda Roma</name></city></empire_localization>");
    load();
    CHECK(empire_city_localization_load());
    CHECK(equals(empire_city_localization_get_name(1, source_rome), "Roma"));
    CHECK(equals(empire_city_localization_get_name(2, (const uint8_t *) "Carthage"), "Cartago"));
    CHECK(empire_city_localization_get_name(2, source_rome) == source_rome);
    uint8_t accented_source[32];
    encoding_from_utf8("Lugdúnum", accented_source, sizeof(accented_source));
    CHECK(equals(empire_city_localization_get_name(3, accented_source), "Lião"));
    CHECK(equals(accented_source, "Lugdúnum"));
    CHECK(empire_city_localization_get_name(99, source_rome) == source_rome);
    CHECK(empire_city_localization_get_name(-1, source_rome) == source_rome);
    CHECK(!empire_city_localization_get_name(1, NULL));
    uint8_t renamed[] = "New Rome";
    uint8_t lowercase[] = "rome";
    CHECK(empire_city_localization_get_name(1, renamed) == renamed);
    CHECK(empire_city_localization_get_name(1, lowercase) == lowercase);
    CHECK(strcmp((const char *) source_rome, "Rome") == 0);
    check_canonical();

    editor_active = 1;
    expect_original();
    CHECK(!empire_city_localization_load());
    editor_active = 0;
    expect_original();
    CHECK(empire_city_localization_load());

    game_campaign_suspend();
    expect_original();
    CHECK(!empire_city_localization_load());
    game_campaign_restore();
    expect_original();
    CHECK(empire_city_localization_load());
    CHECK(equals(empire_city_localization_get_name(1, source_rome), "Roma"));

    scenario_id = 1; // The saved-scenario extension resolves to empire/Two.xml.
    expect_original();
    CHECK(empire_city_localization_load());
    CHECK(equals(empire_city_localization_get_name(1, source_rome), "Segunda Roma"));
    scenario_id = 2; // This scenario has no overlay.
    CHECK(!empire_city_localization_load());
    expect_original();
    scenario_id = 99;
    CHECK(!empire_city_localization_load());
    expect_original();
    scenario_id = 0;
    CHECK(empire_city_localization_load());
    CHECK(game_campaign_load(CAMPAIGN_ORIGINAL_NAME));
    expect_original();
    CHECK(!empire_city_localization_load());
    CHECK(game_campaign_load(fixture_root));
    CHECK(empire_city_localization_load());
    create_fixture(NULL);
    add_empire("pt-BR", "One", empire_xml);
    load();
    expect_original();
    CHECK(empire_city_localization_load());
    CHECK(equals(empire_city_localization_get_name(1, source_rome), "Roma"));
    game_campaign_clear();
    expect_original();
    CHECK(!empire_city_localization_load());
    empire_city_localization_clear();
    expect_original();
}

static void test_invalid_documents(void)
{
    const char *invalid[] = {
        "", "<!-- no root -->", "<wrong/>",
        "<empire_localization/>", "<empire_localization version=\"2\"/>",
        "<empire_localization version=\"1\"><city object-id=\"1\" source=\"Rome\"><name>Roma</name></city>",
        "<empire_localization version=\"1\"><city object-id=\"1\" source=\"Rome\"><name>Roma</city></empire_localization>",
        "<empire_localization version=\"1\"/><empire_localization version=\"1\"/>",
        "<empire_localization version=\"1\"><unknown/></empire_localization>",
        "<empire_localization version=\"1\">unexpected</empire_localization>",
        "<empire_localization version=\"1\"><city object-id=\"1\" source=\"Rome\">unexpected<name>Roma</name></city></empire_localization>",
        "<empire_localization version=\"1\"><city source=\"Rome\"><name>Roma</name></city></empire_localization>",
        "<empire_localization version=\"1\"><city object-id=\"1\"><name>Roma</name></city></empire_localization>",
        "<empire_localization version=\"1\"><city object-id=\"1\" source=\" \"><name>Roma</name></city></empire_localization>",
        "<empire_localization version=\"1\"><city object-id=\"1\" source=\"Rome\"/></empire_localization>",
        "<empire_localization version=\"1\"><city object-id=\"1\" source=\"Rome\"><name/></city></empire_localization>",
        "<empire_localization version=\"1\"><city object-id=\"1\" source=\"Rome\"><name> </name></city></empire_localization>",
        "<empire_localization version=\"1\"><city object-id=\"1\" source=\"Rome\"><name>Roma</name><name>Other</name></city></empire_localization>",
        "<empire_localization version=\"1\"><city object-id=\"1\" source=\"Rome\"><name/><name>Roma</name></city></empire_localization>",
        "<empire_localization version=\"1\"><city object-id=\"1\" source=\"Rome\"><name>Roma</name><name/></city></empire_localization>",
        "<empire_localization version=\"1\"><city object-id=\"1\" source=\"Rome\"><name>Roma</name></city>"
            "<city object-id=\"1\" source=\"Different\"><name>Other</name></city></empire_localization>",
        "<empire_localization version=\"1\"><city object-id=\"1\" source=\"Rome\"><name>Roma</name></city>"
            "<city object-id=\"01\" source=\"Rome\"><name>Other</name></city></empire_localization>"
    };
    for (unsigned int i = 0; i < sizeof(invalid) / sizeof(*invalid); i++) {
        char label[80]; snprintf(label, sizeof(label), "invalid document atomic fallback %u", i);
        begin_case(label, NULL);
        add_empire("pt-BR", "One", empire_xml);
        add_empire("broken", "One", invalid[i]);
        load();
        CHECK(empire_city_localization_load());
        ui_language = "broken";
        CHECK(!empire_city_localization_load());
        expect_original();
        ui_language = "pt-BR";
        CHECK(empire_city_localization_load());
        CHECK(equals(empire_city_localization_get_name(1, source_rome), "Roma"));
        check_canonical();
    }

    const char *invalid_ids[] = {
        "", "-1", "+1", " 1", "1 ", "1.0", "0x1", "1x",
        "2147483648", "4294967295", "9223372036854775807", "9999999999999999999999999999999999999999"
    };
    for (unsigned int i = 0; i < sizeof(invalid_ids) / sizeof(*invalid_ids); i++) {
        char label[80], xml[1024];
        snprintf(label, sizeof(label), "invalid or overflowing object id %s", invalid_ids[i]);
        begin_case(label, NULL);
        snprintf(xml, sizeof(xml), "<empire_localization version=\"1\">"
            "<city object-id=\"1\" source=\"Rome\"><name>Roma</name></city>"
            "<city object-id=\"%s\" source=\"Edge\"><name>Bad</name></city></empire_localization>", invalid_ids[i]);
        add_empire("pt-BR", "One", xml); load();
        CHECK(!empire_city_localization_load());
        expect_original();
        CHECK(equals(empire_city_localization_get_name(INT_MAX, (const uint8_t *) "Edge"), "Edge"));
    }
}

static void test_limits_and_missing(void)
{
    begin_case("valid object id boundaries", NULL);
    add_empire("pt-BR", "One", "<empire_localization version=\"1\">"
        "<city object-id=\"0\" source=\"Zero\"><name>Zero traduzido</name></city>"
        "<city object-id=\"2147483647\" source=\"Edge\"><name>Limite</name></city></empire_localization>");
    load(); CHECK(empire_city_localization_load());
    CHECK(equals(empire_city_localization_get_name(0, (const uint8_t *) "Zero"), "Zero traduzido"));
    CHECK(equals(empire_city_localization_get_name(INT_MAX, (const uint8_t *) "Edge"), "Limite"));
    expect_original();

    begin_case("long localized name does not truncate or alter source", NULL);
    char long_name[2048], xml[2400];
    memset(long_name, 'X', sizeof(long_name) - 1); long_name[sizeof(long_name) - 1] = 0;
    snprintf(xml, sizeof(xml), "<empire_localization version=\"1\"><city object-id=\"1\" source=\"Rome\">"
        "<name>%s</name></city></empire_localization>", long_name);
    add_empire("pt-BR", "One", xml); load(); CHECK(empire_city_localization_load());
    CHECK(equals(empire_city_localization_get_name(1, source_rome), long_name));
    CHECK(strcmp((const char *) source_rome, "Rome") == 0);
    check_canonical();

    begin_case("overlong source rejected", NULL);
    char long_source[FILE_NAME_MAX + 1];
    memset(long_source, 'S', sizeof(long_source) - 1); long_source[sizeof(long_source) - 1] = 0;
    snprintf(xml, sizeof(xml), "<empire_localization version=\"1\"><city object-id=\"1\" source=\"%s\">"
        "<name>Bad</name></city></empire_localization>", long_source);
    add_empire("pt-BR", "One", xml); load();
    CHECK(!empire_city_localization_load()); expect_original();

    begin_case("missing overlay", NULL);
    load(); CHECK(!empire_city_localization_load()); expect_original();

    begin_case("empty valid overlay", NULL);
    add_empire("pt-BR", "One", "<empire_localization version=\"1\"></empire_localization>");
    load(); CHECK(empire_city_localization_load()); expect_original();

    begin_case("multiple dots in scenario stem", "<campaign version=\"1\"><description title=\"Dots\"/>"
        "<missions><mission file=\"One.part.mapx\"/></missions></campaign>");
    add_file("scenario/One.part.mapx", "synthetic");
    add_empire("pt-BR", "One.part", empire_xml);
    load(); CHECK(empire_city_localization_load());
    CHECK(equals(empire_city_localization_get_name(1, source_rome), "Roma"));
}

static void test_locales(void)
{
    const struct { const char *requested, *resolved, *manifest; int expected; } locales[] = {
        { "PT_br", "pt-BR", NULL, 1 },
        { "Portuguese (Brazil)", "pt-BR", "<locales version=\"1\"><locale id=\"pt-BR\" aliases=\"Portuguese (Brazil)\"/></locales>", 1 },
        { "Português", "pt-BR", "<locales version=\"1\"><locale id=\"pt-BR\" aliases=\"Português\"/></locales>", 1 },
        { "", "pt-BR", "<locales version=\"1\"><locale id=\"pt-BR\" default-for=\"pt\"/></locales>", 1 },
        { "", "pt", NULL, 1 },
        { "pt", "pt-BR", "<locales version=\"1\"><locale id=\"pt-BR\" default-for=\"pt\"/></locales>", 0 },
        { "../pt-BR", "pt-BR", NULL, 0 },
        { "French", "fr", "<locales version=\"1\"><locale id=\"fr\" aliases=\"French\"/><locale id=\"fr-FR\" aliases=\"French\"/></locales>", 0 }
    };
    for (unsigned int i = 0; i < sizeof(locales) / sizeof(*locales); i++) {
        begin_case("shared locale resolution", NULL);
        ui_language = locales[i].requested;
        add_empire(locales[i].resolved, "One", empire_xml);
        if (locales[i].manifest) add_file("localization/locales.xml", locales[i].manifest);
        load();
        CHECK(empire_city_localization_load() == locales[i].expected);
        CHECK(equals(empire_city_localization_get_name(1, source_rome), locales[i].expected ? "Roma" : "Rome"));
    }

    const struct { const char *locale, *translation; language_type language; } encodings[] = {
        { "fr", "Cité française", LANGUAGE_FRENCH },
        { "ru", "Рим", LANGUAGE_RUSSIAN },
        { "el", "Ρώμη", LANGUAGE_GREEK },
        { "ja", "日本", LANGUAGE_JAPANESE }
    };
    for (unsigned int i = 0; i < sizeof(encodings) / sizeof(*encodings); i++) {
        begin_case("non-Portuguese internal encoding", NULL);
        ui_language = encodings[i].locale;
        encoding_determine(encodings[i].language);
        char xml[512];
        snprintf(xml, sizeof(xml), "<empire_localization version=\"1\"><city object-id=\"1\" source=\"Rome\">"
            "<name>%s</name></city></empire_localization>", encodings[i].translation);
        add_empire(ui_language, "One", xml); load();
        CHECK(empire_city_localization_load());
        CHECK(equals(empire_city_localization_get_name(1, source_rome), encodings[i].translation));
    }

    begin_case("language reload clears previous translations", NULL);
    add_empire("pt-BR", "One", empire_xml);
    add_empire("fr", "One", "<empire_localization version=\"1\"><city object-id=\"1\" source=\"Rome\">"
        "<name>Rome française</name></city></empire_localization>");
    load(); CHECK(empire_city_localization_load());
    ui_language = "fr"; CHECK(empire_city_localization_load());
    CHECK(equals(empire_city_localization_get_name(1, source_rome), "Rome française"));
    ui_language = "de"; CHECK(!empire_city_localization_load()); expect_original();
    ui_language = "pt-BR"; CHECK(empire_city_localization_load());
    CHECK(equals(empire_city_localization_get_name(1, source_rome), "Roma"));
}

static void test_overlay_isolation(void)
{
    begin_case("independent campaign, message and empire overlays", NULL);
    add_translation("pt-BR", localized);
    add_file("localization/pt-BR/messages/One.xml", "<localization version=\"1\"><message uid=\"briefing\">"
        "<title>Mensagem traduzida</title><text>Texto traduzido</text></message></localization>");
    add_empire("pt-BR", "One", empire_xml);
    add_empire("broken", "One", "<empire_localization version=\"1\"><city");
    load();
    CHECK(custom_messages_localization_load());
    const uint8_t *campaign_name = game_campaign_display_name();
    const uint8_t *message_title = custom_messages_localization_get_title(1);
    CHECK(equals(message_title, "Mensagem traduzida"));
    CHECK(empire_city_localization_load());
    const uint8_t *city_name = empire_city_localization_get_name(1, source_rome);
    CHECK(equals(city_name, "Roma"));
    CHECK(game_campaign_display_name() == campaign_name);
    CHECK(custom_messages_localization_get_title(1) == message_title);
    check_canonical();

    uint8_t *preview = campaign_localization_preview_name(fixture_root);
    CHECK(equals(preview, "Campanha traduzida")); free(preview);
    CHECK(empire_city_localization_get_name(1, source_rome) == city_name);
    game_campaign_reload_localization();
    CHECK(empire_city_localization_get_name(1, source_rome) == city_name);
    CHECK(custom_messages_localization_load());
    CHECK(empire_city_localization_get_name(1, source_rome) == city_name);
    campaign_name = game_campaign_display_name();
    message_title = custom_messages_localization_get_title(1);
    ui_language = "broken";
    CHECK(!empire_city_localization_load()); expect_original();
    CHECK(game_campaign_display_name() == campaign_name);
    CHECK(custom_messages_localization_get_title(1) == message_title);
    CHECK(equals(game_campaign_display_name(), "Campanha traduzida"));
    CHECK(equals(custom_messages_localization_get_title(1), "Mensagem traduzida"));
    check_canonical();
    ui_language = "pt-BR";
    CHECK(empire_city_localization_load());
    custom_messages_localization_clear();
    CHECK(equals(empire_city_localization_get_name(1, source_rome), "Roma"));
    empire_city_localization_clear();
    CHECK(equals(game_campaign_display_name(), "Campanha traduzida"));
    expect_original();
}

int main(void)
{
    if (make_directory("fixtures") != 0 && errno != EEXIST) return 2;
    for (int run = 0; ; run++) {
        snprintf(run_root, sizeof(run_root), "fixtures/empire_%03d", run);
        if (make_directory(run_root) == 0) break;
        if (errno != EEXIST) return 2;
    }
    for (archive_mode = 0; archive_mode < 2; archive_mode++) {
        test_names_and_lifecycle();
        test_invalid_documents();
        test_limits_and_missing();
        test_locales();
        test_overlay_isolation();
    }
    empire_city_localization_clear();
    custom_messages_localization_clear();
    game_campaign_clear();
    printf("RESULT: %d cases, %d checks, %d failures (folder + ZIP, real encoding).\n", cases, checks, failures);
    return failures ? 1 : 0;
}
