#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <map>
#include <optional>

using namespace juce;
#include "FactoryBank.h"
#include "PresetStorage.h"
#include "EnsembleChorus.h"
#include "Reverbs.h"
#include "CharacterDelay.h"
#include "ForceTouch.h"
#include "FMEngine.h"

// OPERA: a six-operator FM synthesizer, sixteen voices (FMEngine.h), then chorus, echo and reverb.
// The class keeps the name Orbit: the panel, preset and packaging code shared with the other SF Instruments synths use it.
class Orbit : public AudioProcessor, private AudioProcessorValueTreeState::Listener, private AudioProcessorListener {
public:
 AudioProcessorValueTreeState state;
 MidiKeyboardState keyboard;
 static String opId(int op,const char* what){return "op"+String(op+1)+what;} // op1level, op3t2 ...
 Orbit() : AudioProcessor(BusesProperties().withOutput("Stereo",AudioChannelSet::stereo(),true)), state(*this,nullptr,"OrbitState",parameters()) {
  for(auto* param:getParameters()) if(auto* identified=dynamic_cast<AudioProcessorParameterWithID*>(param)) state.addParameterListener(identified->paramID,this);
  state.state.setProperty("stateVersion",1,nullptr);
  for(auto& slot:ccTarget)slot.store(-1);addListener(this);
  for(int i=0;i<opera::operators;++i){auto& r=reads[size_t(i)];auto get=[&](const String& w){return state.getRawParameterValue(opId(i,w.toRawUTF8()));};
   r.coarse=get("coarse");r.fine=get("fine");r.detune=get("detune");r.level=get("level");r.velocity=get("vel");r.keyScale=get("keyscale");
   for(int k=0;k<4;++k){r.time[size_t(k)]=get("t"+String(k+1));r.lvl[size_t(k)]=get("l"+String(k+1));}}
 }
 ~Orbit() override {removeListener(this);for(auto* param:getParameters()) if(auto* identified=dynamic_cast<AudioProcessorParameterWithID*>(param)) state.removeParameterListener(identified->paramID,this);}
 String userPresetName() const {return state.state.getProperty("userPresetName").toString();}
 // User presets live in the user's library, so they survive moving or rebuilding the project.
 static File presetRoot(){return File::getSpecialLocation(File::userApplicationDataDirectory).getChildFile("Application Support").getChildFile("SF Instruments").getChildFile("Opera");}
 StringArray userPresetNames() const {return PresetStorage(presetRoot()).names();}
 // Sounds: complete presets shipped by the installer (shared library), plus any the user drops in their own Presets/Sounds.
 static File sharedRoot(){return File::getSpecialLocation(File::commonApplicationDataDirectory).getChildFile("Application Support").getChildFile("SF Instruments").getChildFile("Opera");}
 String soundName() const {return state.state.getProperty("soundName").toString();}
 StringArray soundNames() const {auto names=PresetStorage(sharedRoot(),"Sounds").names();names.mergeArray(PresetStorage(presetRoot(),"Sounds").names());names.sort(true);return names;}
 Result loadSound(const String& name){auto xml=PresetStorage(presetRoot(),"Sounds").load(name);if(!xml)xml=PresetStorage(sharedRoot(),"Sounds").load(name);if(!xml)return Result::fail("Son absent ou invalide dans Presets/Sounds.");
  MemoryBlock data;copyXmlToBinary(*xml,data);setStateInformation(data.getData(),int(data.getSize()));state.state.removeProperty("userPresetName",nullptr);state.state.setProperty("soundName",name,nullptr);modified.store(false);return Result::ok();}
 bool userPresetExists(const String& name) const {return PresetStorage(presetRoot()).exists(name);}
 Result saveUserPreset(const String& name,bool replace=false){auto saved=state.copyState();saved.setProperty("program",currentProgram.load(),nullptr);saved.setProperty("modified",false,nullptr);saved.setProperty("userPresetName",name,nullptr);saved.removeProperty("soundName",nullptr);
  auto xml=saved.createXml();auto result=PresetStorage(presetRoot()).save(name,*xml,replace);if(result.wasOk()){state.state.setProperty("userPresetName",name,nullptr);state.state.removeProperty("soundName",nullptr);modified.store(false);}return result;}
 Result loadUserPreset(const String& name){auto xml=PresetStorage(presetRoot()).load(name);if(!xml)return Result::fail("Preset absent, invalide ou inaccessible dans Presets/User.");MemoryBlock data;copyXmlToBinary(*xml,data);setStateInformation(data.getData(),int(data.getSize()));
  state.state.removeProperty("soundName",nullptr);state.state.setProperty("userPresetName",name,nullptr);modified.store(false);return Result::ok();}
 bool isPatchModified() const {return modified.load();}
 int voicesSounding() const {return shownVoices.load();}
 // RATIO: the coarse stops (0.5, then 1 to 31), as the panel and the host show them.
 static float coarseRatio(int index){return index<=0?.5f:float(index);}

 // Short names for the host and controller displays.
 static String label(const String& id,const String& longName){
  static const std::map<String,String> names{
   {"algorithm","Algorithm"},{"feedback","Feedback"},{"rateScale","Key Rate"},{"lfoRate","LFO Rate"},{"lfoDelay","LFO Delay"},{"lfoWave","LFO Wave"},{"lfoPitch","LFO Pitch"},
   {"lfoAmp","LFO Amp"},{"wheelDepth","Wheel Vib"},{"transpose","Transpose"},{"tune","Tune"},{"bendRange","Bend Range"},{"gain","Volume"},{"chorus","Chorus"},
   {"delayOn","Dly On"},{"delayTime","Dly Time"},{"delaySync","Dly Sync"},{"delayFeedback","Dly Fdbk"},{"delayMod","Dly Mod"},{"delayMix","Dly Level"},
   {"reverbOn","Rev On"},{"reverbType","Rev Type"},{"reverbSize","Rev Size"},{"reverbDamp","Rev Damp"},{"reverbColor","Rev Tone"},{"reverbMix","Rev Level"}};
  const auto found=names.find(id);if(found!=names.end())return found->second;
  if(id.startsWith("op")){ // op3level -> OP3 Level
   static const std::map<String,String> parts{{"coarse","Ratio"},{"fine","Fine"},{"detune","Detune"},{"level","Level"},{"vel","Vel"},{"keyscale","Key Scl"},
    {"t1","Time 1"},{"t2","Time 2"},{"t3","Time 3"},{"t4","Time 4"},{"l1","Level 1"},{"l2","Level 2"},{"l3","Level 3"},{"l4","Level 4"}};
   const auto part=parts.find(id.substring(3));if(part!=parts.end())return "OP"+id.substring(2,3)+" "+part->second;}
  return longName;
 }
 static AudioProcessorValueTreeState::ParameterLayout parameters() {
  AudioProcessorValueTreeState::ParameterLayout l;
  // Values read on the 0-10 scale printed around the knob (-5..+5 when centred on zero); times in ms or s, semitones, cents.
  auto add=[&](const String& id,const String& name,float lo,float hi,float def,float skew=1.f,float step=0.f){
   const NormalisableRange<float> range(lo,hi,step,skew);const bool centred=lo<0.f,whole=step>=1.f||hi>=100.f;
   const String unit=name.fromFirstOccurrenceOf("(",false,false).upToFirstOccurrenceOf(")",false,false);
   auto text=[range,centred,whole,unit](float v,int){
    if(unit=="st"||unit=="ct")return (v>0?"+":"")+String(roundToInt(v))+" "+unit;
    if(unit=="s")return v<1.f?String(roundToInt(v*1000.f))+" ms":String(v,2)+" s";
    const float mark=range.convertTo0to1(range.snapToLegalValue(v))*10.f-(centred?5.f:0.f);
    String shown=(centred&&mark>.04f?"+":"")+String(mark,1);
    if(unit.isNotEmpty())shown+=" ("+(whole?String(roundToInt(v)):String(v,std::abs(v)<1.f?2:1))+" "+unit+")";
    return shown;};
   l.add(std::make_unique<AudioParameterFloat>(ParameterID{id,1},label(id,name),range,def,AudioParameterFloatAttributes().withStringFromValueFunction(text)));};
  auto choice=[&](const String& id,const String& name,StringArray values,int initial=0){l.add(std::make_unique<AudioParameterChoice>(ParameterID{id,1},label(id,name),values,initial));};
  auto toggle=[&](const String& id,const String& name,bool initial){l.add(std::make_unique<AudioParameterBool>(ParameterID{id,1},label(id,name),initial));};
  // ALGORITHM and FEEDBACK first, then the six operators in turn, then the rest: a host page of eight walks through it.
  {StringArray names;for(int i=0;i<16;++i)names.add(String(i+1)+" "+opera::algorithms()[size_t(i)].name);choice("algorithm","Algorithm",names,0);}
  add("feedback","Feedback",0,1,0);
  StringArray ratios;for(int i=0;i<32;++i)ratios.add(i==0?String("0.5"):String(i));
  for(int i=0;i<opera::operators;++i){
   const bool first=i==0;const String op="OP"+String(i+1)+" ";
   choice(opId(i,"coarse"),op+"ratio",ratios,1);add(opId(i,"fine"),op+"fine",0,.99f,0);
   add(opId(i,"detune"),op+"detune (ct)",-10,10,0,1.f,1.f);add(opId(i,"level"),op+"level",0,1,first?.9f:0);
   add(opId(i,"vel"),op+"velocity",0,1,.3f);add(opId(i,"keyscale"),op+"key scaling",0,1,0);
   const float times[]={.003f,.4f,1.5f,.35f},levels[]={1,.75f,.6f,0};
   for(int k=0;k<4;++k)add(opId(i,("t"+String(k+1)).toRawUTF8()),op+"time "+String(k+1)+" (s)",.001f,20,times[k],.25f);
   for(int k=0;k<4;++k)add(opId(i,("l"+String(k+1)).toRawUTF8()),op+"level "+String(k+1),0,1,levels[k]);}
  add("rateScale","Key rate",0,1,.3f);
  add("lfoRate","LFO (Hz)",.05f,20,5.f,.33f);add("lfoDelay","LFO delay (s)",0,4,0,.5f);choice("lfoWave","LFO wave",{"SINE","TRIANGLE","SAW","SQUARE","S&H"});
  add("lfoPitch","LFO pitch",0,2,0,.6f);add("lfoAmp","LFO amp",0,1,0);add("wheelDepth","Wheel vibrato",0,2,.5f,.6f);
  add("transpose","Transpose (st)",-24,24,0,1.f,1.f);add("tune","Tune (ct)",-50,50,0,1.f,1.f);add("bendRange","Pitch bend (st)",0,12,2,1.f,1.f);
  add("gain","Output",0,1,.7f);add("chorus","Chorus",0,1,.3f);
  toggle("delayOn","Delay on",true);add("delayTime","Delay time (s)",.03f,1.5f,.3f);
  choice("delaySync","Delay sync",{"FREE","1/2","1/4 D","1/4","1/4 T","1/8 D","1/8","1/8 T","1/16 D","1/16","1/16 T"},5);
  add("delayFeedback","Delay feedback",0,1.1f,.35f);add("delayMod","Delay modulation",0,1,.15f);add("delayMix","Delay mix",0,.65f,0);
  toggle("reverbOn","Reverb on",true);choice("reverbType","Reverb type",{"VINT","PLATE","SPACE"},2);
  add("reverbSize","Reverb size",0,1,.55f);add("reverbDamp","Reverb damping",0,1,.45f);add("reverbColor","Reverb color",0,1,.5f);add("reverbMix","Reverb mix",0,.65f,.12f);
  return l;
 }
 const String getName() const override {return "Opera";}
 bool acceptsMidi() const override {return true;} bool producesMidi() const override {return false;}
 double getTailLengthSeconds() const override {return 20;}
 int getNumPrograms() override {return jmax(1,FactoryBank::order().size());} int getCurrentProgram() override {return currentProgram.load();}
 // BANK / PATCH: the embedded factory sounds. A patch change keeps the performance settings as they are.
 void setCurrentProgram(int index) override {
  if(!isPositiveAndBelow(index,FactoryBank::order().size())) return;
  if(auto xml=FactoryBank::load(FactoryBank::order()[index])){
   static const StringArray performance{"gain","bendRange","tune"};
   std::array<float,4> kept{};for(int i=0;i<performance.size();++i)kept[size_t(i)]=state.getParameter(performance[i])->getValue();
   MemoryBlock data;copyXmlToBinary(*xml,data);setStateInformation(data.getData(),int(data.getSize()));
   for(int i=0;i<performance.size();++i)state.getParameter(performance[i])->setValueNotifyingHost(kept[size_t(i)]);
   currentProgram.store(index);state.state.removeProperty("userPresetName",nullptr);state.state.removeProperty("soundName",nullptr);modified.store(false);}
 }
 const String getProgramName(int index) override {return isPositiveAndBelow(index,FactoryBank::order().size())?FactoryBank::order()[index].replace(" - "," / "):String("INIT");}
 void changeProgramName(int,const String&) override {}
 void prepareToPlay(double sr,int block) override {
  sampleRate=sr;blockCapacity=jmax(block,8192);chunkMidi.ensureSize(8192);
  synth.prepare(2.*sr);wheel=0;bend=0;lfoPhase=0;lfoFade=0;lfoHeld=0;
  voicePass.reset();voicePass.initProcessing(size_t(blockCapacity));voiceBuffer.setSize(1,blockCapacity);
  chorus.prepare(sr);echo.prepare(sr);reverb.prepare(sr);gain.reset(sr,.02);gain.setCurrentAndTargetValue(value("gain"));
 }
 void releaseResources() override {synth.reset();keyboard.reset();chorus.reset();echo.reset();reverb.reset();}
 bool isBusesLayoutSupported(const BusesLayout& b) const override {return b.getMainOutputChannelSet()==AudioChannelSet::stereo();}
 // A block larger than prepared is played in pieces of the prepared size: nothing is ever allocated while playing.
 void processBlock(AudioBuffer<float>& buffer,MidiBuffer& midi) override {
  const int total=buffer.getNumSamples();if(total<=blockCapacity){processPiece(buffer,midi);return;}
  for(int done=0;done<total;done+=blockCapacity){const int count=jmin(blockCapacity,total-done);
   AudioBuffer<float> piece(buffer.getArrayOfWritePointers(),buffer.getNumChannels(),done,count);
   chunkMidi.clear();chunkMidi.addEvents(midi,done,count,-done);processPiece(piece,chunkMidi);}
  midi.clear();
 }
 void processPiece(AudioBuffer<float>& buffer,MidiBuffer& midi){
  ScopedNoDenormals noDenormals;buffer.clear();keyboard.processNextMidiBuffer(midi,0,buffer.getNumSamples(),true);
  if(const int b=screenPitch.exchange(-1);b>=0)midi.addEvent(MidiMessage::pitchWheel(1,b),0);
  // MIDI learn and learned controls. The mod wheel, sustain, bank select and channel messages keep their own jobs.
  for(const auto event:midi){const auto message=event.getMessage();if(!message.isController())continue;
   const int cc=message.getControllerNumber();if(cc==0||cc==1||cc==32||cc==64||cc>=120)continue;
   if(learning.load()&&lastTouched.load()>=0){const int target=lastTouched.load();forget(target);ccTarget[size_t(cc)].store(target);learning.store(false);}
   const int target=ccTarget[size_t(cc)].load();if(isPositiveAndBelow(target,getParameters().size()))getParameters()[target]->setValueNotifyingHost(float(message.getControllerValue())/127.f);
  }
  readSettings();
  // The voices, sample-accurately between MIDI events, at twice the rate (FM makes many partials: room above them).
  const int n=buffer.getNumSamples();
  dsp::AudioBlock<float> voiceBlock(voiceBuffer.getArrayOfWritePointers(),1,size_t(n));voiceBlock.clear();
  auto up=voicePass.processSamplesUp(voiceBlock);float* twice=up.getChannelPointer(0);
  int at=0;auto renderTo=[&](int when){for(int i=2*at;i<2*when;++i){settings.lfoValue=lfoNext();twice[i]=synth.render(settings)*.25f;}at=jmax(at,when);};
  for(const auto event:midi){renderTo(jlimit(0,n,event.samplePosition));handle(event.getMessage());}
  renderTo(n);
  voicePass.processSamplesDown(voiceBlock);
  for(int c=0;c<buffer.getNumChannels();++c)buffer.copyFrom(c,0,voiceBuffer,0,0,n);
  shownVoices.store(synth.sounding());if(synth.sounding()==0)synthSilent=true;
  chorus.process(buffer,value("chorus")>.001f?2:0,value("chorus"));
  float delaySeconds=value("delayTime");
  if(const int syncStep=int(value("delaySync"));syncStep>0){double tempo=120;
   if(auto* playHead=getPlayHead())if(auto position=playHead->getPosition())if(auto hostBpm=position->getBpm())tempo=*hostBpm;
   static const double beats[]={0,2.,1.5,1.,2./3.,.75,.5,1./3.,.375,.25,1./6.};double seconds=beats[jlimit(1,10,syncStep)]*60./jmax(20.,tempo);
   while(seconds>4.)seconds*=.5;delaySeconds=float(seconds);}
  echo.process(buffer,delaySeconds,value("delayFeedback"),value("delayOn")>.5f?value("delayMix"):0.f,value("delayMod"));
  reverb.process(buffer,int(value("reverbType")),value("reverbSize"),value("reverbDamp"),value("reverbColor"),value("reverbOn")>.5f?value("reverbMix"):0.f);
  gain.setTargetValue(value("gain"));
  const auto limit=[](float x){constexpr float knee=.7f;const float a=std::abs(x);if(a<=knee)return x;return std::copysign(knee+(1.f-knee)*std::tanh((a-knee)/(1.f-knee)),x);};
  for(int i=0;i<n;++i){const float g=gain.getNextValue();for(int c=0;c<buffer.getNumChannels();++c)buffer.setSample(c,i,limit(buffer.getSample(c,i)*g));}
 }
 bool hasEditor() const override {return true;} AudioProcessorEditor* createEditor() override;
 void getStateInformation(MemoryBlock& dest) override {auto saved=state.copyState();saved.setProperty("program",currentProgram.load(),nullptr);saved.setProperty("modified",modified.load(),nullptr);auto xml=saved.createXml();
  auto* map=xml->createNewChildElement("MidiMap");for(int cc=0;cc<128;++cc)if(const int index=ccTarget[size_t(cc)].load();index>=0){auto* entry=map->createNewChildElement("Map");entry->setAttribute("cc",cc);entry->setAttribute("id",parameterId(index));}
  copyXmlToBinary(*xml,dest);}
 // ---- MIDI learn: one CC per control. Learning: touch a control on the panel, then move a knob on the controller.
 void startLearning(){lastTouched.store(-1);learning.store(true);}
 void stopLearning(){learning.store(false);}
 bool isLearning() const {return learning.load();}
 int lastTouchedParameter() const {return lastTouched.load();}
 int ccFor(int index) const {for(int cc=0;cc<128;++cc)if(ccTarget[size_t(cc)].load()==index)return cc;return -1;}
 void forget(int index){for(auto& slot:ccTarget)if(slot.load()==index)slot.store(-1);}
 void forgetAll(){for(auto& slot:ccTarget)slot.store(-1);}
 String parameterId(int index) const {if(auto* p=dynamic_cast<RangedAudioParameter*>(getParameters()[index]))return p->paramID;return {};}
 int parameterIndex(const String& id) const {for(int i=0;i<getParameters().size();++i)if(parameterId(i)==id)return i;return -1;}
 void setStateInformation(const void* data,int size) override {auto xml=getXmlFromBinary(data,size);if(!xml||!xml->hasTagName(state.state.getType()))return;
  // MIDI learn assignments belong to this instance, not to a sound: a session brings them back, a preset leaves them alone.
  if(auto* map=xml->getChildByName("MidiMap")){for(auto& slot:ccTarget)slot.store(-1);
   for(auto* entry:map->getChildIterator()){const int cc=entry->getIntAttribute("cc",-1);const int index=parameterIndex(entry->getStringAttribute("id"));if(isPositiveAndBelow(cc,128)&&index>=0)ccTarget[size_t(cc)].store(index);}
   xml->removeChildElement(map,true);}
  auto restored=ValueTree::fromXml(*xml);restored.setProperty("stateVersion",1,nullptr);
  currentProgram.store(jlimit(0,getNumPrograms()-1,int(restored.getProperty("program",0))));
  for(auto* base:getParameters())if(auto* parameter=dynamic_cast<RangedAudioParameter*>(base)){ // a setting the preset does not list goes back to its default
   bool found=false;for(auto child:restored)if(child.getProperty("id").toString()==parameter->paramID){found=true;break;}
   if(!found){ValueTree child("PARAM");child.setProperty("id",parameter->paramID,nullptr);child.setProperty("value",parameter->convertFrom0to1(parameter->getDefaultValue()),nullptr);restored.addChild(child,-1,nullptr);}}
  state.replaceState(restored);modified.store(bool(restored.getProperty("modified",false)));}
 public: std::atomic<int> screenPitch{-1};
 public: std::atomic<int> screenPressure{-1}; // the panel keyboard's trackpad force (not used by this instrument)
private:
 float value(const char* id) const {return state.getRawParameterValue(id)->load();}
 struct Reads{std::atomic<float> *coarse=nullptr,*fine=nullptr,*detune=nullptr,*level=nullptr,*velocity=nullptr,*keyScale=nullptr;std::array<std::atomic<float>*,4> time{},lvl{};};
 void readSettings(){
  for(int i=0;i<opera::operators;++i){const auto& r=reads[size_t(i)];auto& o=settings.op[size_t(i)];
   o.ratio=coarseRatio(int(r.coarse->load()));o.fine=r.fine->load();o.detune=r.detune->load();o.level=r.level->load();o.velocity=r.velocity->load();o.keyScale=r.keyScale->load();
   for(int k=0;k<4;++k){o.time[size_t(k)]=r.time[size_t(k)]->load();o.level4[size_t(k)]=r.lvl[size_t(k)]->load();}}
  settings.algorithm=int(value("algorithm"));settings.feedback=value("feedback");settings.rateScale=value("rateScale");
  settings.transpose=value("transpose");settings.tune=value("tune");settings.bend=bend*value("bendRange");
  settings.pitchDepth=value("lfoPitch")+wheel*value("wheelDepth");settings.ampDepth=value("lfoAmp");
  lfoStep=value("lfoRate")/(2.*sampleRate);lfoShape=int(value("lfoWave"));lfoRise=1.f/jmax(1.f,value("lfoDelay")*float(2.*sampleRate));
 }
 // The LFO, shared by all the voices; after a silence it fades in over LFO DELAY.
 float lfoNext(){
  lfoPhase+=lfoStep;if(lfoPhase>=1.){lfoPhase-=1.;lfoHeld=random.nextFloat()*2.f-1.f;}
  lfoFade=synthSilent?0.f:jmin(1.f,lfoFade+lfoRise);
  const float t=float(lfoPhase);float w=0;
  switch(lfoShape){case 0:w=std::sin(MathConstants<float>::twoPi*t);break;case 1:w=1.f-4.f*std::abs(t-.5f);break;case 2:w=1.f-2.f*t;break;case 3:w=t<.5f?1.f:-1.f;break;default:w=lfoHeld;}
  return w*lfoFade;
 }
 void handle(const MidiMessage& m){
  if(m.isNoteOn()){synth.noteOn(m.getNoteNumber(),m.getFloatVelocity(),settings);synthSilent=false;}
  else if(m.isNoteOff())synth.noteOff(m.getNoteNumber(),settings);
  else if(m.isSustainPedalOn())synth.sustain(true,settings);else if(m.isSustainPedalOff())synth.sustain(false,settings);
  else if(m.isAllNotesOff()||m.isAllSoundOff())synth.allOff(settings);
  else if(m.isPitchWheel()){bend=float(m.getPitchWheelValue()-8192)/8192.f;settings.bend=bend*value("bendRange");}
  else if(m.isController()&&m.getControllerNumber()==1){wheel=float(m.getControllerValue())/127.f;settings.pitchDepth=value("lfoPitch")+wheel*value("wheelDepth");}
 }
 void parameterChanged(const String&,float) override {modified.store(true);}
 void audioProcessorParameterChanged(AudioProcessor*,int,float) override {}
 void audioProcessorChanged(AudioProcessor*,const AudioProcessorListener::ChangeDetails&) override {}
 void audioProcessorParameterChangeGestureBegin(AudioProcessor*,int index) override {lastTouched.store(index);} // the control just touched on the panel
 std::array<std::atomic<int>,128> ccTarget;std::atomic<bool> learning{false};std::atomic<int> lastTouched{-1};
 std::atomic<bool> modified{false};std::atomic<int> currentProgram{0},shownVoices{0};
 double sampleRate=44100;int blockCapacity=8192;MidiBuffer chunkMidi;
 std::array<Reads,opera::operators> reads{};opera::Settings settings;opera::Synth synth;float wheel=0,bend=0;
 double lfoPhase=0,lfoStep=0;int lfoShape=0;float lfoFade=0,lfoRise=0,lfoHeld=0;bool synthSilent=true;Random random{6};
 AudioBuffer<float> voiceBuffer;dsp::Oversampling<float> voicePass{1,1,dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,true,false};
 EnsembleChorus chorus;CharacterDelay echo;OrbitReverb reverb;SmoothedValue<float> gain;
};
#include "Editor.h"
AudioProcessorEditor* Orbit::createEditor(){return new Editor(*this);}
AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new Orbit();}
