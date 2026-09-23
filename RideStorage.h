#pragma once

#include <Arduino.h>
#include "Config.h"

// RideStorage — manages date-keyed ride files in LittleFS.
// One file per ride: /ride_YYYYMMDDHHMM.csv
// Files are opened at ride start, appended to during the ride,
// closed at ride end. Each row is written with open/write/close.

// Build a filename from an epoch. Fills 'buf' with /ride_YYYYMMDDHHMM.csv
void rideStorageBuildFilename(uint32_t epoch, char* buf, size_t n);

// Open (create) a file for a new ride. Returns true on success.
bool rideStorageCreate(uint32_t startEpoch);

// Append one row to the currently open ride file.
bool rideStorageAppendRow(const RideRow& row);

// Close the currently open ride file.
void rideStorageClose();

// Delete a specific ride file by epoch.
bool rideStorageDelete(uint32_t startEpoch);

// Enumerate ride files. Returns count. Calls visitor(name, size) for each.
// Used by upload code in V2.71.
typedef void (*RideFileVisitor)(const char* name, size_t size);
size_t rideStorageEnumerate(RideFileVisitor visitor);

// Check if a ride file exists.
bool rideStorageExists(uint32_t startEpoch);

// Current open file name (empty string if none).
const char* rideStorageCurrentFile();