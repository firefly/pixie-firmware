#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "font.h"
#include "scene.h"

typedef struct TextInfo {
    // 5 bits alpha, 1 bit isConst, 1 bit isAlloc, 1 bit flipPage
    uint8_t flags;
    uint8_t length;
    rgb_t color;
} TextInfo;

/**
 *  A generic property type to encapsulate many values into 4 bytes.
 */
typedef union Property {
    void* ptr;
    uint8_t* data;
    uint32_t u32;
    int32_t i32;
    Point point;
    Size size;
    TextInfo text;
    SceneAnimationCompletion animationCompletion;
} Property;

struct _Node;
struct _SceneContext;

// When called, adds zero or more Render Nodes to the current
// render list

// Adds 0 or more RenderNodes to the context
typedef void (*SequenceFunc)(struct _SceneContext *context, Point pos, struct _Node *node);

// Renders a RenderNode to the screen
typedef void (*RenderFunc)(Point pos, Property a, Property b,
              uint16_t *frameBuffer, int32_t y0, int32_t height);

typedef void (*AnimateFunc)(struct _Node *node, fixed_t t, Property p0, Property p1);


typedef union NodeFunc {
    SequenceFunc sequenceFunc;
    RenderFunc renderFunc;
    AnimateFunc animateFunc;
    CurveFunc curveFunc;
} NodeFunc;


typedef struct _Node {
    struct _Node *nextNode;
    NodeFunc func;
    Point pos;
    Property a;
    Property b;
} _Node;

typedef struct _SceneContext {
    // All nodes in the scene, including render nodes
    _Node *nodes;

    // The pointer to the next free node to allocate
    _Node *nextFree;

    // The root (group) node
    _Node *root;

    // Gloabl tick
    int32_t tick;

    //  The head and tail of the current render list (may be null)
    _Node *renderHead;
    _Node *renderTail;

    //  The head and tail of the running animations (may be null)
    _Node *animateHead;
    _Node *animateTail;
} _SceneContext;

static const int16_t yHidden = 0x7fff;

const Point PointZero = { .x = 0, .y = 0 };
const Point PointHidden = { .x = 0, .y = yHidden };
const Size SizeZero = { .width = 0, .height = 0 };

const AnimationId AnimationIdNull = 0;

// Adjust for little endian architecture
// uint16_t flipColor(rgb_t color) { return (color << 8) | (color >> 8); }


/**
 *  FreeNode
 *
 *  The free nodes are organized as a linked list, each pointing
 *. to the next FreeNode (terminated with NULL).
 *
 *  nextNode   - pointer to next free node
 */

/**
 *  Render Node
 *
 *  A RenderNode is responsible for rendering contents into a
 *  fragment, a slice of the screen. The screen is draw in multiple
 *  passes to conserve backbuffer memory.
 *
 *  The render nodes are organized as a linked list, each pointing
 *  to the next RenderNode (terminated with NULL).
 *
 *  nextNode   - the next RenderNode to render
 *  func       - the render function that will update the fragment
 *  x, y       - co-ordinates in window space
 *  a:any      - a property value to be used by func (generally a copy of the target node's a property)
 *  b:any      - a property value to be used by func (generally a copy of the target node's a property)
 */

 /**
 *  Scene Node
 *
 *  Each SceneNode represents a node which is part of the scene
 *  graph, which when processed in top-to-bottom, left-to-right
 *  DFS order to render the entire scene.
 *
 *  During sequencing, each node may add any number of Render
 *  Nodes to be blitted. Nodes are responsible for calling
 *  any child SceneNodes, but should not call sibling SceneNodes.
 *
 *  A scene is sequenced once per frame, so (for performance
 *  reasons) not scedule any invisible (hidden or offscreen)
 *  nodes.
 *
 *  nextNode   - the nextSibling in the scene graph
 *  func       - the sequence function
 *  x, y       - co-ordinates in local space
 *  a:any      - dependent on the type of Scene Node
 *  b:any      - dependent on the type of Scene Node
 */

 /**
  *  AnimationNode and PropertyNode
  *
  *  Each AnimateNode manages a running animation, from start time to end time. An animation requires
  *  two node, an AnimateNode and a PropertyNode that stores the properties and curve
  *
  *  nextNode   - the related ProertyNode
  *  func       - the property animation function
  *  pos.x      - duration (in ms)
  *  pos.y      - scheduled stop state (1 for keep current value, 2 for use final state)
  *  a:void*    - target node to animate
  *  b:i32      - end time
  *
  *  Property Node
  *
  *  nextNode   - the next AnimationNode
  *  func       - the curve function
  *  pos.x      - uniqueId
  *  a:any      - The start property
  *  b:any      - the end property
  */

/**
 *  Animation Complete Node
 *
 *  x, y       - animationId
 *  a:func()   - Callback function
 *  b:ptr      - target context
 */


static int32_t _getNodeIndex(_SceneContext *scene, _Node *node) {
    if (node == NULL) { return -1; }
    _Node *base = &scene->nodes[0];
    return (node - base);
}

static _Node* _getAnimationNode(_SceneContext *scene, AnimationId animationId) {
    if (animationId == AnimationIdNull) { return NULL; }

    // Get the AnimateNode
    _Node *node = &scene->nodes[animationId >> 16];
    if (node->nextNode == NULL) { return NULL; }

    // Check the ID matches (otherwise it has been recycled)
    if (node->nextNode->pos.x != (animationId & 0x7fff)) { return NULL; }

    return node;
}

static AnimationId _getAnimationId(_SceneContext *scene, _Node *node) {
    return (_getNodeIndex(scene, node) << 16) | node->nextNode->pos.x;
}

static void _freeNode(_SceneContext *scene, _Node *node) {
    node->nextNode = scene->nextFree;
    scene->nextFree = node;
}

static void _appendChild(_Node *parent, _Node *child) {
    child->nextNode = NULL;

    if (parent->a.ptr == NULL) {
        // First child
        parent->a.ptr = parent->b.ptr = child;

    } else {
        // Append to the child to the end
        _Node *lastChild = parent->b.ptr;
        lastChild->nextNode = child;
        parent->b.ptr = child;
    }
}

// The node.a property is not clearedl the caller must update this
static _Node* _addNode(_SceneContext *scene, Point pos) {
    _Node *free = scene->nextFree;
    if (free == NULL) {
        printf("No free nodes...\n");
        return NULL;
    }

    scene->nextFree = free->nextNode;
    free->pos = pos;
    free->b.ptr = NULL; // @TOOD: don't do this and copy in sequence to avoid unecessary copies
    free->nextNode = NULL;

    return free;
}

static _Node* _addRenderNode(_SceneContext *scene, _Node *sceneNode, Point pos, RenderFunc renderFunc) {
    pos.x += sceneNode->pos.x;
    pos.y += sceneNode->pos.y;

    _Node *node = _addNode(scene, pos);
    if (node == NULL) { return NULL; }

    // node->a = sceneNode->a; // Move these copies to sequencing?
    // node->b = sceneNode->b;
    node->func.renderFunc = renderFunc;

    // Add the RenderNode to the render list
    if (scene->renderHead == NULL) {
        // First render node; it is the head and the tail
        scene->renderHead = scene->renderTail = node;

    } else {
        // Add this to the end of the list and make it the end
        scene->renderTail->nextNode = node;
        scene->renderTail = node;
    }

    return node;
}

static Node _addAnimateNode(_SceneContext *scene, _Node *node, uint32_t duration, AnimateFunc animateFunc, CurveFunc curveFunc) {

    Point point = { .x = duration, .y = 0 };

    _Node* animate = _addNode(scene, point);
    if (animate == NULL) { return NULL; }

    // We use this ID to make sure we are talking to the same 
    static uint32_t nextId = 1;

    point.x = nextId++;

    _Node *prop = _addNode(scene, point);
    if (prop == NULL) {
        _freeNode(scene, animate);
        return NULL;
    }

    animate->nextNode = prop;

    animate->func.animateFunc = animateFunc;
    animate->a.ptr = node;
    animate->b.i32 = scene->tick + duration;

    prop->func.curveFunc = curveFunc;

    // Add the AnimateNode to the animation list
    if (scene->animateHead == NULL) {
        // First Animate Node; it is the head and the tail
        scene->animateHead = animate;
        scene->animateTail = prop;

    } else {
        // Add this to the end of the list and make it the end
        scene->animateTail->nextNode = animate;
        scene->animateTail = prop;
    }

    return animate;  
}

uint32_t scene_sequence(SceneContext _scene) {
    _SceneContext *scene = _scene;

    // Recycle the previous render list
    if (scene->renderHead != NULL) {
        scene->renderTail->nextNode = scene->nextFree;
        scene->nextFree = scene->renderHead;
        scene->renderHead = scene->renderTail = NULL;
    }

    int32_t now = xTaskGetTickCount();
    scene->tick = now;

    // Run all the animations
    if (scene->animateHead) {

        _Node *lastAnimate = NULL;

        _Node *animate = scene->animateHead;
        while (animate) {

            int32_t duration = animate->pos.x;
            int32_t endTime = animate->b.i32;

            _Node *prop = animate->nextNode;
            _Node *nextAnimate = prop->nextNode;

            uint16_t stopType = animate->pos.y;
            if (stopType) { endTime = now; }

            if (stopType != SceneActionStopCurrent) {
                // Compute the curve-adjusted t
                // fixed_t t = FM_1 - divfx((endTime - now) << 16, duration << 16);
                fixed_t t = FM_1 - (tofx(endTime - now) / duration);
                if (t >= FM_1) { t = FM_1; }
                t = prop->func.curveFunc(t);

                // Perform the animation
                animate->func.animateFunc(animate->a.ptr, t, prop->a, prop->b);
            }

            // Remove animation
            if (now >= endTime) {
                // Make the previous AnimateNode link to the next
                if (lastAnimate) {
                    lastAnimate->nextNode->nextNode = nextAnimate;
                } else {
                    scene->animateHead = nextAnimate;
                    if (nextAnimate == NULL) { scene->animateTail = NULL; }
                }

                // Add the node back to the free nodes
                prop->nextNode = scene->nextFree;
                scene->nextFree = animate;

            } else {
                lastAnimate = animate;
            }

            // Next AnimateNode
            animate = nextAnimate;
        }
    }

    // Sequence all the nodes
    scene->root->func.sequenceFunc(scene, PointZero, scene->root);
}

void scene_render(SceneContext _scene, uint8_t *fragment, int32_t y0, int32_t height) {
    _SceneContext *scene = _scene;

    _Node *node = scene->renderHead;
    while (node) {
        // _Node *target = node->a.ptr;
        node->func.renderFunc(node->pos, node->a, node->b, (uint16_t*)fragment, y0, height);
        node = node->nextNode;
    }
}

uint32_t scene_isRunningAnimation(SceneContext _scene, AnimationId animationId) {
    _SceneContext *scene = _scene;

    _Node *node = _getAnimationNode(scene, animationId);
    return (node == NULL) ? 0: 1;
}


void scene_stopAnimation(SceneContext _scene, AnimationId animationId, SceneActionStop stopType) {
    _SceneContext *scene = _scene;

    // Get the AnimateNode
    _Node *node = _getAnimationNode(scene, animationId);
    if (node == NULL) { return; }    

    // Clear its ID, so it cannot be interacted with again
    node->nextNode->pos.x = 0;

    // Schedule it to be stopped on the next sequence
    node->pos.y = stopType;
}


/********************************************************
 * Initialization
 ********************************************************/

SceneContext scene_init(uint32_t nodeCount) {
    // We need at least a root and render head
    if (nodeCount < 2) { return NULL; }

    _SceneContext *scene = malloc(sizeof(_SceneContext));
    if (scene == NULL) { return NULL; }
    memset(scene, 0, sizeof(_SceneContext));

    _Node *nodes = malloc(nodeCount * sizeof(_Node));
    if (nodes == NULL) {
        free(scene);
        return NULL;
    }
    memset(nodes, 0, nodeCount * sizeof(_Node));  
    scene->nodes = nodes;

    // Create a linked list of all nodes linking to the next free
    for (uint32_t i = 0; i < nodeCount - 1; i++) {
        nodes[i].nextNode = &nodes[i + 1];
    }

    // The next free Node to acquire
    scene->nextFree = &nodes[0];

    // Set up the root node and render list
    scene->root = scene_createGroup(scene);
    scene->renderHead = NULL;
    scene->renderTail = NULL;

    return scene;
}

/********************************************************
 * NODE
 ********************************************************/

Node scene_root(SceneContext _scene) {
    _SceneContext *scene = _scene;
    return scene->root;
}

static void _freeSequence(_SceneContext *scene, Point worldPos, _Node* node) {
    printf("WARNING: running free node\n");
}

void scene_nodeFree(Node _node) {
    _Node *node = _node;
    node->func.sequenceFunc = _freeSequence;
}

void scene_nodeSetPosition(Node _node, Point pos) {
    _Node *node = _node;
    if (node == NULL) { return; }
    node->pos = pos;
}

Point scene_nodePosition(Node _node) {
    _Node *node = _node;
    if (node == NULL) { return PointZero; }
    return node->pos;
}

// @TODO: drop SceneContext from this?
static void _nodeAnimatePositionHoriz(_Node *node, fixed_t t, Property p0, Property p1) {
    node->pos.x = p0.point.x + scalarfx(p1.point.x - p0.point.x, t);
}

static void _nodeAnimatePositionVert(_Node *node, fixed_t t, Property p0, Property p1) {
    node->pos.y = p0.point.y + scalarfx(p1.point.y - p0.point.y, t);
}

static void _nodeAnimatePosition(_Node *node, fixed_t t, Property p0, Property p1) {
    Point result = p0.point;
    result.x += scalarfx(p1.point.x - p0.point.x, t);
    result.y += scalarfx(p1.point.y - p0.point.y, t);
    node->pos = result;
}

AnimationId scene_nodeAnimatePosition(SceneContext _scene, Node _node, Point target, uint32_t duration, CurveFunc curve) {
    _SceneContext *scene = _scene;
    _Node *node = _node;

    AnimateFunc animateFunc = _nodeAnimatePosition;
    if (node->pos.x == target.x) {
        animateFunc = _nodeAnimatePositionVert;
    } else if (node->pos.y == target.y) {
        animateFunc = _nodeAnimatePositionHoriz;
    }

    _Node *animate = _addAnimateNode(scene, node, duration, animateFunc, curve);
    if (animate == NULL) { return AnimationIdNull; }
    animate->nextNode->a.point = node->pos;
    animate->nextNode->b.point = target;

    return _getAnimationId(scene, animate);
}

/********************************************************
 * ANIMATION COMPLETE NODE
 ********************************************************/

static void _animationCompletionSequence(_SceneContext *scene, Point worldPos, _Node* node) {
    uint32_t animationId = (node->pos.x << 16) | node->pos.y;

    // Still running; do nothing yet
    if (_getAnimationNode(scene, animationId)) { return; }

    // Prevent me from every running again and schedule me to be freed
    node->func.sequenceFunc = _freeSequence;
    node->pos.x = node->pos.y = 0;

    // Call the callback
    node->a.animationCompletion(scene, node->b.ptr, 0);
}

uint32_t scene_onAnimationCompletion(SceneContext _scene, AnimationId animationId, SceneAnimationCompletion callback, void *context) {
    _SceneContext *scene = _scene;

    // Animation has already completed
    _Node *animate = _getAnimationNode(scene, animationId);
    if (animate == NULL) { return 0; }

    _Node* node = _addNode(scene, PointZero);
    if (node == NULL) { return 0; }

    // Add the node to the scene to be sequenced with Scene Nodes (after the animation nodes have been sequenced)
    _appendChild(scene->root, node);

    node->pos.x = animationId >> 16;
    node->pos.y = animationId & 0x7fff;
    node->a.animationCompletion = callback;
    node->b.ptr = (context != NULL) ? context: animate->a.ptr;
    node->func.sequenceFunc = _animationCompletionSequence;

    return 1;
}


/********************************************************
 * GROUP NODE
 ********************************************************/

/**
 *  Scene Node: Group
 *
 *  A Group Node does not have any visible objects itself, but
 *  allows for hierarchal scenes with multiple "attached" objects
 *  to be moved together.
 *
 *  x, y       - co-ordinates in local space
 *  a:void*    - a pointer to the first child
 *  b:void*    - a pointer to the last child
 */

static void _groupSequence(_SceneContext *context, Point worldPos, _Node* node) {
    // if (node->pos.y == yHidden) { return; }

    Point pos = node->pos;
    worldPos.x += pos.x;
    worldPos.y += pos.y;

    // I have children; visit them each
    _Node *lastChild = NULL;
    _Node *child = node->a.ptr;
    while (child) {
        _Node *nextChild = child->nextNode;

        if (child->func.sequenceFunc == _freeSequence) {
            // Remove child

            // The tail is the child; point the tail to the previous child (NULL if firstChild)
            if (node->b.ptr == child) { node->b.ptr = lastChild; }

            if (lastChild) {
                lastChild->nextNode = child->nextNode;
            } else {
                node->a.ptr = child->nextNode;
            }

            _freeNode(context, child);

        } else {
            // Sequence the child
            child->func.sequenceFunc(context, worldPos, child);
            lastChild = child;
        }

        child = nextChild;
    }
}

Node scene_createGroup(SceneContext _scene) {
    _SceneContext *scene = _scene;

    _Node* node = _addNode(scene, PointZero);
    if (node == NULL) { return NULL; }

    node->a.ptr = node->b.ptr = NULL;
    node->func.sequenceFunc = _groupSequence;

    return node;
}


/**
 *  Adds a child to a parent GroupNode (created using scene_createGroup).
 *
 *  NOTE: Child MUST NOT already have a parent
 */
void scene_appendChild(Node _parent, Node _child) {
    _Node *parent = _parent;
    if (parent == NULL) { return; }

    _Node *child = _child;
    if (child == NULL) { return; }

    _appendChild(parent, child);
}

/********************************************************
 * FILL NODE
 ********************************************************/

/**
 *  Scene Node: Fill
 *
 *  a:uint32   - color (16 bpp) repeated in top and bottom half
 */
static void _fillRender(Point pos, Property a, Property b, uint16_t *_frameBuffer, int32_t y0, int32_t height) {
    uint32_t *frameBuffer = (uint32_t*)_frameBuffer;

    uint32_t value = a.u32;

    for (uint32_t i = 0; i < 240 * height / 2; i++) { *frameBuffer++ = value; }
}

static void _fillSequence(_SceneContext *context, Point worldPos, _Node *node) {
    _Node *render = _addRenderNode(context, node, PointZero, _fillRender);
    if (render == NULL) { return; }
    render->a = node->a;
}

Node scene_createFill(SceneContext _scene, rgb_t color) {
    _SceneContext *scene = _scene;

    _Node* node = _addNode(scene, PointZero);
    if (node == NULL) { return NULL; }

    uint32_t c = __builtin_bswap16(color);
    node->a.u32 = ((c << 16) | c);
    node->func.sequenceFunc = _fillSequence;

    return node;
}

void scene_fillSetColor(Node _node, rgb_t color) {
    _Node *node = _node;
    if (node == NULL) { return; }

    uint32_t c = __builtin_bswap16(color);
    node->a.u32 = ((c << 16) | c);
}

rgb_t scene_fillColor(Node _node) {
    _Node *node = _node;
    if (node == NULL) { return 0; }

    return __builtin_bswap16(node->a.u32 & 0xffff);
}

/********************************************************
 * BOX NODE
 ********************************************************/

/**
 *  Scene Node: Box
 *
 *  a:Size     - (width, height)
 *  b:uint32   - color (16 bpp) repeated in top and bottom half
 */
static void _boxRender(Point pos, Property a, Property b, uint16_t *frameBuffer, int32_t y0, int32_t height) {

    // The box is above the fragment or to the right of the display; skip
    if (pos.y >= y0 + height || pos.x >= 240) { return; }

    // Compute start y and height within the fragment
    int32_t sy = pos.y - y0;
    int32_t h = a.size.height;
    if (sy < 0) {
        h += sy;
        sy = 0;
    }
    if (h <= 0) { return; }
    
    // Compute the start x and width within the fragment
    int32_t sx = pos.x;
    int32_t w = a.size.width;
    if (sx < 0) {
        w += sx;
        sx = 0;
    }
    if (w <= 0) { return; }

    // Extends past the fragment bounds; shrink
    if (sy + h > height) { h = height - sy; }
    if (sx + w > 240) { w = 240 - sx; }

    uint16_t color = b.u32;
    for (uint32_t y = 0; y < h; y++) {
        uint16_t *output = &frameBuffer[240 * (sy + y) + sx]; 
        for (uint32_t x = 0; x < w; x++) {
        *output++ = color;
        }
    }
}

static void _boxSequence(_SceneContext *scene, Point worldPos,  _Node* node) {
    _Node *render = _addRenderNode(scene, node, worldPos, _boxRender);
    if (render == NULL) { return; }
    render->a = node->a;
    render->b = node->b;
}

Node scene_createBox(SceneContext _scene, Size size, rgb_t color) {
    _SceneContext *scene = _scene;

    _Node* node = _addNode(scene, PointZero);
    if (node == NULL) { return NULL; }

    node->a.size = size;
    node->b.u32 = __builtin_bswap16(color);
    node->func.sequenceFunc = _boxSequence;

    return node;
}

void scene_boxSetColor(Node _node, rgb_t color) {
    _Node *node = _node;
    if (node == NULL) { return; }

    node->b.u32 = color;
}

rgb_t scene_boxColor(Node _node) {
    _Node *node = _node;
    if (node == NULL) { return 0; }

    return node->b.u32;
}

void scene_boxSetSize(Node _node, Size size) {
    _Node *node = _node;
    if (node == NULL) { return; }

    node->a.size = size;
}

Size scene_boxGetSize(Node _node) {
    _Node *node = _node;
    if (node == NULL) { return SizeZero; }

    return node->a.size;
}

/********************************************************
 * IMAGE NODE
 ********************************************************/

/**
 *  Scene Node: Image
 *
 *  The image data formats supported include a header which indicates
 *  which specialized RenderFun should be used.
 *
 *  a:uint8_t* - a pointer the image data
 *  b:Size     - this value depends on the image type, and may
 *               represent multiple things, such as alpha or a
 *               mask value, pallette information or
 *               pre-computed data
 */

static  void _imageRender(Point pos, Property a, Property b, uint16_t *frameBuffer, int32_t y0, int32_t height) {
    // The box is above the fragment or to the right of the display; skip
    if (pos.y >= y0 + height || pos.x >= 240) { return; }

    // Compute start y and height within the fragment
    int32_t iy = 0;
    int32_t oy = pos.y - y0;
    int32_t oh = b.size.height;
    if (oy < 0) {
        iy -= oy;
        oh += oy;
        oy = 0;
    }
    if (oh <= 0) { return; }
    
    // Compute the start x and width within the fragment
    int32_t ix = 0;
    int32_t ox = pos.x;
    const int32_t w = b.size.width;
    int32_t ow = w;
    if (ox < 0) {
        ix -= ox;
        ow += ox;
        ox = 0;
    }
    if (ow <= 0) { return; }

    // Extends past the fragment bounds; shrink
    if (oy + oh > height) { oh = height - oy; }
    if (ox + ow > 240) { ow = 240 - ox; }

    // Skip the header bytes
    ix += 2;

    uint16_t *data = (uint16_t*)(a.data);
    for (int32_t y = oh; y; y--) {
        uint16_t *output = &frameBuffer[(240 * (oy + y - 1)) + ox];
        uint16_t *input = &data[2 + ((iy + y - 1) * w) + ix];
        for (int32_t x = ow; x; x--) {
            *output++ = *input++;
        }
    }
}

static void _imageSequence(_SceneContext *scene, Point worldPos,  _Node* node) {
    _Node *render = _addRenderNode(scene, node, worldPos, _imageRender);
    if (render == NULL) { return; }
    render->a = node->a;
    render->b = node->b;
}

static Size _imageValidate(const uint8_t *data, uint32_t dataLength) {
    Size size;
    size.width = data[1] + 1;
    size.height = data[2] + 1;
    
    // @TODO validate image data

    return size;
}

Node scene_createImage(SceneContext _scene, const uint8_t *data, uint32_t dataLength) {
    _SceneContext* scene = _scene;
    
    Size size = _imageValidate(data, dataLength);
    if (size.width == 0 || size.height == 0) { return NULL; }

    _Node* node = _addNode(scene, PointZero);
    if (node == NULL) { return NULL; }

    node->func.sequenceFunc = _imageSequence;

    node->a.data = (uint8_t*)data;
    node->b.size = size;

    return node;
}

uint8_t* scene_imageData(Node _node) {
    _Node *node = _node;
    if (node == NULL) { return NULL; }

    return node->a.data;
}

Size scene_imageSize(Node _node) {
    _Node *node = _node;
    if (node == NULL) { return SizeZero; }

    return node->b.size;
}


/********************************************************
 * TEXT NODE
 ********************************************************/

/**
 *  Scene Node: Text
 *
 *  A text label.
 *
 *  a:uint8_t* - a pointer the text data (type specific)
 *  b:TextInfo - info for the text
 */

#define TextFlagModeConst      (0x04)
#define TextFlagModeAlloc      (0x02)

#define TextFlagFlipPage       (0x01)

#define FONT_BASELINE     ((32 - 6) + 1)
#define FONT_HEIGHT       (32)
#define FONT_WIDTH        (16)

/*
uint32_t fixedMul(uint32_t a, uint32_t b) {
    uint64_t result = ((uint64_t)a) * ((uint64_t)b);
    return result >> 32;
}

uint32_t fixedDiv(uint32_t num, uint32_t den) {
    uint64_t result = (((uint64_t)num) << ((uint64_t)32)) / ((uint64_t)den);
    return result;
}
*/


// typedef struct Bounds {
//     int16_t ix, iy;
//     uint8_t ox, oy, w, h;
// } Bounds;
/**
 *  Given an input box (x, y, w, h) and a fragment (y0, height), update the input box values to reflect
 *  the box that is visible within the fragment (x, y, w, h). Returns 1 if no part of the box overlaps
 *  the fragment.
 *
 *  int32_t ix = pos.x, iy = pos.y, w = size.width, h = size.height, ox, oy;
 *  if (calcBounds(&ix, &iy, &0x, &oy, &w, &h, y0, height)) { return; }
 }
 */
uint32_t calcBounds(int32_t *_pix, int32_t *_piy, int32_t *_ox, int32_t *_oy, int32_t *_w, int32_t *_h, int32_t y0, int32_t height) {
    // The top edge is belog the fragment; skip
    int32_t py = *_piy;
    if (py >= y0 + height) { return 1; }

    // The left edge is to the right of the fragment; skip
    int32_t px = *_pix;
    if (px >= 240) { return 1; }

    // Compute start y and height within the fragment
    int32_t iy = 0;
    int32_t oy = py - y0;
    int32_t h = *_h;
    if (oy < 0) {
        // The input box is above the fragment, trim off some of the top
        iy -= oy;
        h += oy;
        oy = 0;
    }
    if (h <= 0) { return 1; }
    
    // Compute the start x and width within the fragment
    int32_t ix = 0;
    int32_t ox = px;
    int32_t w = *_w;
    if (ox < 0) {
        // The input box is to the left of the fragment, trim off some of the left box
        ix -= ox;
        w += ox;
        ox = 0;
    }
    if (w <= 0) { return 1; }

    // Extends past the fragment bounds; shrink
    if (oy + h > height) { h = height - oy; }
    if (ox + w > 240) { w = 240 - ox; }

    // Copy the results back to the caller
    (*_pix) = ix;
    (*_piy) = iy;
    (*_ox) = ox;
    (*_oy) = oy;
    (*_w) = w;
    (*_h) = h;

    return 0;
}


static  void _textRender(Point pos, Property a, Property b, uint16_t *frameBuffer, int32_t y0, int32_t height) {
    pos.y -= FONT_BASELINE;

    // The text is above the fragment or to the right of the display; skip
    if (pos.y >= y0 + height || pos.x >= 240 || pos.y + FONT_HEIGHT < y0) { return; }

    TextInfo info = b.text;

    int32_t length = info.length;
    int32_t textWidth = ((FONT_WIDTH + 1) * length) - 1;

    // The *max* text length is to the left of the screen
    if (pos.x + textWidth < 0) { return; }

    // Get the actual text content; for non-constant text, select the correct page
    uint8_t *text = a.data;
    if ((info.flags & TextFlagModeConst) == 0 && info.flags & TextFlagFlipPage) {
        text = &text[length]; 
    }

    // Recompute the length and width for the *actual* text
    length = strnlen(text, length);
    textWidth = ((FONT_WIDTH + 1) * length) - 1;

    // The text length is to the left of the screen
    if (pos.x + textWidth < 0) { return; }

    // Compute start input-y, output-y and output-height, clipping to fragment bounds
    int32_t iy = 0;
    int32_t oy = pos.y - y0;
    int32_t oh = FONT_HEIGHT;
    if (oy < 0) {
        iy -= oy;
        oh += oy;
        oy = 0;
    }
    if (oh <= 0) { return; }
    
    // Compute the start input-x, output-x and output-width, clipping to fragment bounds
    int32_t ix = 0;
    int32_t ox = pos.x;
    const int32_t w = textWidth;
    int32_t ow = w;
    if (ox < 0) {
        ix -= ox;
        ow += ox;
        ox = 0;
    }
    if (ow <= 0) { return; }

    // Extends past the fragment bounds; clip to fragment bounds
    if (oy + oh > height) { oh = height - oy; }
    if (ox + ow > 240) { ow = 240 - ox; }

    // Fast-forward to the first on-screen character and track how many bits to trim from the left
    int32_t si = ix / (FONT_WIDTH + 1);
    text = &text[si];
    length -= si;
    si = ix - si * (FONT_WIDTH + 1);

    // For each on-screen (including partial) character...
    for (int32_t i = 0; i < length; i++) {
        // Past the right edge of the screen; done
        if (ox >= 240) { break; }

        uint8_t c = text[i];

        // Null-termination; done
        if (c == 0) { break; }

        // Space; just advance the cursor
        if (c == 32) { 
            ox += FONT_WIDTH + 1 - si;
            si = 0;
            continue;
        }

        // Normalize the code to our font glyph indices
        c -= 33;
        if (c >= 94) { c = 0; } // @TODO: unsupported char

        // Get the font metrics and data offset
        int32_t top = FontData[2 * c + 1] >> 8;
        if (oy + top >= height) {
            ox += FONT_WIDTH + 1 - si;
            si = 0;
            continue; 
        }

        uint32_t offset = FontData[2 * c];
        int32_t rows = FontData[2 * c + 1] & 0xff;

        // Adjust the character-output-y and character-output-height (and possibly font metric rows)
        int32_t coy = oy;
        int32_t coh = oh;
        {
            int32_t dy = iy - top;
            if (dy < 0) {
                coy -= dy;
                coh += dy;
            } else {
                offset += dy;
                rows -= dy;
            }
            if (coh > rows) { coh = rows; }
        }

        const uint16_t *fontData = &FontData[offset];

        // Draw the character rows
        for (int32_t y = 0; y < coh; y++) {
            // Past the bottom edge of the screen; done this character
            if (coy + y >= height) { break; }

            // Target in the frame buffer fragment to blit to
            uint16_t *output = &frameBuffer[(240 * (coy + y)) + ox];

            // Get the font row data, adjusting for trimming off-screen pixels
            uint32_t row = *fontData++;
            {
                int32_t ex = 240 - ox;
                if (ex <= FONT_WIDTH) { row &= (1 << ex) - 1; }
            }
            row >>= si;

            // Blit the pixel data to the frame buffer fragment
            while(row) {
                if (row & 0x01) {
                    *output++ = 0xffff;
                } else {
                    *output++;
                }
                row >>= 1;
            }
        }

        // Advance the cursor
        ox += FONT_WIDTH + 1 - si;
        si = 0;
    }
}

static void _textSequence(_SceneContext *scene, Point worldPos,  _Node* node) {
    _Node *render = _addRenderNode(scene, node, worldPos, _textRender);
    if (render == NULL) { return; }
    render->a = node->a;
    render->b = node->b;
}

Node scene_createText(SceneContext _scene, const char* data, uint32_t dataLength) {
    _SceneContext* scene = _scene;
    
    _Node* node = _addNode(scene, PointZero);
    if (node == NULL) { return NULL; }

    node->func.sequenceFunc = _textSequence;

    node->a.data = (uint8_t*)data;
    TextInfo info = { .flags = TextFlagModeConst, .length = 255, .color = 0x0000 };
    node->b.text = info;

    return node;
}

Node scene_createTextFlip(SceneContext _scene, char* data, uint32_t dataLength) {
    _SceneContext* scene = _scene;
    
    _Node* node = _addNode(scene, PointZero);
    if (node == NULL) { return NULL; }

    node->func.sequenceFunc = _textSequence;

    node->a.data = (uint8_t*)data;
    TextInfo info = { .flags = 0, .length = dataLength / 2, .color = 0x0000 };
    node->b.text = info;

    return node;
}

Node scene_createTextAlloc(SceneContext _scene, uint32_t textLength) {
    _SceneContext* scene = _scene;

    uint8_t *data = malloc(textLength * 2);
    if (data == NULL) { return NULL; }

    _Node* node = _addNode(scene, PointZero);
    if (node == NULL) { return NULL; }

    node->func.sequenceFunc = _textSequence;

    node->a.data = data;
    TextInfo info = { .flags = TextFlagModeAlloc, .length = textLength, .color = 0x0000 };
    node->b.text = info;

    return node;
}

void scene_textSetText(Node _node, const char* const text, uint32_t _length) {
    _Node *node = _node;
    if (node == NULL) { return; }

    uint32_t flags = node->b.text.flags;

    // Cannot modify a const memory address
    if (flags & TextFlagModeConst) { return; }

    uint32_t length = node->b.text.length;
    uint32_t offset = ((flags ^ 0x01) & TextFlagFlipPage) * length; 
    if (_length < length) { length = _length; }

    strncpy(&node->a.data[offset], text, length);
    node->b.text.flags ^= TextFlagFlipPage;
}

void scene_textSetColor(Node _node, rgb_t color) {
    _Node *node = _node;
    if (node == NULL) { return; }

    node->a.text.color = color;
}

void scene_textSetColorAlpha(Node _node, rgba_t color) {
    _Node *node = _node;
    if (node == NULL) { return; }

    node->a.text.color = color & 0xffff;
    node->a.text.flags |= (color >> 16) & 0xf8;
}


/********************************************************
 * DEBUG
 ********************************************************/


static void dumpNode(_SceneContext *scene, _Node *node, uint32_t indent) {

    uint8_t padding[2 * indent + 1];
    memset(padding, 32, sizeof(padding));
    padding[2 * indent] = 0;

    if (node->func.sequenceFunc == _groupSequence) {
        printf("%s    - Group Node %d (x=%d, y=%d)\n", padding, _getNodeIndex(scene, node), node->pos.x, node->pos.y);
        dumpNode(scene, node->a.ptr, indent + 1);

    } else if (node->func.sequenceFunc == _boxSequence) {
        printf("%s    - Box Node %d (x=%d, y=%d, width=%d, height=%d, color=0x%04x)\n", padding,
            _getNodeIndex(scene, node), node->pos.x, node->pos.y, node->a.size.width, node->a.size.height,
            __builtin_bswap16(node->b.u32));
    } else if (node->func.sequenceFunc == _fillSequence) {
        printf("%s    - Fill Node %d (color=0x%04x)\n", padding, _getNodeIndex(scene, node), __builtin_bswap16(node->a.u32));
    } else if (node->func.sequenceFunc == _imageSequence) {
        printf("%s    - Image Node %d (x=%d, y=%d, width=%d, height=%d, type=0x%02x)\n", padding,
            _getNodeIndex(scene, node), node->pos.x, node->pos.y, node->b.size.width, node->b.size.height, node->a.data[0]);
    } else if (node->func.sequenceFunc == _textSequence) {
        TextInfo info = node->b.text;
        printf("%s    - Text Node %d (x=%d, y=%d, text=@TODO, flags=%x, length=%d, color=%04x)\n", padding,
            _getNodeIndex(scene, node), node->pos.x, node->pos.y, info.flags, info.length, info.color);

    } else if (node->func.sequenceFunc == _freeSequence) {
        printf("%s    - Pending Free Node %d\n", padding, _getNodeIndex(scene, node));

    } else if (node->func.sequenceFunc == _animationCompletionSequence) {
        uint32_t animationId = (node->pos.x << 16) | node->pos.y;

        printf("%s    - Animation Completion Callback %d (animationId=%d, target=%d)\n", padding,
            _getNodeIndex(scene, node), animationId, _getNodeIndex(scene, node->b.ptr));

    } else if (node->func.renderFunc == _boxRender) {
        printf("%s    - Box Render Node %d (x=%d, y=%d, width=%d, height=%d, color=0x%04x)\n", padding,
            _getNodeIndex(scene, node), node->pos.x, node->pos.y, node->a.size.width, node->a.size.height,
            __builtin_bswap16 (node->b.u32));
    } else if (node->func.renderFunc == _fillRender) {
        printf("%s    - Fill Render Node %d (color=0x%04x)\n", padding, _getNodeIndex(scene, node), __builtin_bswap16 (node->a.u32));
    } else if (node->func.renderFunc == _imageRender) {
        printf("%s    - Image Render Node %d (x=%d, y=%d, width=%d, height=%d, type=0x%02x)\n", padding,
            _getNodeIndex(scene, node), node->pos.x, node->pos.y, node->b.size.width, node->b.size.height, node->a.data[0]);
    } else if (node->func.renderFunc == _textRender) {
        TextInfo info = node->b.text;
        printf("%s    - Text Render Node %d (x=%d, y=%d, text=@TODO, flags=%x, length=%d, color=%04x)\n", padding,
            _getNodeIndex(scene, node), node->pos.x, node->pos.y, info.flags, info.length, info.color);

    } else if (node->func.animateFunc == _nodeAnimatePosition) {
        uint32_t duration = node->pos.x;
        int32_t end = node->b.i32;
        float t = 1.0f - ((float)(end - scene->tick) / (float)duration);
        if (t >= 1.0f) { t = 1.0f; }
        printf("%s    - Position Animate Node %d (start=%d, end=%d, t=%f, duration=%d, target=%d)\n", padding,
            _getNodeIndex(scene, node), end - duration, end, t, duration, _getNodeIndex(scene, node->a.ptr));

    } else {
        printf("%s    - Unknown Node %d (x=%d, y=%d)\n", padding, _getNodeIndex(scene, node), node->pos.x, node->pos.y);  
    }

    if (node->nextNode != NULL) { dumpNode(scene, node->nextNode, indent); }
}

void scene_dump(SceneContext _scene) {
    _SceneContext *scene = _scene;
    printf("Scene: %p\n", scene);
    printf("  - tick: %d\n", scene->tick);
    printf("  - nextFree: %d\n", _getNodeIndex(scene, scene->nextFree));

    printf("  - Scene Graph:\n");
    dumpNode(scene, scene->root, 0);

    printf("  - Render Sequence:\n");
    if (scene->renderHead) {
        dumpNode(scene, scene->renderHead, 0);
    }

    printf("  - Animations:\n");
    if (scene->animateHead) {
        dumpNode(scene, scene->animateHead, 0);
    }

}