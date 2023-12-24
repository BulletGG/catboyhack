#include "RenderSystem.h"
#include <array>
#include <stdio.h>
void Render::drawText(unsigned long font, int r, int g, int b, int a, ImVec2 pos, std::string string)
{
	interfaces->surface->setTextColor(r,g,b,a);
	interfaces->surface->setTextFont(font);
	interfaces->surface->setTextPosition((int)pos.x, (int)pos.y);
	interfaces->surface->printText(std::wstring(string.begin(), string.end()));
}

void Render::drawLine(ImVec2 min, ImVec2 max, int r, int g, int b, int a)
{
	interfaces->surface->setDrawColor(r, g, b, a);
	interfaces->surface->drawLine(min.x, min.y, max.x, max.y);
}

static bool wts(const Vector& in, Vector& out) noexcept
{
	const auto& matrix = interfaces->engine->worldToScreenMatrix();
	float w = matrix._41 * in.x + matrix._42 * in.y + matrix._43 * in.z + matrix._44;

	if (w > 0.001f) {
		const auto [width, height] = interfaces->surface->getScreenSize();
		out.x = width / 2 * (1 + (matrix._11 * in.x + matrix._12 * in.y + matrix._13 * in.z + matrix._14) / w);
		out.y = height / 2 * (1 - (matrix._21 * in.x + matrix._22 * in.y + matrix._23 * in.z + matrix._24) / w);
		out.z = 0.0f;
		return true;
	}
	return false;
}

void Render::rectFilled(int x, int y, int w, int h, int r, int g, int b, int a)
{
	interfaces->surface->setDrawColor(r, g, b, a);
	interfaces->surface->drawFilledRect(x, y, x + w, y + h);
}

void Render::rect(int x, int y, int w, int h, int r, int g, int b, int a)
{
	interfaces->surface->setDrawColor(r,g,b,a);
	interfaces->surface->drawOutlinedRect(x, y, x + w, y + h);
}

std::pair<int, int> Render::getTextSize(unsigned long font, std::string text)
{
	return interfaces->surface->getTextSize(font, std::wstring(text.begin(), text.end()).c_str());
}

void Render::gradient1(int x, int y, int w, int h, int r1, int g1, int b1, int a1, int r2, int g2, int b2, int a2, float type)
{
	static auto blend = [](int r1a, int g1a, int b1a, int a1a, int r2a, int g2a, int b2a, int a2a, int t) -> std::array<int, 4> {
		return std::array<int, 4>{
				r1a + t * (r2a - r1a),
				g1a + t * (g2a - g1a),
				b1a + t * (b2a - b1a),
				a1a + t * (a2a - a1a)
		};
	};

	if (a1 == 255 && a2 == 255) {
		interfaces->surface->setDrawColor(blend(r1, g1, b1, a1, r2, g2, b2, a2, 127)[0], blend(r1, g1, b1, a1, r2, g2, b2, a2, 127)[1], blend(r1, g1, b1, a1, r2, g2, b2, a2, 127)[2], blend(r1, g1, b1, a1, r2, g2, b2, a2, 127)[3]);
		interfaces->surface->drawFilledRect(x, y, w, h);
	}

	interfaces->surface->setDrawColor(r1,g1,b1,a1);
	interfaces->surface->drawFilledRectFade(x, y, w, h, a1, 0, type == GRADIENT_HORIZONTAL);

	interfaces->surface->setDrawColor(r2,g2,b2,a2);
	interfaces->surface->drawFilledRectFade(x, y, w, h, 0, a2, type == GRADIENT_HORIZONTAL);
}

void Render::gradient(int x, int y, int w, int h, int r1, int g1, int b1, int a1, int r2, int g2, int b2, int a2, float type)
{
	static auto blend = [](int r1a, int g1a, int b1a, int a1a, int r2a, int g2a, int b2a, int a2a, int t)->std::array<int, 4> {
		return std::array<int, 4>{
			r1a + t * (r2a - r1a),
				g1a + t * (g2a - g1a),
				b1a + t * (b2a - b1a),
				a1a + t * (a2a - a1a)
		};
	};

	if (a1 == 255 && a2 == 255) {
		interfaces->surface->setDrawColor(blend(r1, g1, b1, a1, r2, g2, b2, a2, 127)[0], blend(r1, g1, b1, a1, r2, g2, b2, a2, 127)[1], blend(r1, g1, b1, a1, r2, g2, b2, a2, 127)[2], blend(r1, g1, b1, a1, r2, g2, b2, a2, 127)[3]);
		interfaces->surface->drawFilledRect(x, y, x + w, y + h);
	}

	interfaces->surface->setDrawColor(r1, g1, b1, a1);
	interfaces->surface->drawFilledRectFade(x, y, x + w, y + h, a1, 0, type == GRADIENT_HORIZONTAL);

	interfaces->surface->setDrawColor(r2, g2, b2, a2);
	interfaces->surface->drawFilledRectFade(x, y, x + w, y + h, 0, a2, type == GRADIENT_HORIZONTAL);
}

int Render::textWidth(unsigned long font, std::string string)
{
	return interfaces->surface->getTextSize(font, std::wstring(string.begin(), string.end()).c_str()).first;
}

int Render::textHeight(unsigned long font, std::string string)
{
	return interfaces->surface->getTextSize(font, std::wstring(string.begin(), string.end()).c_str()).second;
}
