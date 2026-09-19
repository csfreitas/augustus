#ifndef EMPIRE_CITY_LOCALIZATION_H
#define EMPIRE_CITY_LOCALIZATION_H

#include <stdint.h>

void empire_city_localization_clear(void);
int empire_city_localization_load(void);

/**
 * Returns a localized presentation name when the source name still matches.
 * The canonical name is returned for missing, unknown, or stale entries.
 */
const uint8_t *empire_city_localization_get_name(int object_id, const uint8_t *source_name);

#endif // EMPIRE_CITY_LOCALIZATION_H
