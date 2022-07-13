#include <assert.h>
#include <string.h>

#include "io.h"

int main() {

  FILE *f = fopen("example/test/content.json", "rb");
  unsigned int checksum = checksumFile(f);
  cJSON *c = readJSONFile(f);
  fclose(f);

  assert(checksum == 719410340);
  assert(strcmp(c->child->valuestring, "REV") == 0);
}
