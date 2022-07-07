#ifndef IO_H
#define IO_H

#include <cjson/cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <zlib.h>

unsigned int checksumFile(FILE *f);
cJSON *readJSONFile(FILE *f);

#endif
