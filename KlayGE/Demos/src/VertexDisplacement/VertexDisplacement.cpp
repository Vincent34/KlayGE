#include <KlayGE/KlayGE.hpp>
#include <KlayGE/ThrowErr.hpp>
#include <KlayGE/VertexBuffer.hpp>
#include <KlayGE/Math.hpp>
#include <KlayGE/Font.hpp>
#include <KlayGE/Renderable.hpp>
#include <KlayGE/RenderEngine.hpp>
#include <KlayGE/RenderEffect.hpp>
#include <KlayGE/SceneManager.hpp>
#include <KlayGE/Context.hpp>
#include <KlayGE/ResLoader.hpp>

#include <KlayGE/D3D9/D3D9RenderSettings.hpp>
#include <KlayGE/D3D9/D3D9RenderFactory.hpp>

#include <KlayGE/OCTree/OCTree.hpp>

#include <KlayGE/Input.hpp>
#include <KlayGE/DInput/DInputFactory.hpp>

#include <vector>
#include <sstream>
#include <ctime>

#include "VertexDisplacement.hpp"

using namespace std;
using namespace KlayGE;

namespace
{
	int const WIDTH = 4;
	int const HEIGHT = 3;

	class Flag : public Renderable
	{
	public:
		Flag(int width, int height)
			: vb_(new VertexBuffer(VertexBuffer::BT_TriangleList)),
				model_(MathLib::Translation(-WIDTH / 2.0f, HEIGHT / 2.0f, 0.0f))
		{
			std::vector<Vector3> pos;
			for (int y = 0; y < height + 1; ++ y)
			{
				for (int x = 0; x < width + 1; ++ x)
				{
					pos.push_back(Vector3(+x * (static_cast<float>(WIDTH) / width),
						-y * (static_cast<float>(HEIGHT) / height), 0.0f));
				}
			}

			std::vector<Vector2> tex;
			for (int y = 0; y < height + 1; ++ y)
			{
				for (int x = 0; x < width + 1; ++ x)
				{
					tex.push_back(Vector2(static_cast<float>(x) / width, static_cast<float>(y) / height));
				}
			}

			std::vector<uint16_t> index;
			for (int y = 0; y < height; ++ y)
			{
				for (int x = 0; x < width; ++ x)
				{
					index.push_back((y + 0) * (width + 1) + (x + 0));
					index.push_back((y + 0) * (width + 1) + (x + 1));
					index.push_back((y + 1) * (width + 1) + (x + 1));

					index.push_back((y + 1) * (width + 1) + (x + 1));
					index.push_back((y + 1) * (width + 1) + (x + 0));
					index.push_back((y + 0) * (width + 1) + (x + 0));
				}
			}

			vb_->AddVertexStream(VST_Positions, sizeof(float), 3, true);
			vb_->AddVertexStream(VST_TextureCoords0, sizeof(float), 2, true);
			vb_->GetVertexStream(VST_Positions)->Assign(&pos[0], pos.size());
			vb_->GetVertexStream(VST_TextureCoords0)->Assign(&tex[0], tex.size());

			vb_->AddIndexStream(true);
			vb_->GetIndexStream()->Assign(&index[0], index.size());

			box_ = MathLib::ComputeBoundingBox<float>(pos.begin(), pos.end());

			effect_ = LoadRenderEffect("VertexDisplacement.fx");
			*(effect_->ParameterByName("flag")) = LoadTexture("Flag.dds");
			effect_->SetTechnique("VertexDisplacement");
		}

		RenderEffectPtr GetRenderEffect() const
		{
			return effect_;
		}

		VertexBufferPtr GetVertexBuffer() const
		{
			return vb_;
		}

		Matrix4 GetModelMatrix() const
		{
			return model_;
		}

		Box GetBound() const
		{
			return box_; 
		}

		std::wstring const & Name() const
		{
			static const std::wstring name(L"Flag");
			return name;
		}

	private:
		VertexBufferPtr vb_;
		RenderEffectPtr effect_;

		Matrix4 model_;
		Box box_;
	};


	enum
	{
		Exit,
	};

	InputAction actions[] = 
	{
		InputAction(Exit, KS_Escape),
	};

	class TheRenderSettings : public D3D9RenderSettings
	{
	private:
		bool DoConfirmDevice(D3DCAPS9 const & caps, uint32_t behavior, D3DFORMAT format) const
		{
			if (caps.VertexShaderVersion < D3DVS_VERSION(1, 1))
			{
				return false;
			}
			return true;
		}
	};
}

int main()
{
	OCTree sceneMgr(Box(Vector3(-10, -10, -10), Vector3(10, 10, 10)), 3);

	Context::Instance().RenderFactoryInstance(D3D9RenderFactoryInstance());
	Context::Instance().SceneManagerInstance(sceneMgr);

	Context::Instance().InputFactoryInstance(DInputFactoryInstance());

	TheRenderSettings settings;
	settings.width = 800;
	settings.height = 600;
	settings.colorDepth = 32;
	settings.fullScreen = false;

	VertexDisplacement app;
	app.Create("VertexDisplacement", settings);
	app.Run();

	return 0;
}

VertexDisplacement::VertexDisplacement()
{
	ResLoader::Instance().AddPath("../media");
	ResLoader::Instance().AddPath("../media/VertexDisplacement");
}

void VertexDisplacement::InitObjects()
{
	font_ = Context::Instance().RenderFactoryInstance().MakeFont("gbsn00lp.ttf", 16);

	flag_.reset(new Flag(8 * 2, 6 * 2));
	flag_->AddToSceneManager();

	RenderEngine& renderEngine(Context::Instance().RenderFactoryInstance().RenderEngineInstance());

	renderEngine.ClearColor(Color(0.2f, 0.4f, 0.6f, 1));

	this->LookAt(Vector3(0, 0, -10), Vector3(0, 0, 0));
	this->Proj(0.1f, 20.0f);

	fpcController_.AttachCamera(this->ActiveCamera());
	fpcController_.Scalers(0.005f, 0.1f);

	InputEngine& inputEngine(Context::Instance().InputFactoryInstance().InputEngineInstance());
	KlayGE::InputActionMap actionMap;
	actionMap.AddActions(actions, actions + sizeof(actions) / sizeof(actions[0]));
	action_map_id_ = inputEngine.ActionMap(actionMap, true);
}

void VertexDisplacement::Update()
{
	fpcController_.Update();

	InputEngine& inputEngine(Context::Instance().InputFactoryInstance().InputEngineInstance());
	InputActionsType actions(inputEngine.Update(action_map_id_));
	for (InputActionsType::iterator iter = actions.begin(); iter != actions.end(); ++ iter)
	{
		switch (iter->first)
		{
		case Exit:
			this->Quit();
			break;
		}
	}

	Matrix4 view = this->ActiveCamera().ViewMatrix();
	Matrix4 proj = this->ActiveCamera().ProjMatrix();
	Vector3 eyePos = this->ActiveCamera().EyePos();

	Matrix4 modelView = flag_->GetModelMatrix() * view;

	*(flag_->GetRenderEffect()->ParameterByName("modelview")) = modelView;
	*(flag_->GetRenderEffect()->ParameterByName("proj")) = proj;

	*(flag_->GetRenderEffect()->ParameterByName("modelviewIT")) = MathLib::Transpose(MathLib::Inverse(modelView));


	RenderEngine& renderEngine(Context::Instance().RenderFactoryInstance().RenderEngineInstance());

	float currentAngle(clock() / 400.0f);
	*(flag_->GetRenderEffect()->ParameterByName("currentAngle")) = currentAngle;

	std::wostringstream stream;
	stream << (*renderEngine.ActiveRenderTarget())->FPS();

	font_->RenderText(0, 0, Color(1, 1, 0, 1), L"����λ��");
	font_->RenderText(0, 18, Color(1, 1, 0, 1), stream.str().c_str());

	SceneManager& sceneMgr(Context::Instance().SceneManagerInstance());
	stream.str(L"");
	stream << sceneMgr.NumObjectsRendered() << " Renderables "
		<< sceneMgr.NumPrimitivesRendered() << " Primitives "
		<< sceneMgr.NumVerticesRendered() << " Vertices";
	font_->RenderText(0, 36, Color(1, 1, 1, 1), stream.str().c_str());
}
