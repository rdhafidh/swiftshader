// Copyright 2016 The SwiftShader Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "Device.hpp"

#include "common/Image.hpp"
#include "Texture.h"

#include "Renderer/Renderer.hpp"
#include "Renderer/Clipper.hpp"
#include "Shader/PixelShader.hpp"
#include "Shader/VertexShader.hpp"
#include "Main/Config.hpp"
#include "Main/FrameBuffer.hpp"
#include "Common/Math.hpp"
#include "Common/Configurator.hpp"
#include "Common/Memory.hpp"
#include "Common/Timer.hpp"
#include "../common/debug.h"

namespace es2
{
	using namespace sw;

	Device::Device(Context *context) : Renderer(context, OpenGL, true), context(context)
	{
		for(int i = 0; i < RENDERTARGETS; i++)
		{
			renderTarget[i] = nullptr;
		}

		depthBuffer = nullptr;
		stencilBuffer = nullptr;

		setDepthBufferEnable(true);
		setFillMode(FILL_SOLID);
		setShadingMode(SHADING_GOURAUD);
		setDepthWriteEnable(true);
		setAlphaTestEnable(false);
		setSourceBlendFactor(BLEND_ONE);
		setDestBlendFactor(BLEND_ZERO);
		setCullMode(CULL_COUNTERCLOCKWISE);
		setDepthCompare(DEPTH_LESSEQUAL);
		setAlphaReference(127.5f);
		setAlphaCompare(ALPHA_ALWAYS);
		setAlphaBlendEnable(false);
		setFogEnable(false);
		setSpecularEnable(false);
		setFogColor(0);
		setPixelFogMode(FOG_NONE);
		setFogStart(0.0f);
		setFogEnd(1.0f);
		setFogDensity(1.0f);
		setRangeFogEnable(false);
		setStencilEnable(false);
		setStencilFailOperation(OPERATION_KEEP);
		setStencilZFailOperation(OPERATION_KEEP);
		setStencilPassOperation(OPERATION_KEEP);
		setStencilCompare(STENCIL_ALWAYS);
		setStencilReference(0);
		setStencilMask(0xFFFFFFFF);
		setStencilWriteMask(0xFFFFFFFF);
		setVertexFogMode(FOG_NONE);
		setClipFlags(0);
		setPointSize(1.0f);
		setPointSizeMin(0.125f);
        setPointSizeMax(8192.0f);
		setBlendOperation(BLENDOP_ADD);
		scissorEnable = false;
		setSlopeDepthBias(0.0f);
		setTwoSidedStencil(false);
		setStencilFailOperationCCW(OPERATION_KEEP);
		setStencilZFailOperationCCW(OPERATION_KEEP);
		setStencilPassOperationCCW(OPERATION_KEEP);
		setStencilCompareCCW(STENCIL_ALWAYS);
		setBlendConstant(0xFFFFFFFF);
		setWriteSRGB(false);
		setDepthBias(0.0f);
		setSeparateAlphaBlendEnable(false);
		setSourceBlendFactorAlpha(BLEND_ONE);
		setDestBlendFactorAlpha(BLEND_ZERO);
		setBlendOperationAlpha(BLENDOP_ADD);
		setPointSpriteEnable(true);
		setColorLogicOpEnabled(false);
		setLogicalOperation(LOGICALOP_COPY);

		for(int i = 0; i < 16; i++)
		{
			setAddressingModeU(sw::SAMPLER_PIXEL, i, ADDRESSING_WRAP);
			setAddressingModeV(sw::SAMPLER_PIXEL, i, ADDRESSING_WRAP);
			setAddressingModeW(sw::SAMPLER_PIXEL, i, ADDRESSING_WRAP);
			setBorderColor(sw::SAMPLER_PIXEL, i, 0x00000000);
			setTextureFilter(sw::SAMPLER_PIXEL, i, FILTER_POINT);
			setMipmapFilter(sw::SAMPLER_PIXEL, i, MIPMAP_NONE);
			setMipmapLOD(sw::SAMPLER_PIXEL, i, 0.0f);
		}

		for(int i = 0; i < 4; i++)
		{
			setAddressingModeU(sw::SAMPLER_VERTEX, i, ADDRESSING_WRAP);
			setAddressingModeV(sw::SAMPLER_VERTEX, i, ADDRESSING_WRAP);
			setAddressingModeW(sw::SAMPLER_VERTEX, i, ADDRESSING_WRAP);
			setBorderColor(sw::SAMPLER_VERTEX, i, 0x00000000);
			setTextureFilter(sw::SAMPLER_VERTEX, i, FILTER_POINT);
			setMipmapFilter(sw::SAMPLER_VERTEX, i, MIPMAP_NONE);
			setMipmapLOD(sw::SAMPLER_VERTEX, i, 0.0f);
		}

		for(int i = 0; i < 6; i++)
		{
			float plane[4] = {0, 0, 0, 0};

			setClipPlane(i, plane);
		}

		pixelShader = nullptr;
		vertexShader = nullptr;

		pixelShaderDirty = true;
		pixelShaderConstantsFDirty = 0;
		vertexShaderDirty = true;
		vertexShaderConstantsFDirty = 0;

		for(int i = 0; i < FRAGMENT_UNIFORM_VECTORS; i++)
		{
			float zero[4] = {0, 0, 0, 0};

			setPixelShaderConstantF(i, zero, 1);
		}

		for(int i = 0; i < VERTEX_UNIFORM_VECTORS; i++)
		{
			float zero[4] = {0, 0, 0, 0};

			setVertexShaderConstantF(i, zero, 1);
		}
	}

	Device::~Device()
	{
		for(int i = 0; i < RENDERTARGETS; i++)
		{
			if(renderTarget[i])
			{
				renderTarget[i]->release();
				renderTarget[i] = nullptr;
			}
		}

		if(depthBuffer)
		{
			depthBuffer->release();
			depthBuffer = nullptr;
		}

		if(stencilBuffer)
		{
			stencilBuffer->release();
			stencilBuffer = nullptr;
		}

		delete context;
	}

	// This object has to be mem aligned
	void* Device::operator new(size_t size)
	{
		ASSERT(size == sizeof(Device)); // This operator can't be called from a derived class
		return sw::allocate(sizeof(Device), 16);
	}

	void Device::operator delete(void * mem)
	{
		sw::deallocate(mem);
	}

	void Device::clearColor(float red, float green, float blue, float alpha, unsigned int rgbaMask)
	{
		if(!rgbaMask)
		{
			return;
		}

		float rgba[4];
		rgba[0] = red;
		rgba[1] = green;
		rgba[2] = blue;
		rgba[3] = alpha;

		for(int i = 0; i < RENDERTARGETS; ++i)
		{
			if(renderTarget[i])
			{
				sw::Rect clearRect = renderTarget[i]->getRect();

				if(scissorEnable)
				{
					clearRect.clip(scissorRect.x0, scissorRect.y0, scissorRect.x1, scissorRect.y1);
				}

				clear(rgba, FORMAT_A32B32G32R32F, renderTarget[i], clearRect, rgbaMask);
			}
		}
	}

	void Device::clearDepth(float z)
	{
		if(!depthBuffer)
		{
			return;
		}

		z = clamp01(z);
		sw::Rect clearRect = depthBuffer->getRect();

		if(scissorEnable)
		{
			clearRect.clip(scissorRect.x0, scissorRect.y0, scissorRect.x1, scissorRect.y1);
		}

		depthBuffer->clearDepth(z, clearRect.x0, clearRect.y0, clearRect.width(), clearRect.height());
	}

	void Device::clearStencil(unsigned int stencil, unsigned int mask)
	{
		if(!stencilBuffer)
		{
			return;
		}

		sw::Rect clearRect = stencilBuffer->getRect();

		if(scissorEnable)
		{
			clearRect.clip(scissorRect.x0, scissorRect.y0, scissorRect.x1, scissorRect.y1);
		}

		stencilBuffer->clearStencil(stencil, mask, clearRect.x0, clearRect.y0, clearRect.width(), clearRect.height());
	}

	egl::Image *Device::createDepthStencilSurface(unsigned int width, unsigned int height, sw::Format format, int multiSampleDepth, bool discard)
	{
		if(height > OUTLINE_RESOLUTION)
		{
			ERR("Invalid parameters: %dx%d", width, height);
			return nullptr;
		}

		bool lockable = true;

		switch(format)
		{
		case FORMAT_S8:
	//	case FORMAT_D15S1:
		case FORMAT_D24S8:
		case FORMAT_D24X8:
	//	case FORMAT_D24X4S4:
		case FORMAT_D24FS8:
		case FORMAT_D32:
		case FORMAT_D16:
		case FORMAT_D32F:
		case FORMAT_D32F_COMPLEMENTARY:
			lockable = false;
			break;
	//	case FORMAT_S8_LOCKABLE:
	//	case FORMAT_D16_LOCKABLE:
		case FORMAT_D32F_LOCKABLE:
	//	case FORMAT_D32_LOCKABLE:
		case FORMAT_DF24S8:
		case FORMAT_DF16S8:
		case FORMAT_D32FS8_TEXTURE:
		case FORMAT_D32FS8_SHADOW:
			lockable = true;
			break;
		default:
			UNREACHABLE(format);
		}

		egl::Image *surface = egl::Image::create(width, height, format, multiSampleDepth, lockable);

		if(!surface)
		{
			ERR("Out of memory");
			return nullptr;
		}

		return surface;
	}

	egl::Image *Device::createRenderTarget(unsigned int width, unsigned int height, sw::Format format, int multiSampleDepth, bool lockable)
	{
		if(height > OUTLINE_RESOLUTION)
		{
			ERR("Invalid parameters: %dx%d", width, height);
			return nullptr;
		}

		egl::Image *surface = egl::Image::create(width, height, format, multiSampleDepth, lockable);

		if(!surface)
		{
			ERR("Out of memory");
			return nullptr;
		}

		return surface;
	}

	void Device::drawIndexedPrimitive(sw::DrawType type, unsigned int indexOffset, unsigned int primitiveCount)
	{
		if(!bindResources() || !primitiveCount)
		{
			return;
		}

		draw(type, indexOffset, primitiveCount);
	}

	void Device::drawPrimitive(sw::DrawType type, unsigned int primitiveCount)
	{
		if(!bindResources() || !primitiveCount)
		{
			return;
		}

		setIndexBuffer(nullptr);

		draw(type, 0, primitiveCount);
	}

	void Device::setPixelShader(const PixelShader *pixelShader)
	{
		this->pixelShader = pixelShader;
		pixelShaderDirty = true;
	}

	void Device::setPixelShaderConstantF(unsigned int startRegister, const float *constantData, unsigned int count)
	{
		for(unsigned int i = 0; i < count && startRegister + i < FRAGMENT_UNIFORM_VECTORS; i++)
		{
			pixelShaderConstantF[startRegister + i][0] = constantData[i * 4 + 0];
			pixelShaderConstantF[startRegister + i][1] = constantData[i * 4 + 1];
			pixelShaderConstantF[startRegister + i][2] = constantData[i * 4 + 2];
			pixelShaderConstantF[startRegister + i][3] = constantData[i * 4 + 3];
		}

		pixelShaderConstantsFDirty = max(startRegister + count, pixelShaderConstantsFDirty);
		pixelShaderDirty = true;   // Reload DEF constants
	}

	void Device::setScissorEnable(bool enable)
	{
		scissorEnable = enable;
	}

	void Device::setRenderTarget(int index, egl::Image *renderTarget)
	{
		if(renderTarget)
		{
			renderTarget->addRef();
		}

		if(this->renderTarget[index])
		{
			this->renderTarget[index]->release();
		}

		this->renderTarget[index] = renderTarget;

		Renderer::setRenderTarget(index, renderTarget);
	}

	void Device::setDepthBuffer(egl::Image *depthBuffer)
	{
		if(this->depthBuffer == depthBuffer)
		{
			return;
		}

		if(depthBuffer)
		{
			depthBuffer->addRef();
		}

		if(this->depthBuffer)
		{
			this->depthBuffer->release();
		}

		this->depthBuffer = depthBuffer;

		Renderer::setDepthBuffer(depthBuffer);
	}

	void Device::setStencilBuffer(egl::Image *stencilBuffer)
	{
		if(this->stencilBuffer == stencilBuffer)
		{
			return;
		}

		if(stencilBuffer)
		{
			stencilBuffer->addRef();
		}

		if(this->stencilBuffer)
		{
			this->stencilBuffer->release();
		}

		this->stencilBuffer = stencilBuffer;

		Renderer::setStencilBuffer(stencilBuffer);
	}

	void Device::setScissorRect(const sw::Rect &rect)
	{
		scissorRect = rect;
	}

	void Device::setVertexShader(const VertexShader *vertexShader)
	{
		this->vertexShader = vertexShader;
		vertexShaderDirty = true;
	}

	void Device::setVertexShaderConstantF(unsigned int startRegister, const float *constantData, unsigned int count)
	{
		for(unsigned int i = 0; i < count && startRegister + i < VERTEX_UNIFORM_VECTORS; i++)
		{
			vertexShaderConstantF[startRegister + i][0] = constantData[i * 4 + 0];
			vertexShaderConstantF[startRegister + i][1] = constantData[i * 4 + 1];
			vertexShaderConstantF[startRegister + i][2] = constantData[i * 4 + 2];
			vertexShaderConstantF[startRegister + i][3] = constantData[i * 4 + 3];
		}

		vertexShaderConstantsFDirty = max(startRegister + count, vertexShaderConstantsFDirty);
		vertexShaderDirty = true;   // Reload DEF constants
	}

	void Device::setViewport(const Viewport &viewport)
	{
		this->viewport = viewport;
	}

	void Device::copyBuffer(byte *sourceBuffer, byte *destBuffer, unsigned int width, unsigned int height, unsigned int sourcePitch, unsigned int destPitch, unsigned int bytes, bool flipX, bool flipY)
	{
		if(flipX)
		{
			if(flipY)
			{
				sourceBuffer += (height - 1) * sourcePitch;
				for(unsigned int y = 0; y < height; ++y, sourceBuffer -= sourcePitch, destBuffer += destPitch)
				{
					byte *srcX = sourceBuffer + (width - 1) * bytes;
					byte *dstX = destBuffer;
					for(unsigned int x = 0; x < width; ++x, dstX += bytes, srcX -= bytes)
					{
						memcpy(dstX, srcX, bytes);
					}
				}
			}
			else
			{
				for(unsigned int y = 0; y < height; ++y, sourceBuffer += sourcePitch, destBuffer += destPitch)
				{
					byte *srcX = sourceBuffer + (width - 1) * bytes;
					byte *dstX = destBuffer;
					for(unsigned int x = 0; x < width; ++x, dstX += bytes, srcX -= bytes)
					{
						memcpy(dstX, srcX, bytes);
					}
				}
			}
		}
		else
		{
			unsigned int widthB = width * bytes;

			if(flipY)
			{
				sourceBuffer += (height - 1) * sourcePitch;
				for(unsigned int y = 0; y < height; ++y, sourceBuffer -= sourcePitch, destBuffer += destPitch)
				{
					memcpy(destBuffer, sourceBuffer, widthB);
				}
			}
			else
			{
				for(unsigned int y = 0; y < height; ++y, sourceBuffer += sourcePitch, destBuffer += destPitch)
				{
					memcpy(destBuffer, sourceBuffer, widthB);
				}
			}
		}
	}

	bool Device::stretchRect(sw::Surface *source, const sw::SliceRect *sourceRect, sw::Surface *dest, const sw::SliceRect *destRect, unsigned char flags)
	{
		if(!source || !dest)
		{
			ERR("Invalid parameters");
			return false;
		}

		int sWidth = source->getWidth();
		int sHeight = source->getHeight();
		int dWidth = dest->getWidth();
		int dHeight = dest->getHeight();

		bool flipX = false;
		bool flipY = false;
		if(sourceRect && destRect)
		{
			flipX = (sourceRect->x0 < sourceRect->x1) ^ (destRect->x0 < destRect->x1);
			flipY = (sourceRect->y0 < sourceRect->y1) ^ (destRect->y0 < destRect->y1);
		}
		else if(sourceRect)
		{
			flipX = (sourceRect->x0 > sourceRect->x1);
			flipY = (sourceRect->y0 > sourceRect->y1);
		}
		else if(destRect)
		{
			flipX = (destRect->x0 > destRect->x1);
			flipY = (destRect->y0 > destRect->y1);
		}

		SliceRectF sRect;
		SliceRect dRect;

		if(sourceRect)
		{
			sRect.x0 = (float)(sourceRect->x0);
			sRect.x1 = (float)(sourceRect->x1);
			sRect.y0 = (float)(sourceRect->y0);
			sRect.y1 = (float)(sourceRect->y1);
			sRect.slice = sourceRect->slice;

			if(sRect.x0 > sRect.x1)
			{
				swap(sRect.x0, sRect.x1);
			}

			if(sRect.y0 > sRect.y1)
			{
				swap(sRect.y0, sRect.y1);
			}
		}
		else
		{
			sRect.y0 = 0.0f;
			sRect.x0 = 0.0f;
			sRect.y1 = (float)sHeight;
			sRect.x1 = (float)sWidth;
		}

		if(destRect)
		{
			dRect = *destRect;

			if(dRect.x0 > dRect.x1)
			{
				swap(dRect.x0, dRect.x1);
			}

			if(dRect.y0 > dRect.y1)
			{
				swap(dRect.y0, dRect.y1);
			}
		}
		else
		{
			dRect.y0 = 0;
			dRect.x0 = 0;
			dRect.y1 = dHeight;
			dRect.x1 = dWidth;
		}

		if(sRect.x0 < 0)
		{
			float ratio = static_cast<float>(dRect.width()) / sRect.width();
			float offsetf = roundf(-sRect.x0 * ratio);
			int offset = static_cast<int>(offsetf);
			if(flipX)
			{
				dRect.x1 -= offset;
			}
			else
			{
				dRect.x0 += offset;
			}
			sRect.x0 += offsetf / ratio;
		}
		if(sRect.x1 > sWidth)
		{
			float ratio = static_cast<float>(dRect.width()) / sRect.width();
			float offsetf = roundf((sRect.x1 - (float)sWidth) * ratio);
			int offset = static_cast<int>(offsetf);
			if(flipX)
			{
				dRect.x0 += offset;
			}
			else
			{
				dRect.x1 -= offset;
			}
			sRect.x1 -= offsetf / ratio;
		}
		if(sRect.y0 < 0)
		{
			float ratio = static_cast<float>(dRect.height()) / sRect.height();
			float offsetf = roundf(-sRect.y0 * ratio);
			int offset = static_cast<int>(offsetf);
			if(flipY)
			{
				dRect.y1 -= offset;
			}
			else
			{
				dRect.y0 += offset;
			}
			sRect.y0 += offsetf / ratio;
		}
		if(sRect.y1 > sHeight)
		{
			float ratio = static_cast<float>(dRect.height()) / sRect.height();
			float offsetf = roundf((sRect.y1 - (float)sHeight) * ratio);
			int offset = static_cast<int>(offsetf);
			if(flipY)
			{
				dRect.y0 += offset;
			}
			else
			{
				dRect.y1 -= offset;
			}
			sRect.y1 -= offsetf / ratio;
		}

		if(dRect.x0 < 0)
		{
			float offset = (static_cast<float>(-dRect.x0) / static_cast<float>(dRect.width())) * sRect.width();
			if(flipX)
			{
				sRect.x1 -= offset;
			}
			else
			{
				sRect.x0 += offset;
			}
			dRect.x0 = 0;
		}
		if(dRect.x1 > dWidth)
		{
			float offset = (static_cast<float>(dRect.x1 - dWidth) / static_cast<float>(dRect.width())) * sRect.width();
			if(flipX)
			{
				sRect.x0 += offset;
			}
			else
			{
				sRect.x1 -= offset;
			}
			dRect.x1 = dWidth;
		}
		if(dRect.y0 < 0)
		{
			float offset = (static_cast<float>(-dRect.y0) / static_cast<float>(dRect.height())) * sRect.height();
			if(flipY)
			{
				sRect.y1 -= offset;
			}
			else
			{
				sRect.y0 += offset;
			}
			dRect.y0 = 0;
		}
		if(dRect.y1 > dHeight)
		{
			float offset = (static_cast<float>(dRect.y1 - dHeight) / static_cast<float>(dRect.height())) * sRect.height();
			if(flipY)
			{
				sRect.y0 += offset;
			}
			else
			{
				sRect.y1 -= offset;
			}
			dRect.y1 = dHeight;
		}

		if(!validRectangle(&sRect, source) || !validRectangle(&dRect, dest))
		{
			ERR("Invalid parameters");
			return false;
		}

		bool isDepth = (flags & Device::DEPTH_BUFFER) && Surface::isDepth(source->getInternalFormat());
		bool isStencil = (flags & Device::STENCIL_BUFFER) && Surface::isStencil(source->getInternalFormat());
		bool isColor = (flags & Device::COLOR_BUFFER) == Device::COLOR_BUFFER;

		if(!isColor && !isDepth && !isStencil)
		{
			return true;
		}

		int sourceSliceB = isStencil ? source->getStencilSliceB() : source->getInternalSliceB();
		int destSliceB = isStencil ? dest->getStencilSliceB() : dest->getInternalSliceB();
		int sourcePitchB = isStencil ? source->getStencilPitchB() : source->getInternalPitchB();
		int destPitchB = isStencil ? dest->getStencilPitchB() : dest->getInternalPitchB();

		bool scaling = (sRect.width() != (float)dRect.width()) || (sRect.height() != (float)dRect.height());
		bool equalFormats = source->getInternalFormat() == dest->getInternalFormat();
		bool hasQuadLayout = Surface::hasQuadLayout(source->getInternalFormat()) || Surface::hasQuadLayout(dest->getInternalFormat());
		bool fullCopy = (sRect.x0 == 0.0f) && (sRect.y0 == 0.0f) && (dRect.x0 == 0) && (dRect.y0 == 0) &&
		                (sRect.x1 == (float)sWidth) && (sRect.y1 == (float)sHeight) && (dRect.x1 == dWidth) && (dRect.y1 == dHeight);
		bool alpha0xFF = false;
		bool equalSlice = sourceSliceB == destSliceB;
		bool smallMargin = sourcePitchB <= source->getWidth() * Surface::bytes(source->getInternalFormat()) + 16;

		if((source->getInternalFormat() == FORMAT_A8R8G8B8 && dest->getInternalFormat() == FORMAT_X8R8G8B8) ||
		   (source->getInternalFormat() == FORMAT_X8R8G8B8 && dest->getInternalFormat() == FORMAT_A8R8G8B8))
		{
			equalFormats = true;
			alpha0xFF = true;
		}

		if(fullCopy && !scaling && equalFormats && !alpha0xFF && equalSlice && smallMargin && !flipX && !flipY)
		{
			byte *sourceBuffer = isStencil ? (byte*)source->lockStencil(0, 0, 0, PUBLIC) : (byte*)source->lockInternal(0, 0, 0, LOCK_READONLY, PUBLIC);
			byte *destBuffer = isStencil ? (byte*)dest->lockStencil(0, 0, 0, PUBLIC) : (byte*)dest->lockInternal(0, 0, 0, LOCK_DISCARD, PUBLIC);

			memcpy(destBuffer, sourceBuffer, sourceSliceB);

			isStencil ? source->unlockStencil() : source->unlockInternal();
			isStencil ? dest->unlockStencil() : dest->unlockInternal();
		}
		else if(isDepth && !scaling && equalFormats && !hasQuadLayout)
		{
			byte *sourceBuffer = (byte*)source->lockInternal((int)sRect.x0, (int)sRect.y0, 0, LOCK_READONLY, PUBLIC);
			byte *destBuffer = (byte*)dest->lockInternal(dRect.x0, dRect.y0, 0, fullCopy ? LOCK_DISCARD : LOCK_WRITEONLY, PUBLIC);

			copyBuffer(sourceBuffer, destBuffer, dRect.width(), dRect.height(), sourcePitchB, destPitchB, Surface::bytes(source->getInternalFormat()), flipX, flipY);

			source->unlockInternal();
			dest->unlockInternal();
		}
		else if((flags & Device::COLOR_BUFFER) && !scaling && equalFormats && !hasQuadLayout)
		{
			byte *sourceBytes = (byte*)source->lockInternal((int)sRect.x0, (int)sRect.y0, sourceRect->slice, LOCK_READONLY, PUBLIC);
			byte *destBytes = (byte*)dest->lockInternal(dRect.x0, dRect.y0, destRect->slice, fullCopy ? LOCK_DISCARD : LOCK_WRITEONLY, PUBLIC);

			unsigned int width = dRect.x1 - dRect.x0;
			unsigned int height = dRect.y1 - dRect.y0;

			copyBuffer(sourceBytes, destBytes, width, height, sourcePitchB, destPitchB, Surface::bytes(source->getInternalFormat()), flipX, flipY);

			if(alpha0xFF)
			{
				for(unsigned int y = 0; y < height; y++)
				{
					for(unsigned int x = 0; x < width; x++)
					{
						destBytes[4 * x + 3] = 0xFF;
					}

					destBytes += destPitchB;
				}
			}

			source->unlockInternal();
			dest->unlockInternal();
		}
		else if(isColor || isDepth || isStencil)
		{
			if(flipX)
			{
				swap(dRect.x0, dRect.x1);
			}
			if(flipY)
			{
				swap(dRect.y0, dRect.y1);
			}

			SliceRectF sRectF((float)sRect.x0, (float)sRect.y0, (float)sRect.x1, (float)sRect.y1, sRect.slice);
			blit(source, sRectF, dest, dRect, scaling && (flags & Device::USE_FILTER), isStencil);
		}
		else UNREACHABLE(false);

		return true;
	}

	bool Device::stretchCube(sw::Surface *source, sw::Surface *dest)
	{
		if(!source || !dest || Surface::isDepth(source->getInternalFormat()) || Surface::isStencil(source->getInternalFormat()))
		{
			ERR("Invalid parameters");
			return false;
		}

		int sWidth  = source->getWidth();
		int sHeight = source->getHeight();
		int sDepth  = source->getDepth();
		int dWidth  = dest->getWidth();
		int dHeight = dest->getHeight();
		int dDepth  = dest->getDepth();

		bool scaling = (sWidth != dWidth) || (sHeight != dHeight) || (sDepth != dDepth);
		bool equalFormats = source->getInternalFormat() == dest->getInternalFormat();
		bool alpha0xFF = false;

		if((source->getInternalFormat() == FORMAT_A8R8G8B8 && dest->getInternalFormat() == FORMAT_X8R8G8B8) ||
		   (source->getInternalFormat() == FORMAT_X8R8G8B8 && dest->getInternalFormat() == FORMAT_A8R8G8B8))
		{
			equalFormats = true;
			alpha0xFF = true;
		}

		if(!scaling && equalFormats)
		{
			unsigned int sourcePitch = source->getInternalPitchB();
			unsigned int destPitch = dest->getInternalPitchB();
			unsigned int bytes = dWidth * Surface::bytes(source->getInternalFormat());

			for(int z = 0; z < dDepth; z++)
			{
				unsigned char *sourceBytes = (unsigned char*)source->lockInternal(0, 0, z, LOCK_READONLY, PUBLIC);
				unsigned char *destBytes = (unsigned char*)dest->lockInternal(0, 0, z, LOCK_READWRITE, PUBLIC);

				for(int y = 0; y < dHeight; y++)
				{
					memcpy(destBytes, sourceBytes, bytes);

					if(alpha0xFF)
					{
						for(int x = 0; x < dWidth; x++)
						{
							destBytes[4 * x + 3] = 0xFF;
						}
					}

					sourceBytes += sourcePitch;
					destBytes += destPitch;
				}

				source->unlockInternal();
				dest->unlockInternal();
			}
		}
		else
		{
			blit3D(source, dest);
		}

		return true;
	}

	bool Device::bindResources()
	{
		if(!bindViewport())
		{
			return false;   // Zero-area target region
		}

		bindShaderConstants();

		return true;
	}

	void Device::bindShaderConstants()
	{
		if(pixelShaderDirty)
		{
			if(pixelShader)
			{
				if(pixelShaderConstantsFDirty)
				{
					Renderer::setPixelShaderConstantF(0, pixelShaderConstantF[0], pixelShaderConstantsFDirty);
				}

				Renderer::setPixelShader(pixelShader);   // Loads shader constants set with DEF
				pixelShaderConstantsFDirty = pixelShader->dirtyConstantsF;   // Shader DEF'ed constants are dirty
			}
			else
			{
				setPixelShader(0);
			}

			pixelShaderDirty = false;
		}

		if(vertexShaderDirty)
		{
			if(vertexShader)
			{
				if(vertexShaderConstantsFDirty)
				{
					Renderer::setVertexShaderConstantF(0, vertexShaderConstantF[0], vertexShaderConstantsFDirty);
				}

				Renderer::setVertexShader(vertexShader);   // Loads shader constants set with DEF
				vertexShaderConstantsFDirty = vertexShader->dirtyConstantsF;   // Shader DEF'ed constants are dirty
			}
			else
			{
				setVertexShader(0);
			}

			vertexShaderDirty = false;
		}
	}

	bool Device::bindViewport()
	{
		if(viewport.width <= 0 || viewport.height <= 0)
		{
			return false;
		}

		if(scissorEnable)
		{
			if(scissorRect.x0 >= scissorRect.x1 || scissorRect.y0 >= scissorRect.y1)
			{
				return false;
			}

			sw::Rect scissor;
			scissor.x0 = scissorRect.x0;
			scissor.x1 = scissorRect.x1;
			scissor.y0 = scissorRect.y0;
			scissor.y1 = scissorRect.y1;

			setScissor(scissor);
		}
		else
		{
			sw::Rect scissor;
			scissor.x0 = viewport.x0;
			scissor.x1 = viewport.x0 + viewport.width;
			scissor.y0 = viewport.y0;
			scissor.y1 = viewport.y0 + viewport.height;

			for(int i = 0; i < RENDERTARGETS; ++i)
			{
				if(renderTarget[i])
				{
					scissor.x0 = max(scissor.x0, 0);
					scissor.x1 = min(scissor.x1, renderTarget[i]->getWidth());
					scissor.y0 = max(scissor.y0, 0);
					scissor.y1 = min(scissor.y1, renderTarget[i]->getHeight());
				}
			}

			if(depthBuffer)
			{
				scissor.x0 = max(scissor.x0, 0);
				scissor.x1 = min(scissor.x1, depthBuffer->getWidth());
				scissor.y0 = max(scissor.y0, 0);
				scissor.y1 = min(scissor.y1, depthBuffer->getHeight());
			}

			if(stencilBuffer)
			{
				scissor.x0 = max(scissor.x0, 0);
				scissor.x1 = min(scissor.x1, stencilBuffer->getWidth());
				scissor.y0 = max(scissor.y0, 0);
				scissor.y1 = min(scissor.y1, stencilBuffer->getHeight());
			}

			setScissor(scissor);
		}

		sw::Viewport view;
		view.x0 = (float)viewport.x0;
		view.y0 = (float)viewport.y0;
		view.width = (float)viewport.width;
		view.height = (float)viewport.height;
		view.minZ = viewport.minZ;
		view.maxZ = viewport.maxZ;

		Renderer::setViewport(view);

		return true;
	}

	bool Device::validRectangle(const sw::Rect *rect, sw::Surface *surface)
	{
		if(!rect)
		{
			return true;
		}

		if(rect->x1 <= rect->x0 || rect->y1 <= rect->y0)
		{
			return false;
		}

		if(rect->x0 < 0 || rect->y0 < 0)
		{
			return false;
		}

		if(rect->x1 >(int)surface->getWidth() || rect->y1 >(int)surface->getHeight())
		{
			return false;
		}

		return true;
	}

	bool Device::validRectangle(const sw::RectF *rect, sw::Surface *surface)
	{
		if(!rect)
		{
			return true;
		}

		if(rect->x1 <= rect->x0 || rect->y1 <= rect->y0)
		{
			return false;
		}

		if(rect->x0 < 0 || rect->y0 < 0)
		{
			return false;
		}

		if(rect->x1 >(float)surface->getWidth() || rect->y1 >(float)surface->getHeight())
		{
			return false;
		}

		return true;
	}

	void Device::finish()
	{
		synchronize();
	}
}
