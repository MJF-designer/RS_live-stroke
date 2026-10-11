//========================================================================================
//
//  $File$
//
//  $Revision$
//
//  Copyright 1987 Adobe Systems Incorporated. All rights reserved.
//
//========================================================================================

#ifndef _LIVEDROPSHADOWPLUGIN_H_
#define _LIVEDROPSHADOWPLUGIN_H_

#include "IllustratorSDK.h"
#include "Plugin.hpp"
#include "LiveDropShadowID.h"
#include "LiveDropShadowSuites.h"
#include "LiveDropShadowGroup.h"
#include "SDKAboutPluginsHelper.h"

class LiveDropShadowPlugin : public Plugin {
public:
    LiveDropShadowPlugin(SPPluginRef pluginRef);
    virtual ~LiveDropShadowPlugin() {}

    static void FixupVTable(LiveDropShadowPlugin* plugin);

    virtual ASErr StartupPlugin(SPInterfaceMessage* message);
    virtual ASErr PostStartupPlugin();
    virtual ASErr ShutdownPlugin(SPInterfaceMessage* message);
    virtual ASErr Notify(AINotifierMessage* message);
    virtual ASErr Message(char* caller, char* selector, void *message);
    virtual ASErr PluginGroupUpdate(AIPluginGroupMessage* message);
    virtual ASErr UpdateMenuItem(AIMenuMessage* message);
    virtual ASErr GoMenuItem(AIMenuMessage* message);

private:
    ASErr AddMenus(SPInterfaceMessage* message);
    ASErr AddNotifiers(SPInterfaceMessage* message);

    AIMenuItemHandle fAboutPluginMenu;
    AIMenuItemHandle fMakeMenuHandle;
    AIMenuItemHandle fReleaseMenuHandle;
    AIMenuItemHandle fAdjustMenuHandle;
    AIPluginGroupHandle fPluginGroupHandle;
    AINotifierHandle fAppShutdownNotifier;

    LiveDropShadowGroup* fLiveDropShadowGroup;
    LiveDropShadowGroup::ObjectData fObjectData;
};

#endif // _LIVEDROPSHADOWPLUGIN_H_
