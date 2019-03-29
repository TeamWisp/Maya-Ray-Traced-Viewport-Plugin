#pragma once

#include <vector>

// Maya API
#include <maya/MApiNamespace.h>
#include <maya/MObject.h>
#include <maya/MMessage.h>

// C++ standard
#include <optional>

namespace wr
{
	class MaterialHandle;
	class Material;
}

namespace wmr
{
	class Renderer;
	class TextureManager;
	class MaterialManager;

	namespace detail
	{
		enum class SurfaceShaderType
		{
			UNSUPPORTED = -1,

			LAMBERT,
			PHONG,
			ARNOLD_STANDARD_SURFACE_SHADER
		};

		struct ArnoldStandardSurfaceShaderData
		{
			// Plug names to retrieve values
			static const constexpr char* diffuse_color_plug_name		= "baseColor";
			static const constexpr char* diffuse_roughness_plug_name	= "diffuseRoughness";
			static const constexpr char* metalness_plug_name			= "metalness";
			static const constexpr char* specular_color_plug_name		= "specularColor";
			static const constexpr char* specular_roughness_plug_name	= "specularRoughness";
			static const constexpr char* bump_map_plug_name				= "normalCamera";

			// Flags
			bool using_diffuse_color_value		= true;
			bool using_diffuse_roughness_value	= true;
			bool using_metalness_value			= true;
			bool using_specular_color_value		= true;
			bool using_specular_roughness_value	= true;

			// Values
			float diffuse_color[3]		= { 0.0f, 0.0f, 0.0f };
			float diffuse_roughness		= 0.0f;
			float metalness				= 0.0f;
			float specular_color[3]		= { 0.0f, 0.0f, 0.0f };
			float specular_roughness	= 0.0f;

			// Files
			const char* bump_map_texture_path			= "";
			const char* diffuse_color_texture_path		= "";
			const char* diffuse_roughness_texture_path	= "";
			const char* metalness_texture_path			= "";
			const char* specular_color_texture_path		= "";
			const char* specular_roughness_texture_path = "";
		};
	}

	namespace MayaMaterialProps
	{
		static const constexpr char* arnold_standard_shader_name = "aiStandardSurface";
		static const constexpr char* maya_phong_shader_name = "phong";
		static const constexpr char* maya_lambert_shader_name = "lambert";

		static const constexpr char * surface_shader = "surfaceShader";
					 
		static const constexpr char * plug_color = "color";
		static const constexpr char * plug_reflectivity = "reflectivity";
		static const constexpr char * plug_file_texture_name = "fileTextureName";
					 
		static const constexpr char * plug_color_r = "R";
		static const constexpr char * plug_color_g = "G";
		static const constexpr char * plug_color_b = "B";
		static const constexpr char * plug_color_a = "A";
	};

	class MaterialParser
	{
	public:
		MaterialParser();
		~MaterialParser() = default;

		void InitialMaterialBuild(MPlug & surface_shader, detail::SurfaceShaderType shader_type, wr::MaterialHandle material_handle, MaterialManager & material_manager, TextureManager & texture_manager);

		void OnMeshAdded(MFnMesh& mesh);
		void OnCreateSurfaceShader(MPlug & surface_shader);
		void OnRemoveSurfaceShader(MPlug & surface_shader);

		void ConnectShaderToShadingEngine(MPlug & surface_shader, MObject & shading_engine);
		void DisconnectShaderFromShadingEngine(MPlug & surface_shader, MObject & shading_engine);
		void ConnectMeshToShadingEngine(MObject & mesh, MObject & shading_engine);
		void DisconnectMeshFromShadingEngine(MObject & mesh, MObject & shading_engine);

		void HandleLambertChange(MFnDependencyNode &fn, MPlug & plug, MString & plug_name, wr::Material & material);
		void HandlePhongChange(MFnDependencyNode &fn, MPlug & plug, MString & plug_name, wr::Material & material);
		void HandleArnoldChange(MFnDependencyNode &fn, MPlug & plug, MString & plug_name, wr::Material & material);

		const Renderer & GetRenderer();
		const detail::SurfaceShaderType GetShaderType(const MObject& node);

		struct ShaderDirtyData
		{
			MCallbackId callback_id;				// When removing callbacks, use this id to find what callback must be deleted
			wmr::MaterialParser * material_parser;	// Pointer to the material parser
			MObject surface_shader;					// Surface shader
		};

	private:
		void SubscribeSurfaceShader(MObject & actual_surface_shader);
		void ParseShadingEngineToWispMaterial(MObject & shading_engine, std::optional<MObject> fnmesh = std::nullopt);

		const std::optional<MPlug> GetSurfaceShader(const MObject& node);
		const std::optional<MPlug> GetActualSurfaceShaderPlug(const MPlug & surface_shader_plug);

		const std::optional<MString> GetPlugTexture(MPlug& plug);
		const MPlug GetPlugByName(const MObject& node, MString name);

		void ConfigureWispMaterial(const detail::ArnoldStandardSurfaceShaderData& data, wr::Material* material, TextureManager& texture_manager) const;

		detail::ArnoldStandardSurfaceShaderData ParseArnoldStandardSurfaceShaderData(const MObject& plug);

		// Material parsing
		MColor GetColor(MFnDependencyNode & fn, MString & plug_name);
		std::vector<ShaderDirtyData*> shader_dirty_datas;

	private:
		Renderer& m_renderer;
	};
}
