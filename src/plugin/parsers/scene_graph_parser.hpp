#pragma once
#include <memory>
namespace wr
{
	class SceneGraph;
	class D3D12RenderSystem;
	class TexturePool;
	class MaterialPool;
}

//class MFnMesh;

namespace wmr
{
	class CameraParser;
	class MaterialParser;
	class ModelParser;
	class Renderer;
	class ViewportRendererOverride;
	
	class ScenegraphParser
	{
	public:
		ScenegraphParser();
		~ScenegraphParser();

		void Initialize();

	private:
		Renderer& m_render_system;
		std::unique_ptr<ModelParser> m_model_parser;
		std::unique_ptr<CameraParser> m_camera_parser;
		std::unique_ptr<MaterialParser> m_material_parser;

	};
}
