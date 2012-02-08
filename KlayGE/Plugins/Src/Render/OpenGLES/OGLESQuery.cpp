// OGLESQuery.hpp
// KlayGE OpenGL ES��ѯ�� ʵ���ļ�
// Ver 3.8.0
// ��Ȩ����(C) ������, 2005-2008
// Homepage: http://www.klayge.org
//
// 3.8.0
// ������ConditionalRender (2008.10.11)
//
// 3.0.0
// ���ν��� (2005.9.27)
//
// �޸ļ�¼
/////////////////////////////////////////////////////////////////////////////////

#include <KlayGE/KlayGE.hpp>
#include <KlayGE/ThrowErr.hpp>
#include <KlayGE/Util.hpp>
#include <KlayGE/Math.hpp>
#include <KlayGE/RenderFactory.hpp>

#include <glloader/glloader.h>

#include <KlayGE/OpenGLES/OGLESRenderEngine.hpp>
#include <KlayGE/OpenGLES/OGLESQuery.hpp>

namespace KlayGE
{
	OGLESConditionalRender::OGLESConditionalRender()
	{
		if (glloader_GLES_EXT_occlusion_query_boolean())
		{
			glGenQueriesEXT(1, &query_);
		}
	}

	OGLESConditionalRender::~OGLESConditionalRender()
	{
		if (glloader_GLES_EXT_occlusion_query_boolean())
		{
			glDeleteQueriesEXT(1, &query_);
		}
	}

	void OGLESConditionalRender::Begin()
	{
		if (glloader_GLES_EXT_occlusion_query_boolean())
		{
			glBeginQueryEXT(GL_ANY_SAMPLES_PASSED_EXT, query_);
		}
	}

	void OGLESConditionalRender::End()
	{
		if (glloader_GLES_EXT_occlusion_query_boolean())
		{
			glEndQueryEXT(GL_ANY_SAMPLES_PASSED_EXT);
		}
	}

	void OGLESConditionalRender::BeginConditionalRender()
	{
	}

	void OGLESConditionalRender::EndConditionalRender()
	{
	}

	bool OGLESConditionalRender::AnySamplesPassed()
	{
		GLuint ret;
		glGetQueryObjectuivEXT(query_, GL_QUERY_RESULT_EXT, &ret);
		return (ret != 0);
	}
}