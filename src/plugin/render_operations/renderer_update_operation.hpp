#pragma once

#include <maya/MViewport2Renderer.h>

namespace wmr
{
	// Forward declarations
	class Renderer;

	class RendererUpdateOperation final : public MHWRender::MUserRenderOperation
	{
	public:
		RendererUpdateOperation(const MString& name);
		~RendererUpdateOperation();

	private:
		const MCameraOverride* cameraOverride() override;
		
		MStatus execute(const MDrawContext& draw_context) override;
		
		bool hasUIDrawables() const override;
		bool requiresLightData() const override;

	private:
		Renderer& m_renderer;
	};
}