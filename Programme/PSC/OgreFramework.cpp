#include "OgreFramework.h"
#include "macUtils.h"

#include "Joystick.h"

using namespace Ogre; 

namespace Ogre
{
    template<> OgreFramework* Ogre::Singleton<OgreFramework>::ms_Singleton = 0;
};

OgreFramework::OgreFramework()
{
	m_MoveSpeed			= 0.1f;
	m_RotateSpeed       = 0.3f;
    
	m_bShutDownOgre     = false;
	m_iNumScreenShots   = 0;
    
	m_pRoot				= 0;
	m_pSceneMgr			= 0;
	m_pRenderWnd        = 0;
	m_pCamera			= 0;
	m_pViewport			= 0;
	m_pLog				= 0;
	m_pTimer			= 0;
    
	m_pInputMgr			= 0;
	m_pKeyboard			= 0;
	m_pMouse			= 0;
    
#if OGRE_PLATFORM == OGRE_PLATFORM_APPLE
    m_ResourcePath = macBundlePath() + "/Contents/Resources/";
#elif defined(OGRE_IS_IOS)
    m_ResourcePath = macBundlePath() + "/";
#else
    m_ResourcePath = "";
#endif
    m_pTrayMgr          = 0;
    m_FrameEvent        = Ogre::FrameEvent();
}

//|||||||||||||||||||||||||||||||||||||||||||||||
#if defined(OGRE_IS_IOS)
bool OgreFramework::initOgre(Ogre::String wndTitle, OIS::KeyListener *pKeyListener, OIS::MultiTouchListener *pMouseListener)
#else
bool OgreFramework::initOgre(Ogre::String wndTitle, OIS::KeyListener *pKeyListener, OIS::MouseListener *pMouseListener, OIS::JoyStickListener *pJoyStickListener)
#endif
{
    new Ogre::LogManager();

	m_pLog = Ogre::LogManager::getSingleton().createLog("OgreLogfile.log", true, true, false);
	m_pLog->setDebugOutputEnabled(true);
    
    String pluginsPath;
    // only use plugins.cfg if not static
#ifndef OGRE_STATIC_LIB
    pluginsPath = m_ResourcePath + "plugins.cfg";
#endif
    
    m_pRoot = new Ogre::Root(pluginsPath);//, Ogre::macBundlePath() + "/ogre.cfg");
    
#ifdef OGRE_STATIC_LIB
    m_StaticPluginLoader.load();
#endif
    
	if(!m_pRoot->showConfigDialog())
		return false;
	//m_pRenderWnd = m_pRoot->initialise(true, wndTitle);
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK);
	
	Joystick::handler()->prepareJoystick();
	
	SDL_Surface *screen = SDL_SetVideoMode(640, 480, 0, SDL_OPENGL);
	m_pRoot->initialise(false); // false for SDL
	
	NameValuePairList misc;
	//SDL
#ifdef WINDOWS
	SDL_SysWMinfo wmInfo;
	SDL_VERSION(&wmInfo.version);
	SDL_GetWMInfo(&wmInfo);
	
	size_t winHandle = reinterpret_cast<size_t>(wmInfo.window);
	size_t winGlContext = reinterpret_cast<size_t>(wmInfo.hglrc);
	
	misc["externalWindowHandle"] = StringConverter::toString(winHandle);
	misc["externalGLContext"] = StringConverter::toString(winGlContext);
#else
	misc["currentGLContext"] = String("True");
#endif
	misc["vsync"]=String("True");
	
	m_pRenderWnd = m_pRoot->createRenderWindow("MainRenderWindow", 800, 600, false, &misc);
	m_pRenderWnd->setVisible(true);
    
	m_pSceneMgr = m_pRoot->createSceneManager(ST_GENERIC, "SceneManager");
	m_pSceneMgr->setAmbientLight(Ogre::ColourValue(0.7f, 0.7f, 0.7f));
	
	m_pCamera = m_pSceneMgr->createCamera("Camera");
	m_pCamera->setPosition(Vector3(0,500,2000));
	m_pCamera->lookAt(Vector3(0, 500, -3000));
	m_pCamera->setNearClipDistance(1000);
    
	m_pViewport = m_pRenderWnd->addViewport(m_pCamera);
	m_pViewport->setBackgroundColour(ColourValue(0.8f, 0.7f, 0.6f, 1.0f));
    
	m_pCamera->setAspectRatio(Real(m_pViewport->getActualWidth()) / Real(m_pViewport->getActualHeight()));
	
	m_pViewport->setCamera(m_pCamera);
    
	unsigned long hWnd = 0;
    OIS::ParamList paramList;
    m_pRenderWnd->getCustomAttribute("WINDOW", &hWnd);
    
	paramList.insert(OIS::ParamList::value_type("WINDOW", Ogre::StringConverter::toString(hWnd)));
    
	m_pInputMgr = OIS::InputManager::createInputSystem(paramList);
    
#if !defined(OGRE_IS_IOS)
    m_pKeyboard = static_cast<OIS::Keyboard*>(m_pInputMgr->createInputObject(OIS::OISKeyboard, true));
	m_pMouse = static_cast<OIS::Mouse*>(m_pInputMgr->createInputObject(OIS::OISMouse, true));
	m_pJoyStick = static_cast<OIS::JoyStick*>(m_pInputMgr->createInputObject(OIS::OISJoyStick, true));
    
	m_pMouse->getMouseState().height = m_pRenderWnd->getHeight();
	m_pMouse->getMouseState().width	 = m_pRenderWnd->getWidth();
#else
	m_pMouse = static_cast<OIS::MultiTouch*>(m_pInputMgr->createInputObject(OIS::OISMultiTouch, true));
#endif
    
#if !defined(OGRE_IS_IOS)
	if(pKeyListener == 0)
		m_pKeyboard->setEventCallback(this);
	else
		m_pKeyboard->setEventCallback(pKeyListener);
	if(pJoyStickListener == 0)
		m_pJoyStick->setEventCallback(this);
	else
		m_pJoyStick->setEventCallback(pJoyStickListener);
#endif
    
	if(pMouseListener == 0)
		m_pMouse->setEventCallback(this);
	else
		m_pMouse->setEventCallback(pMouseListener);
    
	Ogre::String secName, typeName, archName;
	Ogre::ConfigFile cf;
    cf.load(m_ResourcePath + "resources.cfg");
    
	Ogre::ConfigFile::SectionIterator seci = cf.getSectionIterator();
    while (seci.hasMoreElements())
    {
        secName = seci.peekNextKey();
		Ogre::ConfigFile::SettingsMultiMap *settings = seci.getNext();
        Ogre::ConfigFile::SettingsMultiMap::iterator i;
        for (i = settings->begin(); i != settings->end(); ++i)
        {
            typeName = i->first;
            archName = i->second;
#if OGRE_PLATFORM == OGRE_PLATFORM_APPLE || defined(OGRE_IS_IOS)
            // OS X does not set the working directory relative to the app,
            // In order to make things portable on OS X we need to provide
            // the loading with it's own bundle path location
            if (!Ogre::StringUtil::startsWith(archName, "/", false)) // only adjust relative dirs
                archName = Ogre::String(m_ResourcePath + archName);
#endif
            Ogre::ResourceGroupManager::getSingleton().addResourceLocation(archName, typeName, secName);
        }
    }
	Ogre::TextureManager::getSingleton().setDefaultNumMipmaps(5);
	Ogre::ResourceGroupManager::getSingleton().initialiseAllResourceGroups();
    
	m_pTimer = OGRE_NEW Ogre::Timer();
	m_pTimer->reset();
	
	m_pTrayMgr = new OgreBites::SdkTrayManager("TrayMgr", m_pRenderWnd, m_pMouse, this);
    m_pTrayMgr->showFrameStats(OgreBites::TL_BOTTOMLEFT);
    m_pTrayMgr->showLogo(OgreBites::TL_BOTTOMRIGHT);
    m_pTrayMgr->hideCursor();
    
	m_pRenderWnd->setActive(true);
    
	return true;
}

OgreFramework::~OgreFramework()
{
    if(m_pInputMgr) OIS::InputManager::destroyInputSystem(m_pInputMgr);
    if(m_pTrayMgr)  delete m_pTrayMgr;
#ifdef OGRE_STATIC_LIB
    m_StaticPluginLoader.unload();
#endif
    if(m_pRoot)     delete m_pRoot;
}

bool OgreFramework::keyPressed(const OIS::KeyEvent &keyEventRef)
{
#if !defined(OGRE_IS_IOS)
	
	if(m_pKeyboard->isKeyDown(OIS::KC_ESCAPE))
	{
        m_bShutDownOgre = true;
        return true;
	}
    
	if(m_pKeyboard->isKeyDown(OIS::KC_SYSRQ))
	{
		m_pRenderWnd->writeContentsToTimestampedFile("BOF_Screenshot_", ".png");
		return true;
	}
    
	if(m_pKeyboard->isKeyDown(OIS::KC_M))
	{
		static int mode = 0;
		
		if(mode == 2)
		{
			m_pCamera->setPolygonMode(PM_SOLID);
			mode = 0;
		}
		else if(mode == 0)
		{
            m_pCamera->setPolygonMode(PM_WIREFRAME);
            mode = 1;
		}
		else if(mode == 1)
		{
			m_pCamera->setPolygonMode(PM_POINTS);
			mode = 2;
		}
	}
    
	if(m_pKeyboard->isKeyDown(OIS::KC_O))
	{
		if(m_pTrayMgr->isLogoVisible())
        {
            m_pTrayMgr->hideLogo();
            m_pTrayMgr->hideFrameStats();
        }
        else
        {
            m_pTrayMgr->showLogo(OgreBites::TL_BOTTOMRIGHT);
            m_pTrayMgr->showFrameStats(OgreBites::TL_BOTTOMLEFT);
        }
	}
    
#endif
	return true;
}

bool OgreFramework::keyReleased(const OIS::KeyEvent &keyEventRef)
{
	return true;
}

//|||||||||||||||||||||||||||||||||||||||||||||||

#if defined(OGRE_IS_IOS)
bool OgreFramework::touchMoved(const OIS::MultiTouchEvent &evt)
{
    OIS::MultiTouchState state = evt.state;
    int origTransX = 0, origTransY = 0;
#if !OGRE_NO_VIEWPORT_ORIENTATIONMODE
    switch(m_pCamera->getViewport()->getOrientationMode())
    {
        case Ogre::OR_LANDSCAPELEFT:
            origTransX = state.X.rel;
            origTransY = state.Y.rel;
            state.X.rel = -origTransY;
            state.Y.rel = origTransX;
            break;
            
        case Ogre::OR_LANDSCAPERIGHT:
            origTransX = state.X.rel;
            origTransY = state.Y.rel;
            state.X.rel = origTransY;
            state.Y.rel = origTransX;
            break;
            
            // Portrait doesn't need any change
        case Ogre::OR_PORTRAIT:
        default:
            break;
    }
#endif
	m_pCamera->yaw(Degree(state.X.rel * -0.1));
	m_pCamera->pitch(Degree(state.Y.rel * -0.1));
	
	return true;
}

//|||||||||||||||||||||||||||||||||||||||||||||||

bool OgreFramework::touchPressed(const OIS:: MultiTouchEvent &evt)
{
#pragma unused(evt)
	return true;
}

//|||||||||||||||||||||||||||||||||||||||||||||||

bool OgreFramework::touchReleased(const OIS:: MultiTouchEvent &evt)
{
#pragma unused(evt)
	return true;
}

bool OgreFramework::touchCancelled(const OIS:: MultiTouchEvent &evt)
{
#pragma unused(evt)
	return true;
}
#else
bool OgreFramework::mouseMoved(const OIS::MouseEvent &evt)
{
	m_pCamera->yaw(Degree(evt.state.X.rel * -0.1f));
	m_pCamera->pitch(Degree(evt.state.Y.rel * -0.1f));
	
	return true;
}

bool OgreFramework::mousePressed(const OIS::MouseEvent &evt, OIS::MouseButtonID id)
{
	return true;
}

bool OgreFramework::mouseReleased(const OIS::MouseEvent &evt, OIS::MouseButtonID id)
{
	return true;
}
#endif

void OgreFramework::updateOgre(double timeSinceLastFrame)
{
	m_MoveScale = m_MoveSpeed   * (float)timeSinceLastFrame;
	m_RotScale  = m_RotateSpeed * (float)timeSinceLastFrame;

#if OGRE_VERSION >= 0x10800
    m_pSceneMgr->setSkyBoxEnabled(true);
#endif

	m_TranslateVector = Vector3::ZERO;
    
	getInput();
	moveCamera();
    
	m_FrameEvent.timeSinceLastFrame = timeSinceLastFrame;
    m_pTrayMgr->frameRenderingQueued(m_FrameEvent);
}

void OgreFramework::moveCamera()
{
#if !defined(OGRE_IS_IOS)
	if(m_pKeyboard->isKeyDown(OIS::KC_LSHIFT)) 
		m_pCamera->moveRelative(m_TranslateVector);
	else
#endif
		m_pCamera->moveRelative(m_TranslateVector / 10);
}

void OgreFramework::getInput()
{
#if !defined(OGRE_IS_IOS)
	if(m_pKeyboard->isKeyDown(OIS::KC_A))
		m_TranslateVector.x = -m_MoveScale;
	
	if(m_pKeyboard->isKeyDown(OIS::KC_D))
		m_TranslateVector.x = m_MoveScale;
	
	if(m_pKeyboard->isKeyDown(OIS::KC_W))
		m_TranslateVector.z = -m_MoveScale;
	
	if(m_pKeyboard->isKeyDown(OIS::KC_S))
		m_TranslateVector.z = m_MoveScale;
#endif
}

#pragma mark -
#pragma mark Joystick

bool OgreFramework::axisMoved(const OIS::JoyStickEvent &arg, int axis)
{
	OIS::JoyStick* joystick = (OIS::JoyStick*)arg.device;
	std::string vendorId = joystick->vendor();
	std::cout << "Axis moved : "<<axis << " for joystick \"" << vendorId << "\"" << std::endl;
	std::cout << "New states :" << std::endl;
	for(int i=0 ; i<arg.state.mAxes.size() ; i++)
	{
		std::cout << " " << arg.state.mAxes[i].abs;
	}
	std::cout << std::endl;
}

bool OgreFramework::buttonPressed (const OIS::JoyStickEvent &arg, int button)
{
	OIS::JoyStick* joystick = (OIS::JoyStick*)arg.device;
	std::string vendorId = joystick->vendor();
	std::cout << "Button pressed : "<<button << " for joystick \"" << vendorId << "\"" << std::endl;
}

bool OgreFramework::buttonReleased (const OIS::JoyStickEvent &arg, int button)
{
	OIS::JoyStick* joystick = (OIS::JoyStick*)arg.device;
	std::string vendorId = joystick->vendor();
	std::cout << "Button released : "<<button << " for joystick \"" << vendorId << "\"" << std::endl;
}

bool OgreFramework::povMoved (const OIS::JoyStickEvent &arg, int povID)
{
	OIS::JoyStick* joystick = (OIS::JoyStick*)arg.device;
	std::string vendorId = joystick->vendor();
	std::cout << "Pov moved : "<<povID << " for joystick \"" << vendorId << "\" direction " << arg.state.mPOV[0].direction << std::endl;
}

bool OgreFramework::sliderMoved (const OIS::JoyStickEvent &arg, int sliderID)
{
	OIS::JoyStick* joystick = (OIS::JoyStick*)arg.device;
	std::string vendorId = joystick->vendor();
	std::cout << "Slider moved : "<<sliderID << " for joystick \"" << vendorId << "\" state " << arg.state.mSliders[0].abX << std::endl;
}

bool OgreFramework::vector3Moved (const OIS::JoyStickEvent &arg, int Vector3ID)
{
	OIS::JoyStick* joystick = (OIS::JoyStick*)arg.device;
	std::string vendorId = joystick->vendor();
	std::cout << "Vector3 Moved : "<<Vector3ID << " for joystick \"" << vendorId << "\"" << std::endl;
}
