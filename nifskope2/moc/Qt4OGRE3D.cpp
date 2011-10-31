/***** BEGIN LICENSE BLOCK *****

BSD License

Copyright (c) 2005-2011, NIF File Format Library and Tools
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the NIF File Format Library and Tools project may not be
   used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

***** END LICENCE BLOCK *****/

/*
*	WIP
*
*	"OGRE" 3D render system for "NifSkope":
*	http://www.ogre3d.org
*
*	help: http://www.ogre3d.org/forums/
*	TFM :): http://www.ogre3d.org/tikiwiki/
*
*	My thanks to all the helpful people at the forums and to the people who
*	created	"QtOgre": http://www.ogre3d.org/tikiwiki/tiki-index.php?page=QtOgre
*	My thanks to the people behind OGRE - you rock!
*/

#include "Qt4OGRE3D.h"
#include "nifskope.h"

#ifdef NIFSKOPE_X
#include <X11/Xlib.h>
#endif /* NIFSKOPE_X */

// "NifLib"
#include "Compiler.h"

// OGRE
#include <OgreSceneNode.h>
#include <OgreManualObject.h>
#include <OgreMaterial.h>
#include <OgreMaterialManager.h>
#include <OgreResourceGroupManager.h>

namespace NifSkope
{
	class NifLoaded: public ICommand // event
	{
		Qt4OGRE3D *obj;
		NifLoaded()
		{
		}
		// TODO: the others ...
	public:
		NifLoaded(Qt4OGRE3D *owner)
		{
			obj = owner;
		}
		virtual void Exec(IEvent *sender)// handler
		{
			obj->LoadNif (sender);
		}
	};

	Qt4OGRE3D::Qt4OGRE3D(void)
		: Qt43D (NULL)
		,ready(0), mRoot(0), mCam(0), mScn(0), mWin(0)
	{
	}

	Qt4OGRE3D::~Qt4OGRE3D(void)
	{
		//delete mRoot;

		// Release NS event handlers
		if (handleNifLoaded)
			delete handleNifLoaded;
	}

	bool
	Qt4OGRE3D::go()
	{
		// Attach to events from "NifSkope"
		handleNifLoaded = new NifLoaded (this);
		App->File.OnLoad.Subscribe (handleNifLoaded);

		// Setup OGRE
		mRoot = new Ogre::Root ("", "", "NifSkopeOgre3D.log");
#ifdef _DEBUG
		Ogre::String ext ("_d")
#else
		Ogre::String ext ("");
#endif
#ifdef NIFSKOPE_OGRE_GL
		mRoot->loadPlugin (Ogre::String ("RenderSystem_GL") + ext);
#endif /* NIFSKOPE_OGRE_GL */
#ifdef NIFSKOPE_OGRE_DX9
		mRoot->loadPlugin (Ogre::String ("RenderSystem_Direct3D9") + ext);
#endif /* NIFSKOPE_OGRE_DX9 */
		mRoot->loadPlugin (Ogre::String ("Plugin_OctreeSceneManager") + ext);
		//	Select OGRE render system
		//	TODO: choice dialog, settings load, etc.
		Ogre::RenderSystem* rs = NULL;
#ifdef NIFSKOPE_OGRE_GL
		rs = mRoot->getRenderSystemByName ("OpenGL Rendering Subsystem");
		if (!rs || rs->getName () != Ogre::String ("OpenGL Rendering Subsystem"))
			return false; // failed
#endif /* NIFSKOPE_OGRE_GL */
#ifdef NIFSKOPE_OGRE_DX9
		//rs = mRoot->getRenderSystemByName("Direct3D9 Rendering Subsystem");
		if (!rs || rs->getName () != Ogre::String ("Direct3D9 Rendering Subsystem"))
			return false; // failed
#endif /* NIFSKOPE_OGRE_DX9 */
		rs->setConfigOption ("Full Screen", "No");
		rs->setConfigOption ("VSync", "Yes");
		rs->setConfigOption ("Video Mode", "800 x 600 @ 32-bit");
		//	TODO: AA, AF, etc. - settings
		mRoot->setRenderSystem (rs);
		//	Setup Qt4 Widget and OGRE working environment*
		Ogre::NameValuePairList misc;
#ifdef NIFSKOPE_WIN
		misc["externalWindowHandle"] = Ogre::StringConverter::toString ((long)winId ());
#else
#ifdef NIFSKOPE_X
		Display* dpy = XOpenDisplay (getenv ("DISPLAY"));
    	int screen = DefaultScreen (dpy);
		QWidget *q_parent = dynamic_cast<QWidget *>(parent ());
		if (q_parent) {
			misc["parentWindowHandle"] = 
				Ogre::StringConverter::toString ((unsigned long)(dpy)) + ":" +
				Ogre::StringConverter::toString ((unsigned int)(screen)) + ":" +
				Ogre::StringConverter::toString ((unsigned long)q_parent->winId ());
		} else
			return false;
#else
#error No supported windowing system found
#endif /* NIFSKOPE_X */
#endif /* NIFSKOPE_WIN */
		misc["vsync"] = "true";// it ignores the above when "createRenderWindow"
		mWin = mRoot->initialise (false, "Nifskope 2");
		mWin = mRoot->createRenderWindow (
			"Nifskope 2", width (), height (), false, &misc);
    	mWin->setActive (true);
    	mWin->resize (width (), height ());
    	mWin->setVisible (true);
#ifdef NIFSKOPE_X
    	//	Get the ID of "OGRE" render window
    	WId window_id;
    	mWin->getCustomAttribute ("WINDOW", &window_id);
    	//	Take over the "OGRE" created window.
    	QWidget::create (window_id); // Qt specific way*
    	resizeEvent (NULL);
    	setAttribute (Qt::WA_PaintOnScreen);
    	setAttribute (Qt::WA_OpaquePaintEvent);
#endif /* NIFSKOPE_X */
		//	Setup the scene*
		mScn = mRoot->createSceneManager ("OctreeSceneManager", "DefaultSceneManager");
		mCam = mScn->createCamera ("camMain");
		mCam->setPosition (Ogre::Vector3 (10, 10, 100));
		mCam->lookAt (Ogre::Vector3 (0, 0, 0));// look at the center*
		mCam->setNearClipDistance (5);// near clipping plane
		mVp = mWin->addViewport (mCam);
		mVp->setBackgroundColour (Ogre::ColourValue (0, 0, 0));
		mCam->setAspectRatio (
			(Ogre::Real)mVp->getActualWidth ()/mVp->getActualHeight ());
		Ogre::TextureManager::getSingleton ().setDefaultNumMipmaps (5);
		Ogre::ResourceGroupManager::getSingleton ().initialiseAllResourceGroups ();
		mScn->setAmbientLight (Ogre::ColourValue (0.1, 0.1, 0.1));// slight global
		Ogre::Light* l = mScn->createLight ("lightMain");
		l->setPosition (100, 100, 100);
		QTimer *timer = new QTimer(this);
		timer->setSingleShot (false);
 		connect(timer, SIGNAL(timeout()), this, SLOT(Render()));
		ready = 1;
		timer->start (1000/25);

		/*
		*/
		// manual material example
		Ogre::MaterialPtr myManualObjectMaterial =
			Ogre::MaterialManager::getSingleton().create("manual1Material",
			Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME); 
		myManualObjectMaterial->setReceiveShadows(false); 
		myManualObjectMaterial->getTechnique(0)->setLightingEnabled(true); 
		myManualObjectMaterial->getTechnique(0)->getPass(0)->setDiffuse(0,0,1,0); 
		myManualObjectMaterial->getTechnique(0)->getPass(0)->setAmbient(0,0,1); 
		myManualObjectMaterial->getTechnique(0)->getPass(0)->setSelfIllumination(0,0,1); 
		//myManualObjectMaterial->dispose();  // no such method

		// show axes so one can orient what is where
		Ogre::SceneNode *axesSn =
			mScn->getRootSceneNode ()->createChildSceneNode ("Axes");
		Ogre::ManualObject* sm = mScn->createManualObject ("axesMobj");
		// colour() works with no lighting material only
		// a wild guess - it uses color material when no lighting
		sm->begin ("BaseWhiteNoLighting", Ogre::RenderOperation::OT_LINE_LIST);
		float al = 100;
		sm->position (0, 0, 0);	sm->colour (0, 1, 0);
		sm->position (al, 0, 0); sm->colour (0, 1, 0);// X axis - green
		sm->position (0, 0, 0);	sm->colour (0, 0, 1);
		sm->position (0, al, 0); sm->colour (0, 0, 1);// Y axis - blue
		sm->position (0, 0, 0);	sm->colour (1, 0, 0);
		sm->position (0, 0, al); sm->colour (1, 0, 0);// Z axis - red
		sm->end ();
		axesSn->attachObject (sm);

		/*
		OGRE main loop when no windowing system is available - eventually
		TODO: roadmap
		mRoot->startRendering();
		while (true) {
			Ogre::WindowEventUtilities::messagePump ();
			mRoot->renderOneFrame ();
			if (mWin->isClosed ())
				return false;
		}*/
		return ready;
	}// go

	/*
	*	QWidget event handler.
	*/
	OVERRIDE void
	Qt4OGRE3D::mouseMoveEvent(QMouseEvent *event)
	{
		// Interactive Event:Mouse:Moving:Button down

		NSINFO("IE:M:M: (" << event->x() << ", " << event->y() << ")")
		// Observed behaviour in X && Qt 4.6.1:
		// - occures not - needs a mouse button down

		// create motion vector
		//int dx = event->x () - lastPos.x ();
		//int dy = event->y () - lastPos.y ();

		if (event->buttons () & Qt::LeftButton) {
			NSINFO("IE:M:M:LB (" << event->x() << ", " << event->y() << ")")
			// Observed behaviour in X && Qt 4.6.1:
			// - repeats
			// - low frequency - filtered to "change" it seems

			//mouseRot += Vector3( dy * .5, 0, dx * .5 );
		} else
		if (event->buttons () & Qt::MidButton) {
			NSINFO("IE:M:M:MB (" << event->x() << ", " << event->y() << ")")
			//float d = axis / ( qMax( width(), height() ) + 1 );
			//mouseMov += Vector3( dx * d, - dy * d, 0 );
		} else
		if (event->buttons () & Qt::RightButton) {
			NSINFO("IE:M:M:RB (" << event->x() << ", " << event->y() << ")")
		//setDistance( Dist - (dx + dy) * ( axis / ( qMax( width(), height() ) + 1 ) ) );
		}
		//lastPos = event->pos();
		//QPoint lastPos;
		//QPoint pressPos;
	}

	/*
	*	QWidget event handler. Occures after the widget is resized.
	*/
	OVERRIDE void
	Qt4OGRE3D::resizeEvent(QResizeEvent *p)// overrides
	{
		if (!ready)
			return;
		mWin->resize (p->size ().width (), p->size ().height ());
		mWin->windowMovedOrResized ();// TODO: not sure why should I call that
	}

	/*
	*	QTimer event handler. Qt "slot".
	*/
	void
	Qt4OGRE3D::Render()
	{
		Ogre::WindowEventUtilities::messagePump ();
		mRoot->renderOneFrame ();

		// update (); causes 100% cpu load for no apparent reason
		// TODO: figure out why
	}

	/*
	*	QWidget event handler.
	*	Occures when the widget needs to update its "outlook".
	*/
	OVERRIDE void
	Qt4OGRE3D::paintEvent(QPaintEvent *p)
	{
		if (!ready)
			return;
		Ogre::WindowEventUtilities::messagePump ();
		mRoot->renderOneFrame ();
	}

	/*
	*	NifSkope::FileIO event handler. Occures after the parser
	*	has loaded a file.
	*/
	void
	Qt4OGRE3D::LoadNif(IEvent *sender)
	{
		// TODO: to settings
		std::string texbase ("/mnt/workspace/rain/c/nif/test/nfiskope_bin/data/");
		Ogre::ResourceGroupManager::getSingleton ().addResourceLocation (
			texbase, "FileSystem");

		NifLib::Compiler *nif = App->File.NifFile;
		std::string tex;
		std::string matname;
		for (int i = 0; i < nif->FCount (); i++) {
			NifLib::Field *f = (*nif)[i];
			//NiTriStripsData
			NifLib::Tag *ft = f->BlockTag;
			if (!ft)
				continue;// field has no block type - its ok
			NifLib::Attr *tname = ft->AttrById (ANAME);
			if (!tname)
				continue;// a tag has no "name" - error
			int block = f->BlockIndex;
			if (tname->Value.Equals ("NiSourceTexture", 15)) {
				ft = f->Tag;
				tname = ft->AttrById (ANAME);
				if (tname->Value.Equals ("Value", 5)) {
					NSINFO("found NiSourceTexture.Value at #" << f->BlockIndex)
					//NSINFO(std::string (f->Value.buf, f->Value.len))
					std::stringstream tmp;
					tmp << std::string (f->Value.buf, f->Value.len);
					tex = tmp.str ();
					int pathlen = tex. length ();
					char *path = new char[pathlen];
					memcpy (path, tex.c_str (), pathlen);
					for (int i = 0; i < pathlen; i++)
						if (path[i] == '\\')
							path[i] = '/';
					tex = std::string (path, pathlen);
//#include <OgreTextureManager.h>
//	Ogre::TextureManager &tm = Ogre::TextureManager::getSingleton();
//#include <OgreTexture.h>
	//Ogre::TexturePtr mSkyTexture = Ogre::TexturePtr(tm.getByName(mSkyTextureFileName));
					NSINFO(tex)
					//std::stringstream matname;
					std::stringstream matnamestream;
					matnamestream << "Tex " << block;
					matname = matnamestream.str ();
					NSINFO("mat set to: \"" << matname << "\"")
					Ogre::MaterialPtr mat =
						Ogre::MaterialManager::getSingleton ().create(matname,
						Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
					Ogre::TextureUnitState* tuisTexture =
						mat->getTechnique (0)->getPass (0)->createTextureUnitState (
						tex);
				}
			}
			else if (tname->Value.Equals ("NiTriStripsData", 15)) {
				int vnum = 0, sl = 0;
				NIFfloat *v = NULL, *n = NULL, *uv = NULL;
				NIFushort *s = NULL;
				NIFushort *si = NULL;
			while (block == f->BlockIndex && ++i < nif->FCount ()) {
				//NSINFO ("found NiTriStripsData at #" << f->BlockIndex)
				ft = f->Tag;
				tname = ft->AttrById (ANAME);
				// need to find a few things:
				// +NiGeometryData
				//  "Vertices" type="Vector3" arr1="Num Vertices" cond="Has Vertices"
				//  "Normals" type="Vector3" arr1="Num Vertices" cond="Has Normals"
				//  "UV Sets" type="TexCoord" arr1="Num UV Sets & 63" arr2="Num Vertices"
				// +NiTriStripsData
				//  "Num Strips" 155
				//  "Strip Lengths" type="ushort" arr1="Num Strips"
				//  "Points" type="ushort" arr1="Num Strips" arr2="Strip Lengths"
				if (tname->Value.Equals ("Vertices", 8)) {
					NSINFO ("found NiTriStripsData.Vertices at #" << f->BlockIndex)
					vnum = f->Value.len / 4;
					v = (NIFfloat *)&f->Value.buf[0];
				}
				else if (tname->Value.Equals ("Normals", 7)) {
					NSINFO ("found NiTriStripsData.Normals at #" << f->BlockIndex)
					n = (NIFfloat *)&f->Value.buf[0];
				}
				else if (tname->Value.Equals ("UV Sets", 7)) {
					NSINFO ("found NiTriStripsData.UV Sets at #" << f->BlockIndex)
					uv = (NIFfloat *)&f->Value.buf[0];
				}
				//if (tname->Value.Equals ("Num Strips", 10))
				//	NSINFO ("found NiTriStripsData.Num Strips at #" << f->BlockIndex)
				else if (tname->Value.Equals ("Strip Lengths", 13)) {
					NSINFO ("found NiTriStripsData.Strip Lengths at #" << f->BlockIndex)
					sl = f->Value.len / 2;
					s = (NIFushort *)&f->Value.buf[0];
				}
				else if (tname->Value.Equals ("Points", 6)) {
					NSINFO ("found NiTriStripsData.Points at #" << f->BlockIndex)
					si = (NIFushort *)&f->Value.buf[0];
					int base = 0;
					std::stringstream objn;
					objn << "Mesh " << block;
					std::stringstream nn;
					nn << "Node " << block;
					Ogre::SceneNode *mySceneNode =
						mScn->getRootSceneNode ()->createChildSceneNode (nn.str ());
					mySceneNode->setScale (0.5, 0.5, 0.5);
					Ogre::ManualObject* sm = mScn->createManualObject (objn.str ());
					NSINFO("Using mat: \"" << matname << "\"")
					for (int m = 0; m < sl; m++) {
						// submesh
						/*ManualObject* sm = mScn->createManualObject ("manual");
						sm->begin ("BaseWhiteNoLighting",
							RenderOperation::OT_TRIANGLE_STRIP);*/
						//INFO ("m[" << m << "]=" << s[m])
						sm->begin (matname,// "BaseWhite",
							Ogre::RenderOperation::OT_TRIANGLE_STRIP);
						for (int vidx = base; vidx < base + s[m]; vidx++) {
							// faces
							int idx = si[vidx];
							NIFfloat *vertex = &v[3*idx];
							NIFfloat *normal = &n[3*idx];
							NIFfloat *tcoord = &uv[2*idx];
							/*INFO("#" << vidx - base << "(("
							<< vertex[0] << ", " << vertex[1] << ", " << vertex[2]
							<< ") ("
							<< normal[0] << ", " << normal[1] << ", " << normal[2]
							<< ") ("
							<< tcoord[0] << ", " << tcoord[1] << "))")*/
							sm->position (vertex[0], vertex[1], vertex[2]);
							sm->normal (normal[0], normal[1], normal[2]);
							sm->textureCoord (tcoord[0], tcoord[1]);
							//sm->index (vidx - base);
						}
						sm->end ();
						base += s[m];
					}//
					mySceneNode->attachObject (sm);
				}
				f = (*nif)[i];
			}
			}// if "NiTriStripsData"
		}// for each field
	}
}
