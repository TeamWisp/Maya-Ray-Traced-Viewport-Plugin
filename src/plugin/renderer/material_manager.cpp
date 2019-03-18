#include "material_manager.hpp"

#include "plugin/renderer/renderer.hpp"
#include "plugin/viewport_renderer_override.hpp"
#include "plugin/renderer/texture_manager.hpp"
#include "miscellaneous/settings.hpp"

#include "d3d12/d3d12_renderer.hpp"

#include <maya/MFnTransform.h>
#include <maya/MGlobal.h>
#include <maya/MViewport2Renderer.h>


wmr::MaterialManager::MaterialManager()
{
}

wmr::MaterialManager::~MaterialManager()
{
}

void wmr::MaterialManager::Initialize()
{
	auto& renderer = dynamic_cast< const ViewportRendererOverride* >( MHWRender::MRenderer::theRenderer()->findRenderOverride( settings::VIEWPORT_OVERRIDE_NAME ) )->GetRenderer();
	m_material_pool = renderer.GetD3D12Renderer().CreateMaterialPool( 0 );

	m_default_material_handle = m_material_pool->Create();

	wr::Material* internal_material = m_material_pool->GetMaterial( m_default_material_handle.m_id );
	auto& texture_manager = renderer.GetTextureManager();

	internal_material->SetAlbedo( texture_manager.GetDefaultTexture() );
	internal_material->SetNormal( texture_manager.GetDefaultTexture() );
	internal_material->SetMetallic( texture_manager.GetDefaultTexture() );
	internal_material->SetRoughness( texture_manager.GetDefaultTexture() );
}

wr::MaterialHandle wmr::MaterialManager::GetDefaultMaterial() noexcept
{
	return m_default_material_handle;
}

wr::MaterialHandle wmr::MaterialManager::CreateMaterial(MObject& object)
{
	MStatus status;
	wr::MaterialHandle material_handle = m_material_pool->Create();

	MFnTransform transform(object, &status);
	if (status != MS::kSuccess)
	{
		MGlobal::displayError("Error: " + status.errorString());
	}

	m_object_material_vector.push_back(std::make_pair(transform.object(), material_handle));

	return material_handle;
}

wr::MaterialHandle wmr::MaterialManager::DoesExist(MObject& object)
{
	MStatus status;
	MFnTransform transform(object, &status);
	if (status != MS::kSuccess)
	{
		MGlobal::displayError("Error: " + status.errorString());
	}

	wr::MaterialHandle handle;
	handle.m_pool = nullptr;

	for (auto& entry : m_object_material_vector)
	{
		if (entry.first == transform.object())
		{
			handle = entry.second;
		}
	}
	return handle;
}
