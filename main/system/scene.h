
#ifndef __SCENE_H__
#define __SCENE_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdint.h>
#include "curves.h"


// Scene Context Object (opaque; do not inspect or rely on internals)
typedef void* SceneContext;

// Scene Node Object (opaque; do not inspect or rely on internals)
typedef void* Node;

// Point
typedef struct Point {
    int16_t x;
    int16_t y;
} Point;

extern const Point PointZero;
extern const Point PointHidden;

// Size
typedef struct Size {
    uint16_t width;
    uint16_t height;
} Size;

extern const Size SizeZero;

typedef uint16_t rgb_t;
typedef uint32_t rgba_t;


// Animation


typedef uint32_t AnimationId;

extern const AnimationId AnimationIdNull;

typedef enum SceneActionStop {
    SceneActionStopNormal      = 0,
    SceneActionStopCurrent     = (1 << 1) | (0 << 0),
    SceneActionStopFinal       = (1 << 1) | (1 << 0),
    SceneActionStopMask        = (1 << 1)
} SceneActionStop;

typedef enum SceneActionOption {
    SceneActionOptionNone      = 0,
    SceneActionOptionReverse   = (1 << 3),
    SceneActionOptionRepeat    = (1 << 4),
    SceneActionOptionDirectionMask = (1 << 2),
} SceneActionOption;

typedef enum SceneNodeEffect {
    SceneNodeEffectDarken     = 1,
    SceneNodeEffectLighten    = 2,
} SceneNodeEffect;

typedef void (*SceneAnimationCompletion)(SceneContext scene, Node node, SceneActionStop stopType);

// typedef enim SceneFilter {
//     SceneFilterDarken,
//     SceneFilterLighten,
// //    SceneFilterBlur
// } SceneFilter;


// Allocate a new SceneContext
SceneContext scene_init(uint32_t nodeCount);
//SceneContext scene_initStatic(uint8_t *data, uint32_t dataLength);

// Release a ScreenContext created using scene_init.
void scene_free(SceneContext scene);

// Debugging; dump the scene graph to the console
void scene_dump(SceneContext scene);

// Rendering
uint32_t scene_sequence(SceneContext scene);
void scene_render(SceneContext scene, uint8_t *fragment, int32_t y0, int32_t height);

// Get the root node of the scene
Node scene_root(SceneContext scene);

uint32_t scene_isRunningAnimation(SceneContext scene, AnimationId animationId);
uint32_t scene_onAnimationCompletion(SceneContext scene, AnimationId animationId, SceneAnimationCompletion callback, void *context);

// Stop an animation
void scene_stopAnimation(SceneContext scene, AnimationId animationId, SceneActionStop stopType);

// @TODO:
//void scene_setAnimationOption(SceneContext scene, AnimationId animationId, SceneActionOption options);

// Schedule the node to be freed on the next sequence
void scene_nodeFree(Node node);

// Move a node within the scene (with respect to its parents in the hierarchy)
void scene_nodeSetPosition(Node node, Point pos);
Point scene_nodePosition(Node node);
AnimationId scene_nodeAnimatePosition(SceneContext scene, Node node, Point targetPosition, uint32_t duration, CurveFunc curve);

// AnimationId scene_nodeAnimatePosition(SceneContext scene, Node node, Point target, uint32_t duration, CurveFunc curve, SceneAnimationCompletion callback, void* context);

// Create a new GroupNode
Node scene_createGroup(SceneContext scene);

// Add a child to the end of the children for a parent GroupNode (created using scene_createGroup)
void scene_appendChild(Node parent, Node child);

// Create a FillNode, filling the entire screen with color
Node scene_createFill(SceneContext scene, uint16_t color);
void scene_fillSetColor(Node node, rgb_t color);
rgb_t scene_fillColor(Node node);

AnimationId scene_fillAnimateColor(SceneContext scene, Node node, uint16_t targetColor, uint32_t duration, CurveFunc curve);

// Create a BoxNode with width and height filled with color.
Node scene_createBox(SceneContext scene, Size size, uint16_t color);
void scene_boxSetColor(Node node, rgb_t color);
void scene_boxSetColorAlpha(Node node, rgba_t color);
void scene_boxSetSize(Node node, Size size);
rgb_t scene_boxColor(Node node);
Size scene_boxGetSize(Node node);

AnimationId scene_boxAnimateColor(SceneContext scene, Node node, rgb_t target, uint32_t duration, CurveFunc curve);
AnimationId scene_boxAnimateColorAlpha(SceneContext scene, Node node, rgba_t target, uint32_t duration, CurveFunc curve);
AnimationId scene_boxAnimateSize(SceneContext scene, Node node, Size target, uint32_t duration, CurveFunc curve);

// Images
Node scene_createImage(SceneContext scene, const uint16_t *data, uint32_t dataLength);
uint16_t* scene_imageData(Node node);
Size scene_imageSize(Node node);

// A spritesheet is a 32x32 tile node where each tile is an index into the spriteData for that position
// - The spriteData is a 256 2-byte entries
// Node scene_createSpritesheet(SceneContext context, uint8_t *spriteData);
// void scene_spriteheetSetSprite(Node spritesheet, uint8_t ix, uint8_t iy, uint8_t index);
// uint8_t *scene_spritesheetGetIndices(Node spritesheet);

/********************************************************
 * TEXT NODE
 ********************************************************/

// Create a TextNode backed by static content, up to dataLength bytes long.
// Text can NOT be updated using scene_setText.
Node scene_createText(SceneContext scene, const char* data, uint32_t dataLength);

// Create a TextNode backed by provided data, up to floor(dataLength / 2) long, which can
// be updated using scene_textSetText. The data is split into two fragments, which are
// swapped on sequencing. If there is data present, the first half will be rendered.
Node scene_createTextFlip(SceneContext scene, char* data, uint32_t dataLength);

// Create a TextNode which will allocate the necessary memory for dataLength strings, which can
// be updated using scene_textSetText.
Node scene_createTextAlloc(SceneContext scene, uint32_t textLength);

void scene_textSetText(Node node, const char* const text, uint32_t length);
void scene_textSetColor(Node node, rgb_t color);
void scene_textSetColorAlpha(Node node, rgba_t color);

AnimationId scene_textAnimateColor(SceneContext scene, Node node, rgb_t target, uint32_t duration, CurveFunc curve);
AnimationId scene_textAnimateColorAlpha(SceneContext scene, Node node, rgba_t target, uint32_t duration, CurveFunc curve);



#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __SCENE_H__ */
