#include "plugin.hpp"


Plugin *pluginInstance;


void init(Plugin *p) {
	pluginInstance = p;

	// Add modules here
	p->addModel(modelTapestry);
	p->addModel(modelTapestryExpander);
	p->addModel(modelKorupt);
	p->addModel(modelDrift);
	p->addModel(modelWyrd);

	// Any other plugin initialization may go here.
	// As an alternative, consider lazy-loading assets and lookup tables when your module is created to reduce startup times of Rack.
}
