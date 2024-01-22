
#include "./springboard.h"

#include <stdlib.h>

#include "./scene.h"

#define FIREFLY_COUNT    (15)

typedef struct FireflySprite {
  Node firefly;

  float x, y, rt;
  float dx, dy, drt;

  float halfLife;
} FireflySprite;

typedef struct _SpringboardContext {
  FireflySprite fireflies[FIREFLY_COUNT];

  Node grass;
  Node treesNear, treesFar;

  Node fireflyField;

} _SpringboardContext;


SpringboardContext springboard_init() {
  _SpringboardContext *context = malloc(sizeof(_SpringboardContext));

  return context;
}

void springboard_free(SpringboardContext context) {
  free(context);
}

void springboard_tick(SpringboardContext context) {

}
