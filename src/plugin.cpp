#include "plugin.hpp"

Plugin* pluginInstance;

void init(Plugin* p) {
	pluginInstance = p;
	p->addModel(modelEFG);
	p->addModel(modelETS);
	p->addModel(modelEGX);
	p->addModel(modelJumbleSale);
}
