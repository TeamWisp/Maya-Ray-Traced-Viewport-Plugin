// Wisp plug-in
#include "miscellaneous/functions.hpp"
#include "miscellaneous/settings.hpp"
#include "plugin/viewport_renderer_override.hpp"
#include "plugin/viewport_renderer_override.hpp"
#include "plugin/callback_manager.hpp"

// Maya API
#include <maya/MFnPlugin.h>
#include <maya/MCommandResult.h>
#include <maya/MGlobal.h>

// C++ standard
#include <memory>
#include <direct.h>


wmr::ViewportRendererOverride* viewport_renderer_override;


class SetWorkDir{							
public:
	SetWorkDir()							
	{										
		_chdir( getenv( "WISP_MAYA" ) );	
	}										
};

static SetWorkDir set_work_dir;

// Global plug-in instance
bool IsSceneDirty()
{
	MStatus status = MStatus::kFailure;

	try
	{
		// Is the scene currently dirty?
		MCommandResult scene_dirty_result( &status );
		wmr::func::ThrowIfFailedMaya( status );

		// Workaround for checking if the scene is, in fact, dirty
		status = MGlobal::executeCommand( "file -query -modified", scene_dirty_result );
		wmr::func::ThrowIfFailedMaya( status );

		int command_result = -1;
		status = scene_dirty_result.getResult( command_result );
		wmr::func::ThrowIfFailedMaya( status );

		return ( command_result != 0 );
	}
	catch( std::exception& )
	{
		return true;
	}
}

void ActOnCurrentDirtyState( const bool& state )
{
	// The scene is dirty, no need to set the flag
	if( !state )
	{
		MGlobal::executeCommand( "file -modified 0" );
	}
}

//! Plug-in entry point
/*! Initializes the application. A plug-in object is created and stored. This object will hold the information Maya
 *  needs to make it all work. Once the plug-in object exists, the instance of the plug-in will be initialized, upon
 *  which lower-level systems will start working.
 *
 *  /param object Inherited function from Maya, see Autodesk documentation.
 *  /return Returns MStatus::kSucccess if everything went all right. */
MStatus initializePlugin(MObject object)
{
	// Register the plug-in to Maya, using the name and version data from the settings header file
	MFnPlugin plugin(object, wmr::settings::COMPANY_NAME, wmr::settings::PRODUCT_VERSION, "Any");

	// Workaround for avoiding dirtying the scene when registering overrides
	const auto is_scene_dirty = IsSceneDirty();

	// Initialize the renderer override
	viewport_renderer_override = new wmr::ViewportRendererOverride( wmr::settings::VIEWPORT_OVERRIDE_NAME );


	// If the scene was previously unmodified, return it to that state to avoid dirtying
	ActOnCurrentDirtyState( is_scene_dirty );

	// If the program did not crash before this point, it means the plug-in was initialized correctly
	return MStatus::kSuccess;
}

//! Plug-in clean-up
/*  As soon as Maya tries to unload the plug-in, this function is called. The plug-in object is referenced and its
 *  destruction (called: "uninitialize") function is called. This will ensure a proper shut-down of all internal system.
 * 
 *  /param object Inherited function from Maya, see Autodesk documentation.
 *  /return Returns MStatus::kSucccess if everything went all right. */
MStatus uninitializePlugin(MObject object)
{
	MFnPlugin plugin(object);


	// Workaround for avoiding dirtying the scene when registering overrides
	const auto is_scene_dirty = IsSceneDirty();
	// Clean-up any used resources
	delete viewport_renderer_override;
	wmr::CallbackManager::Destroy();

	// If the scene was previously unmodified, return it to that state to avoid dirtying
	ActOnCurrentDirtyState( is_scene_dirty );

	// If the program did not crash before this point, the plug-in was uninitialized correctly
	return MStatus::kSuccess;
}
