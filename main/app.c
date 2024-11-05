
uint32_t app_keys(App *app) {
    return (app->latch & app->keys);
}

uint32_t app_keysChanged(App *app, uint32_t keys) {
    keys &= app->keys;
    return (app->previousLatch ^ app->latch) & keys;
}

uint32_t app_keysDown(App *app, uint32_t keys) {
    keys &= context->keys;
    return (context->latch & keys) == keys;
}


void startApp(void* pvParameter, appDefn, void* app) {
    uint32_t *ready = (uint32_t*)pvParameter;
    pvParameter->events = new Queue();

    // Device Information Service Data
    pvParameter->ready = 1;

    DispatchEvent dispatch = { 0 };

    while (true) {
        BaseType_t result = xQueueReceive(eventQueue, &dispatch, QUEUE_TIMEOUT);
        if (result != pdPass) { continue; }
        EventPayload event = { 0 };

        event.id = dispatch.id;
        event.event = dispatch.event;
        event.arg = dispatch.arg;
        
        dispatch.callback(event, );
    }
}

void exampleStartApp(StartApp start) {
    MyStruct myStruct;
    AppDefn appDefn = {
        .update
    };

    start();
    startApp(pvParameter, appDefinition, myStruct);
}

// Custom App
typedef struct MenuApp {
    uint32_t someProp;
} MenuApp;

int menuMain(App *app, void *arg) {
    // Set up state
    app->state->someProp = 32;

    app_onEvent(EventRender, _tick, app);

    // Set up some event handlers
    app_onEvent(EventKeyChange | KeyCancel, _pressFire, app);

    // Return the post-sequence tick() function
    //return updateDisplay;
}

// app.h
typedef struct App {
    FfxScene scene;
    FfxNode node;
    void *state;
    void *arg;
} App;

typedef void (*DisplayFunc)(FfxScene scene, FfxNode node, uint32_t tick);

typedef DisplayFunc (*AppInit)(FfxScene scene, FfxNode node, void* app,
  void* arg);

int app_push(AppInit appInit, size_t appState, void *arg);


// main_app()
app_push(menuMain, sizeof(MenuApp), NULL);


static _startTask(void *pvParameter) {
    AppParams *params = pvParameter;
    uint8_t state[params->stateSize];
    memset(state, 0, sizeof(state));

    params->init(scene, node, app, arg);

    while (true) {
        BaseType_t result = xQueueReceive(eventQueue, &someEvent, QUEUE_TIMEOUT);
        if (result != pdPass) { continue; }
        
    }
}

int app_push(AppInit appInit, size_t stateSize, void* arg) {
    register queue;
    emit(RegisterQueue?)
    vCreateTask();
}
