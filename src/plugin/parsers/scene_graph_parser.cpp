#include "scene_graph_parser.hpp"

#include "plugin/callback_manager.hpp"
#include "plugin/renderer/renderer.hpp"
#include "miscellaneous/settings.hpp"
#include "plugin/viewport_renderer_override.hpp"
#include "plugin/parsers/model_parser.hpp"
#include "plugin/parsers/material_parser.hpp"
#include "plugin/parsers/camera_parser.hpp"

#include "model_pool.hpp"
#include "vertex.hpp"
#include "renderer.hpp"
#include "scene_graph/scene_graph.hpp"
#include "scene_graph/mesh_node.hpp"
#include "d3d12/d3d12_renderer.hpp"
#include "d3d12/d3d12_model_pool.hpp" 


#include <maya/MDagPath.h>
#include <maya/MEulerRotation.h>
#include <maya/MFloatArray.h>
#include <maya/MFloatVectorArray.h>
#include <maya/MFnCamera.h>
#include <maya/MFnMesh.h>
#include <maya/MFnTransform.h>
#include <maya/MGlobal.h>
#include <maya/MItDag.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MPointArray.h>
#include <maya/MQuaternion.h>
#include <maya/M3dView.h>
#include <maya/MStatus.h>
#include <maya/MFnDagNode.h>
#include <maya/MNodeMessage.h>
#include <maya/MPlug.h>
#include <maya/MViewport2Renderer.h>
#include <maya/MUuid.h>
#include <maya/MDGMessage.h>

#include <sstream>


void MeshAddedCallback( MObject &node, void *client_data )
{
	assert( node.apiType() == MFn::Type::kMesh );
	wmr::ScenegraphParser* scenegraph_parser = reinterpret_cast< wmr::ScenegraphParser* >( client_data );

	// Create an attribute changed callback to use in order to wait for the mesh to be ready
	scenegraph_parser->GetModelParser().SubscribeObject( node );
}

void MeshRemovedCallback( MObject& node, void* client_data )
{
	assert( node.apiType() == MFn::Type::kMesh );
	wmr::ScenegraphParser* scenegraph_parser = reinterpret_cast< wmr::ScenegraphParser* >( client_data );

	// Create an attribute changed callback to use in order to wait for the mesh to be ready
	scenegraph_parser->GetModelParser().UnSubscribeObject( node );
}

void MaterialMeshAddedCallback(MObject& node, void* client_data)
{
	assert( node.apiType() == MFn::Type::kMesh );

	// Get the DAG node
	MStatus status;
	MFnDagNode dag_node(node, &status);
	if (status == MStatus::kSuccess)
	{
		auto* material_parser = reinterpret_cast<wmr::MaterialParser*>(client_data);
		MFnMesh mesh(node);
		material_parser->OnMeshAdded(mesh);
	}
	else
	{
		MGlobal::displayInfo(status.errorString());
	}
}

void ConnectionAddedCallback(MPlug& src_plug, MPlug& dest_plug, bool made, void* client_data)
{
	auto* material_parser = reinterpret_cast<wmr::MaterialParser*>(client_data);

	// Get plug types
	auto srcType = src_plug.node().apiType();
	auto destType = dest_plug.node().apiType();

	if (destType == MFn::kShadingEngine)
	{
		// Get destination object from destination plug
		MObject dest_object = dest_plug.node();
		// Bind the mesh to the shading engine if the source plug is a mesh
		if (srcType == MFn::kMesh) 
		{
			MObject src_object(src_plug.node());
			// Check if connection is made
			if (made)
			{
				material_parser->ConnectMeshToShadingEngine(src_object, dest_object);
			}
			else
			{
				material_parser->DisconnectMeshFromShadingEngine(src_object, dest_object);
			}
		}
		else
		{
			// Get shader type of source plug
			auto shaderType = material_parser->GetShaderType(src_plug.node());
			// The type is UNSUPPORTED if we don't support it or if it's not a surface shader
			if (shaderType != wmr::detail::SurfaceShaderType::UNSUPPORTED)
			{
				if (made)
				{
					material_parser->ConnectShaderToShadingEngine(src_plug, dest_object);
				}
				else
				{
					material_parser->DisconnectShaderFromShadingEngine(src_plug, dest_object);
				}
			}
		}
	}
}

wmr::ScenegraphParser::ScenegraphParser( ) :
	m_render_system( dynamic_cast< const ViewportRendererOverride* >(
		MHWRender::MRenderer::theRenderer()->findRenderOverride( settings::VIEWPORT_OVERRIDE_NAME )
		)->GetRenderer() )
{
	m_camera_parser = std::make_unique<CameraParser>();
	m_model_parser = std::make_unique<ModelParser>();
	m_material_parser = std::make_unique<MaterialParser>();
}

wmr::ScenegraphParser::~ScenegraphParser()
{
}

void wmr::ScenegraphParser::Initialize()
{
	m_camera_parser->Initialize();

	MStatus status;
	// Mesh added
	MCallbackId addedId = MDGMessage::addNodeAddedCallback(
		MeshAddedCallback,
		"mesh",
		this,
		&status
	);
	AddCallbackValidation(status, addedId);

	// Mesh added (material)
	addedId = MDGMessage::addNodeAddedCallback(
		MaterialMeshAddedCallback,
		"mesh",
		m_material_parser.get(),
		&status
	);
	AddCallbackValidation(status, addedId);

	// Mesh removed 
	addedId = MDGMessage::addNodeRemovedCallback(
		MeshRemovedCallback,
		"mesh",
		this,
		&status
	);
	AddCallbackValidation(status, addedId);

	// Connection added (material)
	addedId = MDGMessage::addConnectionCallback(
		ConnectionAddedCallback,
		m_material_parser.get(),
		&status
	);
	AddCallbackValidation(status, addedId);

	// Initial parsing
	MStatus load_status = MS::kSuccess;
	MItDag itt( MItDag::kDepthFirst, MFn::kMesh, &load_status );
	if( load_status != MS::kSuccess )
	{
		MGlobal::displayError( "false to get iterator: " + load_status );
	}

	while( !itt.isDone() )
	{
		MFnMesh mesh( itt.currentItem() );
		if( !mesh.isIntermediateObject() )
		{
			m_model_parser->MeshAdded(mesh);
			m_material_parser->OnMeshAdded(mesh);
		}
		itt.next();
	}
}

void wmr::ScenegraphParser::AddCallbackValidation(MStatus status, MCallbackId id)
{
	if (status == MS::kSuccess)
	{
		CallbackManager::GetInstance().RegisterCallback(id);
	}
	else
	{
		assert(false);
	}
}

wmr::ModelParser & wmr::ScenegraphParser::GetModelParser() const noexcept
{
	return *m_model_parser;
}

wmr::MaterialParser & wmr::ScenegraphParser::GetMaterialParser() const noexcept
{
	return *m_material_parser;
}

wmr::CameraParser& wmr::ScenegraphParser::GetCameraParser() const noexcept
{
	return *m_camera_parser;
}
