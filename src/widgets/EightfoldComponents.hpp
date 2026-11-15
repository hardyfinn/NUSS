// Shared module helpers: panel scaffold, RGB light colours, Channel Index menu.
#pragma once
#include "../plugin.hpp"

struct Rgb { float r, g, b; };
namespace col {
constexpr Rgb off{0.f, 0.f, 0.f};
constexpr Rgb white{1.f, 1.f, 1.f};
constexpr Rgb red{1.f, 0.f, 0.f};
constexpr Rgb green{0.f, 1.f, 0.f};
constexpr Rgb blue{0.f, 0.25f, 1.f};
constexpr Rgb yellow{1.f, 0.8f, 0.f};
constexpr Rgb lime{0.6f, 1.f, 0.f};
constexpr Rgb cyan{0.f, 1.f, 1.f};
constexpr Rgb aqua{0.f, 1.f, 0.7f};
constexpr Rgb magenta{1.f, 0.f, 1.f};
constexpr Rgb purple{0.55f, 0.f, 1.f};
constexpr Rgb pink{1.f, 0.3f, 0.65f};
constexpr Rgb hotpink{1.f, 0.05f, 0.5f};
constexpr Rgb orange{1.f, 0.4f, 0.f};
constexpr Rgb gold{1.f, 0.65f, 0.1f};
}  // namespace col

inline void setRgb(Module* m, int id, Rgb c, float bright = 1.f) {
	m->lights[id + 0].setBrightness(c.r * bright);
	m->lights[id + 1].setBrightness(c.g * bright);
	m->lights[id + 2].setBrightness(c.b * bright);
}

template <class W>
inline void efScaffold(W* w, Module* m, const std::string& slug) {
	w->setModule(m);
	w->setPanel(createPanel(asset::plugin(pluginInstance, "res/" + slug + ".svg")));
	float width = w->box.size.x;
	w->addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
	w->addChild(createWidget<ScrewBlack>(Vec(width - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
	if (width > 8 * RACK_GRID_WIDTH) {
		w->addChild(createWidget<ScrewBlack>(Vec(width - 2 * RACK_GRID_WIDTH, 0)));
		w->addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
	}
}

// Right-click menu entry for the Channel Index scale (0.5 V or 1 V per channel).
inline MenuItem* indexScaleItem(float* v) {
	return createIndexSubmenuItem("Channel Index scale", {"0.5 V per channel (default)", "1 V per channel"},
		[=]() { return *v > 0.75f ? 1 : 0; },
		[=](int i) { *v = i ? 1.f : 0.5f; });
}
