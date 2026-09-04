#include "building/building.h"
#include "building/house.h"
#include "core/backtrace.h"
#include "core/time.h"
#include "game/file.h"
#include "game/game.h"
#include "game/settings.h"
#include "game/undo.h"

#ifdef _MSC_VER
#include <direct.h>
#define chdir _chdir
#define getcwd _getcwd
#elif !defined(__vita__)
#include <unistd.h>
#endif

#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "sav_compare.h"

static void handler(int sig)
{
    fprintf(stderr, "Oops, crashed with signal %d :(", sig);
    backtrace_print();
    exit(1);
}

static void run_ticks(int ticks)
{
    setting_reset_speeds(500, setting_scroll_speed());
    time_set_millis(0);
    for (int i = 1; i <= ticks; i++) {
        time_set_millis(2 * i);
        game_run();
    }
}

static int run_autopilot(const char *input_saved_game, const char *output_saved_game, int ticks_to_run)
{
    printf("Running autopilot: %s --> %s in %d ticks\n", input_saved_game, output_saved_game, ticks_to_run);
    signal(SIGSEGV, handler);

    if (!game_pre_init()) {
        printf("Unable to run Game_preInit\n");
        return 1;
    }

    if (!game_init()) {
        printf("Unable to run Game_init\n");
        return 2;
    }

    if (!game_file_load_saved_game(input_saved_game)) {
        char wd[500];
        if (getcwd(wd, 500)) {
            printf("Unable to load saved game from %s\n", wd);
        } else {
            printf("Unable to load saved game\n");
        }
        return 3;
    }
    run_ticks(ticks_to_run);
    printf("Saving game to %s\n", output_saved_game);
    game_file_write_saved_game(output_saved_game);
    printf("Done\n");

    game_exit();

    return 0;
}

static int test_undo_when_merged_house_becomes_vacant(const char *input_saved_game)
{
    if (!game_pre_init()) {
        return 1;
    }
    if (!game_init()) {
        return 2;
    }
    if (!game_file_load_saved_game(input_saved_game)) {
        game_exit();
        return 3;
    }

    building *merged_house = 0;
    for (int id = 1; id <= building_get_highest_id(); id++) {
        building *b = building_get(id);
        if (b->state == BUILDING_STATE_IN_USE && b->house_is_merged) {
            merged_house = b;
            break;
        }
    }
    if (!merged_house) {
        printf("No merged house found in %s\n", input_saved_game);
        game_exit();
        return 4;
    }

    game_undo_start_build(BUILDING_CLEAR_LAND);
    building_house_change_to_vacant_lot(merged_house);
    game_undo_finish_build(0);

    int undo_available = game_can_undo();
    game_exit();
    if (undo_available) {
        printf("Undo remained available after a merged house split\n");
        return 5;
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc == 3 && strcmp(argv[1], "--test-undo-vacant-house-split") == 0) {
        return test_undo_when_merged_house_becomes_vacant(argv[2]);
    }
    if (argc != 5) {
        printf("Incorrect number of arguments (%d)\n", argc);
        return -1;
    }
    const char *input = argv[1];
    const char *output = argv[2];
    const char *expected = argv[3];
    int ticks = atoi(argv[4]);
    if (run_autopilot(input, output, ticks) == 0) {
        return compare_files(expected, output);
    } else {
        return 1;
    }
}
