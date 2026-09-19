/* Production parser depth boundary regression. No SDL or game assets. */
#include "core/log.h"
#include "core/xml_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int checks, failures, event_count;
static char events[32][128];
static const char *test_name;

void log_info(const char *message, const char *detail, int value)
{
    (void) message; (void) detail; (void) value;
}

void log_error(const char *message, const char *detail, int value)
{
    (void) message; (void) detail; (void) value;
}

static void check(int success, const char *expression, int line)
{
    checks++;
    if (!success) {
        failures++;
        printf("FAIL [%s] %d: %s\n", test_name, line, expression);
    }
}
#define CHECK(expression) check(!!(expression), #expression, __LINE__)

static void text_event(const char *text)
{
    CHECK(event_count < 32);
    if (event_count < 32) {
        snprintf(events[event_count++], sizeof(events[0]), "%s:%s",
            xml_parser_get_current_element_name(), text);
    }
}

static const xml_parser_element root_schema[] = {
    { "root", 0, 0, 0, text_event }
};
static const xml_parser_element nested_schema[] = {
    { "root", 0, 0, 0, text_event },
    { "city", 0, 0, "root", text_event },
    { "name", 0, 0, "city", text_event }
};

static void reset_events(void)
{
    event_count = 0;
    memset(events, 0, sizeof(events));
}

static int parse(const char *xml, int final)
{
    return xml_parser_parse(xml, (unsigned int) strlen(xml), final);
}

static void test_root_text(void)
{
    test_name = "root text with exactly one schema entry";
    CHECK(xml_parser_init(root_schema, 1, 1));
    CHECK(parse("<root>Only root text</root>", 1));
    CHECK(event_count == 1);
    CHECK(strcmp(events[0], "root:Only root text") == 0);
    xml_parser_reset();
    reset_events();
    CHECK(parse("<root>After reset</root>", 1));
    CHECK(event_count == 1);
    CHECK(strcmp(events[0], "root:After reset") == 0);
    xml_parser_free();
    xml_parser_free();
}

static void test_root_empty(void)
{
    test_name = "root without text still finishes within one slot";
    CHECK(xml_parser_init(root_schema, 1, 1));
    CHECK(parse("<root></root>", 1));
    CHECK(event_count == 0);
    xml_parser_reset();
    /* Existing tokenizer behavior: a sole self-closing root is BUFFERDRY. */
    CHECK(!parse("<root/>", 1));
    CHECK(event_count == 0);
    xml_parser_free();
}

static void test_nested(void)
{
    test_name = "depth equals number of schema entries";
    CHECK(xml_parser_init(nested_schema, 3, 1));
    CHECK(parse("<root><city><name>Rome</name><name>Ostia</name></city>"
        "<city><name>Valencia</name></city></root>", 1));
    CHECK(event_count == 3);
    CHECK(strcmp(events[0], "name:Rome") == 0);
    CHECK(strcmp(events[1], "name:Ostia") == 0);
    CHECK(strcmp(events[2], "name:Valencia") == 0);

    test_name = "parent text and child text use separate depth slots";
    xml_parser_reset();
    reset_events();
    CHECK(parse("<root>before<city>inside<name>Rome</name>after name</city>after city</root>", 1));
    CHECK(event_count == 5);
    CHECK(strcmp(events[0], "root:before") == 0);
    CHECK(strcmp(events[1], "city:inside") == 0);
    CHECK(strcmp(events[2], "name:Rome") == 0);
    CHECK(strcmp(events[3], "city:after name") == 0);
    CHECK(strcmp(events[4], "root:after city") == 0);

    test_name = "reset incomplete maximum-depth input";
    xml_parser_reset();
    reset_events();
    CHECK(parse("<root><city><name>pending", 0));
    xml_parser_reset();
    reset_events();
    CHECK(parse("<root><city><name>Clean</name></city></root>", 1));
    CHECK(event_count == 1);
    CHECK(strcmp(events[0], "name:Clean") == 0);

    test_name = "recover after malformed maximum-depth close";
    xml_parser_reset();
    reset_events();
    CHECK(!parse("<root><city><name>pending</wrong></city></root>", 1));
    xml_parser_reset();
    reset_events();
    CHECK(parse("<root><city><name>Recovered</name></city></root>", 1));
    CHECK(event_count == 1);
    CHECK(strcmp(events[0], "name:Recovered") == 0);

    test_name = "free and reinitialize incomplete input";
    xml_parser_reset();
    CHECK(parse("<root><city><name>abandoned", 0));
    xml_parser_free();
    xml_parser_free();
    reset_events();
    CHECK(xml_parser_init(nested_schema, 3, 1));
    CHECK(parse("<root><city><name>Fresh</name></city></root>", 1));
    CHECK(event_count == 1);
    CHECK(strcmp(events[0], "name:Fresh") == 0);
    xml_parser_free();
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s root-text|root-empty|nested\n", argv[0]);
        return 2;
    }
    if (strcmp(argv[1], "root-text") == 0) {
        test_root_text();
    } else if (strcmp(argv[1], "root-empty") == 0) {
        test_root_empty();
    } else if (strcmp(argv[1], "nested") == 0) {
        test_nested();
    } else {
        return 2;
    }
    printf("RESULT: %d checks, %d failures (%s).\n", checks, failures, argv[1]);
    return failures ? 1 : 0;
}
