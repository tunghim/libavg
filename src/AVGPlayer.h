//
// $Id$
//

#ifndef _AVGPlayer_H_
#define _AVGPlayer_H_

#include "IAVGPlayer.h"
#include "IAVGEvent.h"
#include "AVGFramerateManager.h"
#include "AVGTimeout.h"

#include <libxml/parser.h>

#include <SDL/SDL.h>

#include <xpcom/nsCOMPtr.h>

#include <map>
#include <string>
#include <vector>

class AVGAVGNode;
class AVGNode;
class AVGVisibleNode;
class AVGContainer;
class AVGEvent;
class AVGSDLDisplayEngine;
class IJSEvalKruecke;

class PLPoint;

//fe906737-5231-4a55-a9da-d348a7da581f
#define AVGPLAYER_CID \
{ 0xfe906737, 0x5231, 0x4a55, { 0xa9, 0xda, 0xd3, 0x48, 0xa7, 0xda, 0x58, 0x1f } }

#define AVGPLAYER_CONTRACTID "@c-base.org/avgplayer;1"

class AVGPlayer: public IAVGPlayer
{
    public:
        AVGPlayer ();
        virtual ~AVGPlayer ();

        NS_DECL_ISUPPORTS

        NS_DECL_IAVGPLAYER

        void loadFile (const std::string& fileName);
        void play ();
        void stop ();
        void doFrame ();

        AVGNode * getElementByID (const std::string id);
        AVGAVGNode * getRootNode ();
        void addEvent (int time, AVGEvent * event);

    private:
        AVGNode * createNodeFromXml(const xmlNodePtr xmlNode, AVGContainer * pParent);
        void getVisibleNodeAttrs (const xmlNodePtr xmlNode, 
                std::string * pid, int * px, int * py, int * pz,
                int * pWidth, int * pHeight, double * pOpacity);
        void initEventHandlers (AVGNode * pAVGNode, const xmlNodePtr xmlNode);

        AVGAVGNode * m_pRootNode;
        AVGSDLDisplayEngine * m_pDisplayEngine;
        bool m_IsFullscreen;

        std::string m_CurDirName;
        AVGFramerateManager m_FramerateManager;
        bool m_bStopping;

        typedef std::map<std::string, AVGNode*> NodeIDMap;
        NodeIDMap m_IDMap;

        // Event handling
        int addTimeout(const AVGTimeout& timeout);
        void handleTimers();
        void handleEvents();
        AVGEvent* createCurEvent();

        void dumpEvent(AVGEvent* pEvent);
        void dumpEventStr(const std::string& EventName, bool IsMouse);
        void handleMouseEvent (AVGEvent* pEvent);

        IAVGEvent * m_CurEvent;
        IJSEvalKruecke * m_pKruecke;
        std::vector<AVGTimeout> m_PendingTimeouts;
        AVGNode * m_pLastMouseNode;
        int m_EventDebugLevel;
};

#endif //_AVGPlayer_H_
