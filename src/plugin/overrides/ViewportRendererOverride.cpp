#include "ViewportRendererOverride.hpp"
#include "QuadRendererOverride.hpp"
#include "UIOverride.hpp"
#include "plugin/FrameGraphManager.hpp"
#include "miscellaneous/Settings.hpp"
#include "miscellaneous/Functions.hpp"

#include <memory>
#include <algorithm>
#include "wisp.hpp"
#include "frame_graph/frame_graph.hpp"
#include "../demo/resources.hpp"

#include "../demo/engine_interface.hpp"
#include "../demo/scene_viknell.hpp"
#include "../demo/scene_cubes.hpp"

#include "scene_graph/camera_node.hpp"
#include "scene_graph/scene_graph.hpp"

#include <maya/MString.h>
#include <maya/M3dView.h>
#include <maya/MMatrix.h>
#include <maya/MDagPath.h>
#include <maya/MFnCamera.h>
#include <maya/MImage.h>
#include <maya/M3dView.h>
#include <maya\MGlobal.h>
#include <maya\MQuaternion.h>
#include <maya\MEulerRotation.h>
#include <maya\MFnTransform.h>

//std include
#include <algorithm>
#include <memory>
#include <sstream>
#include <maya/MGlobal.h>

auto window = std::make_unique<wr::Window>( GetModuleHandleA( nullptr ), "D3D12 Test App", 1280, 720 );

#define SCENE viknell_scene

namespace wmr
{
	static void EnsurePanelDisplayShading( const MString& destination )
	{
		M3dView view;

		if( destination.length() &&
			M3dView::getM3dViewFromModelPanel( destination, view ) == MStatus::kSuccess )
		{
			if( view.displayStyle() != M3dView::kGouraudShaded )
			{
				view.setDisplayStyle( M3dView::kGouraudShaded );
			}
		}
	}

	ViewportRenderer::ViewportRenderer(const MString& name)
		: MRenderOverride(name)
		, m_ui_name(wisp::settings::PRODUCT_NAME)
		, m_current_render_operation(-1)
	{
		ConfigureRenderOperations();
		SetDefaultTextureState();
	}

	ViewportRenderer::~ViewportRenderer()
	{
		ReleaseTextureResources();
	}

	void ViewportRenderer::Initialize()
	{
		CreateRenderOperations();
		InitializeWispRenderer();
	}

	void ViewportRenderer::Destroy()
	{
		m_render_system->WaitForAllPreviousWork();
		m_model_loader.reset();
		m_render_system.reset();
		m_frame_graph_manager.reset();
	}

	void ViewportRenderer::ConfigureRenderOperations()
	{
		m_render_operation_names[0] = "wisp_SceneBlit";
		m_render_operation_names[1] = "wisp_UIDraw";
		m_render_operation_names[2] = "wisp_Present";
	}

	void ViewportRenderer::SetDefaultTextureState()
	{
		m_color_texture.texture = nullptr;
		m_color_texture_desc.setToDefault2DTexture();

		m_depth_texture.texture = nullptr;
		m_depth_texture_desc.setToDefault2DTexture();
	}

	void ViewportRenderer::ReleaseTextureResources() const
	{
		const auto maya_renderer = MHWRender::MRenderer::theRenderer();

		if (!maya_renderer)
			return;

		const auto maya_texture_manager = maya_renderer->getTextureManager();

		if (!maya_texture_manager)
			return;

		if (m_color_texture.texture)
			maya_texture_manager->releaseTexture(m_color_texture.texture);

		if (m_depth_texture.texture)
			maya_texture_manager->releaseTexture(m_depth_texture.texture);
	}

	void ViewportRenderer::CreateRenderOperations()
	{
		if (!m_render_operations[0])
		{
			m_render_operations[0] = std::make_unique<WispScreenBlitter>(m_render_operation_names[0]);
			m_render_operations[1] = std::make_unique<WispUIRenderer>(m_render_operation_names[1]);
			m_render_operations[2] = std::make_unique<MHWRender::MHUDRender>();
			m_render_operations[3] = std::make_unique<MHWRender::MPresentTarget>(m_render_operation_names[2]);
		}
	}

	void ViewportRenderer::InitializeWispRenderer()
	{
		util::log_callback::impl = [ & ]( std::string const & str )
		{
			engine::debug_console.AddLog( str.c_str() );
		};

		m_render_system = std::make_unique<wr::D3D12RenderSystem>();

		m_model_loader = std::make_unique<wr::AssimpModelLoader>();

		m_render_system->Init( window.get() );

		resources::CreateResources( m_render_system.get() );

		m_scenegraph = std::make_shared<wr::SceneGraph>( m_render_system.get() );

		m_viewport_camera = m_scenegraph->CreateChild<wr::CameraNode>( nullptr, 90.f, ( float )window->GetWidth() / ( float )window->GetHeight() );
		m_viewport_camera->SetPosition( { 0, 0, -1 } );

		SCENE::CreateScene( m_scenegraph.get(), window.get() );

		m_render_system->InitSceneGraph( *m_scenegraph.get() );

		// Create the frame graphs and start out using the deferred rendering pipeline
		m_frame_graph_manager = std::make_unique<FrameGraphManager>();
		m_frame_graph_manager->Create(*m_render_system, RendererFrameGraphType::DEFERRED);
	}

	MHWRender::DrawAPI ViewportRenderer::supportedDrawAPIs() const
	{
		return (MHWRender::kOpenGL | MHWRender::kOpenGLCoreProfile | MHWRender::kDirectX11);
	}

	MHWRender::MRenderOperation* ViewportRenderer::renderOperation()
	{
		if (m_current_render_operation >= 0 && m_current_render_operation < 4)
		{
			if (m_render_operations[m_current_render_operation])
			{
				return m_render_operations[m_current_render_operation].get();
			}
		}

		return nullptr;
	}

	void ViewportRenderer::SynchronizeWispWithMayaViewportCamera()
	{
		M3dView maya_view;
		MStatus status = M3dView::getM3dViewFromModelPanel( wisp::settings::VIEWPORT_PANEL_NAME, maya_view );

		if( status != MStatus::kSuccess )
		{
			// Failure means no camera data for this frame, early-out!
			return;
		}

		// Model view matrix
		MMatrix mv_matrix;
		maya_view.modelViewMatrix( mv_matrix );

		MDagPath camera_dag_path;
		maya_view.getCamera( camera_dag_path );
		MFnTransform camera_transform( camera_dag_path.transform() );
		// Additional functionality
		
		MEulerRotation view_rotation;// = MEulerRotation::decompose( mv_matrix.inverse(), MEulerRotation::RotationOrder::kXYZ );
		camera_transform.getRotation( view_rotation );


		std::stringstream strs;
		strs << "X: " << view_rotation.x << " Y: " << view_rotation.y << " Z: " << view_rotation.z << std::endl;
		MGlobal::displayInfo(std::string(strs.str()).c_str());
		
		m_viewport_camera->SetRotation( {  ( float )view_rotation.x,( float )view_rotation.y, ( float )view_rotation.z } );

		
		MMatrix cameraPos = camera_dag_path.inclusiveMatrix();
		MVector eye = MVector( static_cast<float>( cameraPos(3,0)), static_cast< float >( cameraPos( 3, 1 ) ), static_cast< float >( cameraPos( 3, 2 ) ) );
		m_viewport_camera->SetPosition( { ( float )-eye.x, ( float )-eye.y, ( float )-eye.z } );

		
		MFnCamera camera_functions( camera_dag_path );
		m_viewport_camera->m_frustum_far = camera_functions.farClippingPlane();
		m_viewport_camera->m_frustum_near = camera_functions.nearClippingPlane();
		
		m_viewport_camera->SetFov( AI_RAD_TO_DEG( camera_functions.horizontalFieldOfView()) );
	}

	MStatus ViewportRenderer::setup(const MString& destination)
	{
		SynchronizeWispWithMayaViewportCamera();

		auto textures = m_render_system->Render( m_scenegraph, *m_frame_graph_manager->Get());

		auto* const maya_renderer = MHWRender::MRenderer::theRenderer();

		if (!maya_renderer)
		{
			assert( false ); 
			return MStatus::kFailure;
		}

		auto* const maya_texture_manager = maya_renderer->getTextureManager();

		if (!maya_texture_manager)
		{
			assert( false );
			return MStatus::kFailure;
		}

		if ( AreAllRenderOperationsSetCorrectly() )
		{
			assert( false );
			return MStatus::kFailure;
		}

		// Update textures used for scene blit
		if (!UpdateTextures(maya_renderer, maya_texture_manager, textures))
		{
			assert( false );
			return MStatus::kFailure;
		}

		// Force the panel display style to smooth shaded if it is not already
		// this ensures that viewport selection behavior works as if shaded
		EnsurePanelDisplayShading(destination);

		return MStatus::kSuccess;
	}

	bool ViewportRenderer::AreAllRenderOperationsSetCorrectly() const
	{
		return (!m_render_operations[0] ||
				!m_render_operations[1] ||
				!m_render_operations[2] ||
				!m_render_operations[3]);
	}

	// TODO: REFACTOR
	void ViewportRenderer::UpdateTextureData(MHWRender::MTextureAssignment& texture_to_update, WispBufferType type, const wr::CPUTexture& cpu_texture, MHWRender::MTextureManager* texture_manager)
	{
		unsigned int buffer_width = cpu_texture.m_buffer_width;
		unsigned int buffer_height = cpu_texture.m_buffer_height;
		unsigned int buffer_bytes_per_pixel = cpu_texture.m_bytes_per_pixel;

		// Allocate memory to store the Wisp texture data
		auto* wisp_data = new unsigned char[buffer_width * buffer_height * buffer_bytes_per_pixel];
		memcpy(wisp_data, cpu_texture.m_data, sizeof(unsigned char) * buffer_width * buffer_height * buffer_bytes_per_pixel);

		if (!texture_to_update.texture)
		{
			switch (type)
			{
			case WispBufferType::COLOR:
				m_color_texture_desc.fWidth = buffer_width;
				m_color_texture_desc.fHeight = buffer_height;
				m_color_texture_desc.fDepth = 1;
				m_color_texture_desc.fBytesPerRow = buffer_bytes_per_pixel * buffer_width;
				m_color_texture_desc.fBytesPerSlice = m_color_texture_desc.fBytesPerRow * buffer_height;
				m_color_texture_desc.fTextureType = MHWRender::kImage2D;

				// Acquire a new texture
				m_color_texture.texture = texture_manager->acquireTexture("", m_color_texture_desc, wisp_data);

				if (m_color_texture.texture)
					m_color_texture.texture->textureDescription(m_color_texture_desc);
				break;

			case WispBufferType::DEPTH:
				m_depth_texture_desc.fWidth = buffer_width;
				m_depth_texture_desc.fHeight = buffer_height;
				m_depth_texture_desc.fDepth = 1;
				m_depth_texture_desc.fBytesPerRow = buffer_bytes_per_pixel * buffer_width;
				m_depth_texture_desc.fBytesPerSlice = m_color_texture_desc.fBytesPerRow * buffer_height;
				m_depth_texture_desc.fTextureType = MHWRender::kDepthTexture;

				// Acquire a new texture
				m_depth_texture.texture = texture_manager->acquireDepthTexture("", reinterpret_cast<float*>(wisp_data), buffer_width, buffer_height, false, nullptr);

				if (m_depth_texture.texture)
					m_depth_texture.texture->textureDescription(m_depth_texture_desc);
				break;

			default:
				break;
			}
		}
		else
		{
			switch (type)
			{
			case wmr::WispBufferType::COLOR:
				m_color_texture.texture->update(wisp_data, false);
				break;
			
			case wmr::WispBufferType::DEPTH:
				m_depth_texture.texture->update(wisp_data, false);
				break;

			default:
				break;
			}
		}

		delete[] wisp_data;
	}

	MStatus ViewportRenderer::cleanup()
	{
		m_current_render_operation = -1;
		return MStatus::kSuccess;
	}

	MString ViewportRenderer::uiName() const
	{
		return m_ui_name;
	}

	bool ViewportRenderer::startOperationIterator()
	{
		m_current_render_operation = 0;
		return true;
	}

	bool ViewportRenderer::nextRenderOperation()
	{
		++m_current_render_operation;

		if (m_current_render_operation < 4)
		{
			return true;
		}

		return false;
	}

	bool ViewportRenderer::UpdateTextures(MHWRender::MRenderer* maya_renderer, MHWRender::MTextureManager* texture_manager, const wr::CPUTextures& cpu_textures)
	{
		if (!maya_renderer || !texture_manager)
			return false;

		// Early exit, no texture data from Wisp available just yet
		if (cpu_textures.pixel_data == std::nullopt ||
			cpu_textures.depth_data == std::nullopt)
			return true;

		// Get current output size.
		unsigned int target_width = 0;
		unsigned int target_height = 0;
		maya_renderer->outputTargetSize(target_width, target_height);

		bool aquire_new_texture = false;
		bool force_reload = true;

		// If a resize occurred, or we haven't allocated any texture yet,
		// then create new textures which match the output size. 
		// Release any existing textures.
		//
		if (force_reload || !m_color_texture.texture ||
			(m_color_texture_desc.fWidth != target_width || m_color_texture_desc.fHeight != target_height) ||
			(m_depth_texture_desc.fWidth != target_width || m_depth_texture_desc.fHeight != target_height))
		{
			if (m_color_texture.texture)
			{
				texture_manager->releaseTexture(m_color_texture.texture);
				m_color_texture.texture = nullptr;
			}

			if (m_depth_texture.texture)
			{
				texture_manager->releaseTexture(m_depth_texture.texture);
				m_depth_texture.texture = nullptr;
			}
			
			aquire_new_texture = true;
			force_reload = false;
		}

		UpdateTextureData(m_color_texture, WispBufferType::COLOR, cpu_textures.pixel_data.value(), texture_manager);
		UpdateTextureData(m_depth_texture, WispBufferType::DEPTH, cpu_textures.depth_data.value(), texture_manager);

		// Update the textures used for the blit operation.
		if (aquire_new_texture)
		{
			auto* custom_blit = (WispScreenBlitter*)m_render_operations[0].get();
			custom_blit->SetColorTexture(m_color_texture);
			custom_blit->SetDepthTexture(m_depth_texture);
		}

		return (m_color_texture.texture && m_depth_texture.texture);
	}
}