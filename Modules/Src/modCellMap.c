#include "modCellMap.h"
#include "mainDataTypes.h"
#include <string.h>

uint8_t modCellMapMonitorCapacity(uint8_t monitorType) {
    switch (monitorType) {
    case CELL_MON_LTC6811_1: return 12;
    case CELL_MON_LTC6812_1: return 15;
    case CELL_MON_LTC6813_1: return 18;
    default: return 0;
    }
}

bool modCellMapBuild(modCellMap *map, uint16_t seriesCells,
                     uint16_t parallelStrings, uint16_t icCount,
                     uint8_t monitorType, uint8_t firstCells,
                     uint8_t lastCells) {
    uint16_t first = firstCells, last = lastCells, nominal = 0;
    uint16_t count, remainder;
    uint8_t capacity = modCellMapMonitorCapacity(monitorType);
    if (!map) return false;
    memset(map, 0, sizeof(*map));
    if (!capacity || !seriesCells || !parallelStrings || !icCount ||
        icCount > MOD_CELL_MAP_MAX_ICS || icCount % parallelStrings ||
        parallelStrings > icCount ||
        seriesCells > MOD_CELL_MAP_MAX_CELLS / parallelStrings) return false;
    count = icCount / parallelStrings;
    if (count == 1) {
        if ((first && first != seriesCells) || (last && last != seriesCells)) return false;
        first = last = nominal = seriesCells;
    } else if (!first && !last) {
        /* Preserve the existing valid uniform/short-last layout. */
        nominal = (seriesCells + count - 1) / count;
        first = nominal;
        if (seriesCells <= nominal * (count - 1)) return false;
        last = seriesCells - nominal * (count - 1);
    } else if (first && last) {
        if (first + last > seriesCells) return false;
        remainder = seriesCells - first - last;
        if (count == 2) {
            if (remainder) return false;
            nominal = first > last ? first : last;
        } else {
            if (!remainder || remainder % (count - 2)) return false;
            nominal = remainder / (count - 2);
        }
    } else if (first) {
        if (first >= seriesCells) return false;
        remainder = seriesCells - first;
        nominal = (remainder + count - 2) / (count - 1);
        if (remainder <= nominal * (count - 2)) return false;
        last = remainder - nominal * (count - 2);
    } else {
        if (last >= seriesCells) return false;
        remainder = seriesCells - last;
        if (remainder % (count - 1)) return false;
        first = nominal = remainder / (count - 1);
    }
    if (!first || !last || !nominal || first > capacity ||
        last > capacity || nominal > capacity) return false;

    map->totalCells = seriesCells * parallelStrings;
    map->icCount = icCount;
    map->icsPerString = count;
    map->parallelStrings = parallelStrings;
    map->nominalCells = nominal;
    for (uint16_t ic = 0; ic < icCount; ic++) {
        uint16_t position = ic % count;
        map->cellsPerIC[ic] = position == 0 ? first :
                               position == count - 1 ? last : nominal;
    }
    return true;
}

bool modCellMapPhysical(const modCellMap *map, uint16_t logicalCell,
                        uint8_t *ic, uint8_t *channel) {
    if (!map || !ic || !channel || logicalCell >= map->totalCells ||
        !map->icCount || map->icCount > MOD_CELL_MAP_MAX_ICS) return false;
    for (uint8_t index = 0; index < map->icCount; index++) {
        if (logicalCell < map->cellsPerIC[index]) {
            *ic = index;
            *channel = logicalCell;
            return true;
        }
        logicalCell -= map->cellsPerIC[index];
    }
    return false;
}
