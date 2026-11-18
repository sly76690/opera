#pragma once
#include <juce_core/juce_core.h>
#include <FactoryData.h>

// OPERA factory bank: the factory sounds (Presets/Sounds) embedded in the binary, played by BANK and PATCH 1-8 in the
// order below, so the program buttons give Opera's own sounds even before the installer ran.
class FactoryBank {
public:
 static const juce::StringArray& order() {
  static const juce::StringArray names{
   // A: the first sounds
   "EPIANO - Tine Glow","BELL - Glass Tower","BASS - Rubber FM","BRASS - Gold Section",
   "ORGAN - Drawbars","KEYS - Clav Snap","MALLET - Marimba","PAD - Crystal Air"};
  return names;
 }
 static std::unique_ptr<juce::XmlElement> load(const juce::String& name) {
  for(int i=0;i<FactoryData::namedResourceListSize;++i)
   if(juce::String::fromUTF8(FactoryData::originalFilenames[i]).upToLastOccurrenceOf(".orbitpreset",false,false)==name){
    int size=0;const auto* data=FactoryData::getNamedResource(FactoryData::namedResourceList[i],size);
    return data?juce::XmlDocument::parse(juce::String::fromUTF8(data,size)):nullptr;}
  return {};
 }
};
