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
#include "Arpeggiator.h"
#include "ParamLayer.h"

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
  auto raw=[this](const char* id){return state.getRawParameterValue(id);};
  g.algorithm=raw("algorithm");g.feedback=raw("feedback");g.rateScale=raw("rateScale");g.transpose=raw("transpose");g.tune=raw("tune");
  g.bendRange=raw("bendRange");g.lfoPitch=raw("lfoPitch");g.wheelDepth=raw("wheelDepth");g.lfoAmp=raw("lfoAmp");
  liveLayer.build(state,getParameters());recordHeld.ensureStorageAllocated(256);recordChord.ensureStorageAllocated(256);
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
  MemoryBlock data;copyXmlToBinary(*xml,data);loadingPreset.store(true);setStateInformation(data.getData(),int(data.getSize()));loadingPreset.store(false);state.state.removeProperty("userPresetName",nullptr);state.state.setProperty("soundName",name,nullptr);modified.store(false);return Result::ok();}
 bool userPresetExists(const String& name) const {return PresetStorage(presetRoot()).exists(name);}
 Result saveUserPreset(const String& name,bool replace=false){auto saved=state.copyState();saved.setProperty("sequence",sequenceText(),nullptr);saved.setProperty("program",currentProgram.load(),nullptr);saved.setProperty("modified",false,nullptr);saved.setProperty("userPresetName",name,nullptr);saved.removeProperty("soundName",nullptr);
  auto xml=saved.createXml();auto result=PresetStorage(presetRoot()).save(name,*xml,replace);if(result.wasOk()){state.state.setProperty("userPresetName",name,nullptr);state.state.removeProperty("soundName",nullptr);modified.store(false);}return result;}
 Result loadUserPreset(const String& name){auto xml=PresetStorage(presetRoot()).load(name);if(!xml)return Result::fail("Preset absent, invalide ou inaccessible dans Presets/User.");MemoryBlock data;copyXmlToBinary(*xml,data);loadingPreset.store(true);setStateInformation(data.getData(),int(data.getSize()));loadingPreset.store(false);
  state.state.removeProperty("soundName",nullptr);state.state.setProperty("userPresetName",name,nullptr);modified.store(false);return Result::ok();}
 bool isPatchModified() const {return modified.load();}
 int voicesSounding() const {return shownVoices.load();}
 // ---- SEQUENCER: the recorded sequence (up to 64 steps of chords or rests), saved with the patch and the session.
 String sequenceText(){return sequenceCopy().toString();}
 // SEQUENCE MEMORIES: eight slots kept apart from the sounds (Presets/Sequences/SEQ n.txt), so a sequence can be
 // played with any patch. The current slot is shown on the panel; a new recording or a patch's own sequence clears it.
 static constexpr int sequenceSlots=8;
 static File sequenceFile(int slot){return presetRoot().getChildFile("Presets").getChildFile("Sequences").getChildFile("SEQ "+String(slot+1)+".txt");}
 bool sequenceSlotUsed(int slot) const {return sequenceFile(slot).existsAsFile();}
 int sequenceSlotLength(int slot) const {Arpeggiator::Sequence probe;probe.fromString(sequenceFile(slot).loadFileAsString());return probe.length;}
 bool storeSequence(int slot){if(!isPositiveAndBelow(slot,sequenceSlots)||sequenceLength()==0)return false;const auto file=sequenceFile(slot);file.getParentDirectory().createDirectory();
  if(!file.replaceWithText(sequenceText()))return false;currentSequenceSlot.store(slot);return true;}
 bool loadSequence(int slot){if(!isPositiveAndBelow(slot,sequenceSlots)||!sequenceSlotUsed(slot))return false;const auto text=sequenceFile(slot).loadFileAsString();
  Arpeggiator::Sequence loaded;loaded.fromString(text);{const SpinLock::ScopedLockType lock(sequenceLock);sequence=loaded;sequenceSteps.store(sequence.length);}currentSequenceSlot.store(slot);return true;}
 int sequenceSlot() const {return currentSequenceSlot.load();}
 int sequencePosition() const {return arp.position.load();}
 // Step editing from the panel (message thread). editStep: the selected step, -1 when the editor is closed.
 std::atomic<int> editStep{-1};std::atomic<bool> editAdds{false},editLane{false}; // editLane: false = SEQ (recorded) line, true = OVD line
 Arpeggiator::Sequence sequenceCopy(){const SpinLock::ScopedLockType lock(sequenceLock);return sequence;}
 void editSequence(const std::function<void(Arpeggiator::Sequence&)>& change){{const SpinLock::ScopedLockType lock(sequenceLock);change(sequence);sequenceSteps.store(sequence.length);}currentSequenceSlot.store(-1);}
 std::atomic<int> currentSequenceSlot{-1};
 int sequenceLength() const {return sequenceSteps.load();}
 std::atomic<int> sequenceSteps{0};
 // RATIO: the coarse stops (0.5, then 1 to 31), as the panel and the host show them.
 static float coarseRatio(int index){return index<=0?.5f:float(index);}

 // Short names for the host and controller displays.
 static String label(const String& id,const String& longName){
  static const std::map<String,String> names{
   {"algorithm","Algorithm"},{"feedback","Feedback"},{"rateScale","Key Rate"},{"lfoRate","LFO Rate"},{"lfoDelay","LFO Delay"},{"lfoWave","LFO Wave"},{"lfoPitch","LFO Pitch"},
   {"lfoAmp","LFO Amp"},{"wheelDepth","Wheel Vib"},{"transpose","Transpose"},{"tune","Tune"},{"bendRange","Bend Range"},{"gain","Volume"},{"chorus","Chorus"},
   {"delayOn","Dly On"},{"delayTime","Dly Time"},{"delaySync","Dly Sync"},{"delayFeedback","Dly Fdbk"},{"delayMod","Dly Mod"},{"delayMix","Dly Level"},
   {"reverbOn","Rev On"},{"reverbType","Rev Type"},{"reverbSize","Rev Size"},{"reverbDamp","Rev Damp"},{"reverbColor","Rev Tone"},{"reverbMix","Rev Level"},
   {"arpOn","Arp On"},{"arpSync","Arp Sync"},{"arpTempo","Arp Tempo"},{"arpGate","Arp Gate"},{"arpDivision","Arp Step"},{"arpPattern","Arp Mode"},{"arpOctaves","Arp Range"},
   {"seqRec","Seq Rec"},{"seqRest","Seq Rest"},{"seqPlay","Seq Play"},{"seqOvd","Seq Overdub"},{"seqLive","Seq Live"}};
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
  // ARPEGGIO and SEQUENCER (the family's: REC, REST, PLAY, overdub and LIVE), on one clock.
  toggle("arpOn","Arpeggiator",false);toggle("arpSync","Host tempo",false);add("arpTempo","Arp tempo (BPM)",40,300,120,1.f,1.f);add("arpGate","Arp gate",.1f,.95f,.65f);
  choice("arpDivision","Arp division",{"1/4","1/8","1/16","1/32"},1);choice("arpPattern","Arp pattern",{"UP","DOWN","UP/DOWN","RANDOM"});choice("arpOctaves","Arp octaves",{"1 OCT","2 OCT","3 OCT"});
  toggle("seqRec","Seq record",false);toggle("seqRest","Seq rest",false);toggle("seqPlay","Seq play",false);
  toggle("seqOvd","Seq overdub",false); // the REC key's second state
  toggle("seqLive","Seq live",false);   // LIVE: while the sequence plays, the keys play over it with another sound
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
   static const StringArray performance{"gain","bendRange","tune","arpOn","arpSync","arpTempo","arpGate","arpDivision","arpPattern","arpOctaves"};
   std::array<float,16> kept{};for(int i=0;i<performance.size();++i)kept[size_t(i)]=state.getParameter(performance[i])->getValue();
   MemoryBlock data;copyXmlToBinary(*xml,data);loadingPreset.store(true);setStateInformation(data.getData(),int(data.getSize()));loadingPreset.store(false);
   for(int i=0;i<performance.size();++i)state.getParameter(performance[i])->setValueNotifyingHost(kept[size_t(i)]);
   currentProgram.store(index);state.state.removeProperty("userPresetName",nullptr);state.state.removeProperty("soundName",nullptr);modified.store(false);}
 }
 const String getProgramName(int index) override {return isPositiveAndBelow(index,FactoryBank::order().size())?FactoryBank::order()[index].replace(" - "," / "):String("INIT");}
 void changeProgramName(int,const String&) override {}
 void prepareToPlay(double sr,int block) override {
  sampleRate=sr;blockCapacity=jmax(block,8192);chunkMidi.ensureSize(8192);
  synth.prepare(2.*sr);liveSynth.prepare(2.*sr);arp.prepare(sr);lfoTrace.assign(size_t(2*blockCapacity),0.f);wasLayered=wasRecording=wasResting=wasOverdubbing=liveFresh=false;recordHeld.clearQuick();recordChord.clearQuick();wheel=0;bend=0;lfoPhase=0;lfoFade=0;lfoHeld=0;
  voicePass.reset();voicePass.initProcessing(size_t(blockCapacity));voiceBuffer.setSize(1,blockCapacity);
  chorus.prepare(sr);echo.prepare(sr);reverb.prepare(sr);gain.reset(sr,.02);gain.setCurrentAndTargetValue(value("gain"));
 }
 void releaseResources() override {synth.reset();liveSynth.reset();arp.reset();keyboard.reset();chorus.reset();echo.reset();reverb.reset();}
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
  ScopedNoDenormals noDenormals;
  bool restored=false;
  if(liveLayer.applyPending(restored)){restoredLive.store(restored);liveLayerReady.store(restored);synth.reset();liveSynth.reset();arp.reset();wasLayered=wasRecording=wasResting=wasOverdubbing=liveFresh=false;recordHeld.clearQuick();recordChord.clearQuick();}
  buffer.clear();keyboard.processNextMidiBuffer(midi,0,buffer.getNumSamples(),true);
  if(const int b=screenPitch.exchange(-1);b>=0)midi.addEvent(MidiMessage::pitchWheel(1,b),0);
  // MIDI learn and learned controls. The mod wheel, sustain, bank select and channel messages keep their own jobs.
  for(const auto event:midi){const auto message=event.getMessage();if(!message.isController())continue;
   const int cc=message.getControllerNumber();if(cc==0||cc==1||cc==32||cc==64||cc>=120)continue;
   if(learning.load()&&lastTouched.load()>=0){const int target=lastTouched.load();forget(target);ccTarget[size_t(cc)].store(target);learning.store(false);}
   const int target=ccTarget[size_t(cc)].load();if(isPositiveAndBelow(target,getParameters().size()))getParameters()[target]->setValueNotifyingHost(float(message.getControllerValue())/127.f);
  }
  // ARPEGGIO / SEQUENCER clock: the arpeggio TEMPO, or the host's, lined up on its beat grid while it plays.
   double bpm=value("arpTempo"),samplesToGrid=-1;const int division=int(value("arpDivision"));
  if(value("arpSync")>.5f) if(auto* playHead=getPlayHead()) if(auto position=playHead->getPosition()) if(auto hostBpm=position->getBpm()) {
   bpm=*hostBpm;
   // While the host plays, line the steps up with its beat grid (quarter, eighth or sixteenth notes).
   if(position->getIsPlaying()&&bpm>1) if(auto beats=position->getPpqPosition()) {
    const double steps[]={1.,.5,.25,.125},step=steps[jlimit(0,3,division)],halfSample=.5*bpm/(60.*sampleRate);
    double into=std::fmod(*beats,step);if(into<0)into+=step;
    samplesToGrid=(into<halfSample||step-into<halfSample)?0.:(step-into)*60./bpm*sampleRate;
   }
  }
  // SEQUENCER. REC while stopped starts a new sequence: each time every key is up again, the keys pressed since become
  // one step; REST adds a silent step. REC while PLAY runs is OVERDUB: each key pressed joins the step being played.
  // EDIT (from the panel): the next note or chord replaces (or joins) the selected step, then the next step is selected.
  // PLAY runs the arpeggiator clock over the sequence instead of the pattern; a held key transposes it.
  const bool rec=value("seqRec")>.5f,rest=value("seqRest")>.5f,play=value("seqPlay")>.5f;
  const bool ovd=value("seqOvd")>.5f,recording=rec&&!play&&!ovd,overdub=ovd||(rec&&play),playing=play||ovd;
  Arpeggiator::Sequence playingSequence;
  {const SpinLock::ScopedLockType lock(sequenceLock);
   if(recording&&!wasRecording){sequence.clear();recordHeld.clearQuick();recordChord.clearQuick();currentSequenceSlot.store(-1);}
   const int editing=editStep.load();
   // EDIT works while the sequence plays too: the keys are then heard and written, not used to transpose.
   if(recording||editing>=0){for(const auto meta:midi){const auto msg=meta.getMessage();
     if(msg.isNoteOn()){recordHeld.addIfNotAlreadyThere((msg.getChannel()-1)*128+msg.getNoteNumber());recordChord.addIfNotAlreadyThere(msg.getNoteNumber());}
     else if(msg.isNoteOff()){recordHeld.removeFirstMatchingValue((msg.getChannel()-1)*128+msg.getNoteNumber());
      if(recordHeld.isEmpty()&&!recordChord.isEmpty()){recordChord.sort();
       if(recording)sequence.add(recordChord.getRawDataPointer(),recordChord.size());
       else{const int selected=editStep.load();const bool lane=editLane.load();if(editAdds.load()){for(int n:recordChord)sequence.merge(selected,n,lane);}else sequence.setLane(selected,recordChord.getRawDataPointer(),recordChord.size(),lane);
        editStep.store(jmin(selected+1,jmax(0,sequence.length-1)));}
       recordChord.clearQuick();}}}
    if(recording&&rest&&!wasResting&&recordHeld.isEmpty())sequence.add(nullptr,0);}
   else{recordHeld.clearQuick();recordChord.clearQuick();}
   // LIVE RECORDING: overdub on an empty sequence starts a blank 16-step loop; what is played lands on it, quantized to
   // the nearest step, on the SEQ line (on the OVD line when the loop already held notes).
   if(overdub&&!wasOverdubbing){currentSequenceSlot.store(-1);liveFresh=sequence.length==0;if(liveFresh)for(int i=0;i<16;++i)sequence.add(nullptr,0);}
   if(!overdub&&wasOverdubbing&&liveFresh){sequence.updateRoot();liveFresh=false;}
   wasRecording=recording;wasOverdubbing=overdub;wasResting=rest;
   playingSequence=sequence;sequenceSteps.store(sequence.length);}
  const bool live=playing&&value("seqLive")>.5f;
  arp.process(midi,buffer.getNumSamples(),value("arpOn")>.5f||playing,bpm,division,playing?4:int(value("arpPattern")),int(value("arpOctaves"))+1,value("arpGate"),samplesToGrid,&playingSequence,overdub||editStep.load()>=0||live,overdub,live);
  if(arp.capturedCount>0){const SpinLock::ScopedLockType lock(sequenceLock);for(int i=0;i<arp.capturedCount;++i)sequence.merge(arp.capturedStep[size_t(i)],arp.capturedNote[size_t(i)],!liveFresh);
   sequenceSteps.store(sequence.length);}
  // LIVE: the sequence plays on its own sound, frozen when LIVE (or PLAY with LIVE on) started; the keys play the panel's.
  const bool layered=live;
  if(layered&&!wasLayered){if(!restoredLive.exchange(false))liveLayer.capture();liveLayerReady.store(true);}
  if(!layered){restoredLive.store(false);liveLayerReady.store(false);}
  if(!layered&&wasLayered)for(int ch=1;ch<=16;++ch)arp.sequenceOutput.addEvent(MidiMessage::allNotesOff(ch),0); // the frozen sound's notes end and ring out
  wasLayered=layered;
  readSettings(settings,nullptr);
  if(layered||liveSynth.sounding()>0)readSettings(liveSettings,&liveLayer); // LIVE: the sequence's frozen sound
  lfoStep=value("lfoRate")/(2.*sampleRate);lfoShape=int(value("lfoWave"));lfoRise=1.f/jmax(1.f,value("lfoDelay")*float(2.*sampleRate));
  // The voices, sample-accurately between MIDI events, at twice the rate (FM makes many partials: room above them).
  const int n=buffer.getNumSamples();
  dsp::AudioBlock<float> voiceBlock(voiceBuffer.getArrayOfWritePointers(),1,size_t(n));voiceBlock.clear();
  auto up=voicePass.processSamplesUp(voiceBlock);float* twice=up.getChannelPointer(0);
  int at=0;auto renderTo=[&](int when){for(int i=2*at;i<2*when;++i){const float lfo=lfoNext();lfoTrace[size_t(i)]=lfo;settings.lfoValue=lfo;twice[i]=synth.render(settings)*.25f;}at=jmax(at,when);};
  for(const auto event:midi){renderTo(jlimit(0,n,event.samplePosition));handle(event.getMessage());}
  renderTo(n);
  // LIVE: the sequence on its own sound, mixed before the effects (one chorus, echo and reverb for both).
  if(layered||liveSynth.sounding()>0||!arp.sequenceOutput.isEmpty()){at=0;
   auto liveTo=[&](int when){for(int i=2*at;i<2*when;++i){liveSettings.lfoValue=lfoTrace[size_t(i)];twice[i]+=liveSynth.render(liveSettings)*.25f;}at=jmax(at,when);};
   for(const auto event:arp.sequenceOutput){liveTo(jlimit(0,n,event.samplePosition));const auto m=event.getMessage();
    if(m.isNoteOn()){liveSynth.noteOn(m.getNoteNumber(),m.getFloatVelocity(),liveSettings);synthSilent=false;}else if(m.isNoteOff())liveSynth.noteOff(m.getNoteNumber(),liveSettings);
    else if(m.isAllNotesOff()||m.isAllSoundOff())liveSynth.allOff(liveSettings);}
   liveTo(n);}
  arp.sequenceOutput.clear();
  voicePass.processSamplesDown(voiceBlock);
  for(int c=0;c<buffer.getNumChannels();++c)buffer.copyFrom(c,0,voiceBuffer,0,0,n);
  shownVoices.store(synth.sounding()+liveSynth.sounding());if(synth.sounding()+liveSynth.sounding()==0)synthSilent=true;
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
 void getStateInformation(MemoryBlock& dest) override {auto saved=state.copyState();saved.setProperty("sequence",sequenceText(),nullptr);saved.setProperty("program",currentProgram.load(),nullptr);saved.setProperty("modified",modified.load(),nullptr);auto xml=saved.createXml();
  auto* map=xml->createNewChildElement("MidiMap");for(int cc=0;cc<128;++cc)if(const int index=ccTarget[size_t(cc)].load();index>=0){auto* entry=map->createNewChildElement("Map");entry->setAttribute("cc",cc);entry->setAttribute("id",parameterId(index));}
  // LIVE running: the sequence's frozen sound goes with the session.
  if(value("seqLive")>.5f&&(value("seqPlay")>.5f||value("seqOvd")>.5f)){
   bool queued=false,queuedLive=false;const auto frozen=liveLayer.forSave(queued,queuedLive);const bool frozenReady=queued?queuedLive:liveLayerReady.load();
   auto* live=xml->createNewChildElement("LiveSound");
   for(auto* base:getParameters())if(auto* p=dynamic_cast<RangedAudioParameter*>(base)){auto* child=live->createNewChildElement("PARAM");child->setAttribute("id",p->paramID);child->setAttribute("value",frozenReady?liveLayer.get(frozen,state.getRawParameterValue(p->paramID)):state.getRawParameterValue(p->paramID)->load());}}
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
  std::unique_ptr<XmlElement> liveSound;
  if(auto* live=xml->getChildByName("LiveSound")){if(!loadingPreset.load())liveSound=std::make_unique<XmlElement>(*live);xml->removeChildElement(live,true);}
  auto restored=ValueTree::fromXml(*xml);restored.setProperty("stateVersion",1,nullptr);
   // A sound or preset changed on stage keeps the sequencer keys as they are; while the sequence runs it also keeps
   // the sequence and its clock, so the loop goes on with the new sound.
   if(loadingPreset.load()){auto value=[this](const String& id){return state.getRawParameterValue(id)->load();};const bool running=value("seqPlay")>.5f||value("seqOvd")>.5f;
    StringArray kept{"seqRec","seqRest","seqPlay","seqOvd","seqLive"};if(running){kept.addArray(StringArray{"arpTempo","arpDivision","arpGate","arpSync"});restored.removeProperty("sequence",nullptr);}
    for(const auto& id:kept){ValueTree child;for(auto c:restored)if(c.getProperty("id").toString()==id){child=c;break;}
     if(!child.isValid()){child=ValueTree("PARAM");child.setProperty("id",id,nullptr);restored.addChild(child,-1,nullptr);}
     child.setProperty("value",value(id),nullptr);}}
   // A patch saved with a sequence brings it back; one saved without leaves the current sequence alone.
   if(restored.hasProperty("sequence")){Arpeggiator::Sequence loaded;loaded.fromString(restored.getProperty("sequence").toString());{const SpinLock::ScopedLockType lock(sequenceLock);sequence=loaded;sequenceSteps.store(sequence.length);currentSequenceSlot.store(-1);}restored.removeProperty("sequence",nullptr);}
   // Sessions and presets saved before state version 2 stored chorus mode as 0 = I, 1 = II.
  currentProgram.store(jlimit(0,getNumPrograms()-1,int(restored.getProperty("program",0))));
  for(auto* base:getParameters())if(auto* parameter=dynamic_cast<RangedAudioParameter*>(base)){ // a setting the preset does not list goes back to its default
   bool found=false;for(auto child:restored)if(child.getProperty("id").toString()==parameter->paramID){found=true;break;}
   if(!found){ValueTree child("PARAM");child.setProperty("id",parameter->paramID,nullptr);child.setProperty("value",parameter->convertFrom0to1(parameter->getDefaultValue()),nullptr);restored.addChild(child,-1,nullptr);}}
  state.replaceState(restored);
  // A session brings back the sequence's own LIVE sound; a sound or preset loaded on stage leaves it as it is.
  if(!loadingPreset.load()){auto frozen=liveLayer.panel();
   if(liveSound)for(auto* child:liveSound->getChildIterator())if(auto* p=state.getParameter(child->getStringAttribute("id"))){const float v=float(child->getDoubleAttribute("value"));if(std::isfinite(v)){const auto& range=p->getNormalisableRange();liveLayer.assign(frozen,state.getRawParameterValue(p->paramID),jlimit(range.start,range.end,v));}}
   liveLayer.queue(frozen,liveSound!=nullptr);}
  modified.store(bool(restored.getProperty("modified",false)));}
 public: std::atomic<int> screenPitch{-1};
 public: std::atomic<int> screenPressure{-1}; // the panel keyboard's trackpad force (not used by this instrument)
private:
 float value(const char* id) const {return state.getRawParameterValue(id)->load();}
 struct Reads{std::atomic<float> *coarse=nullptr,*fine=nullptr,*detune=nullptr,*level=nullptr,*velocity=nullptr,*keyScale=nullptr;std::array<std::atomic<float>*,4> time{},lvl{};};
 struct Globals{std::atomic<float> *algorithm=nullptr,*feedback=nullptr,*rateScale=nullptr,*transpose=nullptr,*tune=nullptr,*bendRange=nullptr,*lfoPitch=nullptr,*wheelDepth=nullptr,*lfoAmp=nullptr;};
 // The sound's settings, from the panel (layer null) or from LIVE's frozen copy.
 void readSettings(opera::Settings& out,const ParamLayer* layer){
  auto get=[layer](const std::atomic<float>* a){return layer!=nullptr?layer->get(a):a->load();};
  for(int i=0;i<opera::operators;++i){const auto& r=reads[size_t(i)];auto& o=out.op[size_t(i)];
   o.ratio=coarseRatio(int(get(r.coarse)));o.fine=get(r.fine);o.detune=get(r.detune);o.level=get(r.level);o.velocity=get(r.velocity);o.keyScale=get(r.keyScale);
   for(int k=0;k<4;++k){o.time[size_t(k)]=get(r.time[size_t(k)]);o.level4[size_t(k)]=get(r.lvl[size_t(k)]);}}
  out.algorithm=int(get(g.algorithm));out.feedback=get(g.feedback);out.rateScale=get(g.rateScale);
  out.transpose=get(g.transpose);out.tune=get(g.tune);out.bend=bend*get(g.bendRange);
  out.pitchDepth=get(g.lfoPitch)+wheel*get(g.wheelDepth);out.ampDepth=get(g.lfoAmp);
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
 std::array<Reads,opera::operators> reads{};Globals g;opera::Settings settings,liveSettings;opera::Synth synth,liveSynth;std::vector<float> lfoTrace;
 Arpeggiator arp;Arpeggiator::Sequence sequence;SpinLock sequenceLock;Array<int> recordHeld,recordChord;bool wasRecording=false,wasResting=false,wasOverdubbing=false,liveFresh=false,wasLayered=false;
 std::atomic<bool> loadingPreset{false},restoredLive{false},liveLayerReady{false};ParamLayer liveLayer;
 float wheel=0,bend=0;
 double lfoPhase=0,lfoStep=0;int lfoShape=0;float lfoFade=0,lfoRise=0,lfoHeld=0;bool synthSilent=true;Random random{6};
 AudioBuffer<float> voiceBuffer;dsp::Oversampling<float> voicePass{1,1,dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,true,false};
 EnsembleChorus chorus;CharacterDelay echo;OrbitReverb reverb;SmoothedValue<float> gain;
};
#include "Editor.h"
AudioProcessorEditor* Orbit::createEditor(){return new Editor(*this);}
AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new Orbit();}
