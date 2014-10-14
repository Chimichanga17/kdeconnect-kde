/**
 * Copyright 2014 Ahmed I. Khalil <ahmedibrahimkhali@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "mousepadplugin.h"

#include <core/networkpackage.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>
#include <fakekey/fakekey.h>

K_PLUGIN_FACTORY( KdeConnectPluginFactory, registerPlugin< MousepadPlugin >(); )
K_EXPORT_PLUGIN( KdeConnectPluginFactory("kdeconnect_mousepad", "kdeconnect-plugins") )

enum MouseButtons {
    LeftMouseButton = 1,
    MiddleMouseButton = 2,
    RightMouseButton = 3,
    MouseWheelUp = 4,
    MouseWheelDown = 5
};

//Translation table to keep in sync within all the implementations
int SpecialKeysMap[] = {
    0,              // Invalid
    XK_BackSpace,   // 1
    XK_Tab,         // 2
    XK_Linefeed,    // 3
    XK_Left,        // 4
    XK_Up,          // 5
    XK_Right,       // 6
    XK_Down,        // 7
    XK_Page_Up,     // 8
    XK_Page_Down,   // 9
    XK_Home,        // 10
    XK_End,         // 11
    XK_Return,      // 12
    XK_Delete,      // 13
    XK_Escape,      // 14
};

template <typename T, size_t N>
size_t arraySize(T(&arr)[N]) { (void)arr; return N; }

MousepadPlugin::MousepadPlugin(QObject* parent, const QVariantList& args)
    : KdeConnectPlugin(parent, args), m_display(0), m_fakekey(0)
{

}

MousepadPlugin::~MousepadPlugin()
{
    if (m_display) {
        XCloseDisplay(m_display);
        m_display = 0;
    }
    if (m_fakekey) {
        free(m_fakekey);
        m_fakekey = 0;
    }
}

bool MousepadPlugin::receivePackage(const NetworkPackage& np)
{
    //TODO: Split in several package types to avoid big if statements: mouse move, mouse click and keyboard events

    float dx = np.get<float>("dx", 0);
    float dy = np.get<float>("dy", 0);

    bool isSingleClick = np.get<bool>("singleclick", false);
    bool isDoubleClick = np.get<bool>("doubleclick", false);
    bool isMiddleClick = np.get<bool>("middleclick", false);
    bool isRightClick = np.get<bool>("rightclick", false);
    bool isScroll = np.get<bool>("scroll", false);
    QString key = np.get<QString>("key", "");
    int specialKey = np.get<int>("specialKey", 0);

    if (isSingleClick || isDoubleClick || isMiddleClick || isRightClick || isScroll || !key.isEmpty() || specialKey) {

        if(!m_display) {
            m_display = XOpenDisplay(NULL);
            if(!m_display) {
                kDebug(debugArea()) << "Failed to open X11 display";
                return false;
            }
        }

        if (isSingleClick) {
            XTestFakeButtonEvent(m_display, LeftMouseButton, True, 0);
            XTestFakeButtonEvent(m_display, LeftMouseButton, False, 0);
        } else if (isDoubleClick) {
            XTestFakeButtonEvent(m_display, LeftMouseButton, True, 0);
            XTestFakeButtonEvent(m_display, LeftMouseButton, False, 0);
            XTestFakeButtonEvent(m_display, LeftMouseButton, True, 0);
            XTestFakeButtonEvent(m_display, LeftMouseButton, False, 0);
        } else if (isMiddleClick) {
            XTestFakeButtonEvent(m_display, MiddleMouseButton, True, 0);
            XTestFakeButtonEvent(m_display, MiddleMouseButton, False, 0);
        } else if (isRightClick) {
            XTestFakeButtonEvent(m_display, RightMouseButton, True, 0);
            XTestFakeButtonEvent(m_display, RightMouseButton, False, 0);
        } else if( isScroll ) {
            if (dy < 0) {
                XTestFakeButtonEvent(m_display, MouseWheelDown, True, 0);
                XTestFakeButtonEvent(m_display, MouseWheelDown, False, 0);
            } else if (dy > 0) {
                XTestFakeButtonEvent(m_display, MouseWheelUp, True, 0);
                XTestFakeButtonEvent(m_display, MouseWheelUp, False, 0);
            }
        } else if (!key.isEmpty() || specialKey) {

            if (specialKey)
            {
                if (specialKey > (int)arraySize(SpecialKeysMap)) {
                    kDebug(debugArea()) << "Unsupported special key identifier";
                    return false;
                }

                int keycode = XKeysymToKeycode(m_display, SpecialKeysMap[specialKey]);
                XTestFakeKeyEvent (m_display, keycode, True, 0);
                XTestFakeKeyEvent (m_display, keycode, False, 0);

            } else {

                if (!m_fakekey) {
                    m_fakekey = fakekey_init(m_display);
                    if (!m_fakekey) {
                        kDebug(debugArea()) << "Failed to initialize libfakekey";
                        return false;
                    }
                }

                //We use fakekey here instead of XTest (above) because it can handle utf characters instead of keycodes.
                fakekey_press(m_fakekey, (const unsigned char*)key.toUtf8().constData(), -1, 0);
                fakekey_release(m_fakekey);
            }

        }

        XFlush(m_display);

    } else { //Is a mouse move event
        QPoint point = QCursor::pos();
        QCursor::setPos(point.x() + (int)dx, point.y() + (int)dy);
    }
    return true;
}
