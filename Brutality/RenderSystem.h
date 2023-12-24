#pragma once
#include <string>
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "SDK/Vector.h"
#include "SDK/Surface.h"
#include "Interfaces.h"
#include "SDK/Engine.h"

namespace Render
{
	void drawText(unsigned long font, int r, int g, int b, int a, ImVec2 pos, std::string string);
	void drawLine(ImVec2 min, ImVec2 max, int r, int g, int b, int a);
	void rectFilled(int x, int y, int w, int h, int r, int g, int b, int a);
	void rect(int x, int y, int w, int h, int r, int g, int b, int a);
	std::pair<int, int> getTextSize(unsigned long font, std::string text);
	void gradient(int x, int y, int w, int h, int r1, int g1, int b1, int a1, int r2, int g2, int b2, int a2, float type);
	void gradient1(int x, int y, int w, int h, int r1, int g1, int b1, int a1, int r2, int g2, int b2, int a2, float type); //doesnt do x + w or y +h
	int textWidth(unsigned long font, std::string string);
	int textHeight(unsigned long font, std::string string);
	enum GradientType
	{
		GRADIENT_HORIZONTAL,
		GRADIENT_VERTICAL
	};
}