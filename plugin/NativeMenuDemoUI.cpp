/*
 * DISTRHO Plugin Framework (DPF)
 * Copyright (C) 2012-2021 Filipe Coelho <falktx@falktx.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any purpose with
 * or without fee is hereby granted, provided that the above copyright notice and this
 * permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD
 * TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN
 * NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "DistrhoUI.hpp"

#include "ResizeHandle.hpp"

#include "extra/Thread.hpp"
#include <gtk/gtk.h>

#include <string>

START_NAMESPACE_DISTRHO

using DGL_NAMESPACE::ResizeHandle;

// -----------------------------------------------------------------------------------------------------------

class NativeMenuThread : public Thread
{
    // Status
    bool fIsGtkLoaded;

    // GTK/GDK stuff
    GdkScreen *screen;
    GdkWindow *root_window;
    GtkWidget *test_menu, *test_menu_item[3];
    GdkEvent *dummy_trigger_event;
public:

    // Mouse position
    int mouse_X, mouse_Y;

    NativeMenuThread() : fIsGtkLoaded(false), mouse_X(0), mouse_Y(0), test_menu(NULL) {}

    // TODO: Add reference count!
    void stopThread(int timeOutMilliseconds)
    {
        if (fIsGtkLoaded)
            gtk_main_quit();

        Thread::stopThread(timeOutMilliseconds);
    }

    void popUpMenu()
    {
        DISTRHO_SAFE_ASSERT_RETURN(test_menu && fIsGtkLoaded == true, )

        GdkRectangle rectangle;
        rectangle.x = mouse_X;
        rectangle.y = mouse_Y;
        gtk_menu_popup_at_rect(GTK_MENU(this->test_menu), this->root_window, &rectangle, GDK_GRAVITY_NORTH_WEST, GDK_GRAVITY_SOUTH_WEST, dummy_trigger_event);
    }

    static void menu_item_callback(GtkMenuItem *menu_item, gpointer user_data)
    {
        d_stderr("Clicked menu item: %s", (const gchar*)user_data);
    }

private:
    void run() override
    {
        if ( !gtk_init_check( NULL, NULL ) )
        {
            d_stderr2("ERROR: Cannot init GTK!");
            return;
        } else {
            d_stderr2("Info: GTK started!");
            fIsGtkLoaded = true;
        }

        // Get the default screen
        this->screen = gdk_screen_get_default();

        // Get the root window of the default screen
        this->root_window = gdk_screen_get_root_window(screen);

        // Create a pop-up menu
        this->test_menu = gtk_menu_new();
        this->test_menu_item[0] = gtk_menu_item_new_with_label("MenuItem 0");
        this->test_menu_item[1] = gtk_menu_item_new_with_label("MenuItem 1");
        this->test_menu_item[2] = gtk_menu_item_new_with_label("MenuItem 2");
        for (auto i = 0; i < 3; i++)
            gtk_menu_shell_append(GTK_MENU_SHELL(this->test_menu), this->test_menu_item[i]);

        // Connect signal handler to the menu item
        for (auto i = 0; i < 3; i++) {
            char buffer[100];
            snprintf(buffer, 100, "MenuItem %d", i);
            g_signal_connect(G_OBJECT(this->test_menu_item[i]), "activate", G_CALLBACK(menu_item_callback), g_strdup(buffer));
        }

        // Create a dummy trigger event in order to mute the following warnings:
        //   - no trigger event for menu popup
        //   - Event with type 4 not holding a GdkSeat.
        this->dummy_trigger_event = gdk_event_new(GDK_BUTTON_PRESS);
        gdk_event_set_device(this->dummy_trigger_event, gdk_seat_get_pointer(gdk_display_get_default_seat(gdk_screen_get_display(screen))));
        gdk_event_set_screen(this->dummy_trigger_event, gdk_screen_get_default());

        // Activate the menu
        gtk_widget_show_all(this->test_menu);

        // Start GTK Main loop
        gtk_main();

        gtk_widget_destroy(test_menu);

        d_stderr2("Info: GTK exited!");
        fIsGtkLoaded = false;
    }
};

// -----------------------------------------------------------------------------------------------------------

class InfoExampleUI : public UI
{
public:
    InfoExampleUI()
        : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT),
          fSampleRate(getSampleRate()),
          fResizable(isResizable()),
          fScale(1.0f),
          fScaleFactor(getScaleFactor()),
          fResizeHandle(this)
    {
        std::memset(fParameters, 0, sizeof(float)*kParameterCount);
        std::memset(fStrBuf, 0, sizeof(char)*(0xff+1));

#ifdef DGL_NO_SHARED_RESOURCES
        createFontFromFile("sans", "/usr/share/fonts/truetype/ttf-dejavu/DejaVuSans.ttf");
#else
        loadSharedResources();
#endif

        setGeometryConstraints(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT, true);

        // no need to show resize handle if window is user-resizable
        if (fResizable)
            fResizeHandle.hide();

        // Load native menu thread
        fNativeMenuThread.startThread();
    }

    ~InfoExampleUI()
    {
        // Ask GTK to exit, then wait until native menu thread exits
        fNativeMenuThread.stopThread(-1);
    }

protected:
   /* --------------------------------------------------------------------------------------------------------
    * DSP/Plugin Callbacks */

   /**
      A parameter has changed on the plugin side.
      This is called by the host to inform the UI about parameter changes.
    */
    void parameterChanged(uint32_t index, float value) override
    {
        repaint();
    }

   /* --------------------------------------------------------------------------------------------------------
    * DSP/Plugin Callbacks (optional) */

   /**
      Optional callback to inform the UI about a sample rate change on the plugin side.
    */
    void sampleRateChanged(double newSampleRate) override
    {
        fSampleRate = newSampleRate;
        repaint();
    }

   /* --------------------------------------------------------------------------------------------------------
    * Widget Callbacks */

   /**
      The NanoVG drawing function.
    */
    void onNanoDisplay() override
    {
        const float lineHeight = 20 * fScale;

        fontSize(15.0f * fScale);
        textLineHeight(lineHeight);

        float x = 0.0f * fScale;
        float y = 15.0f * fScale;

        // Title
        drawLeft(x, y, "Native Menu Test");
        drawRight(x, y, "GTK");
        y+=lineHeight;

        // Mouse position (for debug)
        drawLeft(x, y, "Mouse X:");
        drawRight(x, y, std::to_string(fNativeMenuThread.mouse_X).c_str());
        y+=lineHeight;

        drawLeft(x, y, "Mouse Y:");
        drawRight(x, y, std::to_string(fNativeMenuThread.mouse_Y).c_str());
        y+=lineHeight;

        // Window offset (for debug)
        drawLeft(x, y, "Wnd offset X:");
        drawRight(x, y, std::to_string(getWindow().getOffsetX()).c_str());
        y+=lineHeight;

        drawLeft(x, y, "Wnd offset Y:");
        drawRight(x, y, std::to_string(getWindow().getOffsetY()).c_str());
        y+=lineHeight;
    }

    bool onMouse(const MouseEvent& ev) override
    {
        if (ev.button == kMouseButtonRight) {
            fNativeMenuThread.popUpMenu();
        }

        return true;
    }

    bool onMotion(const MotionEvent& ev) override
    {
        fNativeMenuThread.mouse_X = ev.pos.getX() + getWindow().getOffsetX();
        fNativeMenuThread.mouse_Y = ev.pos.getY() + getWindow().getOffsetY();

        // Trigger repaint
        repaint();

        return true;
    }

    void onResize(const ResizeEvent& ev) override
    {
        fScale = static_cast<float>(ev.size.getHeight())/static_cast<float>(DISTRHO_UI_DEFAULT_HEIGHT);

        UI::onResize(ev);
    }

    void uiScaleFactorChanged(const double scaleFactor) override
    {
        fScaleFactor = scaleFactor;
    }

    // -------------------------------------------------------------------------------------------------------

private:
    // Parameters
    [[maybe_unused]] float  fParameters[kParameterCount];
    [[maybe_unused]] double fSampleRate;

    // UI stuff
    bool fResizable;
    float fScale; // our internal scaling
    double fScaleFactor; // host reported scale factor
    ResizeHandle fResizeHandle;

    // temp buf for text
    char fStrBuf[0xff+1];

    // Native menu deamon thread
    NativeMenuThread fNativeMenuThread;

    // helpers for putting text into fStrBuf and returning it
    const char* getTextBufInt(const int value)
    {
        std::snprintf(fStrBuf, 0xff, "%i", value);
        return fStrBuf;
    }

    const char* getTextBufFloat(const float value)
    {
        std::snprintf(fStrBuf, 0xff, "%.1f", value);
        return fStrBuf;
    }

    const char* getTextBufFloatExtra(const float value)
    {
        std::snprintf(fStrBuf, 0xff, "%.2f", value + 0.001f);
        return fStrBuf;
    }

    const char* getTextBufTime(const uint64_t frame)
    {
        const uint32_t time = frame / uint64_t(fSampleRate);
        const uint32_t secs =  time % 60;
        const uint32_t mins = (time / 60) % 60;
        const uint32_t hrs  = (time / 3600) % 60;
        std::snprintf(fStrBuf, 0xff, "%02i:%02i:%02i", hrs, mins, secs);
        return fStrBuf;
    }

    // helpers for drawing text
    void drawLeft(float x, const float y, const char* const text, const int offset = 0)
    {
        const float width = (100.0f + offset) * fScale;
        x += offset * fScale;
        beginPath();
        fillColor(200, 200, 200);
        textAlign(ALIGN_RIGHT|ALIGN_TOP);
        textBox(x, y, width, text);
        closePath();
    }

    void drawRight(float x, const float y, const char* const text, const int offset = 0)
    {
        const float width = (100.0f + offset) * fScale;
        x += offset * fScale;
        beginPath();
        fillColor(255, 255, 255);
        textAlign(ALIGN_LEFT|ALIGN_TOP);
        textBox(x + (105 * fScale), y, width, text);
        closePath();
    }

   /**
      Set our UI class as non-copyable and add a leak detector just in case.
    */
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InfoExampleUI)
};

/* ------------------------------------------------------------------------------------------------------------
 * UI entry point, called by DPF to create a new UI instance. */

UI* createUI()
{
    return new InfoExampleUI();
}

// -----------------------------------------------------------------------------------------------------------

END_NAMESPACE_DISTRHO
