#ifndef MOD_CELL_MAP_H
#define MOD_CELL_MAP_H

#include <stdbool.h>
#include <stdint.h>

/* These limits also bound the fixed pack-state arrays. */
#define MOD_CELL_MAP_MAX_ICS 10
#define MOD_CELL_MAP_MAX_CELLS 180

typedef struct {
    uint16_t totalCells;
    uint8_t icCount;
    uint8_t icsPerString;
    uint8_t parallelStrings;
    uint8_t nominalCells;
    uint8_t cellsPerIC[MOD_CELL_MAP_MAX_ICS];
} modCellMap;

/* First/last counts are per parallel string; zero requests automatic layout.
 * Every interior IC has the same count. Invalid layouts never produce a map. */
bool modCellMapBuild(modCellMap *map, uint16_t seriesCells,
                     uint16_t parallelStrings, uint16_t icCount,
                     uint8_t monitorType, uint8_t firstCells,
                     uint8_t lastCells);
bool modCellMapPhysical(const modCellMap *map, uint16_t logicalCell,
                        uint8_t *ic, uint8_t *channel);
uint8_t modCellMapMonitorCapacity(uint8_t monitorType);

#endif
