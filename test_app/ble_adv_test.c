#include "advertiser.h"
#include <gui/gui.h>
#include <gui/elements.h>
#include <gui/view_dispatcher.h>
#include <stdlib.h>

#define TAG "BleAdvTest"

typedef struct {
    bool running;
    bool faulted;
    const char* status;
} TestModel;

typedef struct {
    Advertiser advertiser;
    ViewDispatcher* dispatcher;
    View* view;
    unsigned starts;
    unsigned stops;
} TestApp;

// Experimental manufacturer ID 0xFFFF, literal test marker; no vendor pairing/command data.
static const uint8_t test_payload[] = {0x08, 0xFF, 0xFF, 0xFF, 'T', 'E', 'S', 'T', 0x01};
_Static_assert(sizeof(test_payload) <= EXTRA_BEACON_MAX_DATA_SIZE, "Legacy ADV limit");

static void draw_callback(Canvas* canvas, void* model_ptr) {
    TestModel* model = model_ptr;
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignTop, "BLE Adv Test");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 27, AlignCenter, AlignTop, "Fixed packet / 200 ms");
    canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignTop, model->status);
    if(!model->faulted) elements_button_center(canvas, model->running ? "Stop" : "Start");
    elements_button_left(canvas, "Back: Exit");
}

static bool input_callback(InputEvent* event, void* context) {
    TestApp* app = context;
    // Dispatch all HAL operations onto one app thread; draw only reads the locked model.
    if(event->key == InputKeyOk && event->type == InputTypeShort) {
        view_dispatcher_send_custom_event(app->dispatcher, 1);
        return true;
    }
    return false;
}

static bool custom_callback(void* context, uint32_t event) {
    TestApp* app = context;
    if(event != 1) return false;
    if(app->advertiser.faulted) return true;
    bool ok = true;
    if(app->advertiser.running) {
        ok = advertiser_stop(&app->advertiser);
        if(ok) app->stops++;
    } else {
        if(!app->advertiser.acquired) ok = advertiser_initialize(&app->advertiser);
        if(ok && !app->advertiser.configured)
            ok = advertiser_configure(&app->advertiser, test_payload, sizeof(test_payload));
        if(ok) {
            ok = advertiser_start(&app->advertiser);
            if(ok) app->starts++;
        }
    }
    with_view_model(
        app->view,
        TestModel * model,
        {
            model->running = app->advertiser.running;
            model->faulted = app->advertiser.faulted;
            model->status = model->faulted ? "HAL error: reboot required" :
                            !ok           ? "BLE unavailable or busy" :
                            model->running ? "Advertising" : "Stopped";
        },
        true);
    return true;
}

static void tick_callback(void* context) {
    TestApp* app = context;
    // Allows USB CLI log capture to verify actual HAL success after injected input.
    FURI_LOG_I(
        TAG,
        "Status: %s, starts=%u stops=%u",
        app->advertiser.faulted ? "faulted" :
        app->advertiser.running ? "advertising" : "stopped",
        app->starts,
        app->stops);
}

static bool back_callback(void* context) {
    TestApp* app = context;
    // Final cleanup stops the beacon before releasing any app objects.
    view_dispatcher_stop(app->dispatcher);
    return true;
}

int32_t ble_adv_test(void* context) {
    UNUSED(context);
    FURI_LOG_I(TAG, "App startup (OFW 1.4.3 test)");
    TestApp* app = calloc(1, sizeof(TestApp));
    if(!app) {
        FURI_LOG_E(TAG, "App allocation failed");
        return -1;
    }
    Gui* gui = furi_record_open(RECORD_GUI);
    app->dispatcher = view_dispatcher_alloc();
    app->view = view_alloc();
    view_allocate_model(app->view, ViewModelTypeLocking, sizeof(TestModel));
    with_view_model(app->view, TestModel * model, { model->status = "Stopped"; }, false);
    view_set_context(app->view, app);
    view_set_draw_callback(app->view, draw_callback);
    view_set_input_callback(app->view, input_callback);
    view_dispatcher_set_event_callback_context(app->dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->dispatcher, custom_callback);
    view_dispatcher_set_tick_event_callback(app->dispatcher, tick_callback, 1000);
    view_dispatcher_set_navigation_event_callback(app->dispatcher, back_callback);
    view_dispatcher_add_view(app->dispatcher, 0, app->view);
    view_dispatcher_attach_to_gui(app->dispatcher, gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_switch_to_view(app->dispatcher, 0);
    view_dispatcher_run(app->dispatcher);

    FURI_LOG_I(TAG, "Cleanup started");
    bool clean = advertiser_deinitialize(&app->advertiser);
    view_dispatcher_remove_view(app->dispatcher, 0);
    view_free(app->view);
    view_dispatcher_free(app->dispatcher);
    furi_record_close(RECORD_GUI);
    free(app);
    if(clean) FURI_LOG_I(TAG, "Cleanup completed");
    else FURI_LOG_E(TAG, "App objects freed; BLE cleanup incomplete, reboot required");
    return clean ? 0 : -1;
}
