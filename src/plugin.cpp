// SynTHBloK-JamProbe: registers this plugin's modules.
#include "plugin.hpp"

Plugin* pluginInstance;

extern Model* modelSynTHBloKJamProbe;

void init(Plugin* p) {
    pluginInstance = p;
    p->addModel(modelSynTHBloKJamProbe);
}
