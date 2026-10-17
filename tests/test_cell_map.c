#include "modCellMap.h"
#include "mainDataTypes.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void check_population(const modCellMap *map, uint16_t expected) {
    uint16_t logical = 0;
    uint8_t ic, channel;
    assert(map->totalCells == expected);
    for (uint8_t physical = 0; physical < map->icCount; physical++) {
        for (uint8_t input = 0; input < map->cellsPerIC[physical]; input++) {
            assert(modCellMapPhysical(map, logical++, &ic, &channel));
            assert(ic == physical && channel == input);
        }
    }
    assert(logical == expected);
    assert(!modCellMapPhysical(map, expected, &ic, &channel));
}

int main(void) {
    modCellMap map, normalized;
    uint8_t ic, channel;
    assert(modCellMapBuild(&map, 24, 1, 3, CELL_MON_LTC6811_1, 6, 6));
    assert(map.cellsPerIC[0] == 6 && map.cellsPerIC[1] == 12 && map.cellsPerIC[2] == 6);
    check_population(&map, 24);
    assert(modCellMapBuild(&map, 28, 2, 6, CELL_MON_LTC6811_1, 8, 10));
    assert(modCellMapPhysical(&map, 8, &ic, &channel) && ic == 1 && channel == 0);
    assert(modCellMapPhysical(&map, 36, &ic, &channel) && ic == 4 && channel == 0);
    check_population(&map, 56);
    assert(modCellMapBuild(&map, 35, 1, 3, CELL_MON_LTC6811_1, 0, 0));
    assert(map.cellsPerIC[0] == 12 && map.cellsPerIC[1] == 12 && map.cellsPerIC[2] == 11);
    assert(modCellMapBuild(&map, 20, 1, 2, CELL_MON_LTC6811_1, 8, 12));
    assert(!modCellMapBuild(&map, 24, 0, 3, CELL_MON_LTC6811_1, 6, 6));
    assert(!modCellMapBuild(&map, 24, 2, 3, CELL_MON_LTC6811_1, 6, 6));
    assert(!modCellMapBuild(&map, 24, 1, 11, CELL_MON_LTC6811_1, 0, 0));
    assert(!modCellMapBuild(&map, 25, 1, 3, CELL_MON_LTC6811_1, 6, 6));
    assert(!modCellMapBuild(&map, 24, 1, 2, CELL_MON_LTC6811_1, 0, 11));
    assert(!modCellMapBuild(&map, 24, 1, 1, CELL_MON_LTC6813_1, 0, 0));
    assert(!modCellMapBuild(&map, 13, 1, 1, CELL_MON_LTC6811_1, 0, 0));
    assert(!modCellMapBuild(&map, 16, 1, 1, CELL_MON_LTC6812_1, 0, 0));
    assert(!modCellMapBuild(&map, 18, 1, 1, CELL_MON_NONE, 0, 0));
    assert(!modCellMapBuild(&map, 0, 1, 1, CELL_MON_LTC6811_1, 0, 0));
    assert(!modCellMapBuild(&map, 19, 10, 10, CELL_MON_LTC6813_1, 0, 0));

    /* Exhaust all valid monitor populations, endpoint choices and parallel
     * groups within the fixed array limits. Each accepted cell must have a
     * unique physical channel, and canonical endpoint counts must round-trip. */
    unsigned accepted = 0;
    for (uint8_t type = CELL_MON_LTC6811_1; type <= CELL_MON_LTC6813_1; type++) {
        uint8_t capacity = modCellMapMonitorCapacity(type);
        for (uint8_t count = 1; count <= MOD_CELL_MAP_MAX_ICS; count++) {
            for (uint8_t parallel = 1; parallel <= count; parallel++) {
                if (count % parallel) continue;
                for (uint16_t total = 1; total <= capacity * (count / parallel); total++) {
                    for (uint8_t first = 0; first <= capacity; first++) {
                        for (uint8_t last = 0; last <= capacity; last++) {
                            if (!modCellMapBuild(&map, total, parallel, count, type, first, last)) continue;
                            check_population(&map, total * parallel);
                            assert(modCellMapBuild(&normalized, total, parallel, count, type,
                                                   map.cellsPerIC[0], map.cellsPerIC[map.icsPerString - 1]));
                            assert(!memcmp(map.cellsPerIC, normalized.cellsPerIC, sizeof(map.cellsPerIC)));
                            accepted++;
                        }
                    }
                }
            }
        }
    }
    printf("cell map: %u accepted layouts checked\n", accepted);
    return 0;
}
